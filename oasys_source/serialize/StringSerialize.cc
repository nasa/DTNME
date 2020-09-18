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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "StringSerialize.h"

namespace oasys {

//----------------------------------------------------------------------
StringSerialize::StringSerialize(context_t context, int options)
    : SerializeAction(Serialize::INFO, context, options)
{
    if (options_ & DOT_SEPARATED) {
        sep_ = '.';
    } else {
        sep_ = ' ';
    }
}

//----------------------------------------------------------------------
void
StringSerialize::add_preamble(const char* name, const char* type)
{
    if (options_ & INCLUDE_NAME) {
        buf_.append(name);
        buf_.append(sep_);
    }

    if (options_ & INCLUDE_TYPE) {
        buf_.append(type);
        buf_.append(sep_);
    }
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, u_int64_t* i)
{
    add_preamble(name, "u_int64_t");
    if (options_ & SCHEMA_ONLY) {
        return;
    }
            
    buf_.append_int(*i, 10);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, u_int32_t* i)
{
    add_preamble(name, "u_int32_t");
    if (options_ & SCHEMA_ONLY) {
        return;
    }
            
    buf_.append_int(*i, 10);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, u_int16_t* i)
{
    add_preamble(name, "u_int16_t");
    if (options_ & SCHEMA_ONLY) {
        return;
    }
            
    buf_.append_int(static_cast<u_int32_t>(*i), 10);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, u_int8_t* i)
{
    add_preamble(name, "u_int8_t");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    buf_.append_int(static_cast<u_int32_t>(*i), 10);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, bool* b)
{
    add_preamble(name, "bool");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    if (*b) {
        buf_.append("true", 4);
    } else {
        buf_.append("false", 5);
    }
        
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, u_char* bp, u_int32_t len)
{
    add_preamble(name, "char_buf");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    buf_.append((const char*)bp, len);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::process(const char* name, std::string* s)
{
    add_preamble(name, "string");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    buf_.append(s->data(), s->length());
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void 
StringSerialize::process(const char*            name, 
                         BufferCarrier<u_char>* carrier)
{
    add_preamble(name, "char_buf_var");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    buf_.append(reinterpret_cast<char*>(carrier->buf()), carrier->len());
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void 
StringSerialize::process(const char*            name,
                         BufferCarrier<u_char>* carrier,
                         u_char                 terminator)
{
    add_preamble(name, "char_buf_var");
    if (options_ & SCHEMA_ONLY) {
        return;
    }

    size_t size = 0;
    while (carrier->buf()[size] != terminator)
    {
        ++size;
    }
    buf_.append(reinterpret_cast<char*>(carrier->buf()), size);
    buf_.append(sep_);
}

//----------------------------------------------------------------------
void
StringSerialize::end_action()
{
    // trim trailing separator
    if (buf_.length() != 0) {
        buf_.trim(1);
    }
}

} // namespace oasys
