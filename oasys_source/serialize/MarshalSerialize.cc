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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "debug/DebugUtils.h"
#include "debug/Log.h"
#include "util/StringUtils.h"
#include "util/CRC32.h"

#include "MarshalSerialize.h"

namespace oasys {

/******************************************************************************
 *
 * Marshal
 *
 *****************************************************************************/
Marshal::Marshal(context_t context, u_char* buf, size_t length, int options)
    : BufferedSerializeAction(MARSHAL, context, buf, length, options)
{
}

//----------------------------------------------------------------------------
Marshal::Marshal(context_t context, ExpandableBuffer* buf, int options)
    : BufferedSerializeAction(MARSHAL, context, buf, options)
{
}

//----------------------------------------------------------------------------
void
Marshal::end_action()
{
    if (options_ & USE_CRC)
    {
        CRC32 crc;

        if (buf() != 0) 
        {
            crc.update(buf(), offset());
            CRC32::CRC_t crc_val = crc.value();
            process("crc", &crc_val);

            if (log_) {
                logf(log_, LOG_DEBUG, "crc32 is 0x%x", crc_val);
            }
        }
    }
}

//----------------------------------------------------------------------------
void
Marshal::process(const char* name, u_int64_t* i)
{
    u_char* buf = next_slice(8);
    if (buf == NULL) return;

    buf[0] = ((*i)>>56) & 0xff;
    buf[1] = ((*i)>>48) & 0xff;
    buf[2] = ((*i)>>40) & 0xff;
    buf[3] = ((*i)>>32) & 0xff;
    buf[4] = ((*i)>>24) & 0xff;
    buf[5] = ((*i)>>16) & 0xff;
    buf[6] = ((*i)>>8)  & 0xff;
    buf[7] = (*i)       & 0xff;

    if (log_) logf(log_, LOG_DEBUG, "int64  %s=>(%llu)", name, U64FMT(*i));
}

//----------------------------------------------------------------------------
void
Marshal::process(const char* name, u_int32_t* i)
{
    u_char* buf = next_slice(4);
    if (buf == NULL) return;

    buf[0] = ((*i)>>24) & 0xff;
    buf[1] = ((*i)>>16) & 0xff;
    buf[2] = ((*i)>>8)  & 0xff;
    buf[3] = (*i)       & 0xff;

    if (log_) logf(log_, LOG_DEBUG, "int32  %s=>(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char* name, u_int16_t* i)
{
    u_char* buf = next_slice(2);
    if (buf == NULL) return;

    buf[0] = ((*i)>>8) & 0xff;
    buf[1] = (*i)      & 0xff;
    
    if (log_) logf(log_, LOG_DEBUG, "int16  %s=>(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char* name, u_int8_t* i)
{
    u_char* buf = next_slice(1);
    if (buf == NULL) return;
    
    buf[0] = (*i);
    
    if (log_) logf(log_, LOG_DEBUG, "int8   %s=>(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char* name, bool* b)
{
    u_char* buf = next_slice(1);
    if (buf == NULL) return;

    buf[0] = (*b) ? 1 : 0;
    
    if (log_) logf(log_, LOG_DEBUG, "bool   %s=>(%c)", name, *b ? 'T' : 'F');
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char* name, u_char* bp, u_int32_t len)
{
    u_char* buf = next_slice(len);
    if (buf == NULL) return;

    memcpy(buf, bp, len);
    if (log_) {
        std::string s;
        hex2str(&s, bp, len < 16 ? len : 16);
        logf(log_, LOG_DEBUG, "bufc   %s=>(%u: '%.*s')",
             name, len, (int)s.length(), s.data());
    }
}
    
//----------------------------------------------------------------------------
void 
Marshal::process(const char*            name, 
                 BufferCarrier<u_char>* carrier)
{
    std::string len_name;
    len_name += ".len";

    u_int32_t len = carrier->len();
    process(len_name.c_str(), &len);
    process(name, carrier->buf(), carrier->len());
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char*            name,
                 BufferCarrier<u_char>* carrier,
                 u_char                 terminator)
{
    size_t len = 0;
    while (carrier->buf()[len] != terminator)
    {
        ++len;
    }

    carrier->set_len(len + 1); // include terminator
    process(name, carrier->buf(), carrier->len());
}

//----------------------------------------------------------------------------
void 
Marshal::process(const char* name, std::string* s)
{
    u_int32_t len = s->length();
    process(name, &len);

    u_char* buf = next_slice(len);
    if (buf == NULL) return;
    
    memcpy(buf, s->data(), len);
    
    if (log_) {
        if (len < 32)
            logf(log_, LOG_DEBUG, "string %s=>(%u: '%.*s')",
                 name, len, len, s->data());
        else 
            logf(log_, LOG_DEBUG, "string %s=>(%u: '%.*s'...)",
                 name, len, 32, s->data());
    }
}

/******************************************************************************
 *
 * Unmarshal
 *
 *****************************************************************************/
Unmarshal::Unmarshal(context_t context, const u_char* buf, size_t length,
                     int options)
    : BufferedSerializeAction(UNMARSHAL, context, 
                              (u_char*)(buf), length, options)
{
}

//----------------------------------------------------------------------------
void
Unmarshal::begin_action()
{
    if (options_ & USE_CRC)
    {
        CRC32 crc;
        CRC32::CRC_t crc_val;
        
        crc_val = CRC32::from_bytes(buf() + length() -
                                    sizeof(CRC32::CRC_t)); 
        crc.update(buf(), length() - sizeof(CRC32::CRC_t));
        
        if (crc.value() != crc_val)
        {
            if (log_)
            {
                logf(log_, LOG_WARN, "crc32 mismatch, 0x%x != 0x%x",
                     crc.value(), crc_val);
                signal_error();
            }
        }
        else
        {
            logf(log_, LOG_INFO, "crc32 is good");
        }
    }
}

//----------------------------------------------------------------------------
void
Unmarshal::process(const char* name, u_int64_t* i)
{
    u_char* buf = next_slice(8);
    if (buf == NULL) return;

    *i = (((u_int64_t)buf[0]) << 56) |
         (((u_int64_t)buf[1]) << 48) |
         (((u_int64_t)buf[2]) << 40) |
         (((u_int64_t)buf[3]) << 32) |
         (((u_int64_t)buf[4]) << 24) |
         (((u_int64_t)buf[5]) << 16) |
         (((u_int64_t)buf[6]) << 8) |
         (((u_int64_t)buf[7]));
    
    if (log_) logf(log_, LOG_DEBUG, "int32  %s<=(%llu)", name, U64FMT(*i));
}

//----------------------------------------------------------------------------
void
Unmarshal::process(const char* name, u_int32_t* i)
{
    u_char* buf = next_slice(4);
    if (buf == NULL) return;
    
    *i = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];        
    if (log_) logf(log_, LOG_DEBUG, "int32  %s<=(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char* name, u_int16_t* i)
{
    u_char* buf = next_slice(2);
    if (buf == NULL) return;
    
    *i = (buf[0] << 8) | buf[1];        
    if (log_) logf(log_, LOG_DEBUG, "int16  %s<=(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char* name, u_int8_t* i)
{
    u_char* buf = next_slice(1);
    if (buf == NULL) return;
    
    *i = buf[0];        
    if (log_) logf(log_, LOG_DEBUG, "int8   %s<=(%d)", name, *i);
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char* name, bool* b)
{
    u_char* buf = next_slice(1);
    if (buf == NULL) return;
    
    *b = buf[0];
    if (log_) logf(log_, LOG_DEBUG, "bool   %s<=(%c)", name, *b ? 'T' : 'F');
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char* name, u_char* bp, u_int32_t len)
{
    u_char* buf = next_slice(len);
    if (buf == NULL) return;
    
    memcpy(bp, buf, len);

    if (log_) {
        std::string s;
        hex2str(&s, bp, len < 16 ? len : 16);
        logf(log_, LOG_DEBUG, "bufc   %s<=(%u: '%.*s')",
             name, len, (int)s.length(), s.data());
    }
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char*            name, 
                   BufferCarrier<u_char>* carrier)
{
    u_int32_t len;
    
    std::string len_name = name;
    len_name += ".len";
    process(len_name.c_str(), &len);
    
    if (len == 0)
    {
        carrier->set_buf(0, 0, false);
        return;
    }

    carrier->set_buf(next_slice(len), len, false);
    if (log_ && carrier->len() != 0) 
    {
        std::string s;
        hex2str(&s, carrier->buf(), (len < 16) ? len : 16);
        logf(log_, LOG_DEBUG, "bufc   %s<=(%u: '%.*s')",
             name, len, (int)s.length(), s.data());
    }
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char*            name,
                   BufferCarrier<u_char>* carrier,
                   u_char                 terminator)
{
    (void) name;
    
    // look for terminator
    u_char* cbuf = 0;
    u_char* c;
    size_t len = 0;
    do {
        c = next_slice(1);
        if (cbuf == 0)
        {
            cbuf = c;
        }

        if (c == 0)
        {
            logf(log_, LOG_WARN, "Unmarshal::process error");
            signal_error();
            return;
        }
        ++len;
    } while (*c != terminator);

    carrier->set_buf(cbuf, len + 1, false); // include the terminator
}

//----------------------------------------------------------------------------
void 
Unmarshal::process(const char* name, std::string* s)
{
    ASSERT(s != 0);

    u_int32_t len;
    process(name, &len);

    u_char* buf = next_slice(len);
    if (buf == 0) return;
    
    s->assign((char*)buf, len);
    if (log_) {
        if (len < 32)
            logf(log_, LOG_DEBUG, "string %s<=(%u: '%.*s')",
                 name, len, len, s->data());
        else 
            logf(log_, LOG_DEBUG, "string %s<=(%u: '%.*s'...)",
                 name, len, 32, s->data());
    }
}

/******************************************************************************
 *
 * MarshalSize 
 *
 *****************************************************************************/

void
MarshalSize::begin_action()
{
    if (options_ & USE_CRC) {
        size_ += sizeof(CRC32::CRC_t);
    }
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, u_int64_t* i)
{
    (void)name;
    size_ += get_size(i);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, u_int32_t* i)
{
    (void)name;
    size_ += get_size(i);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, u_int16_t* i)
{
    (void)name;
    size_ += get_size(i);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, u_int8_t* i)
{
    (void)name;
    size_ += get_size(i);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, bool* b)
{
    (void)name;
    size_ += get_size(b);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)name;
    size_ += get_size(bp, len);
}

//----------------------------------------------------------------------------
void
MarshalSize::process(const char* name, std::string* s)
{
    (void)name;
    size_ += get_size(s);
}

//----------------------------------------------------------------------------
void 
MarshalSize::process(const char*            name, 
                     BufferCarrier<u_char>* carrier)
{
    (void) carrier;

    u_int32_t size;
    process(name, &size);
    size_ += carrier->len();
}

//----------------------------------------------------------------------------
void 
MarshalSize::process(const char*            name,
                     BufferCarrier<u_char>* carrier,
                     u_char                 terminator)
{
    (void) name;

    size_t size = 0;
    while (carrier->buf()[size] != terminator)
    {
        ++size;
    }

    size_ += size + 1; // include terminator
}

//  *
//  * MarshalCRC
//  *
//  *****************************************************************************/
/* #define DECL_CRC(_type)                         \
 void                                            \
 MarshalCRC::process(const char* name, _type* i) \
 {                                               \
     (void)name;                                 \
     crc_.update((u_char*)i, sizeof(_type));     \
 }
*/
// DECL_CRC(u_int32_t)
// DECL_CRC(u_int16_t)
// DECL_CRC(u_int8_t)
// DECL_CRC(bool)

// //----------------------------------------------------------------------------
// void
// MarshalCRC::process(const char* name, u_char* bp, u_int32_t len)
// {
//     (void)name;
//     crc_.update(bp, len);
// }

// //----------------------------------------------------------------------------
// void 
// MarshalCRC::process(const char*            name, 
//                     BufferCarrier<u_char>* carrier,
//                     size_t*                lenp)
// {
// }

// //----------------------------------------------------------------------------
// void 
// MarshalCRC::process(const char*            name,
//                     BufferCarrier<u_char>* carrier,
//                     size_t*                lenp,
//                     u_char                 terminator)
// {
// }

// /* //----------------------------------------------------------------------------
// void
// MarshalCRC::process(const char* name, u_char** bp,
//                      u_int32_t* lenp, int flags)
// {
//     (void)name;
//     if(flags & Serialize::NULL_TERMINATED) {
//         crc_.update(*bp, strlen(reinterpret_cast<char*>(*bp)));
//     } else {
//         crc_.update(*bp, *lenp);
//     }
// }*/

// //----------------------------------------------------------------------------
// void
// MarshalCRC::process(const char* name, std::string* s)
// {
//     (void)name;
//     crc_.update((u_char*)s->c_str(), s->size());
// }

/******************************************************************************
 *
 * MarshalCopy
 *
 *****************************************************************************/
u_int32_t
MarshalCopy::copy(ExpandableBuffer* buf,
                  const SerializableObject* src,
                  SerializableObject* dst)
{
    Marshal m(Serialize::CONTEXT_LOCAL, buf);
    if (m.action(src) != 0) {
        PANIC("error marshalling object");
    }
 
    Unmarshal um(Serialize::CONTEXT_LOCAL,
                 (const u_char*)buf->raw_buf(), buf->len());
    if (um.action(dst) != 0) {
        PANIC("error marshalling object");
    }

    return buf->len();
}

} // namespace oasys
