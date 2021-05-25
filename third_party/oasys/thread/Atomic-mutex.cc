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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#ifdef OASYS_ATOMIC_MUTEX

#include "Atomic-mutex.h"
#include "Mutex.h"

namespace oasys {

/**
 * To implement atomic operations without assembly support at
 * userland, we rely on a single global mutex.
 */
Mutex g_atomic_mutex("/XXX/ATOMIC_MUTEX_UNUSED_LOGGER",
                     Mutex::TYPE_FAST,
                     true /* keep_quiet */);

/**
 * Global accessor to the singleton atomic mutex.
 */
Mutex* atomic_mutex() { return &g_atomic_mutex; }

//----------------------------------------------------------------------
void
atomic_add(volatile atomic_t *v, u_int32_t i)
{
    ScopeLock l(atomic_mutex(), "atomic_add");
    v->value += i;
}

//----------------------------------------------------------------------
void
atomic_sub(volatile atomic_t* v, u_int32_t i)
{
    ScopeLock l(atomic_mutex(), "atomic_sub");
    v->value -= i;
}

//----------------------------------------------------------------------
void
atomic_incr(volatile atomic_t* v)
{
    ScopeLock l(atomic_mutex(), "atomic_incr");
    v->value++;
}

//----------------------------------------------------------------------
void
atomic_decr(volatile atomic_t* v)
{
    ScopeLock l(atomic_mutex(), "atomic_decr");
    v->value--;
}

//----------------------------------------------------------------------
bool
atomic_decr_test(volatile atomic_t* v)
{
    ScopeLock l(atomic_mutex(), "atomic_decr_test");
    v->value--;
    return (v->value == 0);
}

//----------------------------------------------------------------------
u_int32_t
atomic_cmpxchg32(volatile atomic_t* v, u_int32_t o, u_int32_t n)
{
    ScopeLock l(atomic_mutex(), "atomic_cmpxchg32");
    u_int32_t ret = v->value;

    if (v->value == o) {
        v->value = n;
    }

    return ret;
}

//----------------------------------------------------------------------
u_int32_t
atomic_incr_ret(volatile atomic_t* v)
{
    ScopeLock l(atomic_mutex(), "atomic_incr_ret");
    v->value++;
    return v->value;
}

//----------------------------------------------------------------------
u_int32_t
atomic_add_ret(volatile atomic_t* v, u_int32_t i)
{
    ScopeLock l(atomic_mutex(), "atomic_add_ret");
    v->value += i;
    return v->value;
}

} // namespace oasys

#endif /* OASYS_ATOMIC_MUTEX */
