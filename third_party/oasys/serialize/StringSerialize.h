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

#ifndef _OASYS_STRING_SERIALIZE_H_
#define _OASYS_STRING_SERIALIZE_H_

#include "Serialize.h"
#include "../util/StringBuffer.h"

namespace oasys {

/**
 * StringSerialize is a SerializeAction that "flattens" the object
 * into a oasys StringBuffer;
 */
class StringSerialize : public SerializeAction {
public:
    /** Valid serialization options for StringSerialize */
    enum {
        INCLUDE_NAME    = 1 << 0, ///< Serialize includes name
        INCLUDE_TYPE    = 1 << 1, ///< Serialize includes type
        SCHEMA_ONLY	= 1 << 2, ///< Don't serialize the value
        DOT_SEPARATED   = 1 << 3, ///< Use . not " " as field separations
    };

    /**
     * Constructor
     */
    StringSerialize(context_t context, int options);
 
    /**
     * We can tolerate a const object.
     */
    using SerializeAction::action;
    int action(const SerializableObject* const_object)
    {
        SerializableObject* object = (SerializableObject*)const_object;
        return SerializeAction::action(object);
    }
    
    using SerializeAction::process;
    void process(const char* name, SerializableObject* const_object)
    {
        SerializableObject* object = (SerializableObject*)const_object;
        return SerializeAction::process(name, object);
    }

    /**
     * Accessor for the serialized string.
     */
    const StringBuffer& buf() { return buf_; }

    /// @{
    /// Virtual functions inherited from SerializeAction
    void end_action();
    void process(const char* name, u_int64_t* i);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char*            name, 
                 BufferCarrier<u_char>* carrier);
    void process(const char*            name,
                 BufferCarrier<u_char>* carrier,
                 u_char                 terminator);
    void process(const char* name, std::string* s);
    /// @}

private:
    void add_preamble(const char* name, const char* type);
    
    StringBuffer buf_; ///< string buffer
    char         sep_; ///< separator character (either " " or ".")
};

} // namespace oasys

#endif /* _OASYS_STRING_SERIALIZE_H_ */
