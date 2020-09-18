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

#include "StringBuffer.h"
#include "TextCode.h"

namespace oasys {

TextCode::TextCode(const char* input_buf, size_t length, 
                   ExpandableBuffer* buf, int cols, int pad)
    : input_buf_(input_buf), length_(length), 
      buf_(buf, false), cols_(cols), pad_(pad)
{
    textcodify();
}

bool
TextCode::is_not_escaped(char c) {
    return c >= 32 && c <= 126 && c != '\\';
}

void 
TextCode::append(char c) {
    if (is_not_escaped(c)) {
        buf_.append(static_cast<char>(c));
    } else if (c == '\\') {
        buf_.appendf("\\\\");
    } else {
        buf_.appendf("\\%02x", ((int)c & 0xff));
    }
}

void
TextCode::textcodify()
{
    for (size_t i=0; i<length_; ++i) 
    {
        if (i % cols_ == 0) 
        {
            if (i != 0) {
                buf_.append('\n');
            }            
            for (int j=0; j<pad_; ++j)
                buf_.append('\t');
        }
        append(input_buf_[i]);
    }
    buf_.append('\n');
    for (int j=0; j<pad_; ++j)
        buf_.append('\t');
    buf_.append("\n");
}

//----------------------------------------------------------------------------
TextUncode::TextUncode(const char* input_buf, size_t length,
                       ExpandableBuffer* buf)
    : input_buf_(input_buf), 
      length_(length), 
      buf_(buf, false), 
      cur_(input_buf), 
      error_(false)
{
    textuncodify();
}

//----------------------------------------------------------------------------
void
TextUncode::textuncodify()
{
    // each line is {\t}*textcoded stuff\n
    while (true) {
        if (! in_buffer()) {
            error_ = true;
            return;
        }

        if (*cur_ == '') {
            break;
        }

        if (*cur_ == '\t' || *cur_ == '\n') {
            ++cur_;
            continue;
        }
        
        if (*cur_ == '\\') {
            if (!in_buffer(1)) {
                error_ = true;
                return;
            }
            
            if (cur_[1] == '\\') {
                buf_.append('\\');
                cur_ += 2;
                continue;
            }
            
            if (!in_buffer(3)) {
                error_ = true;
                return;
            }

            ++cur_;
            int value = strtol(cur_, 0, 16);
            buf_.append(static_cast<char>(value));
        } else {
            buf_.append(*cur_);
            ++cur_;
        }
    }
}

} // namespace oasys
