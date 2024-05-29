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

#ifndef _OASYS_MARSHAL_SERIALIZE_H_
#define _OASYS_MARSHAL_SERIALIZE_H_

#include "Serialize.h"
#include "BufferedSerializeAction.h"
#include "../util/ExpandableBuffer.h"
#include "../util/CRC32.h"


namespace oasys {


/**
 * Marshal is a SerializeAction that flattens an object into a byte
 * stream.
 */
class Marshal : public BufferedSerializeAction {
public:
    /**
     * Constructor with a fixed-size buffer.
     */
    Marshal(context_t context, u_char* buf, size_t length, int options = 0);

    /**
     * Constructor with an expandable buffer.
     */
    Marshal(context_t context, ExpandableBuffer* buf, int options = 0);

    /**
     * Since the Marshal operation doesn't actually modify the
     * SerializableObject, define a variant of action() and process()
     * that allows a const SerializableObject* as the object
     * parameter.
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

    // Virtual functions inherited from SerializeAction
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

private:
    // future use?  bool add_crc_;
};


/**
 * Unmarshal is a SerializeAction that constructs an object's
 * internals from a flat byte stream.
 * 
 * INVARIANT: The length of the buffer must be the length of the serialized
 * buffer.
 */
class Unmarshal : public BufferedSerializeAction {
public:
    /**
     * Constructor
     */
    Unmarshal(context_t context, const u_char* buf, size_t length,
              int options = 0);

    // Virtual functions inherited from SerializeAction
    void begin_action();

    using SerializeAction::process;
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

private:
    // future use?  bool has_crc_;
};


/**
 * MarshalSize is a SerializeAction that determines the buffer size
 * needed to run a Marshal action over the object.
 */
class MarshalSize : public SerializeAction {
public:
    /**
     * Constructor
     */
    MarshalSize(context_t context, int options = 0)
        : SerializeAction(Serialize::INFO, context, options), size_(0)
    {
    }

    /**
     * Again, we can tolerate a const object as well.
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

    /** @return Measured size */
    size_t size() { return size_; }

    /// @{
    /// Static functions to simply return the serialized sizes. Called
    /// from the various process() variants.
    static u_int32_t get_size(u_int64_t*)           { return 8; }
    static u_int32_t get_size(u_int32_t*)           { return 4; }
    static u_int32_t get_size(u_int16_t*)           { return 2; }
    static u_int32_t get_size(u_int8_t*)            { return 1; }
    static u_int32_t get_size(bool*)                { return 1; }
    static u_int32_t get_size(u_char*, u_int32_t len)  { return len; }
    static u_int32_t get_size(std::string* s)       { return s->length() + 4; }
    /// @}
    
    /// @{
    /// Virtual functions inherited from SerializeAction
    void begin_action();
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
    size_t size_;
};

//////////////////////////////////////////////////////////////////////////////
/**
 * MarshalCopy: Copy one object to another using Marshal/Unmarshal.
 */
class MarshalCopy {
public:
    /// Copy method, returns the serialized size
    static u_int32_t copy(ExpandableBuffer* buf,
                          const SerializableObject* src,
                          SerializableObject* dst);
};

} // namespace oasys

#endif /* _OASYS_MARSHAL_SERIALIZE_H_ */
