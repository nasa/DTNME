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


#ifndef _OASYS_STRING_BUFFER_H_
#define _OASYS_STRING_BUFFER_H_

#include "../compat/inttypes.h"
#include <algorithm>

#include "../debug/Log.h" 	// for PRINTFLIKE macro
#include "ScratchBuffer.h"

namespace oasys {

struct ExpandableBuffer;

/**
 * Utility class that wraps a growable string buffer, similar to
 * std::ostringstream, but with printf() style arguments instead of
 * the << operator.
 *
 * The initial size of the buffer is 256 bytes, but can be overridden
 * at the constructor and/or through the reserve() function call.
 */
class StringBuffer {
public:
    /*!
     * @param initsz the initial buffer size
     * @param initstr the initial buffer contents 
     */
    StringBuffer(size_t initsz = 256, const char* initstr = 0);

    //! @param fmt the initial buffer contents
    StringBuffer(const char* fmt, ...) PRINTFLIKE(2, 3);

    //! @{ Create a string buffer with an expandable buffer 
    StringBuffer(ExpandableBuffer* buffer, bool own_buf);
    //! @}
        
    ~StringBuffer();

    /**
     * @return Expandable buffer.
     */
    ExpandableBuffer* expandable_buf() { return buf_; }

    /**
     * @return the data buffer (const variant).
     */
    const char* data() const { return buf_->raw_buf(); }

    /**
     * @return the data buffer (non-const variant)
     */
    char* data() { return buf_->raw_buf(); }

    /**
     * @return String length of the buffer (excluding any '\0'
     * character that is added by c_str().
     */
    size_t length() const { return buf_->len(); }

    /**
     * @return the data buffer, ensuring null termination.
     */
    const char* c_str() const;

    /**
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @param len string length (if unspecified, will call strlen())
     * @return the number of bytes written
     */
    size_t append(const char* str, size_t len = 0);

    /**
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @return the number of bytes written
     */
    size_t append(const std::string& str)
    {
        return append(str.data(), str.length());
    }

    /**
     * Append the character to the tail of the buffer.
     *
     * @param c the character
     * @return the number of bytes written (always one)
     */
    size_t append(char c);

    /**
     * Formatting append function.
     *
     * @param fmt the format string
     * @return the number of bytes written
     */
    size_t appendf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Formatting append function. Note that this may not actually
     * format the full string if there's not enough space in the
     * buffer, so the STRINGBUFFER_VAPPENDF macro below should be used in most
     * cases instead.
     *
     * The lenp parameter can be used to pass in the length that the
     * buffer should be set to (if known) and returns the length of
     * the buffer after formatting. If *lenp is less than the return
     * value, the format string was truncated.
     *
     * @param fmt  the format string
     * @param lenp length of the underlying buffer
     * @param ap   the format argument list
     *
     * @return the return value from vsnprintf, i.e. the length of the
     * full formatted string
     */
    size_t vappendf(const char* fmt, size_t* lenp, va_list ap);
    
    /**
     * Append an ascii representation of the given integer.
     *
     * This is the same as calling appendf("%d", val), only faster.
     */
    size_t append_int(u_int32_t val, int base);

    /**
     * Append an ascii representation of the given integer.
     *
     * This is the same as calling appendf("%d", val), only faster.
     */
    size_t append_int(u_int64_t val, int base);

    /**
     * Trim cnt characters from the tail of the string.
     */
    void trim(size_t cnt)
    {
        ASSERT(buf_->len() >= cnt);
        buf_->set_len(buf_->len() - cnt);
    }

    /**
     * Forcibly set the buffer length to len.
     */
    void set_length(size_t len)
    {
        ASSERT(buf_->buf_len() >= len);
        buf_->set_len(len);
    }

    /**
     * Clear the whole buffer.
     */
    void clear() { buf_->clear(); }

private:
    mutable ExpandableBuffer* buf_;
    bool    own_buf_;
};

/**
 * Since it's unsafe to call a function multiple times with the same
 * va_list, this wrapper macro is needed to wrap multiple calls to
 * vsnprintf in a varargs function.
 */
#define STRINGBUFFER_VAPPENDF(_stringbuf, _fmt)                              \
    do {                                                        \
        size_t ret;                                             \
        size_t len = 0;                                         \
                                                                \
        /* call once optimistically with no known length */     \
        {                                                       \
            va_list ap;                                         \
            va_start(ap, _fmt);                                 \
            ret = (_stringbuf).vappendf(_fmt, &len, ap);        \
            va_end(ap);                                         \
        }                                                       \
                                                                \
        /* call again with the known length */                  \
        if (ret >= len)                                         \
        {                                                       \
            (_stringbuf).trim(len);                             \
            len = ret;                                          \
            va_list ap;                                         \
            va_start(ap, _fmt);                                 \
            ret = (_stringbuf).vappendf(_fmt, &len, ap);        \
            va_end(ap);                                         \
        }                                                       \
                                                                \
        ASSERT(ret == len);                                     \
    } while (0)

/**
 * Initially stack allocated StringBuffer, which handles the common
 * cases where the StringBuffer is used to sprintf a bunch of stuff
 * together.
 *
 * Basically this is just syntactic sugar for passing the correct type
 * of expandable buffer to the StringBuffer class.
 */
template<size_t _sz>
class StaticStringBuffer : public StringBuffer {
public:
    /**
     * Default constructor
     */
    StaticStringBuffer()
        : StringBuffer(new ScratchBuffer<char*, _sz>(), true) {}
    
    /**
     * Constructor with an initial format string.
     *
     * @param fmt Initial string value
     */
    StaticStringBuffer(const char* fmt, ...) PRINTFLIKE(2, 3);
};

template <size_t _sz>
StaticStringBuffer<_sz>::StaticStringBuffer(const char* fmt, ...)
    : StringBuffer(new ScratchBuffer<char*, _sz>(), true)
{
    if (fmt != 0) 
    {
        STRINGBUFFER_VAPPENDF(*this, fmt);
    }
}


} // namespace oasys

#endif /* _OASYS_STRING_BUFFER_H_ */
