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


#ifndef _OASYS_HEX_DUMP_BUFFER_H_
#define _OASYS_HEX_DUMP_BUFFER_H_

#include <string>
#include <sys/types.h>

#include "ExpandableBuffer.h"

namespace oasys {

/**
 * Class to produce pretty printing output from data that may be
 * binary (ala emacs' hexl-mode). Each line includes the offset, plus
 * 16 characters from the file as well as their ascii values (or . for
 * unprintable characters).
 *
 * For example:
 *
 * 00000000: 5468 6973 2069 7320 6865 786c 2d6d 6f64  This is hexl-mod
 * 00000010: 652e 2020 4561 6368 206c 696e 6520 7265  e.  Each line re
 * 00000020: 7072 6573 656e 7473 2031 3620 6279 7465  presents 16 byte
 * 00000030: 7320 6173 2068 6578 6164 6563 696d 616c  s as hexadecimal
 * 00000040: 2041 5343 4949 0a61 6e64 2070 7269 6e74   ASCII.and print
 */
class HexDumpBuffer : public ExpandableBuffer {
public:
    /**
     * Constructor
     *
     * @param initsz the initial buffer size
     * @param initstr the initial buffer contents 
     */
    HexDumpBuffer(size_t initsz = 256)
        : ExpandableBuffer(initsz) {}
    
    void append(const u_char* data, size_t length);

    /**
     * Convert the internal buffer (accumulated into the StringBuffer)
     * into hex dump output format.
     */
    std::string hexify();
};

} // namespace oasys

#endif /* _OASYS_HEX_DUMP_BUFFER_H_ */
