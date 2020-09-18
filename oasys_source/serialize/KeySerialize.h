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

#ifndef __KEYSERIALIZE_H__
#define __KEYSERIALIZE_H__

#include "Serialize.h"
#include "../util/ExpandableBuffer.h"

namespace oasys {

/**
 * Marshaller for key-type objects. The serialized format is filename
 * and null character friendly. (Note: This marshaller will not check
 * to see if stored strings conform, so users of this class will have
 * to do that check themselves).
 *
 * - Numbers are encoded in hex with a fixed length of digits, zero padded.
 * - Booleans are '0' or '1'
 * - Fixed length strings are stored directly
 * - Variable length strings are stored with the length in hex
 *   followed by the string.
 * - Each field is terminated by the border str
 */
class KeyMarshal : public SerializeAction {
public:
    KeyMarshal(ExpandableBuffer* buf,
               const char*       border = 0);
    
    //! @{ Virtual functions inherited from SerializeAction
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
    void process(const char* name, SerializableObject* object);

    void end_action();
    //! @}

    //! object handler
    int action(SerializableObject* object) {
        return SerializeAction::action(object);
    }

    //! Const object handler
    int action(const SerializableObject* object) {
        return SerializeAction::action(
            const_cast<SerializableObject*>(object));
    }
    
private:
    ExpandableBuffer* buf_;
    const char*       border_;

    void process_int(u_int32_t i, u_int32_t size, const char* format);
    void process_int64(u_int64_t i, u_int32_t size, const char* format);
    void border();
};

/**
 * Unmarshaller for keys.
 */
class KeyUnmarshal : public SerializeAction {
public:
    KeyUnmarshal(const char* buf,
                 u_int32_t      buf_len,
                 const char* border = 0);
    
    //! @{ Virtual functions inherited from SerializeAction
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
    void process(const char* name, SerializableObject* object);

    //! @}
private:
    const char* buf_;
    u_int32_t      buf_len_;
    u_int32_t      border_len_;
    u_int32_t      cur_;

    u_int32_t process_int(u_int32_t size);
    u_int64_t process_int64();
    void border();
};

} // namespace oasys

#endif /* __KEYSERIALIZE_H__ */
