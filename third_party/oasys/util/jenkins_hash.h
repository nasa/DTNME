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

#ifndef _OASYS_JENKINS_HASH_H_
#define _OASYS_JENKINS_HASH_H_

#include <sys/types.h>
#include "../compat/inttypes.h"

namespace oasys {

/*
 * Just one thing here, the jenkins hash function.
 */
extern u_int32_t
jenkins_hash(u_int8_t *k,        /* the key */
             u_int32_t length,   /* the length of the key */
             u_int32_t initval); /* the previous hash, or an arbitrary value */

} // namespace oasys

#endif /* _OASYS_JENKINS_HASH_H_ */
