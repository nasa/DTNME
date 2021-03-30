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

#include <ctype.h>
#include <string.h>
#include "HexDumpBuffer.h"
#include "StringBuffer.h"

namespace oasys {
    
    
void
HexDumpBuffer::append(const u_char* data, size_t length)
{
    char* dest = tail_buf(length);
    memcpy(dest, data, length);
    incr_len(length);
}

std::string
HexDumpBuffer::hexify()
{
    StringBuffer builder;
    char strbuf[16];

    // generate the dump
    u_char* bp = (u_char*)raw_buf();
    for (size_t i = 0; i < len(); ++i, ++bp)
    {
        // print the offset on each new line
        if (i % 16 == 0) {
            builder.appendf("%07x ", (u_int)i);
        }
        
        // otherwise print a space every two bytes (except the first)
        else if (i % 2 == 0) {
            builder.append(" ");
        }

        // print the hex character
        builder.appendf("%02x", *bp);
        
        // tack on the ascii character if it's printable, '.' otherwise
        if (isalnum(*bp) || ispunct(*bp) || (*bp == ' ')) {
            strbuf[i % 16] = *bp;
        } else {
            strbuf[i % 16] = '.';
        }
        
        if (i % 16 == 15)
        {
            builder.appendf(" |  %.*s\n", 16, strbuf);
        }
    }

    // deal with the ending partial line
    for (size_t i = len() % 16; i < 16; ++i) {
        if (i % 2 == 0) {
            builder.append(" ");
        }

        builder.append("  ");
    }

    builder.appendf(" |  %.*s\n", (int)len() % 16, strbuf);
    return std::string(builder.c_str(), builder.length());
}

} // namespace oasys
