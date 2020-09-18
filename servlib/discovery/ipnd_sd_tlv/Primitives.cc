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
 * Primitives.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "Primitives.h"
#include "bundling/SDNV.h"
#include <cstring>
#include <endian.h>

namespace ipndtlv {

/*** BOOLEAN ******************************************************************/

Primitives::Boolean::Boolean()
    : PrimitiveType(IPNDSDTLV::P_BOOLEAN, "boolean"),
      val_(false) {
    content_ = new uint8_t;
    *content_ = FALSE;
}

Primitives::Boolean::Boolean(bool value)
    : PrimitiveType(IPNDSDTLV::P_BOOLEAN, "boolean"),
      val_(value) {
    content_ = new uint8_t;

    if(value) {
        *content_ = TRUE;
    } else {
        *content_ = FALSE;
    }

    setFilled();
}

Primitives::Boolean::~Boolean() {
    delete content_;
}

unsigned int Primitives::Boolean::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Boolean::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Boolean::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since this type's
    // constant length *is* the minimum for primitive types

    // just copy the value
    uint8_t val;
    memcpy(&val, *buf, CONTENT_LEN);

    // set our logical value
    val_ = (val == 0)?false:true;

    // set our raw value
    *content_ = (val_ == 0)?
            static_cast<uint8_t>(FALSE):static_cast<uint8_t>(TRUE);

    return (int)CONTENT_LEN;
}

/*** UINT64 *******************************************************************/

Primitives::UInt64::UInt64()
    : PrimitiveType(IPNDSDTLV::P_UINT64, "uint64"),
      val_(0) {
    // easy, set to zero
    content_ = new uint8_t[dtn::SDNV::MAX_LENGTH];
    updateContent();
}

Primitives::UInt64::UInt64(uint64_t value)
    : PrimitiveType(IPNDSDTLV::P_UINT64, "uint64"),
      val_(value) {
    // encode the given value
    content_ = new uint8_t[dtn::SDNV::MAX_LENGTH];
    updateContent();
    setFilled();
}

Primitives::UInt64::~UInt64() {
    delete content_;
}

unsigned int Primitives::UInt64::getRawContentLength() const {
    return enc_len_;
}

u_char *Primitives::UInt64::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::UInt64::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // read the sdnv
    uint64_t val = 0;
    int num = readSdnv(buf, len_remain, &val);

    if(num < 0) {
        log_err("Could not decode SDNV value!");
        return num;
    } else {
        val_ = val;
        updateContent();
        return num;
    }
}

void Primitives::UInt64::updateContent() {
    // reset content and encode new value
    memset(content_, 0, dtn::SDNV::MAX_LENGTH);
    int eLen = dtn::SDNV::encode((u_int64_t)val_, (u_char*)content_,
            dtn::SDNV::MAX_LENGTH);

    if(eLen <= 0) {
        log_err("Could not SDNV encode new value! [%llu]", val_);
        // essentially set value to zero
        enc_len_ = 1;
    } else {
        enc_len_ = (unsigned int)eLen;
    }
}

/*** SINT64 *******************************************************************/

Primitives::SInt64::SInt64()
    : PrimitiveType(IPNDSDTLV::P_SINT64, "sint64"),
      val_(0) {
    // easy, set to zero
    content_ = new uint8_t[dtn::SDNV::MAX_LENGTH];
    updateContent();
}

Primitives::SInt64::SInt64(int64_t value)
    : PrimitiveType(IPNDSDTLV::P_SINT64, "sint64"),
      val_(value) {
    // encode the given value
    content_ = new uint8_t[dtn::SDNV::MAX_LENGTH];
    updateContent();
    setFilled();
}

Primitives::SInt64::~SInt64() {
    delete content_;
}

unsigned int Primitives::SInt64::getRawContentLength() const {
    return enc_len_;
}

u_char *Primitives::SInt64::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::SInt64::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // read the sdnv
    uint64_t val = 0;
    int num = readSdnv(buf, len_remain, &val);

    if(num == 0) {
        log_err("Could not decode SDNV value!");
        return num;
    } else {
        // XXX (agladd): use memcpy here to convert to signed value
        memcpy(&val_, &val, sizeof(val));
        updateContent();
        return num;
    }
}

