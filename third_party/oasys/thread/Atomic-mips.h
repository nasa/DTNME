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


#ifndef _OASYS_ATOMIC_MIPS_H_
#define _OASYS_ATOMIC_MIPS_H_

#include <sys/types.h>

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
 * @param i     integer value to add
 * @param v     pointer to current value
 * 
 */
static inline u_int32_t
atomic_add_ret(volatile atomic_t *v, u_int32_t i)
{
        u_int32_t ret;
        u_int32_t temp;

        __asm__ __volatile__(
                "       .set    mips3                                   \n"
                "1:     ll      %1, %2              # atomic_add_return \n"
                "       addu    %0, %1, %3                              \n"
                "       sc      %0, %2                                  \n"
                "       beqzl   %0, 1b                                  \n"
                "       addu    %0, %1, %3                              \n"
                "       sync                                            \n"
                "       .set    mips0                                   \n"
                : "=&r" (ret), "=&r" (temp), "=m" (v->value)
                : "Ir" (i), "m" (v->value)
                : "memory");

        return ret;
}

/**
 * Atomic subtraction function.
 *
 * @param i     integer value to subtract
 * @param v     pointer to current value
 */
static inline u_int32_t
atomic_sub_ret(volatile atomic_t * v, u_int32_t i)
{
        u_int32_t ret;
        u_int32_t temp;

        __asm__ __volatile__(
                "       .set    mips3                                   \n"
                "1:     ll      %1, %2          # atomic_sub_return     \n"
                "       subu    %0, %1, %3                              \n"
                "       sc      %0, %2                                  \n"
                "       beqzl   %0, 1b                                  \n"
                "       subu    %0, %1, %3                              \n"
                "       sync                                            \n"
                "       .set    mips0                                   \n"
                : "=&r" (ret), "=&r" (temp), "=m" (v->value)
                : "Ir" (i), "m" (v->value)
                : "memory");

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



/**
 * Atomic compare and set. Stores the new value iff the current value
 * is the expected old value.
 *
 * @param v     pointer to current value
 * @param o     old value to compare against
 * @param n     new value to store
 *
 * @return      zero if the compare failed, non-zero otherwise
 */

static inline u_int32_t
atomic_cmpxchg32(volatile atomic_t* v, u_int32_t o, u_int32_t n)
{
        u_int32_t ret;

        __asm__ __volatile__(
                "       .set    push                                    \n"
                "       .set    noat                                    \n"
                "       .set    mips3                                   \n"
                "1:     ll      %0, %2                  # __cmpxchg_u32 \n"
                "       bne     %0, %z3, 2f                             \n"
                "       .set    mips0                                   \n"
                "       move    $1, %z4                                 \n"
                "       .set    mips3                                   \n"
                "       sc      $1, %1                                  \n"
                "       beqzl   $1, 1b                                  \n"
#ifdef CONFIG_SMP
                "       sync                                            \n"
#endif
                "2:                                                     \n"
                "       .set    pop                                     \n"
                : "=&r" (ret), "=R" (*v)
                : "R" (*v), "Jr" (o), "Jr" (n)
                : "memory");

                return ret;
}

}

// namespace oasy

#endif /* _OASYS_ATOMIC_MIPS_H_ */
