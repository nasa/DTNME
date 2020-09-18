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

#ifndef __TEXTSERIALIZE_H__
#define __TEXTSERIALIZE_H__

#include "../serialize/Serialize.h"
#include "../serialize/BufferedSerializeAction.h"
#include "../util/StringBuffer.h"

namespace oasys {

/*!
 *
 * Text marshal format:
 *
 * # Comments
 * {\t\ }*fieldname: value\n
 *   # additional fields...
 * .  # single period
 * 
 */
class TextMarshal : public SerializeAction {
public:
    TextMarshal(context_t         context,
                ExpandableBuffer* buf,
                int               options     = 0,
                const char*       comment     = 0);

    //! @{ Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    void process(const char* name, u_int64_t* i);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s);
    void process(const char* name, SerializableObject* object);

    //! @}

private:
    int indent_;
    StringBuffer buf_;

    void indent()   { indent_++; }
    void unindent() { 
        indent_--;
        ASSERT(indent_ >= 0); 
    }

    void add_indent();
};

class TextUnmarshal : public SerializeAction {
public:
    TextUnmarshal(context_t context, u_char* buf, 
                  size_t length, int options = 0);
    
    //! @{ Virtual functions inherited from BufferedSerializeAction
    using SerializeAction::process;
    void process(const char* name, u_int64_t* i);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s);
    void process(const char* name, SerializableObject* object);
    //! @}

private:
    char* buf_;
    size_t  length_;
    char* cur_;
    
    bool is_within_buf(size_t offset);
    int  get_line(char** end);
    int  match_fieldname(const char* field_name, char* eol);
    int  get_num(const char* field_name, u_int32_t* num);
    int  get_num(const char* field_name, u_int64_t* num);
    int  get_textcode(ExpandableBuffer* buf);
};

} // namespace oasys

#endif /* __TEXTSERIALIZE_H__ */
