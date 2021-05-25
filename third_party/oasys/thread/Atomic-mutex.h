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


#ifndef _OASYS_ATOMIC_MUTEX_H_
#define _OASYS_ATOMIC_MUTEX_H_

/**
 * @file Atomic-mutex.h
 *
 * This file defines an architecture-agnostic implementation of the
 * atomic routines. A singleton global mutex is locked before every
 * operation, ensuring safety, at the potential expense of
 * performance.
 */

#include "../compat/inttypes.h"
#include "../debug/DebugUtils.h"
#include "../util/Singleton.h"

namespace oasys {

class Mutex;

/**
 * Global accessor to the singleton atomic mutex.
 */
Mutex* atomic_mutex();

/**
 * The definition of atomic_t for is just a wrapper around the value,
 * since the mutex is in a singleton.
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
void atomic_add(volatile atomic_t *v, u_int32_t i);

/**
 * Atomic subtraction function.
 *
 * @param i	integer value to subtract
 * @param v	pointer to current value
 */
void atomic_sub(volatile atomic_t* v, u_int32_t i);

/**
 * Atomic increment.
 *
 * @param v	pointer to current value
 */
void atomic_incr(volatile atomic_t* v);

/**
 * Atomic decrement.
 *
 * @param v	pointer to current value
 * 
 */ 
void atomic_decr(volatile atomic_t* v);

/**
 * Atomic decrement and test.
 *
 * @return true if the value zero after the decrement, false
 * otherwise.
 *
 * @param v	pointer to current value
 * 
 */ 
bool atomic_decr_test(volatile atomic_t* v);

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
u_int32_t atomic_cmpxchg32(volatile atomic_t* v, u_int32_t o, u_int32_t n);

/**
 * Atomic increment function that returns the new value.
 *
 * @param v 	pointer to current value
 */
u_int32_t atomic_incr_ret(volatile atomic_t* v);

/**
 * Atomic addition function that returns the new value.
 *
 * @param v 	pointer to current value
 * @param i 	integer to add
 */
u_int32_t atomic_add_ret(volatile atomic_t* v, u_int32_t i);

} // namespace oasys

#endif /* _OASYS_ATOMIC_MUTEX_H_ */
