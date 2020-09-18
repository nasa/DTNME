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


#ifndef _OASYS_ATOMIC_X86_H_
#define _OASYS_ATOMIC_X86_H_

#include "../debug/DebugUtils.h"
#include "../compat/inttypes.h"

namespace oasys {

/**
 * The definition of atomic_t for x86 is just a wrapper around the
 * value, since we have enough synchronization support in the
 * architecture.
 */
struct atomic_t {
    atomic_t(u_int32_t v = 0) : value(v) {}

    volatile u_int32_t value;
};

/**
 * When we're not on a SMP platform, there's no need to lock the bus.
 */
#ifndef NO_SMP
#define LOCK "lock ; "
#else
#define LOCK ""
#endif

/**
 * Atomic addition function.
 *
 * @param i     integer value to add
 * @param v     pointer to current value
 * 
 */
static inline void
atomic_add(volatile atomic_t *v, u_int32_t i)
{
    __asm__ __volatile__(
        LOCK "addl %1,%0"
        :"=m" (v->value)
        :"ir" (i), "m" (v->value));
}

/**
 * Atomic subtraction function.
 *
 * @param i     integer value to subtract
 * @param v     pointer to current value
 
 */
static inline void
atomic_sub(volatile atomic_t* v, u_int32_t i)
{
    __asm__ __volatile__(
        LOCK "subl %1,%0"
        :"=m" (v->value)
        :"ir" (i), "m" (v->value));
}

/**
 * Atomic increment.
 *
 * @param v     pointer to current value
 */
static inline void
atomic_incr(volatile atomic_t* v)
{
    __asm__ __volatile__(
        LOCK "incl %0"
        :"=m" (v->value)
        :"m" (v->value));
}

/**
 * Atomic decrement.
 *
 * @param v     pointer to current value
 * 
 */ 
static inline void
atomic_decr(volatile atomic_t* v)
{
    __asm__ __volatile__(
        LOCK "decl %0"
        :"=m" (v->value)
        :"m" (v->value));
}

/**
 * Atomic decrement and test.
 *
 * @return true if the value zero after the decrement, false
 * otherwise.
 *
 * @param v     pointer to current value
 * 
 */ 
static inline bool
atomic_decr_test(volatile atomic_t* v)
{
    unsigned char c;
    
    __asm__ __volatile__(
        LOCK "decl %0; sete %1"
        :"=m" (v->value), "=qm" (c)
        :"m" (v->value) : "memory");

    return (c != 0);
}

/**
 * Atomic compare and swap. Stores the new value iff the current value
 * is the expected old value.
 *
 * @param v     pointer to current value
 * @param o     old value to compare against
 * @param n     new value to store
 *
 * @return      the value of v before the swap
 */
static inline u_int32_t
atomic_cmpxchg32(volatile atomic_t* v, u_int32_t o, u_int32_t n)
{
    __asm__ __volatile__(
        LOCK "cmpxchgl %1, %2"
        : "+a" (o)
        : "r" (n), "m" (v->value)
        : "memory");
    
    return o;
}

/**
 * Atomic increment function that returns the new value. Note that the
 * implementation loops until it can successfully do the operation and
 * store the value, so there is an extremely low chance that this will
 * never return.
 *
 * @param v     pointer to current value
 */
static inline u_int32_t
atomic_incr_ret(volatile atomic_t* v)
{
    register volatile u_int32_t o, n;
    
#if defined(NDEBUG) && NDEBUG == 1
    while (1)
#else
    register int j;
    for (j = 0; j < 1000000; ++j)
#endif
    {
        o = v->value;
        n = o + 1;
        if (atomic_cmpxchg32(v, o, n) == o)
            return n;
    }
    
    NOTREACHED;
    return 0;
}

/**
 * Atomic addition function that returns the new value. Note that the
 * implementation loops until it can successfully do the operation and
 * store the value, so there is an extremely low chance that this will
 * never return.
 *
 * @param v     pointer to current value
 * @param i     integer to add
 */
static inline u_int32_t
atomic_add_ret(volatile atomic_t* v, u_int32_t i)
{
    register u_int32_t o, n;
    
#if defined(NDEBUG) && NDEBUG == 1
    while (1)
#else
    register int j;
    for (j = 0; j < 1000000; ++j)
#endif
    {
        o = v->value;
        n = o + i;
        if (atomic_cmpxchg32(v, o, n) == o)
            return n;
    }
    
    NOTREACHED;
    return 0;
}

} // namespace oasys

#endif /* _OASYS_ATOMIC_X86_H_ */
