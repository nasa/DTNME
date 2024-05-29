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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BP6_UnknownBlockProcessor.h"

#include "BlockInfo.h"
#include "BundleProtocol.h"
#include "Bundle.h"

namespace oasys {
    template <> dtn::BP6_UnknownBlockProcessor* oasys::Singleton<dtn::BP6_UnknownBlockProcessor>::instance_ = NULL;
}

namespace dtn {

//----------------------------------------------------------------------
BP6_UnknownBlockProcessor::BP6_UnknownBlockProcessor()
    : BP6_BlockProcessor(BundleProtocolVersion6::UNKNOWN_BLOCK)
      // typecode is ignored for this processor
      // pl -- this raises the interesting situation where
      // source->type()                  returns the actual type, and
      // source->owner_->block_type()    will return UNKNOWN_BLOCK
{
	block_name_ = "BP6_Unknown_Block";
}

//----------------------------------------------------------------------
int
BP6_UnknownBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);

    if (source->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
        return BP_SUCCESS;  // Now must return SUCCESS if block is not to be forwarded
    }

    // If we're called for this type then security is not enabled
    // and we should NEVER forward BAB
    // WRONG: non-security aware nodes can forward BAB blocks.  BAB is
    // explicitly hop-by-hop, but the hops are between security aware
    // nodes.
    //if (source->type() == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
    //    return BP_FAIL;
    //}

    return BP6_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//----------------------------------------------------------------------
int
BP6_UnknownBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    // This can only be called if there was a corresponding block in
    // the input path
    const SPtr_BlockInfo source = block->source();
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);

    // We shouldn't be here if the block has the following flags set
    ASSERT((source->block_flags() &
            BundleProtocolVersion6::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) == 0);

    // A change to BundleProtocol results in outgoing discard taking place here
    if (source->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
        return BP_SUCCESS;
    }
    
    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
    
    u_int8_t flags = source->block_flags();
    if (last) {
        flags |= BundleProtocolVersion6::BLOCK_FLAG_LAST_BLOCK;
    } else {
        flags &= ~BundleProtocolVersion6::BLOCK_FLAG_LAST_BLOCK;
    }
    flags |= BundleProtocolVersion6::BLOCK_FLAG_FORWARDED_UNPROCESSED;
    
    block->set_eid_list(source->eid_list());

    generate_preamble(xmit_blocks, block, source->type(), flags,
                      source->data_length());
    ASSERT(block->data_length() == source->data_length());
    
    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->full_length());
    memcpy(contents->buf()          + block->data_offset(),
           source->contents().buf() + source->data_offset(),
           block->data_length());
    contents->set_len(block->full_length());

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
bool
BP6_UnknownBlockProcessor::validate(const Bundle*           bundle,
                                BlockInfoVec*           block_list,
                                BlockInfo*              block,
                                status_report_reason_t* reception_reason,
                                status_report_reason_t* deletion_reason)
{
    // check for generic block errors
    if (!BP6_BlockProcessor::validate(bundle, block_list, block,
                                  reception_reason, deletion_reason)) {
        return false;
    }

    // extension blocks of unknown type are considered to be "invalid"
    if (block->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_REPORT_ONERROR) {
        *reception_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
    }

    if (block->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) {
        log_warn_p("/dtn/bundle/protocol/unk", "Discard Bundle on Error: *%p  Block type: 0x%2x", 
                   bundle, block->type());
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    return true;
}

} // namespace dtn
