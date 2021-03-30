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

#include "BP6_PayloadBlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

//----------------------------------------------------------------------
BP6_PayloadBlockProcessor::BP6_PayloadBlockProcessor()
    : BP6_BlockProcessor(BundleProtocolVersion6::PAYLOAD_BLOCK)
{
}

//----------------------------------------------------------------------
ssize_t
BP6_PayloadBlockProcessor::consume(Bundle*    bundle,
                               SPtr_BlockInfo& block,
                               u_char*    buf,
                               size_t     len)
{
    static const char* log = "/dtn/bundle/protocol";
    (void)log;
    
    SPtr_BlockInfoVec& recv_blocks = bundle->mutable_recv_blocks();
    ssize_t consumed = 0;
    if (block->data_offset() == 0) {
        int cc = consume_preamble(recv_blocks, block, buf, len);
        if (cc == -1) {
            return -1;
        }

        buf += cc;
        len -= cc;

        consumed += cc;

        ASSERT(bundle->payload().length() == 0);
    }
    
    // If we still don't know the data offset, we must have consumed
    // the whole buffer
    if (block->data_offset() == 0) {
        ASSERT(len == 0);
        return consumed;
    }

    // Special case for the simulator -- if the payload location is
    // NODATA, then we're done.
    if (bundle->payload().location() == BundlePayload::NODATA) {
        block->set_complete(true);
        return consumed;
    }

    // If we've consumed the length (because the data_offset is
    // non-zero) and the length is zero, then we're done.
    if (block->data_offset() != 0 && block->data_length() == 0) {
        block->set_complete(true);
        return consumed;
    }

    // Also bail if there's nothing left to do
    if (len == 0) {
        return consumed;
    }

    // Otherwise, the buffer should always hold just the preamble
    // since we store the rest in the payload file
    ASSERT(block->contents().len() == block->data_offset());

    // Now make sure there's still something left to do for the block,
    // otherwise it should have been marked as complete
    ASSERT(block->data_length() > bundle->payload().length());

    size_t rcvd      = bundle->payload().length();
    size_t remainder = block->data_length() - rcvd;
    size_t tocopy;

    if (len >= remainder) {
        block->set_complete(true);
        tocopy = remainder;
    } else {
        tocopy = len;
    }

    bundle->mutable_payload()->set_length(rcvd + tocopy);
    bundle->mutable_payload()->write_data(buf, rcvd, tocopy);

    consumed += tocopy;

    // set the frag_length if appropriate
    if (bundle->is_fragment()) {
        bundle->set_frag_length(bundle->payload().length());
    }

    //log_debug_p(log, "BP6_PayloadBlockProcessor consumed %zu/%u (%s)",
    //            consumed, block->full_length(), 
    //            block->complete() ? "complete" : "not complete");
    
    return consumed;
}

