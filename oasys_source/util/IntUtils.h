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

#ifndef _OASYS_INT_UTILS_H_
#define _OASYS_INT_UTILS_H_

// yes, these are done for us in the modern stl with std::min and
// std::max, but those don't exist on the gcc 2.95 series, so we just
// redefine our own

namespace oasys {

#define MINMAX(_Type)                           \
                                                \
inline _Type                                    \
min(_Type a, _Type b)                           \
{                                               \
    return (a < b) ? a : b;                     \
}                                               \
                                                \
inline _Type                                    \
max(_Type a, _Type b)                           \
{                                               \
    return (a > b) ? a : b;                     \
}

MINMAX(int)
MINMAX(unsigned int)
MINMAX(long)
MINMAX(unsigned long)

} // namespace oasys

#endif /* _OASYS_INT_UTILS_H_ */
