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

#ifndef __ATOMIC_WIN32_H__
#define __ATOMIC_WIN32_H__

#include <Windows.h>

#include "../compat/inttypes.h"

struct atomic_t {
    atomic_t(LONG l = 0) : value(l) {}

    volatile LONG value;
};

/**
 * Atomic addition function.
 *
 * @param i	integer value to add
 * @param v	pointer to current value
 * 
 */
static inline void
atomic_add(volatile atomic_t *v, int32_t i)
{
    InterlockedExchangeAdd(&v->value, i);
}

/**
 * Atomic subtraction function.
 *
 * @param i	integer value to subtract
 * @param v	pointer to current value
 
 */
// static inline void
// atomic_sub(volatile atomic_t* v, int32_t i)
// {
//     __asm__ __volatile__(
//         LOCK "subl %1,%0"
//         :"=m" (v->value)
//         :"ir" (i), "m" (v->value));
// }

/**
 * Atomic increment.
 *
 * @param v	pointer to current value
 */
static inline void
atomic_incr(volatile atomic_t* v)
{
    InterlockedIncrement(&v->value);
}

/**
 * Atomic decrement.
 *
 * @param v	pointer to current value
 * 
 */ 
static inline void
atomic_decr(volatile atomic_t* v)
{
    InterlockedDecrement(&v->value);
}

/**
 * Atomic decrement and test.
 *
 * @return true if the value zero after the decrement, false
 * otherwise.
 *
 * @param v	pointer to current value
 * 
 */ 
static inline bool
atomic_decr_test(volatile atomic_t* v)
{
    return 0 == InterlockedDecrement(&v->value);
}

/**
 * Atomic compare and swap. Stores the new value iff the current value
 * is the expected old value.
 *
 * @param v 	pointer to current value
 * @param o 	old value to compare against
 * @param n 	new value to store
 *
 * @return 	the value of v before the swap
 */
static inline int32_t
atomic_cmpxchg32(volatile atomic_t* v, int32_t o, int32_t n)
{
    return InterlockedCompareExchange(&v->value, 
                                      static_cast<LONG>(n), 
                                      static_cast<LONG>(o));
}

// /**
//  * Atomic increment function that returns the new value. Note that the
//  * implementation loops until it can successfully do the operation and
//  * store the value, so there is an extremely low chance that this will
//  * never return.
//  *
//  * @param v 	pointer to current value
//  */
// static inline int32_t
// atomic_incr_ret(volatile atomic_t* v)
// {
//     register volatile int32_t o, n;
    
// #if defined(NDEBUG) && NDEBUG == 1
//     while (1)
// #else
//     register int j;
//     for (j = 0; j < 1000000; ++j)
// #endif
//     {
//         o = v->value;
//         n = o + 1;
//         if (atomic_cmpxchg32(v, o, n) == o)
//             return n;
//     }
    
//     NOTREACHED;
//     return 0;
// }

// /**
//  * Atomic addition function that returns the new value. Note that the
//  * implementation loops until it can successfully do the operation and
//  * store the value, so there is an extremely low chance that this will
//  * never return.
//  *
//  * @param v 	pointer to current value
//  * @param i 	integer to add
//  */
// static inline int32_t
// atomic_add_ret(volatile atomic_t* v, int32_t i)
// {
//     register int32_t o, n;
    
// #if defined(NDEBUG) && NDEBUG == 1
//     while (1)
// #else
//     register int j;
//     for (j = 0; j < 1000000; ++j)
// #endif
//     {
//         o = v->value;
//         n = o + i;
//         if (atomic_cmpxchg32(v, o, n) == o)
//             return n;
//     }
    
//     NOTREACHED;
//     return 0;
// }


#endif /* __ATOMIC_WIN32_H__ */
