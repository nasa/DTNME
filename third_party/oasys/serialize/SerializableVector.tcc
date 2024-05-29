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


template <typename _Type>
void
SerializableVector<_Type>::serialize(SerializeAction* a)
{
    oasys::Builder builder;
            
    // either marshal or unmarshal the size, which may be zero
    u_int sz = this->size();
    a->process("size", &sz);
    
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        // if we're unmarshalling, then we loop to fill in the
        // mappings vector with newly created objects
        for (size_t i = 0; i < sz; ++i) {
            this->push_back(_Type(builder));
            a->process("element", &(this->back()));
        }
        ASSERT(this->size() == sz);
    } else {
        // if we're marshalling (or sizing), then just call process on
        // all elements in the vector
        typename std::vector<_Type>::iterator i;
        for (i = this->begin(); i != this->end(); ++i) {
            a->process("element", &(*i));
        }
    }
}
