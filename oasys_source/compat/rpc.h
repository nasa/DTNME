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

#ifndef _OASYS_COMPAT_RPC_H_
#define _OASYS_COMPAT_RPC_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Include the rpc headers forcing them to be extern "C".
 */
#include <rpc/rpc.h>

#ifdef __CYGWIN__
// Cygwin's xdr.h file is k&r, so we need to make the declarations
// more specific here to avoid errors when compiling with g++ instead
// of gcc.

void oasys_xdrmem_create(XDR *__xdrs, __const caddr_t __addr,
                                    u_int __size, enum xdr_op __xop);
#define xdrmem_create oasys_xdrmem_create

// these defines add a cast to change the function pointer for a function
// with no args (which we get from xdr.h) into a function pointer with
// args (i.e. k&r to ansi c).

typedef void (*xdr_setpos_t)(XDR *, int);
#undef xdr_setpos
#define xdr_setpos(xdrs, pos) ((xdr_setpos_t)(*(xdrs)->x_ops->x_setpostn))(xdrs, pos)

typedef int (*xdr_getpos_t)(XDR *);
#undef xdr_getpos
#define xdr_getpos(xdrs) ((xdr_getpos_t)(*(xdrs)->x_ops->x_getpostn))(xdrs)

typedef int (*xdr_putlong_t)(XDR *, long *);
#undef xdr_putlong
#define xdr_putlong(xdrs, ptr) ((xdr_putlong_t)(*(xdrs)->x_ops->x_putlong))(xdrs, ptr)

#endif

#ifdef __cplusplus
}
#endif

#endif /* _OASYS_COMPAT_RPC_H_ */
