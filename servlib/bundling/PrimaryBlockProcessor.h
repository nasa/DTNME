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

#ifndef _PRIMARY_BLOCK_PROCESSOR_H_
#define _PRIMARY_BLOCK_PROCESSOR_H_

#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

#include "BlockProcessor.h"

namespace dtn {

class DictionaryVector;
class EndpointID;

/**
 * Block processor implementation for the primary bundle block.
 */
class PrimaryBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    PrimaryBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int64_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len);

    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason);

    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const BlockInfo* source,
                const LinkRef&   link,
                list_owner_t     list);

    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last);

    void generate_primary(const Bundle* bundle,
                          BlockInfoVec* xmit_blocks,
                          BlockInfo*    block);

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL);
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
        BUNDLE_CUSTODY_XFER_REQUESTED  = 1 << 3,
        BUNDLE_SINGLETON_DESTINATION   = 1 << 4,
        BUNDLE_ACK_BY_APP              = 1 << 5,
        BUNDLE_UNUSED                  = 1 << 6
    } bundle_processing_flag_t;
    
    /**
     * The remainder of the fixed-length part of the primary bundle
     * block that immediately follows the block length.
     */
    struct PrimaryBlock {
        u_int8_t version;
        u_int64_t processing_flags;
        u_int64_t block_length;
        u_int64_t dest_scheme_offset;
        u_int64_t dest_ssp_offset;
        u_int64_t source_scheme_offset;
        u_int64_t source_ssp_offset;
        u_int64_t replyto_scheme_offset;
        u_int64_t replyto_ssp_offset;
        u_int64_t custodian_scheme_offset;
        u_int64_t custodian_ssp_offset;
        u_int64_t creation_time;
        u_int64_t creation_sequence;
        u_int64_t lifetime;
        u_int64_t dictionary_length;
    };


    /// @{
    /// Helper functions to parse/format the primary block
    friend class BundleProtocol;
    friend class Comm;
    
    static size_t get_primary_len(const Bundle* bundle,
                                  Dictionary* dict,
                                  PrimaryBlock* primary);

    static u_int64_t format_bundle_flags(const Bundle* bundle);
    static void parse_bundle_flags(Bundle* bundle, u_int64_t flags);

    static u_int64_t format_cos_flags(const Bundle* bundle);
    static void parse_cos_flags(Bundle* bundle, u_int64_t cos_flags);

    static u_int64_t format_srr_flags(const Bundle* bundle);
    static void parse_srr_flags(Bundle* bundle, u_int64_t srr_flags);
    /// @}
};

} // namespace dtn

#endif /* _PRIMARY_BLOCK_PROCESSOR_H_ */
