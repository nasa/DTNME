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

#ifndef _INLINEFORMATTER_H_
#define _INLINEFORMATTER_H_

#include "../util/StringBuffer.h"

namespace oasys {

/**
 * In some cases (such as templated data structures) where we can't or
 * don't want to use the standard Formatter() virtual callback, this
 * provides an alternate mechanism to print out a string for an
 * object.
 *
 * Each type will therefore require a new implementation for the
 * format() method which should fill in the StringBuffer buf_ with the
 * string contents and then return a pointer to that, thus ensuring
 * that the string is valid for the duration of the call.
 */
template <typename _T>
class InlineFormatter {
public:
    const char* format(const _T& t);
    StaticStringBuffer<64> buf_;
};

/**
 * InlineFormatter for a std::string.
 */
template <>
inline const char*
InlineFormatter<std::string>::format(const std::string& s)
{
    buf_.append(s);
    return buf_.c_str();
}

/**
 * InlineFormatter method for an int.
 */
template <>
inline const char*
InlineFormatter<int>::format(const int& i)
{
    buf_.appendf("%d", i);
    return buf_.c_str();
}

} // namespace oasys

#endif /* _INLINEFORMATTER_H_ */
