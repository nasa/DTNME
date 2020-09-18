/*
 *    Copyright 2007 Intel Corporation
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

#include <stdarg.h>

#include <algorithm>

#include "../util/Functors.h"
#include "../util/StringAppender.h"
#include "../thread/Lock.h"

#include "LockDebugger.h"

#if OASYS_DEBUG_LOCKING_ENABLED

namespace oasys {

//----------------------------------------------------------------------------
void
LockDebugger::add_lock(Lock* lock)
{
    LockVector::iterator i;
    i = std::find_if(locks_held_.begin(), locks_held_.end(),
                     oasys::eq_functor(lock, &Ent::lock));

    if (i == locks_held_.end())
    {
        locks_held_.push_back(Ent(lock, 1));
    }
    else
    {
        ++i->count_;
    }
}

//----------------------------------------------------------------------------
void
LockDebugger::remove_lock(Lock* lock)
{
    LockVector::iterator i;
    i = std::find_if(locks_held_.begin(), locks_held_.end(),
                     oasys::eq_functor(lock, &Ent::lock));

    ASSERT(i != locks_held_.end());
    --i->count_;
    ASSERT(i->count_ >= 0);
    if (i->count_ == 0)
    {
        locks_held_.erase(i);
    }
}

//----------------------------------------------------------------------------
int
LockDebugger::format(char* buf, size_t len) const
{
    StringAppender sa(buf, len);

    bool first = true;
    for (LockVector::const_iterator i = locks_held_.begin();
         i != locks_held_.end(); ++i)
    {
        sa.appendf("%s[%p: %d %s]",
                   first ? "" : " ",
                   i->lock_, i->count_, i->lock_->lock_holder_name());
        first = false;
    }

    return sa.desired_length();
}

//----------------------------------------------------------------------------
bool
LockDebugger::check_n(unsigned int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (unsigned int i=0; i<n; ++i)
    {
        Lock* lock = va_arg(ap, Lock*);

        // Check both the lock and our own lock list
        if (! lock->is_locked_by_me())
        {
            log_err_p("/lock",
                      "Lock class=%s should be held, but "
                      "instead is held by %s in a different thread.",
                      lock->lock_class(), lock->lock_holder_name());
            Breaker::break_here();

            return false;
        }
        LockVector::const_iterator itr;
        itr = std::find_if(locks_held_.begin(), locks_held_.end(),
                         oasys::eq_functor(lock, &Ent::lock));

        if (itr == locks_held_.end())
        {
            log_err_p("/lock",
                      "Lock class=%s should be held, but "
                      "instead is held by %s in a different thread.",
                      lock->lock_class(), lock->lock_holder_name());
            Breaker::break_here();

            return false;
        }
        ASSERT(itr->count_ > 0);
    }
    va_end(ap);

    if (locks_held_.size() != n)
    {
        log_err_p("/lock", "Holding %zu locks but expected %u. Lock vector: *%p",
                  locks_held_.size(), n, this);
        Breaker::break_here();

        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
bool
LockDebugger::check() {
    if (locks_held_.size() != 0)
    {
        log_err_p("/lock", "Holding %zu locks but expected 0. Lock vector: *%p",
                  locks_held_.size(), this);
        Breaker::break_here();

        return false;
    }
    return true;
}
bool
LockDebugger::check(Lock* l1) {
    return check_n(1, l1);
}
bool
LockDebugger::check(Lock* l1, Lock* l2) {
    return check_n(2, l1, l2);
}
bool
LockDebugger::check(Lock* l1, Lock* l2, Lock* l3) {
    return check_n(3, l1, l2, l3);
}
bool
LockDebugger::check(Lock* l1, Lock* l2, Lock* l3, Lock* l4) {
    return check_n(4, l1, l2, l3, l4);
}
bool
LockDebugger::check(Lock* l1, Lock* l2, Lock* l3, Lock* l4, Lock* l5) {
    return check_n(5, l1, l2, l3, l4, l5);
}
bool
LockDebugger::check(Lock* l1, Lock* l2, Lock* l3, Lock* l4,
                    Lock* l5, Lock* l6)
{
    return check_n(6, l1, l2, l3, l4, l5, l6);
}

//----------------------------------------------------------------------------
bool
LockDebugger::check_class(const char* c)
{
    for (LockVector::const_iterator i = locks_held_.begin();
         i != locks_held_.end(); ++i)
    {
        if (strcmp(i->lock_->lock_class(), c) == 0)
        {
            return true;
        }
    }

    return false;
}

} // namespace oasys

#endif /* OASYS_DEBUG_LOCKING_ENABLED */

