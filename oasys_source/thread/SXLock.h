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

#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#include <cstdio>

#include "Lock.h"
#include "SpinLock.h"
#include "Mutex.h"

namespace oasys {

/*!
 * Lock with shared and exclusive semantics.
 *
 * XXX/bowei -- convert this to use atomic instructions directly
 */
class SXLock {
public:
    SXLock(Lock* lock)
        : lock_(lock),
          scount_(0),
          xcount_(0)
    {}

    /*! 
     * Acquire a shared lock. Any number of readers are permitted
     * inside a shared lock.
     */
    void shared_lock() {
        lock_->lock();
        while (xcount_ > 0) {
            lock_->unlock();
            Thread::spin_yield();
            lock_->lock();
        }
        ++scount_;
        lock_->unlock();
    }

    //! Drop the shared lock.
    void shared_unlock() {
        lock_->lock();
        --scount_;
        lock_->unlock();
    }

    /*! 
     * Acquire the write lock. Only one writer is permitted to hold
     * the write lock. No readers are allowed inside when the write
     * lock is held.
     */
    void exclusive_lock() {
        lock_->lock();
        while (xcount_ > 0 && scount_ > 0) {
            lock_->unlock();
            Thread::spin_yield();
            lock_->lock();
        }
        ++xcount_;

        if (xcount_ != 1) {
            fprintf(stderr, "more than 1 writer (%d writers) entered lock!!", 
                    xcount_);
            exit(-1);
        }

        lock_->unlock();
    }

    //! Drop the write lock.
    void exclusive_unlock() {
        lock_->lock();

        --xcount_;
        if (xcount_ != 0) {
            fprintf(stderr, "more than 1 writer (%d writers) entered lock!!", 
                    xcount_);
            exit(-1);
        }

        lock_->unlock();        
    }
    
private:
    Lock* lock_;

    int scount_;
    int xcount_;
};

#define SCOPE_LOCK_DEFUN(_name, _fcn)                   \
class ScopeLock_ ## _name {                             \
    ScopeLock_ ## _name (SXLock*     rw_lock,           \
                         const char* lock_user)         \
        : rw_lock_(rw_lock)                             \
    {                                                   \
        do_lock(lock_user);                             \
    }                                                   \
                                                        \
    ScopeLock_ ## _name (ScopePtr<SXLock> rw_lock,      \
                         const char*      lock_user)    \
        : rw_lock_(rw_lock.ptr())                       \
    {                                                   \
        do_lock(lock_user);                             \
    }                                                   \
                                                        \
    ~ScopeLock_ ## _name ()                             \
    {                                                   \
        if (rw_lock_) {                                 \
            do_unlock();                                \
        }                                               \
    }                                                   \
                                                        \
private:                                                \
    SXLock* rw_lock_;                                   \
                                                        \
    void do_lock(const char* lock_user) {               \
        rw_lock_->_fcn ## _lock();                      \
    }                                                   \
                                                        \
    void do_unlock() {                                  \
        rw_lock_->_fcn ## _unlock();                    \
    }                                                   \
};

/*! @{ 
 * Define ScopeLock_Shared and ScopeLock_Exclusive.
 */
SCOPE_LOCK_DEFUN(Shared,    shared);
SCOPE_LOCK_DEFUN(Exclusive, exclusive);
//! @}
#undef SCOPE_LOCK_DEFUN

} // namespace oasys

#endif /* __RWLOCK_H__ */
