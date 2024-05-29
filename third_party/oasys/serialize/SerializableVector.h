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

#ifndef _OASYS_SERIALIZABLE_VECTOR_H_
#define _OASYS_SERIALIZABLE_VECTOR_H_

#include <vector>
#include "Serialize.h"
#include "../debug/DebugUtils.h"

namespace oasys {

/**
 * Utility class to implement a serializable std::vector that contains
 * elements which must also be SerializableObjects. Note this can only
 * be used with underlying durable stores which allow variable-length
 * records (i.e. not SQL tables).
 */
template <typename _Type>
class SerializableVector : public oasys::SerializableObject,
                           public std::vector<_Type>
{
public:
    /**
     * Default constructor.
     */
    SerializableVector() {}

    /**
     * Unserialization constructor.
     */
    SerializableVector(const Builder&) {}
    
    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);
};

#include "SerializableVector.tcc"

} // namespace oasys

#endif /* _OASYS_SERIALIZABLE_VECTOR_H_ */
