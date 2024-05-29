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

#ifndef __DEBUGSERIALIZE_H__
#define __DEBUGSERIALIZE_H__

#include "../serialize/Serialize.h"
#include "../util/StringAppender.h"

namespace oasys {

/*!
 *
 * # Comments
 * {\t\ }*fieldname: value\n
 *   # additional fields...
 * .  # single period
 * 
 */
class DebugSerialize : public SerializeAction {
public:
    DebugSerialize(context_t context, char* buf, size_t len);

    //! @{ Virtual functions inherited from SerializeAction
    void process(const char* name, u_int64_t* i);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* buf, u_int32_t len);
    void process(const char*            name, 
                 BufferCarrier<u_char>* carrier);
    void process(const char*            name,
                 BufferCarrier<u_char>* carrier,
                 u_char                 terminator);
    void process(const char* name, std::string* s);
    void process(const char* name, SerializableObject* object);

    //! @}

private:
    StringAppender buf_;
    int indent_;

    void indent()   { indent_++; }
    void unindent() { 
        indent_--;
    }
    void add_indent();
};

} // namespace oasys

#endif /* __DEBUGSERIALIZE_H__ */
