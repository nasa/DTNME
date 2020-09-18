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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cstdlib>

#include "../debug/DebugUtils.h"
#include "Random.h"

namespace oasys {

ByteGenerator::ByteGenerator(unsigned int seed)
    : cur_(seed) 
{ 
    next(); 
}

void 
ByteGenerator::fill_bytes(void* buf, size_t size)
{
    char* p = static_cast<char*>(buf);
    
    for(size_t i=0; i<size; ++i) {
        *p = cur_ % 256;
        ++p;
        next();
    }
}

void ByteGenerator::next() 
{
    cur_ = (A * cur_ + C) % M;
}

PermutationArray::PermutationArray(size_t size)
    : size_(size)
{
    array_.reserve(size_);
    for(unsigned int i=0; i<size_; ++i) {
        array_[i] = i;
    }

    for(unsigned int i=0; i<size_ - 1; ++i) {
        size_t temp;
        unsigned int pos = Random::rand(size_-i-1)+i+1;
        
        temp = array_[i];
        array_[i]   = array_[pos];
        array_[pos] = temp;
    }
}

unsigned int
PermutationArray::map(unsigned int i) {
    ASSERT(i<size_); 
    return array_[i];
}

}; // namespace oasys

#if 0
#include <iostream>

using namespace std;
using namespace oasys;

int
main()
{
    const size_t SIZE = 20;
    PermutationArray ar(SIZE);

    for(int i=0;i<SIZE;++i) {
        cout << i << "->" << ar.map(i) << endl;
    }
}
#endif
