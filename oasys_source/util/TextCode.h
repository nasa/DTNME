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


#ifndef __TEXTCODE_H__
#define __TEXTCODE_H__

#include "StringBuffer.h"

namespace oasys {

/*! 
 * Outputs a string that a certain column length with all non-printable
 * ascii characters rewritten in hex.
 * 
 * A TextCode block ends with a single raw control-L character
 * followed by a newline character ("\n") on a single line.
 */
class TextCode {
public:
    /*! 
     * @param input_buf Input buffer
     * @param length Length of the input buffer
     * @param buf  Buffer to put the text coded block into.
     * @param cols Number of characters to put in a column.
     * @param pad  String to put in front of each line.
     */
    TextCode(const char* input_buf, size_t length, 
             ExpandableBuffer* buf, int cols, int pad);
    
private:
    const char*  input_buf_;
    size_t       length_;        
    StringBuffer buf_;

    int cols_;
    int pad_;

    //! Whether or not the character is printable ascii
    bool is_not_escaped(char c);

    //! Perform the conversion
    void textcodify(); 

    //! Append a character to the text code
    void append(char c);
};

class TextUncode {
public:
    TextUncode(const char* input_buf, size_t length,
               ExpandableBuffer* buf);

    bool error() { return error_; }

private:
    const char* input_buf_;
    size_t      length_;
    
    StringBuffer buf_;
    const char*  cur_;

    bool error_;

    bool in_buffer(size_t offset = 0) { 
        return cur_ + offset < input_buf_ + length_; 
    }

    void textuncodify();
};

} // namespace oasys

#endif /* __TEXTCODE_H__ */
