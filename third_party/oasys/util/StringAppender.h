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



#ifndef _OASYS_STRING_APPENDER_H_
#define _OASYS_STRING_APPENDER_H_

#include "../debug/Log.h" // for PRINTFLIKE macro

namespace oasys {

/*!
 * Enable appending to a statically sized string without overflow and
 * be able to return the length of buffer needed if the static buffer
 * was not the correct size. Useful for implementing
 * Formatter::format(). 
 */
class StringAppender {
public:
    /*!
     * @param buf  Buffer to append strings to.
     * @param size Size of buf. This includes room for the terminating '\0'.
     */
    StringAppender(char* buf, size_t size);

    /*!
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @param len string length (if unspecified, will call strlen())
     * @return the number of bytes written
     */
    size_t append(const char* str, size_t len = 0);

    /*!
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @return the number of bytes written
     */
    size_t append(const std::string& str)
    {
        return append(str.data(), str.length());
    }

    /*!
     * Append the character to the tail of the buffer.
     *
     * @param c the character
     * @return the number of bytes written (always one)
     */
    size_t append(char c);

    /*!
     * Formatting append function.
     *
     * @param fmt the format string
     * @return the number of bytes written
     */
    size_t appendf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /*!
     * Formatting append function.
     *
     * @param fmt the format string
     * @param ap the format argument list
     * @return the number of bytes written
     */
    size_t vappendf(const char* fmt, va_list ap);

    /*!
     * @return resulting length of the buffer.
     */
    size_t length() { return len_; }

    /*!
     * @return Length of the whole string, if there was infinite
     * space.
     */
    size_t desired_length() { return desired_; }
    
private:
    char*  cur_;
    size_t remaining_;
    size_t len_;
    size_t desired_;
};

} // namespace oasys

#endif // OASYS_STRING_APPENDER
