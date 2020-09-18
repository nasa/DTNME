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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/Log.h>

#include "BlockProcessor.h"
#include "BlockInfo.h"
#include "Bundle.h"
#include "SDNV.h"

namespace dtn {

static const char* log = "/dtn/bundle/protocol";

//----------------------------------------------------------------------
BlockProcessor::BlockProcessor(int block_type)
    : block_type_(block_type)
{
    // quiet down non-debugging build
    (void)log;
}

//----------------------------------------------------------------------
BlockProcessor::~BlockProcessor()
{
}

//----------------------------------------------------------------------
int
BlockProcessor::consume_preamble(BlockInfoVec* recv_blocks,
                                 BlockInfo*    block,
                                 u_char*       buf,
                                 size_t        len,
                                 u_int64_t*    flagp)
{
    static const char* log = "/dtn/bundle/protocol";
    int sdnv_len;
    ASSERT(! block->complete());
    ASSERT(block->data_offset() == 0);

    // The block info buffer usually will already contain enough space
    // for the preamble in the static part of the scratch buffer, but the 
    // presence of an EID-ref-list might cause it to be bigger.
    // So we'll copy up to what will fit in the static part, or
    // expand the buffer if it is already full. 
    // Actually, we will probably never get here, as doing do
    // would mean that there were about fifteen EID refs in the preamble.
    if ( block->contents().nfree() == 0 ) {
        block->writable_contents()->reserve(block->contents().len() + 64);
    }    
    
    size_t max_preamble  = block->contents().buf_len();
    size_t prev_consumed = block->contents().len();
    size_t tocopy        = std::min(len, max_preamble - prev_consumed);
    
    ASSERT(max_preamble > prev_consumed);
    BlockInfo::DataBuffer* contents = block->writable_contents();
    ASSERT(contents->nfree() >= tocopy);
    memcpy(contents->end(), buf, tocopy);
    contents->set_len(contents->len() + tocopy);

    // Make sure we have at least one byte of sdnv before trying to
    // parse it.
    if (contents->len() <= BundleProtocol::PREAMBLE_FIXED_LENGTH) {
//dz debug        ASSERT(tocopy == len);
        if (tocopy != len) return -1;

        return len;
    }
    
    size_t buf_offset = BundleProtocol::PREAMBLE_FIXED_LENGTH;
    u_int64_t flags;
    
    // Now we try decoding the sdnv that contains the block processing
    // flags. If we can't, then we have a partial preamble, so we can
    // assert that the whole incoming buffer was consumed.
    sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                            contents->len() - buf_offset,
                            &flags);
    if (sdnv_len == -1) {
//dz debug        ASSERT(tocopy == len);
        if (tocopy != len) return -1;
        return len;
    }
    
    if (flagp != NULL)
        *flagp = flags;
    
    buf_offset += sdnv_len;
    
    // point at the local dictionary
    Dictionary* dict = recv_blocks->dict();

    // Now we try decoding the EID-references field, if it is present.
    // As with the flags, if we don't finish then we have a partial
    // preamble and will try again when we get more, so we first
    // construct a temporary eid list and then only assign it to the
    // block if we've parsed the whole preamble.
    //
    // We assert that the whole incoming buffer was consumed.
    u_int64_t eid_ref_count = 0LLU;
    u_int64_t scheme_offset;
    u_int64_t ssp_offset;
    
    ASSERT(block->eid_list().empty());
    EndpointIDVector eid_list;
        
    if ( flags & BundleProtocol::BLOCK_FLAG_EID_REFS ) {
        sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                                contents->len() - buf_offset,
                                &eid_ref_count);
        if (sdnv_len == -1) {
//dz debug            ASSERT(tocopy == len);
            if (tocopy != len) return -1;

            return len;
        }
            
        buf_offset += sdnv_len;
            
        for ( u_int32_t i = 0; i < eid_ref_count; ++i ) {
            // Now we try decoding the sdnv pair with the offsets
            sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                                    contents->len() - buf_offset,
                                    &scheme_offset);
            if (sdnv_len == -1) {
//dz debug                ASSERT(tocopy == len);
                if (tocopy != len) return -1;

                return len;
            }
            buf_offset += sdnv_len;
                    
            sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                                    contents->len() - buf_offset,
                                    &ssp_offset);
            if (sdnv_len == -1) {
//dz debug                ASSERT(tocopy == len);
                if (tocopy != len) return -1;
                return len;
            }
            buf_offset += sdnv_len;
                
            EndpointID eid;
            dict->extract_eid(&eid, scheme_offset, ssp_offset);
            eid_list.push_back(eid);
        }
    }
    
    // Now we try decoding the sdnv that contains the actual block
    // length. If we can't, then we have a partial preamble, so we can
    // assert that the whole incoming buffer was consumed.
    u_int64_t block_len;
    sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                            contents->len() - buf_offset,
                            &block_len);
    if (sdnv_len == -1) {
//dz debug        ASSERT(tocopy == len);
                if (tocopy != len) return -1;
        return len;
    }

    if (block_len > 0xFFFFFFFFLL) {
        // XXX/demmer implement big blocks
        log_err_p(log, "overflow in SDNV value for block type 0x%x",
                  *contents->buf());
        return -1;
    }
    
    buf_offset += sdnv_len;

    // We've successfully consumed the preamble so initialize the
    // data_length and data_offset fields of the block and adjust the
    // length field of the contents buffer to include only the
    // preamble part (even though a few more bytes might be in there.
    block->set_data_length(static_cast<u_int32_t>(block_len));
    block->set_data_offset(buf_offset);
    contents->set_len(buf_offset);

    block->set_eid_list(eid_list);

    log_debug_p(log, "BlockProcessor type 0x%x "
                "consumed preamble %zu/%u for block: "
                "data_offset %u data_length %u eid_ref_count %llu",
                block_type(), buf_offset + prev_consumed,
                block->full_length(),
                block->data_offset(), block->data_length(),
                U64FMT(eid_ref_count));
    
    // Finally, be careful to return only the amount of the buffer
    // that we needed to complete the preamble.
//    ASSERT(buf_offset > prev_consumed);
    return buf_offset - prev_consumed;
}

//----------------------------------------------------------------------
void
BlockProcessor::generate_preamble(BlockInfoVec* xmit_blocks,
                                  BlockInfo*    block,
                                  u_int8_t      type,
                                  u_int64_t     flags,
                                  u_int64_t     data_length)
{
    char      work[1000];
    char*     ptr = work;
    size_t    len = sizeof(work);
    int32_t   sdnv_len;             // must be signed
    u_int32_t scheme_offset;
    u_int32_t ssp_offset;
    
    // point at the local dictionary
    Dictionary* dict = xmit_blocks->dict();
    
    // see if we have EIDs in the list, and process them
    u_int32_t eid_count = block->eid_list().size();
    if ( eid_count > 0 ) {
        flags |= BundleProtocol::BLOCK_FLAG_EID_REFS;
        sdnv_len = SDNV::encode(eid_count, ptr, len);
        ptr += sdnv_len;
        len -= sdnv_len;
        EndpointIDVector::const_iterator iter = block->eid_list().begin();
        for ( ; iter < block->eid_list().end(); ++iter ) {
            dict->add_eid(*iter);
            dict->get_offsets(*iter, &scheme_offset, &ssp_offset);
            sdnv_len = SDNV::encode(scheme_offset, ptr, len);
            ptr += sdnv_len;
            len -= sdnv_len;
            sdnv_len = SDNV::encode(ssp_offset, ptr, len);
            ptr += sdnv_len;
            len -= sdnv_len;
        }
    }
    
    size_t eid_field_len = ptr - work;    // size of the data in the work buffer
    
    size_t flag_sdnv_len = SDNV::encoding_len(flags);
    size_t length_sdnv_len = SDNV::encoding_len(data_length);
    ASSERT(block->contents().len() == 0);
    ASSERT(block->contents().buf_len() >= BundleProtocol::PREAMBLE_FIXED_LENGTH 
           + flag_sdnv_len + eid_field_len + length_sdnv_len);

    u_char* bp = block->writable_contents()->buf();
    len = block->contents().buf_len();
    
    *bp = type;
    bp  += BundleProtocol::PREAMBLE_FIXED_LENGTH;
    len -= BundleProtocol::PREAMBLE_FIXED_LENGTH;
    
    SDNV::encode(flags, bp, flag_sdnv_len);
    bp  += flag_sdnv_len;
    len -= flag_sdnv_len;
    
    memcpy(bp, work, eid_field_len);
    bp  += eid_field_len;
    len -= eid_field_len;
    
    SDNV::encode(data_length, bp, length_sdnv_len);
    bp  += length_sdnv_len;
    len -= length_sdnv_len;

    block->set_data_length(data_length);
    u_int32_t offset = BundleProtocol::PREAMBLE_FIXED_LENGTH + 
                       flag_sdnv_len + eid_field_len + length_sdnv_len;
    block->set_data_offset(offset);
    block->writable_contents()->set_len(offset);

    log_debug_p(log, "BlockProcessor type 0x%x "
                "generated preamble for block type 0x%x flags 0x%llx: "
                "data_offset %u data_length %u eid_count %u",
                block_type(), block->type(), U64FMT(block->flags()),
                block->data_offset(), block->data_length(), eid_count);
}

