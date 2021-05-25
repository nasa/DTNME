/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <ext/rope>

#include "StreamSerialize.h"

#include "../util/Pointers.h"
#include "../io/ByteStream.h"

namespace oasys {

//------------------------------------------------------------------
StreamSerialize::StreamSerialize(OutByteStream* stream,
                                 context_t      context)
    : SerializeAction(Serialize::MARSHAL, context),
      stream_(stream)
{}

//------------------------------------------------------------------
int
StreamSerialize::action(const SerializableObject* object)
{
    return SerializeAction::action(const_cast<SerializableObject*>(object));
}

//------------------------------------------------------------------
void 
StreamSerialize::begin_action()
{
    stream_->begin();
}

//------------------------------------------------------------------
void 
StreamSerialize::end_action()
{
    stream_->end();
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, u_int64_t* i)
{
    (void) name;
    
    if (error())
        return;

    u_char buf[8];

    buf[0] = ((*i)>>56) & 0xff;
    buf[1] = ((*i)>>48) & 0xff;
    buf[2] = ((*i)>>40) & 0xff;
    buf[3] = ((*i)>>32) & 0xff;
    buf[4] = ((*i)>>24) & 0xff;
    buf[5] = ((*i)>>16) & 0xff;
    buf[6] = ((*i)>>8)  & 0xff;
    buf[7] = (*i)       & 0xff;

    int err = stream_->write(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, u_int32_t* i)
{
    (void) name;

    if (error())
        return;

    u_char buf[4];

    buf[0] = ((*i)>>24) & 0xff;
    buf[1] = ((*i)>>16) & 0xff;
    buf[2] = ((*i)>>8)  & 0xff;
    buf[3] = (*i)       & 0xff;

    int err = stream_->write(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, u_int16_t* i)
{
    (void) name;

    if (error())
        return;

    u_char buf[2];

    buf[0] = ((*i)>>8) & 0xff;
    buf[1] = (*i)      & 0xff;

    int err = stream_->write(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, u_int8_t* i)
{
    (void) name;

    if (error())
        return;
    
    u_char buf[1];
    buf[0] = (*i);
    
    int err = stream_->write(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, bool* b)
{
    (void) name;

    if (error())
        return;
    
    u_char buf[1];
    buf[0] = (*b) ? 1 : 0;

    int err = stream_->write(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, u_char* bp, u_int32_t len)
{
    (void) name;

    if (error())
        return;
    
    int err = stream_->write(bp, len);
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char*            name, 
                         BufferCarrier<u_char>* carrier)
{
    std::string len_name;
    len_name += ".len";
    u_int32_t len = carrier->len();

    process(len_name.c_str(), &len);
    process(name, carrier->buf(), len);
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char*            name,
                         BufferCarrier<u_char>* carrier,
                         u_char                 terminator)
{
    // find where the terminating character is
    u_int32_t len = 0;
    while (carrier->buf()[len] != terminator)
    {
        ++len;
    }
    carrier->set_len(len + 1);  // Also store the terminator
    process(name, carrier);
}

//------------------------------------------------------------------
void 
StreamSerialize::process(const char* name, std::string* s)
{
    (void) name;

    if (error())
        return;

    u_int32_t len = s->length();
    std::string len_name = name;
    len_name += ".len";
    process(len_name.c_str(), &len);
    if (error())
        return;

    int err = stream_->write(reinterpret_cast<const u_char*>(s->data()), len);
    if (err != 0) 
    {
        signal_error();
    }
}

//------------------------------------------------------------------
StreamUnserialize::StreamUnserialize(InByteStream* stream,
                                     context_t      context)
    : SerializeAction(Serialize::UNMARSHAL, context),
      stream_(stream)
{}

//------------------------------------------------------------------
void 
StreamUnserialize::begin_action()
{
    stream_->begin();
}

//------------------------------------------------------------------
void 
StreamUnserialize::end_action()
{
    stream_->end();
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, u_int64_t* i)
{
    (void) name;
    
    if (error())
        return;

    u_char buf[8];
    int err = stream_->read(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
        return;
    }
    
    *i = (((u_int64_t)buf[0]) << 56) |
         (((u_int64_t)buf[1]) << 48) |
         (((u_int64_t)buf[2]) << 40) |
         (((u_int64_t)buf[3]) << 32) |
         (((u_int64_t)buf[4]) << 24) |
         (((u_int64_t)buf[5]) << 16) |
         (((u_int64_t)buf[6]) << 8) |
         (((u_int64_t)buf[7]));
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, u_int32_t* i)
{
    (void) name;

    if (error())
        return;

    u_char buf[4];
    int err = stream_->read(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
        return;
    }

    *i = (buf[0] << 24) | 
         (buf[1] << 16) | 
         (buf[2] << 8)  | 
         (buf[3] << 0);        
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, u_int16_t* i)
{
    (void) name;

    if (error())
        return;

    u_char buf[2];
    int err = stream_->read(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
        return;
    }
    
    *i = (buf[0] << 8) | buf[1];
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, u_int8_t* i)
{
    (void) name;

    if (error())
        return;
    
    u_char buf[1];
    int err = stream_->read(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
        return;
    }
    
    *i = buf[0];
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, bool* b)
{
    (void) name;

    if (error())
        return;
    
    u_char buf[1];
    int err = stream_->read(buf, sizeof(buf));
    if (err != 0) 
    {
        signal_error();
        return;
    }

    *b = buf[0];
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, u_char* bp, u_int32_t len)
{
    (void) name;

    if (error())
        return;
    
    int err = stream_->read(bp, len);
    if (err != 0) 
    {
        signal_error();
        return;
    }
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char*            name, 
                           BufferCarrier<u_char>* carrier)
{
    std::string len_name = name;
    len_name += ".len";
    u_int32_t len;
    process(len_name.c_str(), &len);
    
    u_char* buf = static_cast<u_char*>(malloc(sizeof(u_char) * len));
    int err = stream_->read(buf, len);
    if (err != 0)
    {
        signal_error();
        return;
    }
    carrier->set_buf(buf, len, true);
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char*            name,
                           BufferCarrier<u_char>* carrier,
                           u_char                 terminator)
{
    (void) terminator;
    process(name, carrier);
}

//------------------------------------------------------------------
void 
StreamUnserialize::process(const char* name, std::string* s)
{
    (void) name;

    if (error())
        return;

    u_int32_t len;
    std::string len_name = name;
    len_name += ".len";
    process(len_name.c_str(), &len);
    if (error()) {
        return;
    }

    // The use of a temporary here is unfortunate, but unavoidable
    // without too many gyrations.
    char* tempbuf = static_cast<char*>(malloc(sizeof(char) * len));
    ScopeMalloc cleanup(tempbuf);

    int err = stream_->read(reinterpret_cast<u_char*>(tempbuf), len);
    if (err != 0) 
    {
        signal_error();
        return;
    }

    s->assign(tempbuf, len);
}

} // namespace oasys
