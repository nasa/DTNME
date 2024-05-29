/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <sys/types.h>
#include <netinet/in.h>

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/util/StringUtils.h>

#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "BP6_IMCStateBlockProcessor.h"
#include "BP6_MetadataBlockProcessor.h"
#include "BP6_PayloadBlockProcessor.h"
#include "BP6_PreviousHopBlockProcessor.h"
#include "BP6_PrimaryBlockProcessor.h"
#include "BP6_UnknownBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocolVersion6.h"
#include "BundleTimestamp.h"
#include "SDNV.h"

#include "BP6_CustodyTransferEnhancementBlockProcessor.h"

#ifdef ECOS_ENABLED
#  include "BP6_ExtendedClassOfServiceBlockProcessor.h"
#endif // ECOS_ENABLED

namespace dtn {

static const char* LOG = "/dtn/bundle/protocol6";

BlockProcessor* BundleProtocolVersion6::processors_[256];

//----------------------------------------------------------------------
void
BundleProtocolVersion6::register_processor(BlockProcessor* bp)
{
    log_debug_p(LOG, "BundleProtocolVersion6::register_processor registering block type=%d", bp->block_type());
    // can't override an existing processor
    ASSERT(processors_[bp->block_type()] == 0);
    processors_[bp->block_type()] = bp;
}

//----------------------------------------------------------------------
BlockProcessor*
BundleProtocolVersion6::find_processor(u_int8_t type)
{
    BlockProcessor* ret = processors_[type];

    if (ret == 0) {
        ret = BP6_UnknownBlockProcessor::instance();
    }
    return ret;
}

//----------------------------------------------------------------------
void
BundleProtocolVersion6::init_default_processors()
{
    // register default block processor handlers
    BundleProtocolVersion6::register_processor(new BP6_PrimaryBlockProcessor());
    BundleProtocolVersion6::register_processor(new BP6_PayloadBlockProcessor());

    BundleProtocolVersion6::register_processor(new BP6_PreviousHopBlockProcessor());

    BundleProtocolVersion6::register_processor(new BP6_IMCStateBlockProcessor());
    BundleProtocolVersion6::register_processor(new BP6_MetadataBlockProcessor());

    BundleProtocolVersion6::register_processor(new BP6_CustodyTransferEnhancementBlockProcessor());

#ifdef ECOS_ENABLED
    BundleProtocolVersion6::register_processor(new BP6_ExtendedClassOfServiceBlockProcessor());
#endif // ECOS_ENABLED

}

void BundleProtocolVersion6::delete_block_processors() {
    for(int i=0;i<256;i++) {
        if(processors_[i]!=NULL) {
        delete processors_[i];
        processors_[i] = NULL;
        }
    }
}

//----------------------------------------------------------------------
void
BundleProtocolVersion6::reload_post_process(Bundle* bundle)
{
    SPtr_BlockInfoVec sptr_api_blocks = bundle->api_blocks();
    SPtr_BlockInfoVec sptr_recv_blocks = bundle->mutable_recv_blocks();

    //log_debug_p(LOG, "BundleProtocolVersion6::reload_post_process");
    for (BlockInfoVec::iterator iter = sptr_api_blocks->begin();
         iter != sptr_api_blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        // allow BlockProcessors [and Ciphersuites] a chance to re-do
        // things needed after a load-from-store
        //log_debug_p(LOG, "reload_post_process on bundle of type %d", iter->owner()->block_type());
        blkptr->owner()->reload_post_process(bundle, sptr_api_blocks.get(), blkptr.get());
    }

    for (BlockInfoVec::iterator iter = sptr_recv_blocks->begin();
         iter != sptr_recv_blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;
        // allow BlockProcessors [and Ciphersuites] a chance to re-do
        // things needed after a load-from-store
        //log_debug_p(LOG, "reload_post_process on bundle of type %d", iter->owner()->block_type());
        blkptr->owner()->reload_post_process(bundle, sptr_recv_blocks.get(), blkptr.get());
    }
}
        
//----------------------------------------------------------------------
SPtr_BlockInfoVec
BundleProtocolVersion6::prepare_blocks(Bundle* bundle, const LinkRef& link)
{

    //log_debug_p(LOG, "BundleProtocolVersion6::prepare_blocks begin");
    // create a new block list for the outgoing link by first calling
    // prepare on all the BlockProcessor classes for the blocks that
    // arrived on the link
    SPtr_BlockInfoVec sptr_xmit_blocks = bundle->xmit_blocks()->create_blocks(link);
    const SPtr_BlockInfoVec sptr_recv_blocks = bundle->recv_blocks();
    SPtr_BlockInfoVec sptr_api_blocks  = bundle->api_blocks();
    BlockProcessor* bp;
    int i;
    BlockInfoVec::const_iterator recv_iter;
    BlockInfoVec::iterator api_iter;
    BlockProcessor* metadata_processor;
    SPtr_BlockInfo dummy_source;

    if (sptr_recv_blocks->size() > 0) {
        // if there is a received block, the first one better be the primary
        ASSERT(sptr_recv_blocks->front()->type() == PRIMARY_BLOCK);
    
        for (recv_iter = sptr_recv_blocks->begin();
             recv_iter != sptr_recv_blocks->end();
             ++recv_iter)
        {
            SPtr_BlockInfo blkptr = *recv_iter;

            // if this block follows the payload block, and the bundle was
            // reactively fragmented, this block should be in the later
            // fragment, so don't include it
            //
            // XXX/demmer it seems to me like this should just break
            // out of the loop once it's processed the PAYLOAD_BLOCK
            // if fragmented_incoming_ is true
        //log_debug_p(LOG, "BundleProtocolVersion6::prepare_blocks: about to check whether this is a reactively fragmented bundle.");
            if (bundle->fragmented_incoming()
                && sptr_xmit_blocks->find_block(BundleProtocolVersion6::PAYLOAD_BLOCK)) {
                continue;
            }
            
            if(BP_FAIL == blkptr->owner()->prepare(bundle, sptr_xmit_blocks.get(), blkptr, link,
                                   BlockInfo::LIST_RECEIVED)) {
                log_err_p(LOG, "BundleProtocolVersion6::prepare_blocks: %d->prepare returned BP_FAIL on bundle %" PRIbid, blkptr->owner()->block_type(), bundle->bundleid());
                goto fail;
        }else{
            //log_debug_p(LOG, "BundleProtocolVersion6::prepare_block %d->prepare returned BP_SUCCESS on  bundle %" PRIbid " when run on a received block", 
            //                 blkptr->owner()->block_type(), bundle->bundleid());
            }
        }
    }
    else {
        //log_debug_p(LOG, "adding primary and payload block");
        bp = find_processor(PRIMARY_BLOCK);
        if(BP_FAIL == bp->prepare(bundle, sptr_xmit_blocks.get(), dummy_source, link, BlockInfo::LIST_NONE)) {
            log_err_p(LOG, "BundleProtocolVersion6::prepare_blocks: %d->prepare (PRIMARY_BLOCK) returned BP_FAIL on bundle %" PRIbid, 
                           bp->block_type(), bundle->bundleid());
            goto fail;
        }
        bp = find_processor(PAYLOAD_BLOCK);
        if(BP_FAIL == bp->prepare(bundle, sptr_xmit_blocks.get(), dummy_source, link, BlockInfo::LIST_NONE)) {
            log_err_p(LOG, "BundleProtocolVersion6::prepare_blocks: %d->prepare (PAYLOAD_BLOCK) returned BP_FAIL on bundle %" PRIbid, 
                           bp->block_type(),bundle->bundleid());
            goto fail;
        }
    }

    // locally generated bundles need to include blocks specified at the API
    for (api_iter = sptr_api_blocks->begin();
         api_iter != sptr_api_blocks->end();
         ++api_iter)
    {
        SPtr_BlockInfo blkptr = *api_iter;

        //log_debug_p(LOG, "adding api_block");
        if(BP_FAIL == blkptr->owner()->prepare(bundle, sptr_xmit_blocks.get(), 
                               blkptr, link, BlockInfo::LIST_API)){

            log_err_p(LOG, "BundleProtocolVersion6::prepare_blocks: %d->prepare returned BP_FAIL on bundle %" PRIbid, 
                           blkptr->owner()->block_type(), bundle->bundleid());
            goto fail;
        }
    }

    // now we also make sure to prepare() on any registered processors
    // that don't already have a block in the output list. this
    // handles the case where we have a locally generated block with
    // nothing in the recv_blocks vector
    //
    // XXX/demmer this needs some options for the router to select
    // which block elements should be in the list, i.e. security, etc

    for (i = 0; i < 256; ++i) {
        bp = find_processor(i);

        if (bp == BP6_UnknownBlockProcessor::instance()) {
            continue;
        }

        if (! sptr_xmit_blocks->has_block(i)) {
            //XXX/dz Last chance for a block processor to add its block on the fly??

            // Don't check return value here because most of these
            // normally fail.
            bp->prepare(bundle, sptr_xmit_blocks.get(), dummy_source, link, BlockInfo::LIST_NONE);
        }
    }

    // include any metadata locally generated by the API or DP
    metadata_processor = find_processor(METADATA_BLOCK);
    ASSERT(metadata_processor != NULL);
    ASSERT(metadata_processor->block_type() == METADATA_BLOCK);
    ((BP6_MetadataBlockProcessor *)metadata_processor)->
        prepare_generated_metadata(bundle, sptr_xmit_blocks.get(), link);

    return sptr_xmit_blocks;
fail:
    bundle->xmit_blocks()->delete_blocks(link);
    return NULL;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion6::generate_blocks(Bundle*        bundle,
                                BlockInfoVec*  blocks,
                                const LinkRef& link)
{
    BP6_PrimaryBlockProcessor* pbp;
    size_t total_len = 0;
    // now assert there's at least 2 blocks (primary + payload) and
    // that the primary is first
    ASSERT(blocks->size() >= 2);
    ASSERT(blocks->front()->type() == PRIMARY_BLOCK);

    // now we make another pass through the list and call generate on
    // each block processor

    //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks: calling generate() on each block processor");
    BlockInfoVec::iterator last_block = blocks->end() - 1;
    for (BlockInfoVec::iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        bool last = (iter == last_block);
        if(BP_FAIL == blkptr->owner()->generate(bundle, blocks, blkptr.get(), link, last)) {
            log_err_p(LOG, "BundleProtocolVersion6::generate_blocks had %d->generate() return BP_FAIL", blkptr->owner()->block_type());

            goto fail;
        }

        //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks: generated block (owner 0x%x type 0x%x) "
        //            "data_offset %u data_length %u contents length %zu",
        //            blkptr->owner()->block_type(), blkptr->type(),
        //            blkptr->data_offset(), blkptr->data_length(),
        //            blkptr->contents().len());

        if (last) {
            ASSERT(blkptr->last_block());
        } else {
            ASSERT(!blkptr->last_block());
        }
    }
    
    // Now that all the EID references are added to the dictionary,
    // generate the primary block.
    //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks: calling generate() on PRIMARY block");
    pbp =
        (BP6_PrimaryBlockProcessor*)find_processor(PRIMARY_BLOCK);
    ASSERT(blocks->front()->owner() == pbp);
    pbp->generate_primary(bundle, blocks, blocks->front().get());
    
    // make a final pass through, calling finalize() and extracting
    // the block length
    //
    // NOTE: this pass must be in reverse order, from end of the list
    // to the begining, in order for security processing to work.
    //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks: calling finalize() on all blocks");
    for (BlockInfoVec::reverse_iterator iter = blocks->rbegin();
         iter != blocks->rend();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks considering block of type %d", iter->type());
        if(BP_FAIL == blkptr->owner()->finalize(bundle, blocks, blkptr.get(), link)) {
            log_err_p(LOG, "BundleProtocolVersion6::generate_blocks had %d->finalize() return BP_FAIL", blkptr->owner()->block_type());
            goto fail;
        }
    }
    
    for (BlockInfoVec::iterator iter= blocks->begin();
            iter != blocks->end();
            ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        total_len += blkptr->full_length();
    }

    //log_debug_p(LOG, "BundleProtocolVersion6::generate_blocks: end");
    
    return total_len;
fail:
    bundle->xmit_blocks()->delete_blocks(link);
    return 0;
}

//----------------------------------------------------------------------
void
BundleProtocolVersion6::delete_blocks(Bundle* bundle, const LinkRef& link)
{
    ASSERT(bundle != NULL);

    bundle->xmit_blocks()->delete_blocks(link);

    BlockProcessor* metadata_processor = find_processor(METADATA_BLOCK);
    if (!BundleDaemon::instance()->shutting_down()) {
        ASSERT(metadata_processor != NULL);
        ASSERT(metadata_processor->block_type() == METADATA_BLOCK);
        ((BP6_MetadataBlockProcessor *)metadata_processor)->
                                   delete_generated_metadata(bundle, link);
    }
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion6::total_length(Bundle* bundle, const BlockInfoVec* blocks)
{
    (void) bundle;
    size_t ret = 0;
    for (BlockInfoVec::const_iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        ret += blkptr->full_length();
    }

    return ret;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion6::payload_offset(const BlockInfoVec* blocks)
{
    size_t ret = 0;
    for (BlockInfoVec::const_iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        if (blkptr->type() == PAYLOAD_BLOCK) {
            ret += blkptr->data_offset();
            return ret;
        }

        ret += blkptr->full_length();
    }

    return ret;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion6::produce(const Bundle* bundle, const BlockInfoVec* blocks,
                        u_char* data, size_t offset, size_t len, bool* last)
{
    size_t origlen = len;
    *last = false;

    if (len == 0)
        return 0;
    
    // advance past any blocks that are skipped by the given offset
    ASSERT(!blocks->empty());
    BlockInfoVec::const_iterator iter = blocks->begin();
    SPtr_BlockInfo blkptr = *iter;
    while (offset >= blkptr->full_length()) {
        //log_debug_p(LOG, "BundleProtocolVersion6::produce skipping block type 0x%x "
        //            "since offset %zu >= block length %u",
        //            blkptr->type(), offset, blkptr->full_length());
        
        offset -= blkptr->full_length();
        iter++;
        ASSERT(iter != blocks->end());

        blkptr = *iter;
    }
    
    // the offset should be within the bounds of this block
    ASSERT(iter != blocks->end());
        
    // figure out the amount to generate from this block
    while (1) {
        size_t remainder = blkptr->full_length() - offset;
        size_t tocopy    = std::min(len, remainder);
        //log_debug_p(LOG, "BundleProtocolVersion6::produce copying %zu/%zu bytes from "
        //            "block type 0x%x at offset %zu",
        //            tocopy, remainder, blkptr->type(), offset);
        blkptr->owner()->produce(bundle, blkptr.get(), data, offset, tocopy);
        
        len    -= tocopy;
        data   += tocopy;
        offset = 0;

        // if we've copied out the full amount the user asked for,
        // we're done. note that we need to check the corner case
        // where we completed the block exactly to properly set the
        // *last bit
        if (len == 0) {
            if ((tocopy == remainder) && (blkptr->last_block()))
            {
                ASSERT(iter + 1 == blocks->end());
                *last = true;
            }
            
            break;
        }

        // we completed the current block, so we're done if this
        // is the last block, even if there's space in the user buffer
        ASSERT(tocopy == remainder);
        if (blkptr->last_block()) {
            ASSERT(iter + 1 == blocks->end());
            *last = true;
            break;
        }
        
        ++iter;
        ASSERT(iter != blocks->end());
        blkptr = *iter;
    }
    
    //log_debug_p(LOG, "BundleProtocolVersion6::produce complete: "
    //            "produced %zu bytes, bundle %s",
    //            origlen - len, *last ? "complete" : "not complete");
    
    return origlen - len;
}
    
//----------------------------------------------------------------------
ssize_t
BundleProtocolVersion6::consume(Bundle* bundle,
                        u_char* data,
                        size_t  len,
                        bool*   last)
{
    ssize_t origlen = len;
    *last = false;

    SPtr_BlockInfoVec sptr_recv_blocks = bundle->mutable_recv_blocks();
    
    // special case for first time we get called, since we need to
    // create a BlockInfo struct for the primary block without knowing
    // the typecode or the length
    if (sptr_recv_blocks->empty()) {
        //log_debug_p(LOG, "consume: got first block... "
        //            "creating primary block info");
        sptr_recv_blocks->append_block(find_processor(PRIMARY_BLOCK));
    }

    // loop as long as there is data left to process
    while (len != 0) {
        //log_debug_p(LOG, "consume: %zu bytes left to process", len);
        SPtr_BlockInfo info = sptr_recv_blocks->back();

        // if the last received block is complete, create a new one
        // and push it onto the vector. at this stage we consume all
        // blocks, even if there's no BlockProcessor that understands
        // how to parse it
        if (info->complete()) {
            info = sptr_recv_blocks->append_block(find_processor(*data));
            //log_debug_p(LOG, "consume: previous block complete, "
            //            "created new BlockInfo type 0x%x",
            //            info->owner()->block_type());
        }
        
        // now we know that the block isn't complete, so we tell it to
        // consume a chunk of data
        //log_debug_p(LOG, "consume: block processor 0x%x type 0x%x incomplete, "
        //            "calling consume (%zu bytes already buffered)",
        //            info->owner()->block_type(), info->type(),
        //            info->contents().len());
        
        ssize_t cc = info->owner()->consume(bundle, info.get(), data, len);
        if (cc < 0) {
            log_err_p(LOG, "consume: protocol error handling block 0x%x",
                      info->type());
            return -1;
        }
        
        // decrement the amount that was just handled from the overall
        // total. verify that the block was either completed or
        // consumed all the data that was passed in.
        len  -= cc;
        data += cc;

        //log_debug_p(LOG, "consume: consumed %u bytes of block type 0x%x (%s)",
        //            cc, info->type(),
        //            info->complete() ? "complete" : "not complete");

        if (info->complete()) {
            // check if we're done with the bundle
            if (info->last_block()) {
                *last = true;
                break;
            }
                
        } else {
            ASSERT(len == 0);
        }
    }
    
    //log_debug_p(LOG, "consume completed, %zu/%zu bytes consumed %s",
    //            origlen - len, origlen, *last ? "(completed bundle)" : "");
    return origlen - len;
}

//----------------------------------------------------------------------
bool
BundleProtocolVersion6::validate(Bundle* bundle,
                         BundleProtocol::status_report_reason_t* reception_reason,
                         BundleProtocol::status_report_reason_t* deletion_reason)
{
    //log_debug_p(LOG, "BundleProtocolVersion6::validate begin");

    int primary_blocks = 0, payload_blocks = 0;
    SPtr_BlockInfoVec sptr_recv_blocks = bundle->mutable_recv_blocks();
 
    // a bundle must include at least two blocks (primary and payload)
    if (sptr_recv_blocks->size() < 2) {
        log_err_p(LOG, "bundle fails to contain at least two blocks");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // the first block of a bundle must be a primary block
    if (sptr_recv_blocks->front()->type() != BundleProtocolVersion6::PRIMARY_BLOCK) {
        log_err_p(LOG, "bundle fails to contain a primary block");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // validate each individual block
    //log_debug_p(LOG, "validating each individual blocks");

    BlockInfoVec::iterator last_block = sptr_recv_blocks->end() - 1;
    for (BlockInfoVec::iterator iter = sptr_recv_blocks->begin();
         iter != sptr_recv_blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        // a block may not have enough data for the preamble
        if (blkptr->data_offset() == 0) {
            // either the block is not the last one and something went
            // badly wrong, or it is the last block present
            if (iter != last_block) {
                log_err_p(LOG, "bundle block too short for the preamble");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
            // this is the last block, so drop it
            //log_debug_p(LOG, "forgetting preamble-starved last block");
            sptr_recv_blocks->erase(iter);
            if (sptr_recv_blocks->size() < 2) {
                log_err_p(LOG, "bundle fails to contain at least two blocks");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
            // continue with the tests; results may have changed now that
            // a different block is last
            iter = last_block = sptr_recv_blocks->end() - 1;
        }
        else {
            if (blkptr->type() == BundleProtocolVersion6::PRIMARY_BLOCK) {
                primary_blocks++;
            }

            if (blkptr->type() == BundleProtocolVersion6::PAYLOAD_BLOCK) {
                payload_blocks++;
            }
        }
        
        // this is where we validate extension blocks...?
        if (!blkptr->owner()->validate(bundle, sptr_recv_blocks.get(), blkptr.get(), 
                                reception_reason, deletion_reason)) {
            return false;
        }

        // a bundle's last block must be flagged as such,
        // and all other blocks should not be flagged
        if (iter == last_block) {
            if (!blkptr->last_block()) {
                // this may be okay, if it is a reactive fragment
                if (!bundle->fragmented_incoming()) {
                    log_err_p(LOG, "bundle's last block not flagged");
                    *deletion_reason
                        = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                    return false;
                }
                else {
                    //log_debug_p(LOG, "bundle's last block not flagged, but "
                    //                 "it is a reactive fragment");
                }
            }
        } else {
            if (blkptr->last_block()) {
                log_err_p(LOG, "bundle block incorrectly flagged as last");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
        }
    }

    // a bundle must contain one, and only one, primary block
    if (primary_blocks != 1) {
        log_err_p(LOG, "bundle contains %d primary blocks", primary_blocks);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
           
    // a bundle must not contain more than one payload block
    if (payload_blocks > 1) {
        log_err_p(LOG, "bundle contains %d payload blocks", payload_blocks);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    //log_debug_p(LOG, "BundleProtocolVersion6::validate end");

    return true;
}

//----------------------------------------------------------------------
int
BundleProtocolVersion6::set_timestamp(u_char* ts, size_t len, const BundleTimestamp& tv)
{
    int sec_size = SDNV::encode(tv.secs_or_millisecs_, ts, len);
    if (sec_size < 0)
        return -1;
    
    int seqno_size = SDNV::encode(tv.seqno_, ts + sec_size, len - sec_size);
    if (seqno_size < 0)
        return -1;
    
    return sec_size + seqno_size;
}

//----------------------------------------------------------------------
u_int64_t
BundleProtocolVersion6::get_timestamp(BundleTimestamp* tv, const u_char* ts, size_t len)
{
    u_int64_t tmp;
    
    int sec_size = SDNV::decode(ts, len, &tmp);
    if (sec_size < 0)
        return -1;
    tv->secs_or_millisecs_ = tmp;
    
    int seqno_size = SDNV::decode(ts + sec_size, len - sec_size, &tmp);
    if (seqno_size < 0)
        return -1;
    tv->seqno_ = tmp;
    
    return sec_size + seqno_size;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion6::ts_encoding_len(const BundleTimestamp& tv)
{
    size_t time_secs = tv.secs_or_millisecs_;
    return SDNV::encoding_len(time_secs) + SDNV::encoding_len(tv.seqno_);
}

//----------------------------------------------------------------------
bool
BundleProtocolVersion6::get_admin_type(const Bundle* bundle, admin_record_type_t* type)
{
    if (! bundle->is_admin()) {
        return false;
    }

    u_char buf[16];
    // XXX/dz - aborts if payload is less than 16 bytes and we only need 
    //          one byte to determine the admin type
    //const u_char* bp = bundle->payload().read_data(0, sizeof(buf), buf);
    if (bundle->payload().length() < 1) {
        return false;
    }
    const u_char* bp = bundle->payload().read_data(0, 1, buf);

    switch (bp[0] >> 4)
    {
#define CASE(_what) case _what: *type = _what; return true;

        CASE(ADMIN_STATUS_REPORT);
        CASE(ADMIN_CUSTODY_SIGNAL);
        CASE(ADMIN_AGGREGATE_CUSTODY_SIGNAL);
        CASE(ADMIN_MULTICAST_PETITION);

#undef  CASE
    default:
        return false; // unknown type
    }

    NOTREACHED;
}

} // namespace dtn
