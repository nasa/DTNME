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

#include <stdlib.h>

#include "StringBuffer.h"
#include "StringUtils.h"

#include "ExpandableBuffer.h"
#include "io/IOClient.h"

namespace oasys {

StringBuffer::StringBuffer(size_t initsz, const char* initstr)
    : buf_(0), own_buf_(true)
{
    buf_ = new ExpandableBuffer();
    ASSERT(buf_ != 0);

    ASSERT(initsz != 0);
    buf_->reserve(initsz);

    if (initstr) {
        append(initstr);
    }
}
    
StringBuffer::StringBuffer(const char* fmt, ...)
    : buf_(0), own_buf_(true)
{
    buf_ = new ExpandableBuffer();
    ASSERT(buf_);
    buf_->reserve(256);

    if (fmt != 0) 
    {
        STRINGBUFFER_VAPPENDF(*this, fmt);
    }
}

StringBuffer::StringBuffer(ExpandableBuffer* buffer, bool own_buf)
    : buf_(buffer), own_buf_(own_buf)
{
    ASSERT(buf_ != 0);
    buf_->reserve(256);
}

StringBuffer::~StringBuffer()
{
    if (own_buf_)
        delete_z(buf_);
}

const char*
StringBuffer::c_str() const
{
    // we make sure there's a null terminator but don't bump up len_
    // to count it, just like std::string
    if (buf_->len() == 0 || (*buf_->at(buf_->len() - 1) != '\0'))
    {
        if (buf_->nfree() == 0) {
            buf_->reserve(buf_->len() + 1);
        }
        
        *buf_->end() = '\0';
    }
    
    return data();
}

size_t
StringBuffer::append(const char* str, size_t len)
{
    if (len == 0) {
        len = strlen(str);

        // might be a zero length string after all
        if (len == 0) {
            return 0;
        }
    }
    
    // len is not past the end of str
    ASSERT(len <= strlen(str));

    buf_->reserve(buf_->len() + len);
    memcpy(buf_->end(), str, len);
    buf_->set_len(buf_->len() + len);
    
    return len;
}

size_t
StringBuffer::append(char c)
{
    buf_->reserve(buf_->len() + 1);
    *buf_->end() = c;
    buf_->set_len(buf_->len() + 1);

    return 1;
}

size_t
StringBuffer::append_int(u_int32_t val, int base)
{
    char tmp[16];
    size_t len = fast_ultoa(val, base, &tmp[15]);

    ASSERT(len < 16);
    
    buf_->reserve(buf_->len() + len);
    memcpy(buf_->end(), &tmp[16 - len], len);
    buf_->set_len(buf_->len() + len);

    return len;
}

size_t
StringBuffer::append_int(u_int64_t val, int base)
{
    char tmp[16];
    size_t len = fast_ultoa(val, base, &tmp[15]);

    ASSERT(len < 16);
    
    buf_->reserve(buf_->len() + len);
    memcpy(buf_->end(), &tmp[16 - len], len);
    buf_->set_len(buf_->len() + len);

    return len;
}

size_t
StringBuffer::vappendf(const char* fmt, size_t* lenp, va_list ap)
{
    if (buf_->nfree() < (*lenp + 1))
    {
        ASSERT(buf_->buf_len() != 0);
        buf_->reserve(std::max(length() + *lenp + 1, buf_->buf_len() * 2));
        ASSERT(buf_->nfree() >= (*lenp + 1));
    }

    int ret = log_vsnprintf(buf_->end(), buf_->nfree(), fmt, ap);

    // Note that we don't support old glibc implementations that
    // return -1 from vsnprintf when the output is truncated, but
    // depend on vsnprintf returning the length that the formatted
    // string would have been
    ASSERT(ret >= 0);

    *lenp = std::min(ret, (int)buf_->nfree());
    buf_->set_len(buf_->len() + *lenp);
        
    return ret;
}

size_t
StringBuffer::appendf(const char* fmt, ...)
{
    size_t oldlen = buf_->len();
    STRINGBUFFER_VAPPENDF(*this, fmt);
    return buf_->len() - oldlen;
}

} // namespace oasys
