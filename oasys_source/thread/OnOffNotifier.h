/*
 *    Copyright 2006 Intel Corporation
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


#ifndef _OASYS_ONOFFNOTIFIER_H_
#define _OASYS_ONOFFNOTIFIER_H_

#include "SpinLock.h"
#include "../debug/Log.h"
#include "../debug/Logger.h"

namespace oasys {

class Lock;

/**
 * OnOffNotifier is a binary state synchronization object. It has two
 * states, signalled and not signalled. When a OnOffNotifier becomes
 * active, threads that was blocked waiting for the OnOffNotifier becomes
 * unblocked. Also, any subsequent calls to wait on an active OnOffNotifier
 * returns immediately.
 *
 * When inactive, threads block waiting for the OnOffNotifier.
 */
class OnOffNotifier : public Logger {
public:
    /**
     * Constructor that takes the logging path and an optional boolean
     * to suppress all logging.
     */
    OnOffNotifier(const char* logpath = 0, bool keep_quiet = true);

    /**
     * Destructor
     */
    ~OnOffNotifier();

    /**
     * Block the calling thread, pending a switch to the active
     * state. 
     *
     * @param lock If a lock is passed in, wait() will unlock the lock
     * before the thread blocks and re-take it when the end
     * unblocks.
     *
     * @param timeout Timeout in milliseconds.
     *
     * Returns true if the thread was notified, false if a timeout
     * occurred.
     */
    bool wait(Lock* lock = 0, int timeout = -1);

    /**
     * Switch to active state. Please note that there is not
     * guarrantee that when signal_OnOffNotifier returns, all of the
     * threads waiting on the OnOffNotifier have been released.
     */
    void signal();
    
    /**
     * Clear the signaled state.
     */
    void clear();
    
    /**
     * @return the read side of the pipe
     */ 
    int read_fd() { return pipe_[0]; }
    
protected:
    bool waiter_; // for debugging only
    bool quiet_;  // no logging

    SpinLock notifier_lock_;
    bool     active_;
    int      pipe_[2];
};

} // namespace oasys

#endif /* _OASYS_ONOFFNOTIFIER_H_ */