void Primitives::SInt64::updateContent() {
    // reset content and encode new value
    memset(content_, 0, dtn::SDNV::MAX_LENGTH);

    // convert to unsigned int
    uint64_t val;
    memcpy(&val, &val_, sizeof(val));

    int eLen = dtn::SDNV::encode((u_int64_t)val, (u_char*)content_,
            dtn::SDNV::MAX_LENGTH);

    if(eLen <= 0) {
        log_err("Could not SDNV encode new value! [%lli]", val_);
        // essentially set value to zero
        enc_len_ = 1;
    } else {
        enc_len_ = (unsigned int)eLen;
    }
}

/*** FIXED16 ******************************************************************/

Primitives::Fixed16::Fixed16()
    : PrimitiveType(IPNDSDTLV::P_FIXED16, "fixed16"),
      val_(0) {
    content_ = new uint8_t[CONTENT_LEN];
    memset(content_, 0, CONTENT_LEN);
}

Primitives::Fixed16::Fixed16(uint16_t value)
    : PrimitiveType(IPNDSDTLV::P_FIXED16, "fixed16"),
      val_(value) {
    content_ = new uint8_t[CONTENT_LEN];
    uint16_t beVal = htobe16(val_);
    memcpy(content_, &beVal, CONTENT_LEN);
    setFilled();
}

Primitives::Fixed16::~Fixed16() {
    delete content_;
}

unsigned int Primitives::Fixed16::getMinLength() const {
    return 1 + CONTENT_LEN;
}

unsigned int Primitives::Fixed16::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Fixed16::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Fixed16::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since we overrode
    // the 'getMinLength' function with our constant length
    uint16_t beVal;
    memcpy(&beVal, *buf, CONTENT_LEN);
    memcpy(content_, &beVal, CONTENT_LEN);
    val_ = be16toh(beVal);
    return CONTENT_LEN;
}

/*** FIXED32 ******************************************************************/

Primitives::Fixed32::Fixed32()
    : PrimitiveType(IPNDSDTLV::P_FIXED32, "fixed32"),
      val_(0) {
    content_ = new uint8_t[CONTENT_LEN];
    memset(content_, 0, CONTENT_LEN);
}

Primitives::Fixed32::Fixed32(uint32_t value)
    : PrimitiveType(IPNDSDTLV::P_FIXED32, "fixed32"),
      val_(value) {
    content_ = new uint8_t[CONTENT_LEN];
    uint32_t beVal = htobe32(val_);
    memcpy(content_, &beVal, CONTENT_LEN);
    setFilled();
}

Primitives::Fixed32::~Fixed32() {
    delete content_;
}

unsigned int Primitives::Fixed32::getMinLength() const {
    return 1 + CONTENT_LEN;
}

unsigned int Primitives::Fixed32::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Fixed32::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Fixed32::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since we overrode
    // the 'getMinLength' function with our constant length
    uint32_t beVal;
    memcpy(&beVal, *buf, CONTENT_LEN);
    memcpy(content_, &beVal, CONTENT_LEN);
    val_ = be32toh(beVal);
    return CONTENT_LEN;
}

/*** FIXED64 ******************************************************************/

Primitives::Fixed64::Fixed64()
    : PrimitiveType(IPNDSDTLV::P_FIXED64, "fixed64"),
      val_(0) {
    content_ = new uint8_t[CONTENT_LEN];
    memset(content_, 0, CONTENT_LEN);
}

Primitives::Fixed64::Fixed64(uint64_t value)
    : PrimitiveType(IPNDSDTLV::P_FIXED64, "fixed64"),
      val_(value) {
    content_ = new uint8_t[CONTENT_LEN];
    uint64_t beVal = htobe64(val_);
    memcpy(content_, &beVal, CONTENT_LEN);
    setFilled();
}

Primitives::Fixed64::~Fixed64() {
    delete content_;
}

unsigned int Primitives::Fixed64::getMinLength() const {
    return 1 + CONTENT_LEN;
}

unsigned int Primitives::Fixed64::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Fixed64::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Fixed64::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since we overrode
    // the 'getMinLength' function with our constant length
    uint64_t beVal;
    memcpy(&beVal, *buf, CONTENT_LEN);
    memcpy(content_, &beVal, CONTENT_LEN);
    val_ = be64toh(beVal);
    return CONTENT_LEN;
}

/*** FLOAT ********************************************************************/

