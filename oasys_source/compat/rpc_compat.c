/*
 *    Copyright 2006-2007 Intel Corporation
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

#include "../compat/rpc.h"

#ifdef __CYGWIN__
void oasys_xdrmem_create(XDR *__xdrs, __const caddr_t __addr,
                         u_int __size, enum xdr_op __xop)
{
    xdrmem_create(__xdrs, __addr, __size, __xop);
}
#endif