//----------------------------------------------------------------------
bool
BP6_PayloadBlockProcessor::validate(const Bundle*           bundle,
                                SPtr_BlockInfoVec&           block_list,
                                SPtr_BlockInfo&              block,
                                status_report_reason_t* reception_reason,
                                status_report_reason_t* deletion_reason)
{
    static const char* log = "/dtn/bundle/protocol";

    // check for generic block errors
    if (!BP6_BlockProcessor::validate(bundle, block_list, block,
                                  reception_reason, deletion_reason)) {
        return false;
    }

    if (!block->complete()) {
        // We do not need the block to be complete because we may be
        // able to reactively fragment it, but we must have at least
        // the full preamble to do so.
        if (block->data_offset() == 0
            
            // There is not much value in a 0-byte payload fragment so
            // discard those as well.
            || (block->data_length() != 0 &&
                bundle->payload().length() == 0)
            
            // If the bundle should not be fragmented and the payload
            // block is not complete, we must discard the bundle.
            || bundle->do_not_fragment())
        {
            log_err_p(log, "payload incomplete and cannot be fragmented");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
int
BP6_PayloadBlockProcessor::generate(const Bundle*  bundle,
                                SPtr_BlockInfoVec&  xmit_blocks,
                                SPtr_BlockInfo&     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)link;
    (void)xmit_blocks;
    
        //log_always_p(log_path(), "BP6_PayloadBlockProcessor::generate");

    // in the ::generate pass, we just need to set up the preamble,
    // since the payload stays on disk
    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocolVersion6::PAYLOAD_BLOCK,
                      last ? BundleProtocolVersion6::BLOCK_FLAG_LAST_BLOCK : 0,
                      bundle->payload().length());

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
BP6_PayloadBlockProcessor::produce(const Bundle*    bundle,
                               const SPtr_BlockInfo& block,
                               u_char*          buf,
                               size_t           offset,
                               size_t           len)
{
    // First copy out the specified range of the preamble
    if (offset < block->data_offset()) {
        size_t tocopy = std::min(len, block->data_offset() - offset);
        memcpy(buf, block->contents().buf() + offset, tocopy);
        buf    += tocopy;
        offset += tocopy;
        len    -= tocopy;
    }

    if (len == 0)
        return;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - block->data_offset();

    size_t tocopy = std::min(len, bundle->payload().length() - payload_offset);
    bundle->payload().read_data(payload_offset, tocopy, buf);
    
    return;
}

//----------------------------------------------------------------------
void
BP6_PayloadBlockProcessor::process(process_func*    func,
                               const Bundle*    bundle,
                               const SPtr_BlockInfo& caller_block,
                               const SPtr_BlockInfo& target_block,
                               size_t           offset,            
                               size_t           len,
                               OpaqueContext*   context)
{
    const u_char* buf;
    u_char  work[1024]; // XXX/pl TODO rework buffer usage
                        // and look at type for the payload data
    size_t  len_to_do = 0;
    
    // re-do these appropriately for the payload
    //ASSERT(offset < target_block->contents().len());
    //ASSERT(target_block->contents().len() >= offset + len);
    
    // First work on specified range of the preamble
    if (offset < target_block->data_offset()) {
        len_to_do = std::min(len, target_block->data_offset() - offset);

        // convert the offset to a pointer in the target block
        buf = target_block->contents().buf() + offset;

        // call the processing function to do the work
        (*func)(bundle, caller_block, target_block, buf, len_to_do, context);
        buf    += len_to_do;
        offset += len_to_do;
        len    -= len_to_do;
    }

    if (len == 0)
        return;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - target_block->data_offset();
    size_t  remaining = std::min(len, bundle->payload().length() - payload_offset);

    buf = work;     // use local buffer

    while ( remaining > 0 ) {        
        len_to_do = std::min(remaining, sizeof(work));   
        buf = bundle->payload().read_data(payload_offset, len_to_do, work);
        
        // call the processing function to do the work
        (*func)(bundle, caller_block, target_block, buf, len_to_do, context);
        
        payload_offset  += len_to_do;
        remaining       -= len_to_do;
    }
}

//----------------------------------------------------------------------
bool
BP6_PayloadBlockProcessor::mutate(mutate_func*     func,
                              Bundle*          bundle,
                              const SPtr_BlockInfo& caller_block,
                              SPtr_BlockInfo&       target_block,
                              size_t           offset,
                              size_t           len,
                              OpaqueContext*   r)
{
    bool changed = false;
    u_char* buf;
    u_char  work[1024];         // XXX/pl TODO rework buffer usage
    //    and look at type for the payload data
    size_t  len_to_do = 0;
    
    // re-do these appropriately for the payload
    //ASSERT(offset < target_block->contents().len());
    //ASSERT(target_block->contents().len() >= offset + len);
    
    // First work on specified range of the preamble
    if (offset < target_block->data_offset()) {
        len_to_do = std::min(len, target_block->data_offset() - offset);
        // convert the offset to a pointer in the target block
        buf = target_block->contents().buf() + offset;
        // call the processing function to do the work
        changed = (*func)(bundle, caller_block, target_block, buf, len_to_do, r);
        buf    += len_to_do;
        offset += len_to_do;
        len    -= len_to_do;
    }

    if (len == 0)
        return changed;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - target_block->data_offset();
    size_t remaining = std::min(len, bundle->payload().length() - payload_offset);

    buf = work;     // use local buffer

    while ( remaining > 0 ) {        
        len_to_do = std::min(remaining, sizeof(work));   
        bundle->payload().read_data(payload_offset, len_to_do, buf);
        
        // call the processing function to do the work
        bool chunk_changed =
            (*func)(bundle, caller_block, target_block, buf, len_to_do, r);
        
        // if we need to flush changed content back to disk, do it 
        if ( chunk_changed )            
            bundle->mutable_payload()->
                write_data(buf, payload_offset, len_to_do);
        
        changed |= chunk_changed;
                   
        payload_offset  += len_to_do;
        remaining       -= len_to_do;
    }

    return changed;
}

//----------------------------------------------------------------------
int
BP6_PayloadBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Payload");
}

} // namespace dtn
