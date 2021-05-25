#ifndef __STREAMSERIALIZE_H__
#define __STREAMSERIALIZE_H__

#include "../serialize/Serialize.h"

namespace oasys {

class OutByteStream;
class InByteStream;

/*!
 * XXX/bowei - Ideally, we would like every serialization to move to
 * the stream format.
 */
class StreamSerialize : public SerializeAction {
public:
    StreamSerialize(OutByteStream* stream,
                    context_t      context);

    // const parameter for action
    using SerializeAction::action;
    int action(const SerializableObject* object);

    //! @{ virtual from SerializeAction
    void begin_action();
    void end_action();

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
    void process(const char* name, std::string* str);
    //! @}
    
private:
    OutByteStream* stream_;
};

class StreamUnserialize : public SerializeAction {
public:
    StreamUnserialize(InByteStream* stream,
                    context_t      context);

    //! @{ virtual from SerializeAction
    void begin_action();
    void end_action();

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
    //! @}
    
private:
    InByteStream* stream_;
};

} // namespace oasys

#endif /* __STREAMSERIALIZE_H__ */
