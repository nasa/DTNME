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
 * ConstructedType.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "ConstructedType.h"

#include "bundling/SDNV.h"

namespace ipndtlv {

ConstructedType::ConstructedType(const uint8_t &tag_value,
        const std::string &name)
    : IPNDSDTLV::BaseDatatype(tag_value,
            (name.size() == 0)?"c/undef":"c/"+name) {

    if(getTag() < IPNDSDTLV::PRIMITIVE_MASK) {
        log_warn("Constructed type tag value out-of-spec [%u]!", getTag());
    }

    if(name.size() == 0) {
        log_warn("Constructing constructed type with no defined name!");
    }
}

ConstructedType::~ConstructedType() {
    // deconstruct all components
    CompIterator iter;
    for(iter = components_.begin(); iter != components_.end(); iter++) {
        delete (*iter).second;
    }

    // clear components
    components_.clear();
}

unsigned int ConstructedType::getLength() const {
    unsigned int tagLen = 1, compLen, lenLen;

    // first calculate total length of components
    compLen = getComponentsLength();

    // second calculate length of compLen
    lenLen = dtn::SDNV::encoding_len((u_int64_t)compLen);

    return tagLen + lenLen + compLen;
}

unsigned int ConstructedType::getMinLength() const {
    return MIN_LEN;
}

int ConstructedType::read(const u_char **buf, const unsigned int &len_remain) {
    if(!lengthCheck(len_remain, getMinLength(), "read")) {
        return IPNDSDTLV::ERR_LENGTH;
    }

    int readBytes, remainingBytes = (int)len_remain;
    unsigned int totalBytes = 0;
    u_char *ptr = (u_char*)*buf;

    // first read/check the tag
    uint8_t tag;
    memcpy(&tag, ptr, 1);
    remainingBytes--;
    if(!tagCheck(tag, "read")) {
        // bad tag
        return IPNDSDTLV::ERR_TAG;
    } else if(remainingBytes <= 0 ||
            !lengthCheck((unsigned int)remainingBytes, (MIN_LEN - 1), "read")) {
        // not enough data left
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        ptr++;
        totalBytes++;
    }

    // read the structure length
    uint64_t expectedBytes;
    readBytes = readSdnv((const u_char**)&ptr, (unsigned int)remainingBytes,
            &expectedBytes);
    remainingBytes -= readBytes;
    if(readBytes < 0) {
        // return the same code
        return readBytes;
    } else if(remainingBytes <= 0 ||
            !lengthCheck((unsigned int)remainingBytes,
                    (unsigned int)expectedBytes, "read")) {
        // not enough data left
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        ptr += readBytes;
        totalBytes += readBytes;
    }

    // loop and read components up to expectedBytes
    IPNDSDTLV::BaseDatatype *child;
    unsigned int structs = 0, expectedStructs = components_.size();
    unsigned int structBytes = 0;
    int code = 0;
    while(structBytes < (unsigned int)expectedBytes &&
            structs < expectedStructs) {
        if(remainingBytes <= 0 || !lengthCheck((unsigned int)remainingBytes,
                static_cast<unsigned int>(PrimitiveType::MIN_LEN), "read")) {
            // absolute minimum length check
            code = IPNDSDTLV::ERR_LENGTH;
            break;
        }

        // peek at the tag
        memcpy(&tag, ptr, 1);
        child = getChild(tag);
        if(child == NULL) {
            // unexpected structure; fail
            log_err("Found unexpected child structure! [tag=0x%02x]", tag);
            code = IPNDSDTLV::ERR_PROCESSING;
            break;
        } else if(child->isFilled()) {
            // child type has already been written; something's fishy
            log_err("Found duplicate child structure! [tag=0x%02x]", tag);
            code = IPNDSDTLV::ERR_PROCESSING;
            break;
        } else {
            // let the child component read itself
            readBytes = child->read((const u_char**)&ptr,
                    (unsigned int)remainingBytes);
            if(readBytes < 0) {
                // problems
                code = readBytes;
                break;
            } else if(readBytes < (int)PrimitiveType::MIN_LEN) {
                // should have at least read that much
                code = IPNDSDTLV::ERR_PROCESSING;
                break;
            } else {
                // all good
                ptr += readBytes;
                remainingBytes -= readBytes;
                structBytes += readBytes;
                structs++;
            }
        }
    }

    // check for continuity
    if(code < 0) {
        log_err("Failure while reading child structures!");
        return code;
    } else if(structBytes != (unsigned int)expectedBytes) {
        log_err("Continuity failure in structure size! [%u!=%u]", structBytes,
                (unsigned int)expectedBytes);
        return IPNDSDTLV::ERR_LENGTH;
    } else if(structs != expectedStructs) {
        log_err("Continuity failure in num structures! [%u!=%u]", structs,
                expectedStructs);
        return IPNDSDTLV::ERR_PROCESSING;
    } else {
        // great success!
        totalBytes += structBytes;
        setFilled();
        return (int)totalBytes;
    }
}

int ConstructedType::write(const u_char **buf,
        const unsigned int &len_remain) const {
    // initial length check
    if(!lengthCheck(len_remain, getLength(), "write")) {
        return IPNDSDTLV::ERR_LENGTH;
    }

    int remainingBytes = (int)len_remain, wBytes;
    unsigned int compLen, lenLen, totalBytes = 0;
    u_char *ptr = (u_char*)*buf;

    // write the tag
    uint8_t tag = getTag();
    memcpy(ptr, &tag, 1);
    remainingBytes--;
    ptr++;
    totalBytes++;

    // write the length
    compLen = getComponentsLength();
    lenLen = dtn::SDNV::encoding_len((u_int64_t)compLen);
    wBytes = dtn::SDNV::encode((u_int64_t)compLen, ptr,
            (unsigned int)remainingBytes);

    if(wBytes < 0) {
        log_err("Failed to encode component length value!");
        return IPNDSDTLV::ERR_LENGTH;
    } else if((unsigned int)wBytes != lenLen) {
        log_err("Continuity failure in encoded length size [%u!=%u]",
                (unsigned int)wBytes, lenLen);
        return IPNDSDTLV::ERR_PROCESSING;
    } else {
        ptr += wBytes;
        totalBytes += wBytes;
        remainingBytes -= wBytes;
    }

    // write the components
    CompIterator iter = components_.begin();
    unsigned int structBytes = 0;
    int code = 0;
    while(iter != components_.end()) {
        // check some constraints
        if(remainingBytes <= 0 || structBytes >= compLen) {
            code = IPNDSDTLV::ERR_LENGTH;
            break;
        }

        // write the child (it does its own length check)
        wBytes = (*iter).second->write((const u_char**)&ptr,
                (unsigned int)remainingBytes);
        if(wBytes < 0) {
            // problems
            code = wBytes;
            break;
        } else if(wBytes < (int)PrimitiveType::MIN_LEN) {
            // should have at least written that much
            code = IPNDSDTLV::ERR_PROCESSING;
            break;
        } else {
            // good
            ptr += wBytes;
            structBytes += wBytes;
            remainingBytes -= wBytes;
        }

        iter++;
    }

    // continuity checks
    if(code < 0) {
        log_err("Failure while writing child structures!");
        return code;
    } else if(structBytes != compLen) {
        log_err("Continuity failure in structure sizes [%u!=%u]", structBytes,
                compLen);
        return IPNDSDTLV::ERR_LENGTH;
    } else if(iter != components_.end()) {
        log_err("Failed to write all child structures!");
        return IPNDSDTLV::ERR_PROCESSING;
    } else {
        // success!
        totalBytes += structBytes;
        return totalBytes;
    }
}

void ConstructedType::addChild(IPNDSDTLV::BaseDatatype *type) {
    if(type == NULL) {
        log_err("Cannot add null pointer datatype to composition!");
        return;
    }

    CompIterator iter = components_.find(type->getTag());
    if(iter != components_.end()) {
        log_warn("Composition already contains type with tag [0x%02x]; "
                "overwriting!", type->getTag());
    }

    components_[type->getTag()] = type;
}

IPNDSDTLV::BaseDatatype *ConstructedType::getChild(uint8_t tag) const {
    CompIterator iter = components_.find(tag);

    if(iter == components_.end()) {
        log_info("Requested child does not exist! [tag=0x%02x]", tag);
        return NULL;
    } else {
        return (*iter).second;
    }
}

unsigned int ConstructedType::getComponentsLength() const {
    // calculate total length of components
    unsigned int compLen = 0;
    CompIterator iter;
    for(iter = components_.begin(); iter != components_.end(); iter++) {
        compLen += (*iter).second->getLength();
    }

    return compLen;
}

}
