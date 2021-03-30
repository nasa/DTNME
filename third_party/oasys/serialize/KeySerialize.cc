/*
 *    Copyright 2006 Intel Corporation
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

#include "KeySerialize.h"

namespace oasys {

//////////////////////////////////////////////////////////////////////////////
KeyMarshal::KeyMarshal(ExpandableBuffer* buf,
                       const char*       border)
    : SerializeAction(Serialize::MARSHAL, Serialize::CONTEXT_LOCAL),
      buf_(buf),
      border_(border)
{}
    
//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    u_int64_t*  i)
{
    (void)name;
    process_int64(*i, 16, "%16x");
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    u_int32_t*  i)
{
    (void)name;
    process_int(*i, 8, "%08x");
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    u_int16_t*  i)
{
    (void)name;
    process_int(*i, 4, "%04x");
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    u_int8_t*   i)
{
    (void)name;
    process_int(*i, 2, "%02x");
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    bool*       b)
{
    (void)name;
    process_int( (*b) ? 1 : 0, 1, "%1u");
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char* name,
                    u_char*     bp,
                    u_int32_t   len)
{
    (void)name;
    if (error()) 
        return;

    buf_->reserve(buf_->len() + len);
    memcpy(buf_->end(), bp, len);
    buf_->set_len(buf_->len() + len);
    border();
}

//////////////////////////////////////////////////////////////////////////////
void
KeyMarshal::process(const char*            name, 
                    BufferCarrier<u_char>* carrier)
{
    (void) name;

    if (error()) {
        return;
    }
    
    process_int(carrier->len(), 8, "%08x");
    buf_->reserve(buf_->len() + carrier->len());
    memcpy(buf_->end(), carrier->buf(), carrier->len());
    buf_->set_len(buf_->len() + carrier->len());
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char*            name,
                    BufferCarrier<u_char>* carrier,
                    u_char                 terminator)
{
    (void) name;

    if (error()) {
        return;
    }

    size_t size = 0;
    while (carrier->buf()[size] != terminator)
    {
        ++size;
    }
    carrier->set_len(size);
    
    process(name, carrier);
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char*  name,
                    std::string* s)
{
    (void)name;
    if (error()) 
        return;

    process_int(s->length(), 8, "%08x");
    buf_->reserve(buf_->len() + s->size());
    memcpy(buf_->end(), s->c_str(), s->size());
    buf_->set_len(buf_->len() + s->size());
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyMarshal::process(const char*         name,
                    SerializableObject* object)
{
    (void)name;
    if (error()) 
        return;

    int err = action(object);
    if (err != 0) {
        signal_error();
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
void
KeyMarshal::end_action()
{
    buf_->reserve(1);
    *(buf_->end()) = '\0';
}

//////////////////////////////////////////////////////////////////////////////
void
KeyMarshal::process_int(u_int32_t i, u_int32_t size, const char* format)
{
    if (error()) 
        return;

    buf_->reserve(buf_->len() + size + 1);
    int cc = snprintf(buf_->end(), size + 1, format, i);
    ASSERT(cc == (int)size);
    buf_->set_len(buf_->len() + size);
}

//////////////////////////////////////////////////////////////////////////////
void
KeyMarshal::process_int64(u_int64_t i, u_int32_t size, const char* format)
{
    if (error()) 
        return;

    buf_->reserve(buf_->len() + size + 1);
    int cc = snprintf(buf_->end(), size + 1, format, i);
    ASSERT(cc == (int)size);
    buf_->set_len(buf_->len() + size);
}

//////////////////////////////////////////////////////////////////////////////
void
KeyMarshal::border()
{
    if (error() || border_ == 0) {
        return;
    }

    u_int32_t border_len = strlen(border_);
    buf_->reserve(border_len);
    memcpy(buf_->end(), border_, border_len);
    buf_->set_len(buf_->len() + border_len);
}

//////////////////////////////////////////////////////////////////////////////
KeyUnmarshal::KeyUnmarshal(const char* buf,
                           u_int32_t   buf_len,
                           const char* border)
    : SerializeAction(Serialize::UNMARSHAL, Serialize::CONTEXT_LOCAL),
      buf_(buf),
      buf_len_(buf_len),
      border_len_( (border == 0) ? 0 : strlen(border)),
      cur_(0)
{}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, u_int64_t* i)
{
    (void)name;
    u_int64_t val = process_int64();
    if (! error()) {
        *i = val;
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, u_int32_t* i)
{
    (void)name;
    u_int32_t val = process_int(8);
    if (! error()) {
        *i = val;
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, u_int16_t* i)
{
    (void)name;
    u_int16_t val = static_cast<u_int16_t>(process_int(4));
    if (! error()) {
        *i = val;
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, u_int8_t* i)
{
    (void)name;
    u_int8_t val = static_cast<u_int8_t>(process_int(2));
    if (! error()) {
        *i = val;
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, bool* b)
{
    (void)name;
    if (error()) {
        return;
    }

    if (cur_ + 1 > buf_len_) {
        signal_error();
        return;
    }
    
    *b = (buf_[cur_] == '1') ? true : false;
    cur_ += 1;
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)name;
    if (error()) {
        return;
    }

    if (cur_ + len > buf_len_) {
        signal_error();
        return;
    }

    memcpy(bp, &buf_[cur_], len);
    cur_ += len;
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char*            name, 
                      BufferCarrier<u_char>* carrier)
{
    (void)name;

    ASSERT(carrier->is_empty());

    if (error()) {
        return;
    }
    
    u_int32_t len = process_int(8);
    if (cur_ + len > buf_len_) {
        signal_error();
        return;
    }

    u_char* buf = static_cast<u_char*>(malloc(sizeof(u_char) * len));
    ASSERT(buf != 0);
    memcpy(buf, &buf_[cur_], len);
    cur_ += len;
    border();

    carrier->set_buf(buf, len, true);
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char*            name,
                      BufferCarrier<u_char>* carrier,
                      u_char                 terminator)
{
    (void)name;

    ASSERT(carrier->is_empty());

    if (error()) {
        return;
    }
    
    u_int32_t len = process_int(8);
    if (cur_ + len > buf_len_) {
        signal_error();
        return;
    }

    u_char* buf = static_cast<u_char*>(malloc(sizeof(u_char) * len + 1));
    ASSERT(buf != 0);

    memcpy(buf, &buf_[cur_], len);
    buf[len] = terminator;

    cur_ += len;
    border();

    carrier->set_buf(buf, len + 1, true);
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, std::string* s) 
{
    (void)name;
    if (error()) {
        return;
    }

    u_int32_t len = process_int(8);
    if (error()) {
        return;
    }

    s->assign(&buf_[cur_], len);
    cur_ += len;
    border();
}

//////////////////////////////////////////////////////////////////////////////
void 
KeyUnmarshal::process(const char* name, SerializableObject* object)
{
    (void)name;
    if (error()) {
        return;
    }

    if (action(object) != 0) {
        signal_error();
    }
    border();
}

//////////////////////////////////////////////////////////////////////////////
u_int32_t
KeyUnmarshal::process_int(u_int32_t size)
{
    char buf[9];

    if (cur_ + size > buf_len_) {
        signal_error();
        return 0;
    }

    memset(buf, 0, 9);
    memcpy(buf, &buf_[cur_], size);
    
    char* endptr;
    u_int32_t val = strtoul(buf, &endptr, 16);
    
    if (endptr == &buf_[cur_]) {
        signal_error();
        return 0;
    }

    cur_ += size;

    return val;
}

//////////////////////////////////////////////////////////////////////////////
u_int64_t
KeyUnmarshal::process_int64()
{
    u_int32_t size = 16;
    char buf[32];

    if (cur_ + size > buf_len_) {
        signal_error();
        return 0;
    }

    memset(buf, 0, 32);
    memcpy(buf, &buf_[cur_], size);
    
    char* endptr;
    u_int64_t val = strtoull(buf, &endptr, 16);
    
    if (endptr == &buf_[cur_]) {
        signal_error();
        return 0;
    }

    cur_ += size;

    return val;
}

//////////////////////////////////////////////////////////////////////////////
void
KeyUnmarshal::border()
{
    cur_ += border_len_;
}

} // namespace oasys