//----------------------------------------------------------------------
int64_t
BlockProcessor::consume(Bundle*    bundle,
                        BlockInfo* block,
                        u_char*    buf,
                        size_t     len)
{
    (void)bundle;
    
    static const char* log = "/dtn/bundle/protocol";
    (void)log;
    
    size_t consumed = 0;

    ASSERT(! block->complete());
    BlockInfoVec* recv_blocks = bundle->mutable_recv_blocks();

    // Check if we still need to consume the preamble by checking if
    // the data_offset_ field is initialized in the block info
    // structure.
    if (block->data_offset() == 0) {
        int cc = consume_preamble(recv_blocks, block, buf, len);
        if (cc == -1) {
            return -1;
        }

        buf += cc;
        len -= cc;

        consumed += cc;
    }

    // If we still don't know the data offset, we must have consumed
    // the whole buffer
    if (block->data_offset() == 0) {
        ASSERT(len == 0);
    }

    // If the preamble is complete (i.e., data offset is non-zero) and
    // the block's data length is zero, then mark the block as complete
    if (block->data_offset() != 0 && block->data_length() == 0) {
        block->set_complete(true);
    }
    
    // If there's nothing left to do, we can bail for now.
    if (len == 0)
        return consumed;

    // Now make sure there's still something left to do for the block,
    // otherwise it should have been marked as complete
    ASSERT(block->data_length() == 0 ||
           block->full_length() > block->contents().len());

    // make sure the contents buffer has enough space
    block->writable_contents()->reserve(block->full_length());

    size_t rcvd      = block->contents().len();
    size_t remainder = block->full_length() - rcvd;
    size_t tocopy;

    if (len >= remainder) {
        block->set_complete(true);
        tocopy = remainder;
    } else {
        tocopy = len;
    }
    
    // copy in the data
    memcpy(block->writable_contents()->end(), buf, tocopy);
    block->writable_contents()->set_len(rcvd + tocopy);
    len -= tocopy;
    consumed += tocopy;

    log_debug_p(log, "BlockProcessor type 0x%x "
                "consumed %zu/%u for block type 0x%x (%s)",
                block_type(), consumed, block->full_length(), block->type(),
                block->complete() ? "complete" : "not complete");
    
    return consumed;
}

//----------------------------------------------------------------------
bool
BlockProcessor::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    static const char * log = "/dtn/bundle/protocol";
    (void)block_list;
    (void)reception_reason;

    // An administrative bundle MUST NOT contain an extension block
    // with a processing flag that requires a reception status report
    // be transmitted in the case of an error
    if (bundle->is_admin() &&
        block->type() != BundleProtocol::PRIMARY_BLOCK &&
        block->flags() & BundleProtocol::BLOCK_FLAG_REPORT_ONERROR) {
        log_err_p(log, "invalid block flag 0x%x for received admin bundle",
                  BundleProtocol::BLOCK_FLAG_REPORT_ONERROR);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
        
    return true;
}

