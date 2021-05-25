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

#include "TextSerialize.h"
#include "../util/TextCode.h"

namespace oasys {

//----------------------------------------------------------------------------
TextMarshal::TextMarshal(context_t         context,
                         ExpandableBuffer* buf,
                         int               options,
                         const char*       comment)
    : SerializeAction(Serialize::MARSHAL, context, options),
      indent_(0), 
      buf_(buf, false)
{
    buf_.append("# -- text marshal start --\n");
    if (comment != 0) 
        buf_.append(comment);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_int64_t* i)
{
    buf_.appendf("%s: %llu\n", name, U64FMT(*i));
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_int32_t* i)
{
    buf_.appendf("%s: %u\n", name, *i);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_int16_t* i)
{
    buf_.appendf("%s: %u\n", name, (u_int32_t)*i);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_int8_t* i)
{
    buf_.appendf("%s: %u\n", name, (u_int32_t)*i);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, bool* b)
{
    buf_.appendf("%s: %s\n", name, (*b) ? "true" : "false" );
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_char* bp, u_int32_t len)
{
    buf_.appendf("%s: TextCode\n", name);
    TextCode coder(reinterpret_cast<char*>(bp), len,
                   buf_.expandable_buf(), 40, indent_ + 1);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, u_char** bp, u_int32_t* lenp, int flags)
{
    (void)flags;
    buf_.appendf("%s: TextCode\n", name);
    TextCode coder(reinterpret_cast<char*>(*bp), *lenp,
                   buf_.expandable_buf(), 40, indent_ + 1);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, std::string* s)
{
    buf_.appendf("%s: TextCode\n", name);
    TextCode coder(reinterpret_cast<const char*>(s->c_str()),
                   strlen(s->c_str()),
                   buf_.expandable_buf(), 
                   40, indent_ + 1);
}

//----------------------------------------------------------------------------
void 
TextMarshal::process(const char* name, SerializableObject* object)
{
    buf_.appendf("%s: SerializableObject\n", name);
    indent();
    object->serialize(this);
    unindent();
}


//----------------------------------------------------------------------------
void
TextMarshal::add_indent()
{
    for (int i=0; i<indent_; ++i)
        buf_.append('\t');
}

//----------------------------------------------------------------------------
TextUnmarshal::TextUnmarshal(context_t context, u_char* buf, 
                             size_t length, int options)
    : SerializeAction(Serialize::UNMARSHAL, context, options),
      buf_(reinterpret_cast<char*>(buf)), 
      length_(length), 
      cur_(reinterpret_cast<char*>(buf))
{}
 
//----------------------------------------------------------------------------   
void 
TextUnmarshal::process(const char* name, u_int64_t* i)
{
    if (error()) 
        return;

    u_int64_t num;
    int err = get_num(name, &num);
    
    if (err != 0) 
        return;

    *i = num;
}

//----------------------------------------------------------------------------   
void 
TextUnmarshal::process(const char* name, u_int32_t* i)
{
    if (error()) 
        return;

    u_int32_t num;
    int err = get_num(name, &num);
    
    if (err != 0) 
        return;

    *i = num;
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, u_int16_t* i)
{
    if (error()) 
        return;

    u_int32_t num;
    int err = get_num(name, &num);
    
    if (err != 0) 
        return;

    *i = static_cast<u_int16_t>(num);
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, u_int8_t* i)
{
    if (error()) 
        return;

    u_int32_t num;
    int err = get_num(name, &num);
    
    if (err != 0) 
        return;

    *i = static_cast<u_int8_t>(num);
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, bool* b)
{
    if (error()) 
        return;

    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return;
    }
    ASSERT(*eol == '\n');

    if (match_fieldname(name, eol) != 0)
        return;
    
    if (!is_within_buf(4)) {
        signal_error();
        return;
    }
    
