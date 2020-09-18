/*
 *    Copyright 2005-2006 Intel Corporation
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


#ifndef _OASYS_ATOMIC_PPC_H_
#define _OASYS_ATOMIC_PPC_H_

#include "../debug/DebugUtils.h"

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
 * Atomic addition function.
 *
 * @param i	integer value to add
 * @param v	pointer to current value
 * 
 */
static inline u_int32_t
atomic_add_ret(volatile atomic_t* v, u_int32_t i)
{
    register u_int32_t ret;

    __asm__ __volatile__(
        "1:	lwarx %0, 0, %2\n"       /* load old value */
        "	add %0, %3, %0\n"        /* calculate new value */
        "	stwcx. %0, 0, %2\n"      /* attempt to store */
        "	bne- 1b\n"               /* spin if failed */
        : "=&r" (ret), "=m" (v->value)
        : "r" (v), "r" (i), "m" (v->value)
        : "cc", "memory");

    return ret;
}

/**
 * Atomic subtraction function.
 *
 * @param i	integer value to subtract
 * @param v	pointer to current value
 */
static inline u_int32_t
atomic_sub_ret(volatile atomic_t* v, u_int32_t i)
{
    register u_int32_t ret;

    __asm__ __volatile__(
        "1:	lwarx %0, 0, %2\n"       /* load old value */
        "	subfc %0, %3, %0\n"      /* calculate new value */
        "	stwcx. %0, 0, %2\n"      /* attempt to store */
        "	bne- 1b\n"               /* spin if failed */
        : "=&r" (ret), "=m" (v->value)
        : "r" (v), "r" (i), "m" (v->value)
        : "cc", "memory");

    return ret;
}

/// @{
/// Wrapper variants around the basic add/sub functions above

static inline void
atomic_add(volatile atomic_t* v, u_int32_t i)
{
    atomic_add_ret(v, i);
}

static inline void
atomic_sub(volatile atomic_t* v, u_int32_t i)
{
    atomic_sub_ret(v, i);
}

static inline void
atomic_incr(volatile atomic_t* v)
{
    atomic_add(v, 1);
}

static inline void
atomic_decr(volatile atomic_t* v)
{
    atomic_sub(v, 1);
}

static inline u_int32_t
atomic_incr_ret(volatile atomic_t* v)
{
    return atomic_add_ret(v, 1);
}

static inline u_int32_t
atomic_decr_ret(volatile atomic_t* v)
{
    return atomic_sub_ret(v, 1);
}

static inline bool
atomic_decr_test(volatile atomic_t* v)
{
    return (atomic_sub_ret(v, 1) == 0);
}

/// @}

/**
 * Atomic compare and set. Stores the new value iff the current value
 * is the expected old value.
 *
 * @param v 	pointer to current value
 * @param o 	old value to compare against
 * @param n 	new value to store
 *
 * @return 	zero if the compare failed, non-zero otherwise
 */
static inline u_int32_t
atomic_cmpxchg32(volatile atomic_t* v, u_int32_t o, u_int32_t n)
{
    register u_int32_t ret;

    __asm __volatile (
        "1:	lwarx %0, 0, %2\n"       /* load old value */
        "	cmplw %3, %0\n"          /* compare */
        "	bne 2f\n"                /* exit if not equal */
        "	stwcx. %4, 0, %2\n"      /* attempt to store */
        "	bne- 1b\n"               /* spin if failed */
        "	b 3f\n"                  /* we've succeeded */
        "	2:\n"
        "	stwcx. %0, 0, %2\n"      /* clear reservation (74xx) */
        "	3:\n"
        : "=&r" (ret), "=m" (v->value)
        : "r" (v), "r" (o), "r" (n), "m" (v->value)
        : "cc", "memory");

    return (ret);
}

} // namespace oasys

#endif /* _OASYS_ATOMIC_PPC_H_ */
