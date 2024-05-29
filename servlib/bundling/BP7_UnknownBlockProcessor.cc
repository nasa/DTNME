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

#include "BP7_UnknownBlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"


//#include <oasys/util/HexDumpBuffer.h>

namespace oasys {
    template <> dtn::BP7_UnknownBlockProcessor* oasys::Singleton<dtn::BP7_UnknownBlockProcessor>::instance_ = nullptr;
}

namespace dtn {

//----------------------------------------------------------------------
BP7_UnknownBlockProcessor::BP7_UnknownBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::UNKNOWN_BLOCK)
{
    log_path_ = "/dtn/bp7/unknownblock";
    block_name_ = "BP7_Unknown_Block";
}




//----------------------------------------------------------------------
ssize_t
BP7_UnknownBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    (void) bundle;

    ssize_t consumed = 0;

            //log_always_p(log_path(), "BP7_PrimaryBlockProcessor::consume - first CBOR charater: %02x", buf[0]);

    ASSERT(! block->complete());

    int64_t block_len;
    size_t prev_consumed = block->contents().len();

    if (prev_consumed == 0)
    {
        // try to decode the block from the received data
        block_len = decode_canonical_block(block, (uint8_t*) buf, len);

        if (block_len == BP_FAIL)
        {
            // protocol error - abort
            consumed = BP_FAIL;
        }
        else if (block_len == BP7_UNEXPECTED_EOF)
        {
            // not enogh data received yet
            // - store this chunk in the block contents and wait for more
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(contents->len() + len);
            memcpy(contents->end(), buf, len);
            contents->set_len(contents->len() + len);
            consumed = len;
        }
        else
        {
            // got the entire block in this first chunk of data
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(block_len);
            memcpy(contents->end(), buf, block_len);
            contents->set_len(block_len);

            block->set_complete(true);
            bundle->set_highest_rcvd_block_number(block->block_number());

            consumed = block_len;

            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "consumed %zu CBOR bytes:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

        }
    }
    else
    {
        // combine the previous data with the new data and 
        // try again to decode the block

        // instead of adding all of the new data to the contents
        // using a temp buffer and then only adding the needed
        // bybtes to the contents

        BlockInfo::DataBuffer* contents = block->writable_contents();

        size_t combined_size = prev_consumed + len;
        u_char* temp_buf = (u_char*) malloc(combined_size);

        memcpy(temp_buf, contents->buf(), prev_consumed);
        memcpy(temp_buf + prev_consumed, buf, len);

        // try to decode the block 
        block_len = decode_canonical_block(block, (uint8_t*) temp_buf, combined_size);

        free(temp_buf);

        if (block_len == BP_FAIL)
        {
            // protocol error - abort
            consumed = BP_FAIL;
        }
        else if (block_len == BP7_UNEXPECTED_EOF)
        {
            // not enogh data received yet
            // - store this chunk in the block contents and wait for more
            contents->reserve(contents->len() + len);
            memcpy(contents->end(), buf, len);
            contents->set_len(contents->len() + len);
            consumed = len;
        }
        else
        {
            // we now have the entire block
            contents->reserve(block_len);
            memcpy(contents->end(), buf, block_len - prev_consumed);
            contents->set_len(block_len);

            block->set_complete(true);
            bundle->set_highest_rcvd_block_number(block->block_number());

            consumed = block_len - prev_consumed;

            //oasys::HexDumpBuffer hex;
            //hex.append(contents->buf(), block_len);

            //log_always_p(log_path(), "consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());
        }

    }

            //log_always_p(log_path(), "BP7_UnknwonBlockProcessor::consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_UnknownBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    ASSERT(source != nullptr);
    ASSERT(source->owner() == this);

    if (source->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
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

    return BP7_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}


//----------------------------------------------------------------------
int
BP7_UnknownBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)last;
    
    // This can only be called if there was a corresponding block in
    // the input path
    const SPtr_BlockInfo source = block->source();
    ASSERT(source != nullptr);
    ASSERT(source->owner() == this);

    // We shouldn't be here if the block has the following flags set
    ASSERT((source->block_flags() &
            BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) == 0);

    // A change to BundleProtocol results in outgoing discard taking place here
    if (source->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
        return BP_SUCCESS;
    }
    
    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
  
    // just copy the block's data from the source
    *block = *source;


            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes (full_length=%zu) :",
            //                         block->contents().len(), block->full_length());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

    return BP_SUCCESS;
}


//----------------------------------------------------------------------
bool
BP7_UnknownBlockProcessor::validate(const Bundle*           bundle,
                                    BlockInfoVec*           block_list,
                                    BlockInfo*              block,
                                    status_report_reason_t* reception_reason,
                                    status_report_reason_t* deletion_reason)
{
    // check for generic block errors
    if (!BP7_BlockProcessor::validate(bundle, block_list, block,
                                      reception_reason, deletion_reason)) {
        return false;
    }

    // extension blocks of unknown type are considered to be "unsupported"
    if (block->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_REPORT_ONERROR) {
        *reception_reason = BundleProtocol::REASON_BLOCK_UNSUPPORTED;
    }

    if (block->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) {
        log_warn_p(log_path(), "Discard Bundle on Error: *%p  Block type: 0x%2x", 
                   bundle, block->type());
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNSUPPORTED;
        return false;
    }

    return true;
}


} // namespace dtn
