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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "SpinLock.h"

#include "../debug/StackTrace.h"
#include "../thread/LockDebugger.h"

namespace oasys {

bool     SpinLock::warn_on_contention_(true);
#ifndef NDEBUG
atomic_t SpinLock::total_spins_(0);
atomic_t SpinLock::total_yields_(0);
#endif

int
SpinLock::lock(const char* lock_user)
{
    if (is_locked_by_me()) {
        lock_count_.value++;

#if OASYS_DEBUG_LOCKING_ENABLED
        // Thread::lock_debugger()->add_lock(this);
        if (Thread::lock_debugger() != NULL ) {
            Thread::lock_debugger()->add_lock(this);  // Only do this if the calling
                                                      // thread is an oasys thread.
        }
#endif

        return 0;
    }

    atomic_incr(&lock_waiters_);
    
    int nspins = 0;
    (void)nspins;
    while (atomic_cmpxchg32(&lock_count_, 0, 1) != 0)
    {
        Thread::spin_yield();
        
#ifndef NDEBUG
#    if OASYS_DEBUG_LOCKING_ENABLED
        atomic_incr(&total_spins_);
        if (warn_on_contention_ && ++nspins > 1000000) {
            fprintf(stderr,
                    "warning: %s is waiting for spin lock held by %s, which has reached spin limit\n",
                    lock_user, lock_holder_name_);
            StackTrace::print_current_trace(false);
            nspins = 0;
        }
#    endif
#endif
    }

    atomic_decr(&lock_waiters_);

    ASSERT(lock_count_.value == 1);

    lock_holder_      = Thread::current();
    lock_holder_name_ = lock_user;

#if OASYS_DEBUG_LOCKING_ENABLED
    if (Thread::lock_debugger() != NULL ) {
        Thread::lock_debugger()->add_lock(this);  // Only do this if the calling
                                                  // thread is an oasys thread.
    }
#endif

    return 0;
};

int
SpinLock::unlock()
{
    ASSERT(is_locked_by_me());

    if (lock_count_.value > 1) {
        lock_count_.value--;

#if OASYS_DEBUG_LOCKING_ENABLED
        // Thread::lock_debugger()->remove_lock(this);
        if (Thread::lock_debugger() != NULL ) {
            Thread::lock_debugger()->remove_lock(this);  // Only do this if the calling
                                                         // thread is an oasys thread.
        }
#endif

        return 0;
    }

#if OASYS_DEBUG_LOCKING_ENABLED
    // Thread::lock_debugger()->remove_lock(this);
    if (Thread::lock_debugger() != NULL ) {
        Thread::lock_debugger()->remove_lock(this);   // Only do this if the calling
                                                      // thread is an oasys thread.
    }
#endif

    lock_holder_      = 0;
    lock_holder_name_ = 0;
    lock_count_.value = 0;
    
    if (lock_waiters_.value != 0) {
#ifndef NDEBUG
        atomic_incr(&total_yields_);
#endif
        Thread::spin_yield();
    }


    return 0;
};
 
int
SpinLock::try_lock(const char* lock_user)
{
    if (is_locked_by_me()) {
        lock_count_.value++;
        return 0;
    }

    int got_lock = atomic_cmpxchg32(&lock_count_, 0, 1);
    
    if (0 == got_lock) 
    {
        ASSERT(lock_holder_ == 0);

        lock_holder_      = Thread::current();
        lock_holder_name_ = lock_user;

#ifdef OASYS_DEBUG_LOCKING_ENABLED
        // Thread::lock_debugger()->add_lock(this);
        if (Thread::lock_debugger() != NULL ) {
            Thread::lock_debugger()->add_lock(this);  // Only do this if the calling
                                                      // thread is an oasys thread.
        }
#endif
        
        return 0; // success        
    } 
    else 
    {
        return 1; // already locked
    }
};

} // namespace oasys
