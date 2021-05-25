/*
 *    Copyright 2007-2008 Darren Long, darren.long@mac.com
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

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

#include "io/NetUtils.h"
#include "util/Random.h"
#include "debug/Log.h"
#include "debug/DebugUtils.h"

#include "AX25ConnectedModeServer.h"

namespace oasys {

/*****************************************************************************/
//                  Class AX25ConnectedModeServer Implementation
/*****************************************************************************/

AX25ConnectedModeServer::AX25ConnectedModeServer(const char* logbase)
    : AX25Socket(SOCK_SEQPACKET, logbase)
{
    //params_.reuseaddr_ = 1;
}

int
AX25ConnectedModeServer::listen()
{
    logf(LOG_DEBUG, "listening");
    ASSERT(fd_ != -1);

    if (::listen(fd_, SOMAXCONN) == -1) {
        logf(LOG_ERR, "error in listen(): %s",strerror(errno));
        return -1;
    }
    
    set_state(LISTENING);
    return 0;
}
    
int
AX25ConnectedModeServer::accept(int *fd, std::string* remote_addr)
{
    ASSERTF(state_ == LISTENING,
            "accept() expected state LISTENING, not %s", statetoa(state_));
    
    struct full_sockaddr_ax25 sax25;
    socklen_t sl = sizeof(sax25);
    memset(&sax25, 0, sizeof(sax25));
    int ret = ::accept(fd_, (sockaddr*)&sax25, &sl);
    if (ret == -1) {
        if (errno != EINTR)
            logf(LOG_ERR, "error in accept(): %s", strerror(errno));
        return ret;
    }
    
    *fd = ret;
    *remote_addr = 
        ::ax25_ntoa((const ax25_address*)sax25.fsa_ax25.sax25_call.ax25_call);  
    monitor(IO::ACCEPT, 0); // XXX/bowei

    return 0;
}

int
AX25ConnectedModeServer::timeout_accept(int *fd, std::string* remote_addr,
                          int timeout_ms)
{
    int ret = poll_sockfd(POLLIN, NULL, timeout_ms);

    if (ret != 1) return ret;
    ASSERT(ret == 1);

    ret = accept(fd, remote_addr);

    if (ret < 0) {
        return IOERROR;
    }

    monitor(IO::ACCEPT, 0); // XXX/bowei

    return 0; 
}

AX25ConnectedModeServerThread::AX25ConnectedModeServerThread(const char* name,
                                 const char* logbase,
                                 int         flags)
    : AX25ConnectedModeServer(logbase), Thread(name, flags)
{
    // assign the notifier to be used for interrupt in the
    // IOHandlerBase
    set_notifier(new Notifier(logpath()));
}

AX25ConnectedModeServerThread::~AX25ConnectedModeServerThread()
{
    stop();
}

void
AX25ConnectedModeServerThread::run()
{
    int fd;
    std::string addr;
    
    log_debug("server thread %p running", this);

    while (1) {
        // check if someone has told us to quit by setting the
        // should_stop flag. if so, we're all done.
        if (should_stop())
            break;

        // now call poll() to wait forever for a new connection to
        // accept or an indication that we should stop
        short revents = 0;
        int ret = IO::poll_single(AX25ConnectedModeServer::fd(), POLLIN, &revents, -1,
                                  get_notifier(), logpath());
        if (ret == IOINTR) {
            ASSERT(should_stop());
            break;
         }

        if (ret <= 0) {
            logf(LOG_ERR, "error %d in poll(): %d %s",
                 ret, errno, strerror(errno));
            close();
            break;
        }
                
        ret = accept(&fd, &addr);

        if (ret != 0) {
            if (errno == EINTR)
                continue;
            
            logf(LOG_ERR, "error %d in accept(): %d %s",
                 ret, errno, strerror(errno));
            close();
            break;
        }

        logf(LOG_DEBUG, "accepted connection fd %d from %s",
             fd, addr.c_str());

        accepted(fd, addr);
    }

    log_debug("server thread %p exiting", this);    
}

void
AX25ConnectedModeServerThread::stop()
{
    bool finished = false;
 
    set_should_stop();

    if (is_stopped()) {
        finished = true;
    } else {
        interrupt_from_io();

        // wait for 10 seconds (i.e. 20 sleep periods)
        for (int i = 0; i < 20; ++i) {            
            if ((finished = is_stopped()))
                break;
            usleep(500000);
        }
    }

    if (!finished) {
        log_err("AX25ConnectedModeServer thread didn't die after 10 seconds");
    } else {
        // call subclass-specific shutdown hook
        shutdown_hook();
    }
}

int
AX25ConnectedModeServerThread::bind_listen_start(std::string& axport, 
                                                 std::string& local_call)
{
    if (bind(axport,local_call) != 0)
        return -1;
    
    if (listen() != 0)
        return -1;
    
    start();
    
    return 0;
}

} // namespace oasys

#endif /* #ifdef OASYS_AX25_ENABLED  */
