/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>

#include "thread/Lock.h"
#include "io/IO.h"

#include "OnOffNotifier.h"
#include "Lock.h"

namespace oasys {

//----------------------------------------------------------------------------
OnOffNotifier::OnOffNotifier(const char* logpath, bool quiet)
    : Logger("OnOffNotifier", "%s", (logpath == 0) ? "" : logpath), 
      waiter_(false),
      quiet_(quiet),
      active_(false)
{ 
    if (logpath == 0)
    { 
        logpathf("/notifier");
    }
    else
    {
        logpath_appendf("/notifier");
    }

    if (pipe(pipe_) != 0) {
        PANIC("can't create pipe for notifier");
    }

    if (!quiet_) {
        log_debug("created pipe, fds: %d %d", pipe_[0], pipe_[1]);
    }
    
    for (int n = 0; n < 2; ++n) {
        if (IO::set_nonblocking(pipe_[n], true, quiet ? 0 : logpath_) != 0) 
        {
            PANIC("error setting fd %d to nonblocking: %s",
                  pipe_[n], strerror(errno));
        }
    }
}

//----------------------------------------------------------------------------
OnOffNotifier::~OnOffNotifier()
{
    if (!quiet_) 
    {
        log_debug("OnOffNotifier shutting down (closing fds %d %d)",
                  pipe_[0], 
                  pipe_[1]);
    }
}

//----------------------------------------------------------------------------
bool
OnOffNotifier::wait(Lock* lock, int timeout)
{ 
    notifier_lock_.lock("OnOffNotifier::wait");
    if (waiter_) 
    {
        PANIC("OnOffNotifier doesn't support multiple waiting threads");
    }
    if (!quiet_)
    {
        log_debug("wait() on %s notifier", active_ ? "active" : "inactive");
    }

    if (active_)
    {
        notifier_lock_.unlock();
        return true;
    }
    else
    {
        waiter_ = true;
        
        notifier_lock_.unlock();

        if (lock) {
            lock->unlock();
        }
        int ret = IO::poll_single(read_fd(), POLLIN, 0, timeout, 0, logpath_);
        if (lock) {
            lock->lock("OnOffNotifier::wait()");
        }

        notifier_lock_.lock("OnOffNotifier::wait");
        waiter_ = false;
        notifier_lock_.unlock();
        
        if (ret < 0 && ret != IOTIMEOUT) 
        {
            PANIC("fatal: error return from notifier poll: %s",
                  strerror(errno));
        }
        else if (ret == IOTIMEOUT) 
        {
            if (! quiet_) {
                log_debug("wait() timeout");
            }
            return false;
        }
        else
        {
            if (! quiet_) { 
                log_debug("wait() notified");
            }
        }
        return true;
    }
}

//----------------------------------------------------------------------------
void
OnOffNotifier::signal()
{
    ScopeLock l(&notifier_lock_, "OnOffNotifier::signal");
    if (active_)
    {
        return;
    }
    else
    {
        int cc = write(pipe_[1], "+", 1);
        ASSERT(cc == 1);
        active_ = true;
    }
}

//----------------------------------------------------------------------------
void
OnOffNotifier::clear()
{
    ScopeLock l(&notifier_lock_, "OnOffNotifier::clear");

    if (active_)
    {
        char buf[2];
        int cc = read(pipe_[0], &buf, 1);
        ASSERT(cc == 1);
        active_ = false;
    }
    else
    {
        return;
    }
}

} // namespace oasys
