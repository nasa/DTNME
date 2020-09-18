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

#ifndef _OASYS_LRU_LIST_H_
#define _OASYS_LRU_LIST_H_

#include <list>

namespace oasys {

/**
 * A simple extension of the STL list class that adds a method
 * move_to_back to abstract away the call to splice().
 */
template <typename _Type>
class LRUList : public std::list<_Type> {
public:
    void move_to_back(typename std::list<_Type>::iterator iter)
    {
        // STL note: This is a constant time operation
        this->splice(this->end(), *this, iter);
    }
};

} // namespace oasys

#endif /* _OASYS_LRU_LIST_H_ */
