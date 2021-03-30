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


#ifndef _OASYS_SPINLOCK_H_
#define _OASYS_SPINLOCK_H_

#include "Lock.h"

namespace oasys {

/**
 * A SpinLock is a Lock that busy waits to get a lock. The
 * implementation supports recursive locking.
 */
class SpinLock : public Lock {
public:

public:
    SpinLock(const char* classname = "GENERIC") 
        : Lock(classname), lock_waiters_(0) {}

    virtual ~SpinLock() {}

    /// @{
    /// Virtual override from Lock
    int lock(const char* lock_user);
    int unlock();
    int try_lock(const char* lock_user);
    /// @}

    /// Control whether or not to log warnings if the spin lock is
    /// under contention
    static bool warn_on_contention_;
    
private:
    atomic_t lock_waiters_; 	///< count of waiting threads

#ifndef NDEBUG
public:
    static atomic_t total_spins_;	///< debugging variable
    static atomic_t total_yields_;	///< debugging variable
#endif
};

} // namespace oasys

#endif /* _OASYS_SPINLOCK_H_ */
