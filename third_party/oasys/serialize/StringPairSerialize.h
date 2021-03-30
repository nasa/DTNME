// Copyright
#ifndef _OASYS_STRING_PAIR_SERIALIZE_H_
#define _OASYS_STRING_PAIR_SERIALIZE_H_

/**
 * @file
 *
 * This file defines the serialization and deserialization objects to
 * be used with an external data store.
 */

#include "Serialize.h"
#include "../util/StringUtils.h" /* for StringPair */
#include <memory>
#include <string>
#include <vector>

namespace oasys {

/**
 * 
 */
class StringPairSerialize : public SerializeAction {

public:
    /**
     * Constructor.
     */
    StringPairSerialize(action_t action, 
                        std::vector<StringPair> *rep,
                        context_t context = Serialize::CONTEXT_LOCAL);

    virtual ~StringPairSerialize();
    void begin_action();
    void end_action();

    using SerializeAction::process;
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);

    class Marshal;
    class Unmarshal;
    class Info;

protected:
    // pointer to the vector of strings we're using for serialization
    std::vector<StringPair> *rep_;
};

class StringPairSerialize::Marshal: public StringPairSerialize {
public:
    Marshal(std::vector<StringPair> *rep);
    virtual ~Marshal();

    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    virtual void process(const char* name, u_int64_t* i);
    virtual void process(const char* name, u_int32_t* i);
    virtual void process(const char* name, u_int16_t* i);
    virtual void process(const char* name, u_int8_t* i);
    virtual void process(const char* name, int32_t* i);
    virtual void process(const char* name, int16_t* i);
    virtual void process(const char* name, int8_t* i);
    virtual void process(const char* name, bool* b);
    virtual void process(const char* name, u_char* bp, u_int32_t len);
    virtual void process(const char* name, std::string* s);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*, u_char);
};

class StringPairSerialize::Unmarshal: public StringPairSerialize {
public:
    Unmarshal(std::vector<StringPair> *rep);
    virtual ~Unmarshal();

    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    virtual void process(const char* name, u_int64_t* i);
    virtual void process(const char* name, u_int32_t* i);
    virtual void process(const char* name, u_int16_t* i);
    virtual void process(const char* name, u_int8_t* i);
    virtual void process(const char* name, int32_t* i);
    virtual void process(const char* name, int16_t* i);
    virtual void process(const char* name, int8_t* i);
    virtual void process(const char* name, bool* b);
    virtual void process(const char* name, u_char* bp, u_int32_t len);
    virtual void process(const char* name, std::string* s);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*, u_char);

protected:
    size_t find(const char *name);
};

class StringPairSerialize::Info: public StringPairSerialize {
public:
    Info(std::vector<StringPair> *rep);
    virtual ~Info();

    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    virtual void process(const char* name, u_int64_t* i);
    virtual void process(const char* name, u_int32_t* i);
    virtual void process(const char* name, u_int16_t* i);
    virtual void process(const char* name, u_int8_t* i);
    virtual void process(const char* name, int32_t* i);
    virtual void process(const char* name, int16_t* i);
    virtual void process(const char* name, int8_t* i);
    virtual void process(const char* name, bool* b);
    virtual void process(const char* name, u_char* bp, u_int32_t len);
    virtual void process(const char* name, std::string* s);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*);
    virtual void process(const char*, oasys::BufferCarrier<unsigned char>*, u_char);
};

} // namespace oasys

#endif // _OASYS_STRING_PAIR_SERIALIZE_H_
