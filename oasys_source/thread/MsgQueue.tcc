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

/*!
 * \file
 *
 * NOTE: This file is included by MsgQueue.h and should _not_ be
 * included in the regular Makefile build because of template
 * instantiation issues. As of this time, g++ does not have a good,
 * intelligent way of managing template instantiations. The other
 * route to go is to use -fno-implicit-templates and manually
 * instantiate template types. That however would require changing a
 * lot of the existing code, so we will just bear the price of
 * redundant instantiations for now.
 */

template<typename _elt_t>
MsgQueue<_elt_t>::MsgQueue(const char* logpath, SpinLock* lock, bool delete_lock)
    : Notifier(logpath), delete_lock_(delete_lock), notify_when_empty_(false)
{
    logpath_appendf("/msgqueue");
    
    if (lock != NULL) {
        lock_ = lock;
    } else {
        lock_ = new SpinLock();
    }
}

template<typename _elt_t>
MsgQueue<_elt_t>::~MsgQueue()
{
    if (size() != 0)
    {
        log_err("not empty at time of destruction, size=%zu",
                queue_.size());
    }
    
    if (delete_lock_)
        delete lock_;
    
    lock_ = 0;
}

template<typename _elt_t> 
void MsgQueue<_elt_t>::push(_elt_t msg, bool at_back)
{
    ScopeLock l(lock_, "MsgQueue::push");

    /*
     * It's important that we first call notify() and then put the
     * message on the queue. This is because notify might need to
     * release the lock and sleep in the rare case where the pipe is
     * full. Therefore, we wait until the notification is safely in
     * the pipe before putting the message on the queue, with the lock
     * held for both calls to ensure atomicity.
     */
    if (notify_when_empty_ == false || queue_.empty()) {
        notify(lock_);
    }
    
    if (at_back)
        queue_.push_back(msg);
    else
        queue_.push_front(msg);
}

template<typename _elt_t> 
_elt_t MsgQueue<_elt_t>::pop_blocking()
{
    ScopeLock l(lock_, "MsgQueue::pop_blocking");

    /*
     * If the queue is empty, wait for new input.
     */
    bool used_wait = false;

    if (queue_.empty()) {
        
        wait(lock_);
        ASSERT(lock_->is_locked_by_me());
        used_wait = true;
    }

    /*
     * Can't be empty now.
     */
    ASSERT(!queue_.empty());

    _elt_t elt  = queue_.front();
    queue_.pop_front();
    
    if (!used_wait && (notify_when_empty_ == false || queue_.empty())) {
        drain_pipe(1);
    }
    
    return elt;
}

template<typename _elt_t> 
bool MsgQueue<_elt_t>::try_pop(_elt_t* eltp)
{
    ScopeLock lock(lock_, "MsgQueue::try_pop");

    // nothing in the queue, nothing we can do...
    if (queue_.size() == 0) {
        return false;
    }
    
    // but if there is something in the queue, then return it
    *eltp = queue_.front();
    queue_.pop_front();
    
    if (notify_when_empty_ == false || queue_.empty()) {
        drain_pipe(1);
    }
        
    return true;
}

template <typename _elt_t>
void MsgQueue<_elt_t>::notify_when_empty()
{
    ScopeLock lock(lock_, "MsgQueue::notify_when_empty");

    // the queue must be empty or assertions won't hold
    ASSERT(queue_.size() == 0);
    notify_when_empty_ = true;
}

