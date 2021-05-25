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

#ifndef __DURABLESTOREKEY_H__
#define __DURABLESTOREKEY_H__

#include <string>

#include "../serialize/Serialize.h"

namespace oasys {

/**
 * SerializableKey - Used by the keys to the store to avoid
 * unnecessary serialization.
 */
class DurableStoreKey : public SerializableObject {
public:
    /**
     * Compare function used by the tables for finding the key.
     */
    virtual int compare(const DurableStoreKey& other) = 0;

    /**
     * @return Key expressed as an ascii string. This is easier than a
     * generalized marshalling/unmarshalling which is quite a heavy
     * weight process.
     */
    virtual std::string as_ascii() = 0;

    /**
     * @return True if a key could be converted from the ascii.
     */
    virtual bool from_ascii(const std::string& ascii) = 0;
};

} // namespace oasys

#endif /* __DURABLESTOREKEY_H__ */
