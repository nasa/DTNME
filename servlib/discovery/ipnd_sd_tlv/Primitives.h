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
 * Primitives.h
 *
 * Defines the set of reserved primitive types for IPND-SD-TLV as specified in
 * draft-irtf-dtnrg-ipnd-02.
 */

#ifndef PRIMITIVES_H_
#define PRIMITIVES_H_

#include "PrimitiveType.h"
#include <string>

namespace ipndtlv {

class Primitives {
public:

    /**
     * boolean datatype. Zero is considered false, anything else considered
     * true. Defaults to false.
     */
    class Boolean: public PrimitiveType {
    public:
        Boolean();
        Boolean(bool value);
        virtual ~Boolean();

        bool getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 1;
        static const uint8_t TRUE = 0xff;
        static const uint8_t FALSE = 0x00;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        bool val_;
        uint8_t *content_;
    };

    /**
     * uint64 (variable length unsigned integer) datatype. In raw form, this
     * datatype uses SDNV values. Default value is 0. Values may not actually
     * require 64 bits for storage; users may cast as needed.
     */
    class UInt64: public PrimitiveType {
    public:
        UInt64();
        UInt64(uint64_t value);
        virtual ~UInt64();

        uint64_t getValue() { return val_; }

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        uint64_t val_;
        unsigned int enc_len_;
        uint8_t *content_;

        /**
         * Updates the content_ and enc_len_ based on the current value of val_
         */
        void updateContent();
    };

    /**
     * sint64 (variable length signed integer) datatype. In raw form, this
     * datatype uses SDNV values. We perform some trickery to use SDNV with
     * signed values. Default value is 0. Values may not actually require 64
     * bits for storage; users may cast as needed.
     */
    class SInt64: public PrimitiveType {
    public:
        SInt64();
        SInt64(int64_t value);
        virtual ~SInt64();

        int64_t getValue() { return val_; }

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        int64_t val_;
        unsigned int enc_len_;
        uint8_t *content_;

        /**
         * Updates the content_ and enc_len_ based on the current value of val_
         */
        void updateContent();
    };

    /**
     * fixed16 (fixed width 16-bit) datatype. Does not use any special encoding.
     * Defaults to 0. Users may cast to signed values so long as they fit
     * within the fixed width.
     */
    class Fixed16: public PrimitiveType {
    public:
        Fixed16();
        Fixed16(uint16_t value);
        virtual ~Fixed16();

        uint16_t getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 2;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        uint16_t val_;
        uint8_t *content_;
    };

    /**
     * fixed32 (fixed width 32-bit) datatype. Does not use any special encoding.
     * Defaults to 0. Users may cast to signed values so long as they fit
     * within the fixed width.
     */
    class Fixed32: public PrimitiveType {
    public:
        Fixed32();
        Fixed32(uint32_t value);
        virtual ~Fixed32();

        uint32_t getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 4;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        uint32_t val_;
        uint8_t *content_;
    };

    /**
     * fixed64 (fixed width 64-bit) datatype. Does not use any special encoding.
     * Defaults to 0. Users may cast to signed values so long as they fit
     * within the fixed width.
     */
    class Fixed64: public PrimitiveType {
    public:
        Fixed64();
        Fixed64(uint64_t value);
        virtual ~Fixed64();

        uint64_t getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 8;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        uint64_t val_;
        uint8_t *content_;
    };

    /**
     * float (fixed width single-precision floating point) datatype. Does not
     * use any special encoding. Defaults to 0.0
     */
    class Float: public PrimitiveType {
    public:
        Float();
        Float(float value);
        virtual ~Float();

        float getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 4;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        float val_;
        uint8_t *content_;
    };

    /**
     * double (fixed width double-precision floating point) datatype. Does not
     * use any special encoding. Defaults to 0.0
     */
    class Double: public PrimitiveType {
    public:
        Double();
        Double(double value);
        virtual ~Double();

        double getValue() { return val_; }

        static const unsigned int CONTENT_LEN = 8;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        double val_;
        uint8_t *content_;
    };

    /**
     * string datatype. Default value is the empty string. Content contains
     * an explicit length value (SDNV encoded), followed by the string itself.
     *
     * The empty string is a special case represented by a length value of 1
     * followed by a single null byte.
     */
    class String: public PrimitiveType {
    public:
        String();
        String(std::string value);
        virtual ~String();

        std::string getValue() { return val_; }

        static const unsigned int MIN_CONTENT_LEN = 2;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        std::string val_;
        uint8_t *content_;
        unsigned int cont_len_;

        /**
         * Updates content based on val_
         */
        void updateContent();
    };

    /**
     * bytes (simple byte array) datatype. Default value is an empty array.
     * Content contains an explicit length value (SDNV encoded), followed by
     * the byte array itself.
     *
     * The empty array is a special case represented by a length value of 1
     * followed by a single null byte.
     *
     * The non-default constructor creates a copy of the given array. Changes
     * made to the given array after the constructor call will not be reflected
     * in this datatype.
     */
    class Bytes: public PrimitiveType {
    public:
        Bytes();
        Bytes(u_char *bytes, unsigned int len);
        virtual ~Bytes();

        unsigned int getValueLen() { return val_len_; }
        u_char *getValue() { return val_; }

        static const unsigned int MIN_CONTENT_LEN = 2;

        /**
         * See IPNDSDTLV::BaseDatatype
         */
        unsigned int getMinLength() const;

        /**
         * See PrimitiveType
         */
        unsigned int getRawContentLength() const;
        u_char *getRawContent() const;
        int readRawContent(const u_char **buf, const unsigned int &len_remain);

    private:
        unsigned int val_len_;
        u_char *val_;
        unsigned int cont_len_;
        uint8_t *content_;

        /**
         * Updates content based on val_ and val_len_
         */
        void updateContent();
    };

protected:
    // Should never be instantiated
    Primitives();
    virtual ~Primitives();
};

}

#endif /* PRIMITIVES_H_ */