Primitives::Float::Float()
    : PrimitiveType(IPNDSDTLV::P_FLOAT, "float"),
      val_(0.0F) {
    content_ = new uint8_t[CONTENT_LEN];
    uint32_t hVal;
    memcpy(&hVal, &val_, CONTENT_LEN);
    uint32_t beVal = htobe32(hVal);
    memcpy(content_, &beVal, CONTENT_LEN);
}

Primitives::Float::Float(float value)
    : PrimitiveType(IPNDSDTLV::P_FLOAT, "float"),
      val_(value) {
    content_ = new uint8_t[CONTENT_LEN];
    uint32_t hVal;
    memcpy(&hVal, &val_, CONTENT_LEN);
    uint32_t beVal = htobe32(hVal);
    memcpy(content_, &beVal, CONTENT_LEN);
    setFilled();
}

Primitives::Float::~Float() {
    delete content_;
}

unsigned int Primitives::Float::getMinLength() const {
    return 1 + CONTENT_LEN;
}

unsigned int Primitives::Float::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Float::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Float::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since we overrode
    // the 'getMinLength' function with our constant length
    uint32_t beVal;
    memcpy(&beVal, *buf, CONTENT_LEN);
    memcpy(content_, &beVal, CONTENT_LEN);
    uint32_t hVal = be32toh(beVal);
    memcpy(&val_, &hVal, CONTENT_LEN);
    return CONTENT_LEN;
}

/*** DOUBLE *******************************************************************/

Primitives::Double::Double()
    : PrimitiveType(IPNDSDTLV::P_DOUBLE, "double"),
      val_((double)0.0) {
    content_ = new uint8_t[CONTENT_LEN];
    uint64_t hVal;
    memcpy(&hVal, &val_, CONTENT_LEN);
    uint64_t beVal = htobe64(hVal);
    memcpy(content_, &beVal, CONTENT_LEN);
}

Primitives::Double::Double(double value)
    : PrimitiveType(IPNDSDTLV::P_DOUBLE, "double"),
      val_(value) {
    content_ = new uint8_t[CONTENT_LEN];
    uint64_t hVal;
    memcpy(&hVal, &val_, CONTENT_LEN);
    uint64_t beVal = htobe64(hVal);
    memcpy(content_, &beVal, CONTENT_LEN);
    setFilled();
}

Primitives::Double::~Double() {
    delete content_;
}

unsigned int Primitives::Double::getMinLength() const {
    return 1 + CONTENT_LEN;
}

unsigned int Primitives::Double::getRawContentLength() const {
    return CONTENT_LEN;
}

u_char *Primitives::Double::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Double::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // superclass has already done a sufficient length check since we overrode
    // the 'getMinLength' function with our constant length
    uint64_t beVal;
    memcpy(&beVal, *buf, CONTENT_LEN);
    memcpy(content_, &beVal, CONTENT_LEN);
    uint64_t hVal = be64toh(beVal);
    memcpy(&val_, &hVal, CONTENT_LEN);
    return CONTENT_LEN;
}

/*** STRING *******************************************************************/

Primitives::String::String()
    : PrimitiveType(IPNDSDTLV::P_STRING, "string"),
      val_(""),
      content_(NULL) {
    updateContent();
}

Primitives::String::String(std::string value)
    : PrimitiveType(IPNDSDTLV::P_STRING, "string"),
      val_(value),
      content_(NULL) {
    updateContent();
    setFilled();
}

Primitives::String::~String() {
    if(content_ != NULL) {
        delete content_;
    }
}

unsigned int Primitives::String::getMinLength() const {
    return 1 + MIN_CONTENT_LEN;
}

unsigned int Primitives::String::getRawContentLength() const {
    return cont_len_;
}

u_char *Primitives::String::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::String::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // read the sdnv length value
    uint64_t strLen = 0;
    int num = readSdnv(buf, len_remain, &strLen);

    if(num < 0) {
        log_err("Could not decode SDNV string length value!");
        return num;
    } else if (!lengthCheck((len_remain - num), strLen, "read")) {
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        // good to go
        const u_char *ptr = ((*buf) + num);
        char str[strLen];

        memcpy(str, ptr, strLen);

        if(strLen == 1 && str[0] == '\0') {
            // empty string case
            val_ = "";
        } else {
            val_ = std::string(str, strLen);
        }

        updateContent();

        return num + strLen;
    }
}

