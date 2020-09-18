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
 * PrimitiveType.cc
 */

#include "PrimitiveType.h"
#include <cstring>

namespace ipndtlv {

PrimitiveType::PrimitiveType(const uint8_t &tag_value,
        const std::string &name)
    : IPNDSDTLV::BaseDatatype(tag_value,
            (name.size() == 0)?"p/undef":"p/"+name) {

    if(getTag() >= IPNDSDTLV::PRIMITIVE_MASK) {
        log_warn("Primitive type tag value out-of-spec [%u]!", getTag());
    }

    if(name.size() == 0) {
        log_warn("Constructing Primitive type with no defined name!");
    }
}

PrimitiveType::~PrimitiveType() {}

unsigned int PrimitiveType::getLength() const {
    // length = (tag length) + (content length)
    return 1 + getRawContentLength();
}

unsigned int PrimitiveType::getMinLength() const {
    return MIN_LEN;
}

int PrimitiveType::read(const u_char **buf, const unsigned int &len_remain) {
    if(!lengthCheck(len_remain, getMinLength(), "read")) {
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        u_char *ptr = (u_char*)*buf;

        // check the tag
        uint8_t tag;
        memcpy(&tag, ptr, 1);

        if(!tagCheck(tag, "read")) {
            return IPNDSDTLV::ERR_TAG;
        } else {
            ptr++;

            int readLen =
                    readRawContent((const u_char**)&ptr, (len_remain - 1));

            if(readLen < 0) {
                return readLen;
            } else {
                setFilled();
                return 1 + readLen;
            }
        }
    }
}

int PrimitiveType::write(const u_char **buf,
        const unsigned int &len_remain) const {

    const unsigned int len = getLength();

    if(!lengthCheck(len_remain, len, "write")) {
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        u_char *ptr = (u_char*)*buf;

        // copy the tag value
        uint8_t tag = getTag();
        memcpy(ptr, &tag, 1);

        // copy the content
        const unsigned int cLen = getRawContentLength();
        u_char *content = getRawContent();
        memcpy(ptr+1, content, cLen);

        return len;
    }
}

}
