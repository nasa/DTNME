/*
 *    Copyright 2006 Intel Corporation
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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#ifndef _BP7_PRIMARY_BLOCK_PROCESSOR__H_
#define _BP7_PRIMARY_BLOCK_PROCESSOR__H_

#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
#include <cbor.h>

#include "BP7_BlockProcessor.h"

namespace dtn {


/**
 * Block processor implementation for the primary bundle block.
 */
class BP7_PrimaryBlockProcessor : public BP7_BlockProcessor {
public:
    /// Constructor
    BP7_PrimaryBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len) override;

    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason) override;

    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const SPtr_BlockInfo source,
                const LinkRef&   link,
                list_owner_t     list) override;

    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last) override;

    void generate_primary(const Bundle* bundle,
                          BlockInfoVec* xmit_blocks,
                          BlockInfo*    block);

    /// @}

protected:
    /**
     * Values for bundle processing flags that appear in the primary
     * block.
     */
    typedef enum {
        BUNDLE_IS_FRAGMENT             = 1 << 0,
        BUNDLE_IS_ADMIN                = 1 << 1,
        BUNDLE_DO_NOT_FRAGMENT         = 1 << 2,
        // bits 3 to 4 reserved
        BUNDLE_ACK_BY_APP              = 1 << 5,
        REQ_TIME_IN_STATUS_REPORTS     = 1 << 6,
        // bits 7 to 13 reserved
        REQ_REPORT_OF_RECEPTION        = 1 << 14,
        // bit 15 reserved
        REQ_REPORT_OF_FORWARDING       = 1 << 16,
        REQ_REPORT_OF_DELIVERY         = 1 << 17,
        REQ_REPORT_OF_DELETION         = 1 << 18,
        // bits 19 to 20 reserved
        // bits 21 to 63 unassigned

    } bundle_processing_flag_t;
    
    /// @{
    /// Helper functions to parse/format the primary block
//    friend class BundleProtocol;
//    friend class Comm;
    
    static u_int64_t format_bundle_flags(const Bundle* bundle);
    static void parse_bundle_flags(Bundle* bundle, u_int64_t flags);

    virtual int validate_bundle_array_size(uint64_t block_elements, bool is_fragment, uint64_t crc_type, std::string& fld_name);
    virtual int64_t encode_cbor(uint8_t* buf, uint64_t buflen, const Bundle* bundle, int64_t& encoded_len);

    virtual int64_t decode_cbor(Bundle* bundle, BlockInfo* block, u_char*  buf, size_t buflen);
    /// @}
};

} // namespace dtn

#endif /* _BP7_PRIMARY_BLOCK_PROCESSOR__H_ */