void Primitives::String::updateContent() {
    if(content_ != NULL) {
        delete content_;
    }

    if(val_.size() == 0) {
        // empty string case
        content_ = new uint8_t[MIN_CONTENT_LEN];
        content_[0] = 0x01;
        content_[1] = 0x00;
        cont_len_ = MIN_CONTENT_LEN;
    } else {
        // regular case
        unsigned int strLen = val_.size();
        unsigned int lenLen = dtn::SDNV::encoding_len((u_int64_t)strLen);
        cont_len_ = lenLen + strLen;

        content_ = new uint8_t[cont_len_];
        memset(content_, 0, cont_len_);
        int eLen = dtn::SDNV::encode((u_int64_t)strLen, (u_char*)content_,
                cont_len_);

        if(eLen < 0 || (unsigned int)eLen != lenLen) {
            log_err("Could not SDNV encode string length value! [%u]", strLen);
            // essentially make into the empty string
            content_[0] = 0x01;
            content_[1] = 0x00;
            cont_len_ = MIN_CONTENT_LEN;
        } else {
            memcpy((content_ + lenLen), val_.c_str(), strLen);
        }
    }

}

/*** BYTES ********************************************************************/

Primitives::Bytes::Bytes()
    : PrimitiveType(IPNDSDTLV::P_BYTES, "bytes"),
      val_len_(0),
      val_(NULL),
      content_ (NULL) {
    updateContent();
}

Primitives::Bytes::Bytes(u_char *bytes, unsigned int len)
    : PrimitiveType(IPNDSDTLV::P_BYTES, "bytes"),
      val_len_(len),
      content_(NULL) {
    val_ = new u_char[val_len_];
    memcpy(val_, bytes, val_len_);
    updateContent();
    setFilled();
}

Primitives::Bytes::~Bytes() {
    if(val_ != NULL) {
        delete val_;
    }

    if(content_ != NULL) {
        delete content_;
    }
}

unsigned int Primitives::Bytes::getMinLength() const {
    return 1 + MIN_CONTENT_LEN;
}

unsigned int Primitives::Bytes::getRawContentLength() const {
    return cont_len_;
}

u_char *Primitives::Bytes::getRawContent() const {
    return (u_char*)content_;
}

int Primitives::Bytes::readRawContent(const u_char **buf,
        const unsigned int &len_remain) {
    // read the sdnv length value
    uint64_t arrLen = 0;
    int num = readSdnv(buf, len_remain, &arrLen);

    if(num < 0) {
        log_err("Could not decode SDNV byte array length value!");
        return num;
    } else if (!lengthCheck((len_remain - num), arrLen, "read")) {
        return IPNDSDTLV::ERR_LENGTH;
    } else {
        // good to go
        if(val_ != NULL) {
            delete val_;
        }

        val_len_ = (unsigned int)arrLen;
        const u_char *ptr = ((*buf) + num);
        u_char bArray[val_len_];

        memcpy(bArray, ptr, val_len_);

        if(arrLen == 1 && bArray[0] == '\0') {
            // empty array case
            val_ = NULL;
            val_len_ = 0;
        } else {
            val_ = new u_char[val_len_];
            memcpy(val_, bArray, val_len_);
        }

        updateContent();

        return num + arrLen;
    }
}

void Primitives::Bytes::updateContent() {
    if(content_ != NULL) {
        delete content_;
    }

    if(val_len_ == 0) {
        // empty array case
        content_ = new uint8_t[MIN_CONTENT_LEN];
        content_[0] = 0x01;
        content_[1] = 0x00;
        cont_len_ = MIN_CONTENT_LEN;
    } else {
        // regular case
        unsigned int lenLen = dtn::SDNV::encoding_len((u_int64_t)val_len_);
        cont_len_ = lenLen + val_len_;

        content_ = new uint8_t[cont_len_];
        memset(content_, 0, cont_len_);
        int eLen = dtn::SDNV::encode((u_int64_t)val_len_, (u_char*)content_,
                cont_len_);

        if(eLen < 0 || (unsigned int)eLen != lenLen) {
            log_err("Could not SDNV encode byte array length value! [%u]",
                    val_len_);
            // essentially make into the empty array
            content_[0] = 0x01;
            content_[1] = 0x00;
            cont_len_ = MIN_CONTENT_LEN;
        } else {
            memcpy((content_ + lenLen), val_, val_len_);
        }
    }
}

/*** END **********************************************************************/

Primitives::Primitives() {
    // should never get here
    // TODO (agladd): maybe should log a warning here?
}

Primitives::~Primitives() {
    // should never get here
}

}
