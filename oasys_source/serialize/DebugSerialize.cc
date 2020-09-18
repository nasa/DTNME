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

#include "DebugSerialize.h"

namespace oasys {

//----------------------------------------------------------------------------
DebugSerialize::DebugSerialize(context_t context, char* buf, size_t len)
    : SerializeAction(Serialize::MARSHAL, context),
      buf_(buf, len),
      indent_(0)
{}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, u_int64_t* i)
{
    buf_.appendf("%s: %llu\n", name, U64FMT(*i));
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, u_int32_t* i)
{
    buf_.appendf("%s: %u\n", name, *i);
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, u_int16_t* i)
{
    buf_.appendf("%s: %u\n", name, (u_int32_t)*i);
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, u_int8_t* i)
{
    buf_.appendf("%s: %u\n", name, (u_int32_t)*i);
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, bool* b)
{
    buf_.appendf("%s: %s\n", name, (*b) ? "true" : "false" );
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, u_char* buf, u_int32_t len)
{
    buf_.appendf("%s: binary addr=%p length=%u\n", name, buf, len);
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char*            name, 
                        BufferCarrier<u_char>* carrier)
{
    buf_.appendf("%s: binary addr=%p length=%zu\n", 
                 name, carrier->buf(), carrier->len());
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char*            name,
                        BufferCarrier<u_char>* carrier,
                        u_char                 terminator)
{
    size_t len = 0;
    while (carrier->buf()[len] != terminator)
        ++len;

    buf_.appendf("%s: binary addr=%p length=%zu\n", 
                 name, carrier->buf(), len);
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, std::string* s)
{
    buf_.appendf("%s: %s\n", name, s->c_str());
}

//----------------------------------------------------------------------------
void 
DebugSerialize::process(const char* name, SerializableObject* object)
{
    buf_.appendf("%s: object addr=%p\n", name, object);
    indent();
    object->serialize(this);
    unindent();
}

//----------------------------------------------------------------------------
void
DebugSerialize::add_indent()
{
    for (int i=0; i<indent_; ++i)
        buf_.append("  ");
}

} // namespace oasys