#ifdef BSP_ENABLED
//----------------------------------------------------------------------
bool
BlockProcessor::validate_security_result(const Bundle*           bundle,
                                         const BlockInfoVec*     block_list,
                                         BlockInfo*              block)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    return true;
}
#endif

//----------------------------------------------------------------------
int
BlockProcessor::reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    
    block->set_reloaded(true);
    return 0;
}

//----------------------------------------------------------------------
int
BlockProcessor::prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list)
{
    (void)bundle;
    (void)link;
    (void)list;
    
    // Received blocks are added to the end of the list (which
    // maintains the order they arrived in) but blocks from any other
    // source are added after the primary block (that is, before the
    // payload and the received blocks). This places them "outside"
    // the original blocks.
    if (list == BlockInfo::LIST_RECEIVED) {
        xmit_blocks->append_block(this, source);
    }
    else {
        ASSERT((*xmit_blocks)[0].type() == BundleProtocol::PRIMARY_BLOCK);
        xmit_blocks->insert(xmit_blocks->begin() + 1, BlockInfo(this, source));
    }
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BlockProcessor::finalize(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link)
{
    (void)xmit_blocks;
    (void)link;
        
    if (bundle->is_admin() && block->type() != BundleProtocol::PRIMARY_BLOCK) {
        ASSERT((block->flags() &
                BundleProtocol::BLOCK_FLAG_REPORT_ONERROR) == 0);
    }
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
BlockProcessor::process(process_func*    func,
                        const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        size_t           offset,
                        size_t           len,
                        OpaqueContext*   context)
{
    u_char* buf;
    
    ASSERT(offset < target_block->contents().len());
    ASSERT(target_block->contents().len() >= offset + len);
    
    // convert the offset to a pointer in the target block
    buf = target_block->contents().buf() + offset;
    
    // call the processing function to do the work
    (*func)(bundle, caller_block, target_block, buf, len, context);
}

//----------------------------------------------------------------------
bool
BlockProcessor::mutate(mutate_func*     func,
                       Bundle*          bundle,
                       const BlockInfo* caller_block,
                       BlockInfo*       target_block,
                       size_t           offset,   
                       size_t           len,
                       OpaqueContext*   context)
{
    u_char* buf;
    
    ASSERT(offset < target_block->contents().len());
    ASSERT(target_block->contents().len() >= offset + len);
    
    // convert the offset to a pointer in the target block
    buf = target_block->contents().buf() + offset;
    
    // call the processing function to do the work
    return (*func)(bundle, caller_block, target_block, buf, len, context);
    
    // if we need to flush changed content back to disk, do it here
}

//----------------------------------------------------------------------
void
BlockProcessor::produce(const Bundle*    bundle,
                        const BlockInfo* block,
                        u_char*          buf,
                        size_t           offset,
                        size_t           len)
{
    (void)bundle;
    ASSERT(offset < block->contents().len());
    ASSERT(block->contents().len() >= offset + len);
    memcpy(buf, block->contents().buf() + offset, len);
}

//----------------------------------------------------------------------
void
BlockProcessor::init_block(BlockInfo*    block,
                           BlockInfoVec* block_list,
                           Bundle*       bundle,
                           u_int8_t      type,
                           u_int8_t      flags,
                           const u_char* bp,
                           size_t        len)
{
	(void)bundle; // Not used in generic case
	ASSERT(block->owner() != NULL);
    generate_preamble(block_list, block, type, flags, len);
    ASSERT(block->data_offset() != 0);
    block->writable_contents()->reserve(block->full_length());
    block->writable_contents()->set_len(block->full_length());
    memcpy(block->writable_contents()->buf() + block->data_offset(),
           bp, len);
}

//----------------------------------------------------------------------
int
BlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
	(void) b;

	buf->append("Generic Block Processor (Base Class)");
	return 0;
}

} // namespace dtn
