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

#ifndef __DEADBEEF_H__
#define __DEADBEEF_H__

namespace oasys {

inline void 
fill_with_the_beef(void* buf, size_t len) {
    char beef[]     = { 0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf };
    size_t beef_len = 8;
    char* p = reinterpret_cast<char*>(buf);
    
    for (size_t i=0; i<len; ++i)
        *p++ = beef[i%beef_len];
}

} // namespace oasys
#endif /* __DEADBEEF_H__ */
