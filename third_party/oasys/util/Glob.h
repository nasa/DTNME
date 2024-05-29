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

#ifndef __GLOB_H__
#define __GLOB_H__

namespace oasys {

class Glob {
public:
    struct State {
        const char* pat_;
        const char* to_match_;
    };

    /*! Cheapo glob with a fixed size stack and no external memory
     *  usage. XXX/bowei - handle escapes. */
    static bool fixed_glob(const char* pat, const char* str);
};

} // namespace oasys

#endif /* __GLOB_H__ */
