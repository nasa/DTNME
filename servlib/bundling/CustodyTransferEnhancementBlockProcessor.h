/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifndef _CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_PROCESSOR_H_
#define _CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_PROCESSOR_H_

#ifdef ACS_ENABLED

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the Custody Transfer Enhancement Block
 *
 * Reference: draft-jenkins-custody-transfer-enhancement-block-01
 */
class CustodyTransferEnhancementBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    CustodyTransferEnhancementBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
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
    
    int64_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len);

    int format(oasys::StringBuffer* buf, BlockInfo *b);
    /// @}

    /// Tests if the CTEB creator is equal to the current bundle custodian
    bool is_cteb_creator_custodian(const char* custodian, const char* creator);
};

} // namespace dtn

#endif // ACS_ENABLED

#endif //_CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_PROCESSOR_H_
