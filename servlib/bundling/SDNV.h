/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _SDNV_H_
#define _SDNV_H_

#include <oasys/compat/inttypes.h>

#ifdef __cplusplus

namespace dtn {

/**
 * Class to handle parsing and formatting of self describing numeric
 * values (SDNVs).
 *
 * The basic idea is to enable a compact byte representation of
 * numeric values that may widely vary in size. This encoding is based
 * on the ASN.1 specification for encoding Object Identifier Arcs.
 *
 * Conceptually, the integer value to be encoded is split into 7-bit
 * segments. These are encoded into the output byte stream, such that
 * the high order bit in each byte is set to one for all bytes except
 * the last one.
 *
 * Note that this implementation only handles values up to 64-bits in
 * length (since the conversion is between a u_int64_t and the encoded
 * byte sequence).
 */
class SDNV {
public:
    /**
     * The maximum length for this SDNV implementation is 10 bytes,
     * since the maximum value is 64 bits wide.
     */
    static const size_t MAX_LENGTH = 10;
    
    /**
     * Return the number of bytes needed to encode the given value.
     */
    static size_t encoding_len(u_int64_t val);
    
    /**
     * Convert the given 64-bit integer into an SDNV.
     *
     * @return The number of bytes used, or -1 on error.
     */
    static int encode(u_int64_t val, u_char* bp, size_t len);
    
    /**
     * Convert an SDNV pointed to by bp into a unsigned 64-bit
     * integer.
     *
     * @return The number of bytes of bp consumed, or -1 on error.
     */
    static int decode(const u_char* bp, size_t len, u_int64_t* val);

    /**
     * Convert an SDNV pointed to by bp into a unsigned 32-bit
     * integer. Checks for overflow in the SDNV.
     *
     * @return The number of bytes of bp consumed, or -1 on error.
     */
    static int decode(const u_char* bp, size_t len, u_int32_t* val)
    {
        u_int64_t lval;
        int ret = decode(bp, len, &lval);
        
        if (lval > 0xffffffffLL) {
            return -1;
        }

        *val = (u_int32_t)lval;
        
        return ret;
    }

    /// @{
    /// Variants of encode/decode that take a char* for the buffer
    /// instead of a u_char*
    ///
    /// XXX/demmer all this crap should be replaced with a byte_t for
    /// consistency
    ///
    static int encode(u_int64_t val, char* bp, size_t len)
    {
        return encode(val, (u_char*)bp, len);
    }
    
    static int decode(char* bp, size_t len, u_int64_t* val)
    {
        return decode((u_char*)bp, len, val);
    }
    
    static int decode(char* bp, size_t len, u_int32_t* val)
    {
        return decode((u_char*)bp, len, val);
    }
    /// @}

    /**
     * Return the number of bytes which comprise the given value.
     * Assumes that bp points to a valid encoded SDNV.
     */
    static size_t len(const u_char* bp);
};

} // namespace dtn

#endif /* __cplusplus */

#endif /* _SDNV_H_ */
