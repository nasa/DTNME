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


/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _OASYS_NOTIFIER_H_
#define _OASYS_NOTIFIER_H_

#include "../debug/Log.h"
#include "Lock.h"

namespace oasys {

class SpinLock;

/**
 * Thread notification abstraction that wraps an underlying pipe. This
 * can be used as a generic abstraction to pass messages between
 * threads.
 *
 * A call to notify() sends a byte down the pipe, while calling wait()
 * causes the thread to block in poll() waiting for some data to be
 * sent on the pipe.
 *
 * In addition, through read_fd() a thread can get the file descriptor
 * to explicitly block on while awaiting notification. In that case,
 * the calling thread should explicitly call drain_pipe().
 */
class Notifier : public Logger {
public:
    /**
     * Constructor that takes the logging path and an optional boolean
     * to suppress all logging.
     */
    Notifier(const char* logpath, bool keep_quiet = false);

    /**
     * Destructor
     */
    ~Notifier();

    /**
     * Block the calling thread, pending a call to notify(). If a lock
     * is passed in, wait() will unlock the lock before the thread
     * blocks and re-take it when the thread unblocks. If a timeout is
     * specified, only wait for that amount of time.
     *
     * Returns true if the thread was notified, false if a timeout
     * occurred.
     */
    bool wait(SpinLock* lock      = NULL, 
              int timeout         = -1, 
              bool drain_the_pipe = true);

    /**
     * Notify a waiter.
     *
     * In general, this function should not block, but there's a
     * chance that it might if the pipe ends up full. In that case, it
     * will unlock the given lock (if any) and will block until the
     * notification ends up in the pipe.
     */
    void notify(SpinLock* lock = NULL);

    /**
     * @param bytes Drain this many bytes from the pipe. 0 means to
     *     drain all of the bytes possible in the pipe. The default is to
     *     drain 1 byte. Anything different will probably be a race
     *     condition unless there is some kind of locking going on.
     */
    void drain_pipe(size_t bytes);

    /**
     * The read side of the pipe, suitable for an explicit call to
     * poll().
     */
    int read_fd() { return pipe_[0]; }

    /**
     * The write side of the pipe.
     */
    int write_fd() { return pipe_[1]; }
    
protected:
    bool waiter_; // for debugging only
    int  count_;  // for debugging as well
    int  pipe_[2];
    bool quiet_;  // no logging
    u_int32_t pending_;
    atomic_t busy_notifiers_; // keep track notifications in progress
};

} // namespace oasys

#endif /* _OASYS_NOTIFIER_H_ */