    if (memcmp(cur_, "true", 4) == 0) {
        *b = true;
        cur_ = eol + 1;
    } else if (memcmp(cur_, "fals", 4) == 0) {
        *b = false;
        cur_ = eol + 1;
    } else {
        signal_error();
        return;
    }
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, u_char* bp, u_int32_t len)
{
    if (error()) 
        return;
    
    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return;
    }

    if (match_fieldname(name, eol) != 0) {
        signal_error();
        return;        
    }

    cur_ = eol + 1;
    if (! is_within_buf(0)) {
        signal_error();
        return;        
    }
    
    ScratchBuffer<char*, 1024> scratch;
    if (get_textcode(&scratch) != 0) {
        signal_error();
        return;        
    }

    if (len != scratch.len()) {
        signal_error();
        return;                
    }
    
    memcpy(bp, scratch.buf(), len);
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, u_char** bp, 
                       u_int32_t* lenp, int flags)
{
    (void)name;
    (void)bp;
    (void)lenp;
    (void)flags;
    
    if (error()) 
        return;

    NOTIMPLEMENTED;
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, std::string* s)
{
    if (error()) 
        return;
    
    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return;
    }

    if (match_fieldname(name, eol) != 0) {
        signal_error();
        return;        
    }

    cur_ = eol + 1;
    if (! is_within_buf(0)) {
        signal_error();
        return;        
    }
    
    ScratchBuffer<char*, 1024> scratch;
    if (get_textcode(&scratch) != 0) {
        signal_error();
        return;        
    }

    *s = std::string(scratch.buf(), scratch.len());
}

//----------------------------------------------------------------------------
void 
TextUnmarshal::process(const char* name, SerializableObject* object)
{
    if (error())
        return;
    
    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return;
    }

    if (match_fieldname(name, eol) != 0) {
        signal_error();
        return;
    }
    
    cur_ = eol + 1;
    if (! is_within_buf(0)) {
        signal_error();
        return;
    }

    object->serialize(this);
}

//----------------------------------------------------------------------------
bool 
TextUnmarshal::is_within_buf(size_t offset)
{
    return cur_ + offset < buf_ + length_;
}

//----------------------------------------------------------------------------
int  
TextUnmarshal::get_line(char** end)
{
  again:    
    size_t offset = 0;
    while (is_within_buf(offset) && cur_[offset] != '\n') {
        ++offset;
    }

    if (!is_within_buf(offset)) {
        return -1;
    }

    // comment, skip to next line
    if (*cur_ == '#') {
        cur_ = &cur_[offset] + 1;
        goto again;
    }
    
    *end = &cur_[offset];

    return 0;
}

//----------------------------------------------------------------------------
int  
TextUnmarshal::match_fieldname(const char* field_name, char* eol)
{   
    // expecting {\t}* field_name: num\n 
    char* field_name_ptr = 0;
    while (is_within_buf(0) && *cur_ != ':') {
        if (*cur_ != '\t' && *cur_ != ' ' && field_name_ptr == 0)
            field_name_ptr = cur_;
        ++cur_;
    }
    
    if (*cur_ != ':' || cur_ > eol) {
        signal_error();
        return -1;
    }

    if (memcmp(field_name_ptr, field_name, strlen(field_name)) != 0) {
        signal_error();
        return -1;
    }

    cur_ += 2;
    if (!is_within_buf(0)) {
        signal_error();
        return -1;
    }
    
    return 0;
}

//----------------------------------------------------------------------------
int
TextUnmarshal::get_num(const char* field_name, u_int32_t* num)
{
    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return -1;
    }

    ASSERT(*eol == '\n');
    if (match_fieldname(field_name, eol) != 0)
        return -1;
    
    *num = strtoul(cur_, &eol, 0);
    ASSERT(*eol == '\n');
    
    cur_ = eol + 1;
    
    return 0;
}

//----------------------------------------------------------------------------
int
TextUnmarshal::get_num(const char* field_name, u_int64_t* num)
{
    char* eol;
    if (get_line(&eol) != 0) {
        signal_error();
        return -1;
    }

    ASSERT(*eol == '\n');
    if (match_fieldname(field_name, eol) != 0)
        return -1;
    
    *num = strtoull(cur_, &eol, 0);
    ASSERT(*eol == '\n');
    
    cur_ = eol + 1;
    
    return 0;
}

int
TextUnmarshal::get_textcode(ExpandableBuffer* buf)
{
    // find the ^L+1, which is the end of the text coded portion
    size_t end_offset = 0;
    while (true) {
        if (!is_within_buf(end_offset)) {
            signal_error();
            return -1;
        }
        
        if (cur_[end_offset] == '') {
            break;
        }

        ++end_offset;
    }

    ++end_offset;
    if (!is_within_buf(end_offset)) {
        signal_error();
        return -1;
    }

    ASSERT(cur_[end_offset] == '\n');

    ScratchBuffer<char*, 1024> scratch;
    TextUncode uncoder(cur_, end_offset, buf);
    
    if (uncoder.error()) {
        signal_error();
        return -1;
    }

    cur_ += end_offset + 1;
    
    return 0;
}

} // namespace oasys
