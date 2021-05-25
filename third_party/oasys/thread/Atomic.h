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


#ifndef _OASYS_ATOMIC_H_
#define _OASYS_ATOMIC_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

/**
 * Include the appropriate architecture-specific version of the atomic
 * functions here. Each defines an atomic_t structure with (at least)
 * a single u_int32_t field called value, though architectures might
 * add additional fields. For example:
 *
 * @code
 * typedef struct {
 *      volatile u_int32_t value;
 * } atomic_t;
 * @endcode
 *
 * It is possible, however, to define atomic functions to only use a
 * pthread mutex, in which case a single global mutex is used for
 * atomicity guarantees.
 */

#ifdef OASYS_ATOMIC_NONATOMIC
#include "Atomic-fake.h"
#elif defined(OASYS_ATOMIC_MUTEX)
#include "Atomic-mutex.h"
#else

#if (defined(__i386__) || defined(__amd64__)) && defined(__GNUC__)
#include "Atomic-x86.h"
#elif defined(__POWERPC__) || defined(PPC)
#include "Atomic-ppc.h"
#elif defined(__arm__)
#include "Atomic-arm.h"
#elif defined(__mips__)
#include "Atomic-mips.h"
#elif defined(__win32__)
#include "Atomic-win32.h"
#else
#error "No Atomic.h variant found for this architecture... \
        implement one or configure with --disable-atomic-asm"
#endif
#endif /* !OASYS_ATOMIC_NONATOMIC && !OASYS_ATOMIC_MUTEX */

#endif /* _OASYS_ATOMIC_H_ */
