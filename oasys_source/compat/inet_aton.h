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

#ifndef _OASYS_INET_ATON_H_
#define _OASYS_INET_ATON_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

#if !defined(HAVE_INET_ATON)
#if  defined(HAVE_INET_PTON)

/*
 * Redefine inet_aton to use the compat function.
 */
EXTERN_C int inet_aton_with_pton(const char *, struct in_addr *);
#define inet_aton inet_aton_with_pton

#else

#error need either inet_aton or inet_pton

#endif
#endif

#endif /* _OASYS_INET_ATON_H_ */
