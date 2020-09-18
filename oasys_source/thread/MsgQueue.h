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


#ifndef _OASYS_MSG_QUEUE_H_
#define _OASYS_MSG_QUEUE_H_

#include <queue>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>

#include "Notifier.h"
#include "SpinLock.h"
#include "../debug/Log.h"
#include "../debug/DebugUtils.h"

namespace oasys {

/**
 * A producer/consumer queue for passing data between threads in the
 * system, using the Notifier functionality to block and wakeup
 * threads.
 */
template<typename _elt_t>
class MsgQueue : public Notifier {
public:    
    /*!
     * Constructor.
     */
    MsgQueue(const char* logpath, 
             SpinLock* lock = NULL, 
             bool delete_lock = true);
        
    /**
     * Destructor.
     */
    ~MsgQueue();

    /**
     * Atomically add msg to the back of the queue and signal a
     * waiting thread.
     */
    void push(_elt_t msg, bool at_back = true);
    
    /**
     * Atomically add msg to the front of the queue, and signal
     * waiting threads.
     */
    void push_front(_elt_t msg)
    {
        push(msg, false);
    }

    /**
     * Atomically add msg to the back of the queue, and signal
     * waiting threads.
     */
    void push_back(_elt_t msg)
    {
        push(msg, true);
    }

    /**
     * Block and pop msg from the queue.
     */
    _elt_t pop_blocking();

    /**
     * Try to pop a msg from the queue, but don't block. Return
     * true if there was a message on the queue, false otherwise.
     */
    bool try_pop(_elt_t* eltp);
    
    /**
     * \return Size of the queue.
     */
    size_t size()
    {
        ScopeLock l(lock_, "MsgQueue::size");
        return queue_.size();
    }

    /**
     * Change the semantics of the queue such that notify() is only
     * called if the queue is empty.
     *
     * This assumes that the user of the queue will completely drain
     * it before blocking on the notifier, unlike the simple model of
     * calling pop_blocking() each time.
     *
     * This can be used in situations where a large number of messages
     * may potentially be queued, such that calling notify() on each
     * one might fill up the pipe.
     */
    void notify_when_empty();

protected:
    SpinLock*          lock_;
    std::deque<_elt_t> queue_;
    bool               delete_lock_;
    bool               notify_when_empty_;
};

#include "MsgQueue.tcc"

} // namespace oasys

#endif //_OASYS_MSG_QUEUE_H_
