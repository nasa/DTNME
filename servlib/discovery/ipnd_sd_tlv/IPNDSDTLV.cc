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
 * IPNDSDTLV.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "IPNDSDTLV.h"
#include "bundling/SDNV.h"
#include <cstring>

namespace ipndtlv {

IPNDSDTLV::BaseDatatype::BaseDatatype(const uint8_t &tag_value,
        const std::string &name)
    : oasys::Logger("BaseDatatype", "/ipnd-tlv/type/%s",
            (name.size() == 0)?"undef":name.c_str()),
      tag_(tag_value),
      log_name_(name),
      content_read_(false) {

    if(log_name_.size() == 0) {
        log_warn("Constructing BaseDatatype with no defined name!");
    }
}

IPNDSDTLV::BaseDatatype::~BaseDatatype() {
    // nop
}

uint8_t IPNDSDTLV::BaseDatatype::getTag() const {
    return (const uint8_t)tag_;
}

bool IPNDSDTLV::BaseDatatype::isFilled() const {
    return content_read_;
}

void IPNDSDTLV::BaseDatatype::setFilled() {
    content_read_ = true;
}

bool IPNDSDTLV::BaseDatatype::lengthCheck(const unsigned int &len,
        const unsigned int &req, const std::string &action) const {
    if(len < req) {
        log_err("Type %s failure; insufficient remaining length [%u<%u]",
                action.c_str(), len, req);
        return false;
    } else {
        return true;
    }
}

bool IPNDSDTLV::BaseDatatype::tagCheck(const uint8_t &tag,
        const std::string &action) const {
    if(tag != getTag()) {
        log_err("Type %s failure; tag mismatch [%u/%u]!", action.c_str(), tag,
                getTag());
        return false;
    } else {
        return true;
    }
}

int IPNDSDTLV::BaseDatatype::readSdnv(const u_char **buf,
        const unsigned int &len_remain, uint64_t *value) const {
    if(!lengthCheck(len_remain, 1, "read sdnv")) {
        return IPNDSDTLV::ERR_LENGTH;
    }

    // XXX (agladd) We're doing all this before calling SDNV::decode in order to
    // perform our own sanity checks of the lengths.

    u_char sdnv[dtn::SDNV::MAX_LENGTH] = {'\0'};
    u_char *ptr = sdnv, *mbuf = (u_char*)*buf;
    unsigned int len = 0;
    unsigned int bound = (len_remain < dtn::SDNV::MAX_LENGTH)?
            len_remain:static_cast<unsigned int>(dtn::SDNV::MAX_LENGTH);
    bool hit_bound = false;

    memcpy(ptr, mbuf, 1);
    len++;

    // keep reading as long as the MSB is set and we don't hit bound
    while(*ptr & 0x80) {
        if(len >= bound) {
            hit_bound = true;
            break;
        } else {
            ptr++;
            mbuf++;
            memcpy(ptr, mbuf, 1);
            len++;
        }
    }

    if(hit_bound) {
        log_err("Could not read full SDNV due to length constraints! [%u]",
                len);
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        unsigned int dLen = dtn::SDNV::decode(sdnv, len, (u_int64_t*)value);
        if(dLen != len) {
            log_err("SDNV decode length mismatch! [%u!=%u]", dLen, len);
            return IPNDSDTLV::ERR_PROCESSING;
        } else {
            return len;
        }
    }
}

bool IPNDSDTLV::isReservedType(const uint8_t &tag) {
    if((tag & RESERVED_MASK) == 0) {
        return true;
    } else {
        return false;
    }
}

bool IPNDSDTLV::isReservedPrimitive(const uint8_t &tag) {
    if(isReservedType(tag) && (tag & PRIMITIVE_MASK) == 0) {
        return true;
    } else {
        return false;
    }
}

bool IPNDSDTLV::isReservedConstructed(const uint8_t &tag) {
    if(isReservedType(tag) && (tag & PRIMITIVE_MASK) > 0) {
        return true;
    } else {
        return false;
    }
}

IPNDSDTLV::IPNDSDTLV() {
    // should never get here
    // TODO (agladd): maybe should log a warning here?
}

IPNDSDTLV::~IPNDSDTLV() {
    // should never get here
}

}
