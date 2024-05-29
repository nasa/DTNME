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


#ifndef __TYPE_SHIMS_H__
#define __TYPE_SHIMS_H__

#include <string>
#include "../debug/Formatter.h"
#include "Serialize.h"

namespace oasys {

//----------------------------------------------------------------------------
class Int8Shim : public Formatter, public SerializableObject {
public:
    Int8Shim(int8_t value = 0, const char* name = "int")
        : name_(name), value_(value) {}
    Int8Shim(const Builder&) {}

    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        return snprintf(buf, sz, "%d", static_cast<int>(value_));
    }
    
    // virtual from SerializableObject
    void serialize(SerializeAction* a) {
        a->process(name_.c_str(), &value_);
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return 1; }

    int8_t value() const { return value_; }
    void assign(int8_t value) { value_ = value; }

    
private:
    std::string name_;
    int8_t     value_;
};

//----------------------------------------------------------------------------
class IntShim : public Formatter, public SerializableObject {
public:
    IntShim(int32_t value = 0, const char* name = "int")
        : name_(name), value_(value) {}
    IntShim(const Builder&) {}

    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        // cast is needed for Cygwin since an int32_t is a long int
        return snprintf(buf, sz, "%d", static_cast<int>(value_));
    }
    
    // virtual from SerializableObject
    void serialize(SerializeAction* a) {
        a->process(name_.c_str(), &value_);
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return sizeof(int32_t); }

    int32_t value() const { return value_; }
    void assign(int32_t value) { value_ = value; }

    
private:
    std::string name_;
    int32_t     value_;
};

//----------------------------------------------------------------------------
class UIntShim : public Formatter, public SerializableObject {
public:
    UIntShim(u_int32_t value = 0, const char* name = "u_int")
        : name_(name), value_(value) {}
    UIntShim(const Builder&) {}
    
    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        return snprintf(buf, sz, "%u", value_);
    }
    
    // virtual from SerializableObject
    void serialize(SerializeAction* a) {
        a->process(name_.c_str(), &value_);
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return sizeof(u_int32_t); }

    u_int32_t value() const { return value_; }
    void assign(u_int32_t value) { value_ = value; }

    bool operator==(const UIntShim& other) const {
        return value_ == other.value_;
    }
    
private:
    std::string name_;
    u_int32_t   value_;
};

//----------------------------------------------------------------------------
class StringShim : public Formatter, public SerializableObject {
public:
    StringShim(const std::string& str = "", const char* name = "string")
        : name_(name), str_(str) {}
    StringShim(const Builder&) {}
    
    virtual ~StringShim() {}
    
    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        return snprintf(buf, sz, "%s", str_.c_str());
    }
    
    // virtual from SerializableObject
    void serialize(SerializeAction* a) {
        a->process(name_.c_str(), &str_);
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return 0; }

    const std::string& value() const { return str_; }
    void assign(const std::string& str) { str_.assign(str); }

private:
    std::string name_;
    std::string str_;
};

//----------------------------------------------------------------------------
/* XXX/bowei -- This needs to be updated. It's wrong. Don't uncomment
without fixing.

class NullStringShim : public Formatter, public SerializableObject {
public:
    NullStringShim(const char* str, const char* name = "string") 
        : name_(name), 
          str_(const_cast<char*>(str)),
          free_mem_(false)
    {
        free_mem_ = (str == 0);
    }

    NullStringShim(const Builder&)
        : name_("string"), 
          str_(NULL), 
          free_mem_(false)
    {}

    ~NullStringShim() {
        if(free_mem_) {
            free(str_); 
        } 
    }
    
    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        return snprintf(buf, sz, "%s", str_);
    }
    
    // virtual from SerializableObject
    void serialize(SerializeAction* a)
    {
        u_int32_t len = 0;
        BufferCarrier<char> bc(str_, false);
        
        a->process(name_.c_str(), &bc, &len, '\0');
        if (bc.owned())
        {
            str_ = bc.take_buf();
        }
        else if (a->action_code() == Serialize::UNMARSHAL)
        {
            str_ = static_cast<char*>(malloc(sizeof(char) * len));
            memcpy(str_, bc.buf(), len);
        }
        free_mem_ = true;
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return 0; }

    const char* value() const { return str_; }

private:
    std::string name_;
    char*       str_;
    bool        free_mem_;
};
*/

/*
XXX/bowei -- This needs to be updated. It's wrong. Don't uncomment
without fixing.
//----------------------------------------------------------------------------
class ByteBufShim : public Formatter, public SerializableObject {
public:
    ByteBufShim(char* buf, size_t size)
        : buf_(buf), size_(size), own_buf_(false) {}

    ByteBufShim(const Builder&) 
        : buf_(0), size_(0), own_buf_(false) {}

    ~ByteBufShim() {
        if (buf_ != 0 && own_buf_) {
            free(buf_);
        }
    }

    // virtual from Formatter
    int format(char* buf, size_t sz) const {
        return snprintf(buf, sz, "NEED TO IMPLEMENT ByteBufShim::format");
    }

    // virtual from SerializableObject
    void serialize(SerializeAction* a)
    {
        BufferCarrier<char> bc(buf_, false);
        a->process("bytes", &bc, &size_, '\0');

        if (bc.owned())
        {
            buf_ = bc.take_buf();
        }
        else
        {
            buf_ = static_cast<char*>(malloc(sizeof(char) * size_));
            memcpy(str_, bc.buf(), len);
        }
    }
 
    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return 0; }

    const char* value() const { return buf_; }
    char*       take_buf()    { own_buf_ = false; return buf_; }
    u_int32_t   size()  const { return size_; }

private:
    char*     buf_;
    u_int32_t size_;    
    bool      own_buf_;
};
*/

//----------------------------------------------------------------------------
/*!
 * This is used for generating prefixes for the SerializableObject
 * strings used as keys for tables. Instead of creating by hand
 * another kind of SerializableObject that introduces some kind of
 * prefixing system in the serialize call of the key, you can use
 * prefix_adapter:
 *
 * SerializableObject* real_key;
 * table->get(oasys::prefix_adapter(1000, real_key));
 *
 * Which will automatically append 1000 to the key entry.
 *
 */
template<typename _SerializablePrefix, typename _SerializableObject>
struct PrefixAdapter : public SerializableObject {
    PrefixAdapter() {}

    PrefixAdapter(const _SerializablePrefix& prefix,
                  const _SerializableObject& obj)
        : prefix_(prefix),
          obj_(obj)
    {}
    
    void serialize(SerializeAction* a) 
    {
        a->process("prefix", &prefix_);
        a->process("obj",    &obj_);
    }

    _SerializablePrefix prefix_;
    _SerializableObject obj_;
};

template
<
    typename _SerializablePrefix, 
    typename _SerializableObject
>
PrefixAdapter
<
    _SerializablePrefix, 
    _SerializableObject
>
prefix_adapter(const _SerializablePrefix& prefix,
               const _SerializableObject& obj)
{
    return PrefixAdapter<_SerializablePrefix, 
                         _SerializableObject>(prefix, obj);
}

}; // namespace oasys

#endif //__TYPE_SHIMS_H__
