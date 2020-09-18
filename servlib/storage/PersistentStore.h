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

#ifndef _PERSISTENT_STORE_H_
#define _PERSISTENT_STORE_H_


#include <vector>
#include <oasys/serialize/Serialize.h>

namespace dtn {

/**
 * The abstract base class implementing a persistent storage system.
 * Specific implementations (i.e. Berkeley DB or SQL) should derive
 * from this class.
 *
 * TODO:
 *   * should the key be an int or a std::string?
 */
class PersistentStore {
public:
    /**
     * Close and flush the store.
     */
    virtual int close() = 0;
    
    /**
     * Fill in the fields of the object referred to by *obj with the
     * value stored at the given key.
     */
    virtual int get(oasys::SerializableObject* obj, const int key) = 0;
    
    /**
     * Store the object with the given key.
     */
    virtual int add(oasys::SerializableObject* obj, const int key) = 0;

    /**
     * Update the object with the given key.
     */
    virtual int update(oasys::SerializableObject* obj, const int key) = 0;

    /**
     * Delete the object at the given key.
     */
    virtual int del(const int key) = 0;

    /**
     * Return the number of elements in the table.
     */
    virtual int num_elements() = 0;
    
    /**
     * Fill in the given vector with the keys currently stored in the
     * table.
     */
    virtual void keys(std::vector<int> * v) = 0;

    virtual ~PersistentStore();

};
} // namespace dtn

#endif /* _PERSISTENT_STORE_H_ */
