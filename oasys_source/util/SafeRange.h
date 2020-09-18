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

#ifndef __SAFERANGE_H__
#define __SAFERANGE_H__

#include <cstddef>

namespace oasys {

/*!
 * Implements a SafeRange which throws an exception if the range
 * constraints are violated. This is probably the cleanest way to
 * write safe parsing code for strings.
 */
template<typename _Type>
class SafeRange {
public:
    /*!
     * Thrown by SafeRange if the range constraint is violated. This
     * maybe? should inherit from std::out_of_range.
     */
    struct Error {
        Error(size_t offset) : offset_(offset) {}
        size_t offset_;
    };

    SafeRange(_Type* a, size_t size)
        : array_(a), size_(size) {}
    
    _Type& operator[](size_t offset) {
        if (offset >= size_) {
            throw Error(offset);
        }
        
        return array_[offset];
    }

private:
    _Type* array_;
    size_t size_;
};

} // namespace oasys

#endif /* __SAFERANGE_H__ */
