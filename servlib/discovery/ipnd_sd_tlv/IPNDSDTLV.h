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
 * IPNDSDTLV.h
 *
 * Implements the basic features of the IPND Service Block TLV specification
 * as defined in draft-irtf-dtnrg-ipnd-02. Changes here should always be
 * mirrored in the draft and vice versa (if applicable).
 *
 * Initial implementation of IPND-SD-TLV by Alex Gladd (agladd at bbn dot com)
 */

#ifndef IPNDSDTLV_H_
#define IPNDSDTLV_H_

#include <oasys/debug/Logger.h>
#include <string>
#include <stdint.h>

namespace ipndtlv {

class IPNDSDTLV {
public:

    /**
     * This simple class represents the base type for all IPND-SD-TLV datatypes.
     * The only features all datatypes share are a tag value and a length (in
     * bytes) value. All datatypes must know how to read and write themselves
     * from/to a raw byte stream.
     * Subclasses should add features as necessary.
     */
    class BaseDatatype : public oasys::Logger {
    public:
        /**
         * Returns the tag value of this type
         */
        uint8_t getTag() const;

        /**
         * Returns true if this datatype has been filled out with content,
         * either by the explicit constructor or by reading itself out of raw
         * data.
         *
         * Subclasses should call 'setFilled()' when this type should become
         * immutable.
         */
        bool isFilled() const;

        /**
         * Returns the actual length (in bytes) of the entire datatype structure
         * (including the tag field and the length field, if applicable).
         * Subclasses must implement.
         */
        virtual unsigned int getLength() const = 0;

        /**
         * Returns the minimum length of the entire datatype structure.
         * Subclasses are encouraged to override this where possible in order to
         * assist with short-circuiting cases where malformed raw data is being
         * manipulated.
         */
        virtual unsigned int getMinLength() const = 0;

        /**
         * All datatypes must know how to read themselves from a raw byte
         * stream. Must return the exact number of bytes read.
         * Subclasses must implement.
         *
         * @param buf Pointer to pointer to the location where this datatype
         *   should begin to read itself from.
         * @param len_remain The remaining length of the buffer. Subclasses
         *   should monitor this length to ensure they don't overstep (which
         *   would indicate a read error).
         * @return The number of bytes read or error code
         */
        virtual int read(const u_char **buf,
                const unsigned int &len_remain) = 0;

        /**
         * All datatypes must know how to write themselves to a raw byte
         * stream. Must return the exact number of bytes written.
         * Subclasses must implement.
         *
         * @param buf Pointer to pointer to the location where this datatype
         *   should begin to write itself to.
         * @param len_remain The remaining length of the buffer. Subclasses
         *   should monitor this length to ensure they don't overstep (which
         *   would indicate a write error).
         * @return The number of bytes written or error code
         */
        virtual int write(const u_char **buf,
                const unsigned int &len_remain) const = 0;

        virtual ~BaseDatatype();

    protected:
        /**
         * Constructor - Should never be directly instantiated.
         *
         * @param tag_value The tag value of this datatype
         * @param name Human-readable name for datatype (for logging only)
         */
        BaseDatatype(const uint8_t &tag_value, const std::string &name);

        /**
         * Sets an internal flag that indicates this datatype has been filled
         * out with data and should no longer be modified. Note that enforcement
         * of this is left up to the parent of this datatype (See
         * ConstructedType::read).
         */
        void setFilled();

        /**
         * Convenience function for subclasses to check length requirements.
         * Logs a standard message to ERROR if the check fails.
         *
         * @param len The length to check
         * @param req The requirement to check against
         * @param action The action word to use for logging (default="action")
         * @return true if len >= req, false otherwise
         */
        bool lengthCheck(const unsigned int &len,
                const unsigned int &req,
                const std::string &action = std::string("action")) const;

        /**
         * Convenience function for subclasses to check tags (e.g. when reading
         * raw streams).
         * Logs a standard message to ERROR if the check fails.
         *
         * @param tag The length to check
         * @param action The action word to use for logging (default="action")
         * @return true if tag == getTag(), false otherwise
         */
        bool tagCheck(const uint8_t &tag,
                const std::string &action = std::string("action")) const;

        /**
         * Convenience function for subclasses to read an SDNV value from a raw
         * byte stream. Will read up to len_remain or SDNV::MAX_LENGTH bytes
         * (whichever is less). Writes the resulting value to the location
         * specified by the given pointer. Returns the number of bytes read from
         * the raw stream in order to decode the value. Returns zero on any
         * errors (all SDNVs are expected to have raw encoded length >= 1).
         *
         * @param buf Pointer to pointer to the beginning of the encoded SDNV
         *   value.
         * @param len_remain The number of bytes remaining in the buffer.
         * @param value Pointer of location to write decoded value.
         * @return Number of bytes read off raw stream or an error code.
         */
        int readSdnv(const u_char **buf, const unsigned int &len_remain,
                uint64_t *value) const;

    private:
        uint8_t tag_;
        std::string log_name_;
        bool content_read_;
    };

    /**
     * This enum defines the tag values according to the IANA registry specified
     * in draft-irtf-dtnrg-ipnd-02. Should not be changed without first
     * registering changes/additions with IANA.
     *
     * P_* = Primative types
     * C_* = Constructed types
     */
    typedef enum Tags_e {
        P_BOOLEAN = 0,
        P_UINT64 = 1,
        P_SINT64 = 2,
        P_FIXED16 = 3,
        P_FIXED32 = 4,
        P_FIXED64 = 5,
        P_FLOAT = 6,
        P_DOUBLE = 7,
        P_STRING = 8,
        P_BYTES = 9,
        // 10-63 Unassigned (primitives only)
        C_CLA_TCPv4 = 64,
        C_CLA_UDPv4 = 65,
        C_CLA_TCPv6 = 66,
        C_CLA_UDPv6 = 67,
        C_CLA_TCP_HN = 68,
        C_CLA_UDP_HN = 69,
        // 68-125 Unassigned (constructed only)
        C_NBF_HASHES = 126,
        C_NBF_BITS = 127
        // 128-255 Private/Experimental use (should never be assigned here)
    } Tags;

    /**
     * Error codes that may be used within IPND TLV processing
     */
    typedef enum ErrorCodes_e {
        ERR_PROCESSING = -1,    ///< A processing error occurred
        ERR_LENGTH = -2,        ///< A length/bound check error occurred
        ERR_TAG = -3            ///< A tag continuity error occurred
    } ErrorCodes;

    /**
     * Constants that may be used to check tag value properties for reserved
     * types.
     */
    static const uint8_t RESERVED_MASK = 0x80;
    static const uint8_t PRIMITIVE_MASK = 0x40;

    /**
     * Quick check to determine if the datatype defined by the given tag is a
     * reserved type.
     *
     * @return True if the tag represents a reserved type; false otherwise.
     */
    static bool isReservedType(const uint8_t &tag);

    /**
     * Quick check to determine if the datatype defined by the given tag is a
     * reserved primitive type.
     *
     * @return True if the tag represents a reserved primitive type; false
     *   otherwise or if type is not reserved.
     */
    static bool isReservedPrimitive(const uint8_t &tag);

    /**
     * Quick check to determine if the datatype defined by the given tag is a
     * reserved constructed type.
     *
     * @return True if the tag represents a reserved constructed type; false
     *   otherwise or if type is not reserved.
     */
    static bool isReservedConstructed(const uint8_t &tag);

    virtual ~IPNDSDTLV();

protected:
    // Should never be instantiated
    IPNDSDTLV();
};

}

#endif /* IPNDSDTLV_H_ */
