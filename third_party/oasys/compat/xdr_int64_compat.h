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

#ifndef _XDR_INT64_COMPAT_H_
#define _XDR_INT64_COMPAT_H_

#include "inttypes.h"

/*
 * The XDR routine for 64 bit values varies across platforms and in
 * some cases doesn't exist. This compatibility shim defines types for
 * xint64_t and u_xint64_t as well as portable xdr routines for those
 * types.
 */
typedef int64_t   xint64_t;
typedef u_int64_t u_xint64_t;

#endif /* _XDR_INT64_COMPAT_H_ */
