/*
 *    Copyright 2012 Raytheon BBN Technologies
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
 * 
 * PrimitiveType.h
 *
 * Defines the primitive datatype abstract class shell for IPND-SD-TLV.
 */

#ifndef PRIMITIVE_TYPE_H_
#define PRIMITIVE_TYPE_H_

#include "IPNDSDTLV.h"
#include <string>

namespace ipndtlv {

class PrimitiveType: public IPNDSDTLV::BaseDatatype {
public:

    /** Minimum length of any primitive datatype is two bytes */
    static const unsigned int MIN_LEN = 2;

    /**
     * See IPNDSDTLV::BaseDatatype
     */
    unsigned int getLength() const;
    virtual unsigned int getMinLength() const;
    virtual int read(const u_char **buf, const unsigned int &len_remain);
    virtual int write(const u_char **buf,
            const unsigned int &len_remain) const;

    /**
     * Returns the length (in bytes) of datatype structure excluding the tag
     * field. The getLength function will add the tag length to this value.
     * Subclasses must implement.
     *
     * @return The length in bytes of the datatype structure, excluding the tag
     */
    virtual unsigned int getRawContentLength() const = 0;

    /**
     * Returns a pointer to a memory location containing the raw content of this
     * structure (excluding the tag). The raw content should be
     * getRawContentLength() bytes in size. It is recommended that subclasses
     * manage this block of memory internally and simply hand out pointers to
     * it when requested.
     * Subclasses must implement
     *
     * @return A pointer to this structure's raw content in memory
     */
    virtual u_char *getRawContent() const = 0;

    /**
     * Reads out of the given buffer and interprets as the raw content of
     * this datatype. The *buf pointer is positioned immediately after the tag
     * value for this datatype. Subclasses should use the len_remain parameter
     * to make sure they don't read beyond the bounds of the buffer.
     * Subclasses must implement.
     *
     * @param buf Pointer to pointer to the location where this datatype
     *   should begin to read raw content from.
     * @param len_remain The remaining length of the buffer.
     * @return The number of bytes read or N < 0 on any errors
     */
    virtual int readRawContent(const u_char **buf,
            const unsigned int &len_remain) = 0;

    virtual ~PrimitiveType();

protected:
    /**
     * Constructor - Should never be directly instantiated.
     *
     * @param tag_value The tag value of this datatype
     * @param name Human-readable name for datatype (for logging only)
     */
    PrimitiveType(const uint8_t &tag_value, const std::string &name);
};

}

#endif /* PRIMITIVE_TYPE_H_ */
