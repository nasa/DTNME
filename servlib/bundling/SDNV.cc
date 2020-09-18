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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

/*
 * This file is a little funky since it's compiled into both C and C++
 * (after being #included into sdnv-c.c).
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef __cplusplus
#include "SDNV.h"
#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>

#define SDNV_FN(_what) SDNV::_what

namespace dtn {

#else // ! __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <oasys/compat/inttypes.h>

#define SDNV_FN(_what) sdnv_##_what

//----------------------------------------------------------------------
#define ASSERT(x)                                               \
do {                                                            \
    if (! (x)) {                                                \
        fprintf(stderr, "ASSERTION FAILED (" #x ") at %s:%d\n", \
                __FILE__, __LINE__);                            \
        exit(1);                                                \
    }                                                           \
} while (0)
            
//----------------------------------------------------------------------
#define log_err_p(p, args...) fprintf(stderr, "error: (" p ") " args);

#define MAX_LENGTH 10

#endif // __cplusplus
            
//----------------------------------------------------------------------
int
SDNV_FN(encode)(u_int64_t val, u_char* bp, size_t len)
{
    u_char* start = bp;

    /*
     * Figure out how many bytes we need for the encoding.
     */
    size_t val_len = 0;
    u_int64_t tmp = val;

    do {
        tmp = tmp >> 7;
        val_len++;
    } while (tmp != 0);

    ASSERT(val_len > 0);
    ASSERT(val_len <= MAX_LENGTH);

    /*
     * Make sure we have enough buffer space.
     */
    if (len < val_len) {
        return -1;
    }

    /*
     * Now advance bp to the last byte and fill it in backwards with
     * the value bytes.
     */
    bp += val_len;
    u_char high_bit = 0; // for the last octet
    do {
        --bp;
        *bp = (u_char)(high_bit | (val & 0x7f));
        high_bit = (1 << 7); // for all but the last octet
        val = val >> 7;
    } while (val != 0);

    ASSERT(bp == start);

    return val_len;
}

//----------------------------------------------------------------------
size_t
SDNV_FN(encoding_len)(u_int64_t val)
{
    u_char buf[16];
    int ret = SDNV_FN(encode)(val, buf, sizeof(buf));
    ASSERT(ret != -1 && ret != 0);
    return ret;
}

//----------------------------------------------------------------------
int
SDNV_FN(decode)(const u_char* bp, size_t len, u_int64_t* val)
{
    const u_char* start = bp;
    
    if (!val) {
        return -1;
    }

    /*
     * Zero out the existing value, then shift in the bytes of the
     * encoding one by one until we hit a byte that has a zero
     * high-order bit.
     */
    size_t val_len = 0;
    *val = 0;
    do {
        if (len <= 0)
            return -1; // buffer too short
        
        *val = (*val << 7) | (*bp & 0x7f);
        ++val_len;
        
        if ((*bp & (1 << 7)) == 0)
            break; // all done;

        ++bp;
        --len;
    } while (1);

    /*
     * Since the spec allows for infinite length values but this
     * implementation only handles up to 64 bits, check for overflow.
     * Note that the only supportable 10 byte SDNV must store exactly
     * one bit in the first byte of the encoding (i.e. the 64'th bit
     * of the original value).
     * This is OK because a spec update says that behavior
     * is undefined for values > 64 bits.
     */
    if ((val_len > MAX_LENGTH) ||
        ((val_len == MAX_LENGTH) && (*start != 0x81)))
    {
        log_err_p("/dtn/bundle/sdnv", "overflow value in sdnv!!!");
        return -1;
    }

    // XXX/demmer shouldn't use -1 as indication of both too short of
    // a buffer and of error due to overflow or malformed SDNV since
    // callers just assume that they need more data to decode and
    // don't really check for errors

    return val_len;
}

//----------------------------------------------------------------------
size_t
SDNV_FN(len)(const u_char* bp)
{
    size_t          val_len = 1;
    
    for ( ; *bp++ & 0x80; ++val_len )
        ;
    return val_len;
}

#ifdef __cplusplus
} // namespace dtn
#endif

