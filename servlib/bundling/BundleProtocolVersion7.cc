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
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocolVersion7.h"
#include "BundleTimestamp.h"
#include "BP7_BundleAgeBlockProcessor.h"
#include "BP7_HopCountBlockProcessor.h"
#include "BP7_IMCDestinationsBlockProcessor.h"
#include "BP7_IMCStateBlockProcessor.h"
#include "BP7_PayloadBlockProcessor.h"
#include "BP7_PreviousNodeBlockProcessor.h"
#include "BP7_PrimaryBlockProcessor.h"
#include "BP7_UnknownBlockProcessor.h"


#define BP7_CBOR_BREAK_CHAR 0xff

namespace dtn {

static const char* LOG = "/dtn/bundle/protocol7";

BlockProcessor* BundleProtocolVersion7::processors_[256];


//----------------------------------------------------------------------
void
BundleProtocolVersion7::register_processor(BlockProcessor* bp)
{
    log_debug_p(LOG, "BundleProtocolVersion7::register_processor registering block type=%d", bp->block_type());
    // can't override an existing processor
    ASSERT(processors_[bp->block_type()] == nullptr);
    processors_[bp->block_type()] = bp;
}

//----------------------------------------------------------------------
BlockProcessor*
BundleProtocolVersion7::find_processor(u_int8_t type)
{
    BlockProcessor* ret = processors_[type];

    if (ret == nullptr) {
        ret = BP7_UnknownBlockProcessor::instance();
    }
    return ret;
}

//----------------------------------------------------------------------
void
BundleProtocolVersion7::init_default_processors()
{
    // register default block processor handlers
    BundleProtocolVersion7::register_processor(new BP7_PrimaryBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_PayloadBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_PreviousNodeBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_HopCountBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_IMCDestinationsBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_IMCStateBlockProcessor());
    BundleProtocolVersion7::register_processor(new BP7_BundleAgeBlockProcessor());

//    BundleProtocolVersion7::register_processor(new AgeBlockProcessorBPv7());
}

void BundleProtocolVersion7::delete_block_processors() {
    for (int i=0; i<256; i++) {
        if (processors_[i] != nullptr) {
            delete processors_[i];
            processors_[i] = nullptr;
        }
    }
}

//----------------------------------------------------------------------
void
BundleProtocolVersion7::reload_post_process(Bundle* bundle)
{
    SPtr_BlockInfoVec sptr_api_blocks = bundle->api_blocks();
    SPtr_BlockInfoVec sptr_recv_blocks = bundle->mutable_recv_blocks();

    //log_debug_p(LOG, "BundleProtocolVersion7::reload_post_process");
    for (BlockInfoVec::iterator iter = sptr_api_blocks->begin();
         iter != sptr_api_blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        // allow BlockProcessors [and Ciphersuites] a chance to re-do
        // things needed after a load-from-store
        //log_debug_p(LOG, "reload_post_process on bundle of type %d", blkptr->owner()->block_type());
        blkptr->owner()->reload_post_process(bundle, sptr_api_blocks.get(), blkptr.get());
    }

    for (BlockInfoVec::iterator iter = sptr_recv_blocks->begin();
         iter != sptr_recv_blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        // allow BlockProcessors [and Ciphersuites] a chance to re-do
        // things needed after a load-from-store
        //log_debug_p(LOG, "reload_post_process on bundle of type %d", blkptr->owner()->block_type());
        blkptr->owner()->reload_post_process(bundle, sptr_recv_blocks.get(), blkptr.get());
    }
}
        
//----------------------------------------------------------------------
SPtr_BlockInfoVec
BundleProtocolVersion7::prepare_blocks(Bundle* bundle, const LinkRef& link)
{

    //log_debug_p(LOG, "BundleProtocolVersion7::prepare_blocks begin");
    // create a new block list for the outgoing link by first calling
    // prepare on all the BlockProcessor classes for the blocks that
    // arrived on the link
    SPtr_BlockInfoVec sptr_xmit_blocks = bundle->xmit_blocks()->create_blocks(link);
    SPtr_BlockInfoVec sptr_recv_blocks = bundle->recv_blocks();
    SPtr_BlockInfoVec sptr_api_blocks  = bundle->api_blocks();
    BlockProcessor* bp;
    int i;
    BlockInfoVec::const_iterator recv_iter;
    BlockInfoVec::iterator api_iter;
    SPtr_BlockInfo dummy_source;

    if (sptr_recv_blocks->size() > 0) {
        // if there is a received block, the first one better be the primary
        if (sptr_recv_blocks->front()->type() != PRIMARY_BLOCK) {
            //dzdebug
            log_crit_p("/bp7", "prepare_blocks - bundle %" PRIbid " had recv_blocks:  %zu", bundle->bundleid(), sptr_recv_blocks->size());

            log_crit_p("/bp7", "prepare_blocks - first block of %zu is not a primary block - got 0x%2.2x", 
                       sptr_recv_blocks->size(), sptr_recv_blocks->front()->type());

            int ctr = 0;
            for (recv_iter = sptr_recv_blocks->begin();
                 recv_iter != sptr_recv_blocks->end();
                 ++recv_iter)
            {
                SPtr_BlockInfo blkptr = *recv_iter;
                log_crit_p("/bp7", "prepare_blocks - Block #%d is type 0x%2.2x", ctr, sptr_recv_blocks->front()->type());
                ctr++;
            }

            goto fail;
        }
        //ASSERT(sptr_recv_blocks->front()->type() == PRIMARY_BLOCK);
    
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
            if (bundle->fragmented_incoming()
                && sptr_xmit_blocks->find_block(BundleProtocolVersion7::PAYLOAD_BLOCK)) {
                continue;
            }
            
            if (BP_FAIL == blkptr->owner()->prepare(bundle, sptr_xmit_blocks.get(), blkptr, link,
                                   BlockInfo::LIST_RECEIVED)) {
                log_err_p(LOG, "BundleProtocolVersion7::prepare_blocks: %d->prepare returned BP_FAIL on bundle %" PRIbid, 
                               blkptr->owner()->block_type(), bundle->bundleid());
                goto fail;
            }
        }
    }
    else {
        //log_debug_p(LOG, "adding primary block");
        bp = find_processor(PRIMARY_BLOCK);
        if(BP_FAIL == bp->prepare(bundle, sptr_xmit_blocks.get(), dummy_source, link, BlockInfo::LIST_NONE)) {
            log_err_p(LOG, "BundleProtocolVersion7::prepare_blocks: %d->prepare (PRIMARY_BLOCK) returned BP_FAIL on bundle %" PRIbid, 
                           bp->block_type(), bundle->bundleid());
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

            log_err_p(LOG, "BundleProtocolVersion7::prepare_blocks: %d->prepare returned BP_FAIL on bundle %" PRIbid, 
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

        if (bp == BP7_UnknownBlockProcessor::instance()) {
            continue;
        }

        if (! sptr_xmit_blocks->has_block(i)) {
            //XXX/dz Last chance for a block processor to add its block on the fly?

            // Don't check return value here because most of these
            // normally fail.
            bp->prepare(bundle, sptr_xmit_blocks.get(), dummy_source, link, BlockInfo::LIST_NONE);
        }
    }

    return sptr_xmit_blocks;

fail:
    bundle->xmit_blocks()->delete_blocks(link);
    return NULL;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion7::generate_blocks(Bundle*        bundle,
                                BlockInfoVec*  blocks,
                                const LinkRef& link)
{
    size_t total_len = 2; // 2 bytes for the bundle indefinite length array and its terminating break indicator

    // now assert there's at least 2 blocks (primary + payload) and
    // that the primary is first
    ASSERT(blocks->size() >= 2);
    ASSERT(blocks->front()->type() == PRIMARY_BLOCK);

    // now we make another pass through the list and call generate on
    // each block processor

    BlockInfoVec::iterator last_block = blocks->end() - 1;
    for (BlockInfoVec::iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        bool last = (iter == last_block);
        if(BP_FAIL == blkptr->owner()->generate(bundle, blocks, blkptr.get(), link, last)) {
            log_err_p(LOG, "BundleProtocolVersion7::generate_blocks had %d->generate() return BP_FAIL", blkptr->owner()->block_type());

            goto fail;
        }

        //log_debug_p(LOG, "BundleProtocolVersion7::generate_blocks: generated block (owner 0x%x type 0x%x) "
        //            "data_offset %u data_length %u contents length %zu",
        //            blkptr->owner()->block_type(), blkptr->type(),
        //            blkptr->data_offset(), blkptr->data_length(),
        //            blkptr->contents().len());
    }
    
    // make a final pass through, calling finalize() and extracting
    // the block length
    //
    // NOTE: this pass must be in reverse order, from end of the list
    // to the begining, in order for security processing to work.
    //log_debug_p(LOG, "BundleProtocolVersion7::generate_blocks: calling finalize() on all blocks");
    for (BlockInfoVec::reverse_iterator iter = blocks->rbegin();
         iter != blocks->rend();
         ++iter)
    {
        SPtr_BlockInfo blkptr = *iter;

        //log_debug_p(LOG, "BundleProtocolVersion7::generate_blocks finalizing block of type %d", blkptr->type());
        if(BP_FAIL == blkptr->owner()->finalize(bundle, blocks, blkptr.get(), link)) {
            log_err_p(LOG, "BundleProtocolVersion7::generate_blocks had %d->finalize() return BP_FAIL", blkptr->owner()->block_type());
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

    //log_debug_p(LOG, "BundleProtocolVersion7::generate_blocks: end");
    
    return total_len;
fail:
    bundle->xmit_blocks()->delete_blocks(link);
    return 0;
}

//----------------------------------------------------------------------
void
BundleProtocolVersion7::delete_blocks(Bundle* bundle, const LinkRef& link)
{
    ASSERT(bundle != NULL);

    bundle->xmit_blocks()->delete_blocks(link);
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion7::total_length(Bundle* bundle, const BlockInfoVec* blocks)
{
    (void) bundle;

    size_t ret = 2; // 2 bytes for the bundle indefinite length array and its terminating break indicator

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
BundleProtocolVersion7::payload_offset(const BlockInfoVec* blocks)
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
BundleProtocolVersion7::produce(const Bundle* bundle, const BlockInfoVec* blocks,
                        u_char* data, size_t offset, size_t len, bool* last)
{
    size_t origlen = len;
    *last = false;

    if (len == 0)
        return 0;
    
    // advance past any blocks that are skipped by the given offset
    ASSERT(!blocks->empty());


    // 1st byte is a CBOR indefnite length Array hdr byte
    if (offset == 0) {
        data[0] = 0x9f;  // Major Type 4 (3 bits) Indefinite Length = 31 (5 bits)
        --len;
        ++data;
    } else {
        --offset;  // skip first CBOR byte
    }



    BlockInfoVec::const_iterator iter = blocks->begin();
    SPtr_BlockInfo blkptr = *iter;

    while (iter != blocks->end()) {
        blkptr = *iter;
        if (blkptr) {
            if (offset >= blkptr->full_length()) {
                //log_debug_p(LOG, "BundleProtocolVersion7::produce skipping block type 0x%x "
                //            "since offset %zu >= block length %u",
                //            blkptr->type(), offset, blkptr->full_length());

                offset -= blkptr->full_length();

                iter++;
            } else {
                break;
            }
        } else {
            log_crit_p("/bp7", "BP7::%s - null block pointer - abort", __func__);
        }
    }
    
    if (iter == blocks->end())
    {
        // last chunk completed the payload block with no room for the CBOR break
        // byte closing the bundle indefinite length array - provide it now
        data[0] = BP7_CBOR_BREAK_CHAR;  // 0xff is the CBOR break value
        *last = true;
        return 1;
    }

    // the offset should be within the bounds of this block
    ASSERT(iter != blocks->end());
        
    // figure out the amount to generate from this block
    while (1) {
        size_t remainder = blkptr->full_length() - offset;
        size_t tocopy    = std::min(len, remainder);
        //log_debug_p(LOG, "BundleProtocolVersion7::produce copying %zu/%zu bytes from "
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
            // always have to finish witht the CBOR break value to close 
            // the bundle's indefinite length array so this cannot be the last chunk
            break;
        }

        // we completed the current block, so we're done if this
        // is the last block, even if there's space in the user buffer
        ASSERT(tocopy == remainder);
        if (blkptr->type() == PAYLOAD_BLOCK) {
            break;
        }
        
        ++iter;
        ASSERT(iter != blocks->end());
        blkptr = *iter;
    }

    if (len > 0) {
        // there is room to close the bundle indefinitelength array
        // wth the CBOR break byte
        data[0] = BP7_CBOR_BREAK_CHAR;  // 0xff is the CBOR break value
        len -= 1;
        *last = true;
    }
    
    //log_debug_p(LOG, "BundleProtocolVersion7::produce complete: "
    //            "produced %zu bytes, bundle %s",
    //            origlen - len, *last ? "complete" : "not complete");
    
    return origlen - len;
}
    
//----------------------------------------------------------------------
int
BundleProtocolVersion7::peek_into_cbor_for_block_type(u_char* buf, size_t buflen, uint8_t& block_type)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    #define CHECK_ERR_RETURN \
        if ((err == CborErrorAdvancePastEOF) || (err == CborErrorUnexpectedEOF)) \
        { \
            return BP7_UNEXPECTED_EOF; \
        } \
        else if (err != CborNoError) \
        { \
            log_err_p(LOG, "BP7::peek_into_cbor_for_block_type - cbor error"); \
            return BP_FAIL; \
        }

    err = cbor_parser_init(buf, buflen, 0, &parser, &cvBlockArray);
    CHECK_ERR_RETURN

    if (!cbor_value_is_container(&cvBlockArray)) {
        log_err_p(LOG, "BP7::peek_into_cbor_for_block_type - error: not a container");

        return BP_FAIL;
    }

    err = cbor_value_enter_container(&cvBlockArray, &cvBlockElements);
    CHECK_ERR_RETURN

    if (!cbor_value_is_unsigned_integer(&cvBlockElements)) {
        log_err_p(LOG, "BP7::peek_into_cbor_for_block_type - error: block type field not an unsigned integer");

        return BP_FAIL;
    }

    uint64_t ui64_type = 0;
    err = cbor_value_get_uint64(&cvBlockElements, &ui64_type);
    CHECK_ERR_RETURN

    block_type = (ui64_type & 0xff);

    #undef CHECK_ERR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
ssize_t
BundleProtocolVersion7::consume(Bundle* bundle,
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
        // verify that the first byte of data is the CBOR indefinite length array
        if (data[0] != 0x9f)
        {
            log_err_p(LOG, "BundleProtocolVersion7::consume - Invalid first CBOR character: %02x ", data[0]);
            return BP_FAIL;
        }
        else
        {
            // skip past the CBOR indefinie array indicator
            len -= 1;
            ++data;
            //dzdebug
            //log_always_p(LOG, "BundleProtocolVersion7::consume - got correct CBOR indefinite length aray - bytes to process: %zu",
            //             len);
        }

        //log_debug_p(LOG, "consume: got first block... "
        //            "creating primary block info");
        sptr_recv_blocks->append_block(find_processor(PRIMARY_BLOCK));
    } else {
            //dzdebug
            //log_always_p(LOG, "BundleProtocolVersion7::consume - resuming bundle - bytes to process: %zu first byte = %2.2x",
            //             len, data[0]);
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
            uint8_t block_type = 0;

            int status = peek_into_cbor_for_block_type(data, len, block_type);
            if (BP_SUCCESS != status)
            {
                if (BP7_UNEXPECTED_EOF == status) {
                    //log_debug_p(LOG, "BundleProtocolVersion7::consume - need more data (origlen= %zu len= %zu) first byte: %2.2x",
                    //          origlen, len, data[0]);
                    return origlen - len;
                } else {
                    log_err_p(LOG, "BundleProtocolVersion7::consume - error peek at next block type  (origlen= %zu len= %zu)     first byte: %2.2x",
                              origlen, len, data[0]);
                    return -1;
                }
            }

            //log_always_p(LOG, "BundleProtocolVersion7::consume - next block type = %u", block_type);

            info = sptr_recv_blocks->append_block(find_processor(block_type));

            //log_always_p(LOG, "consume: previous block complete, "
            //            "created new BlockInfo type 0x%x",
            //            info->owner()->block_type());
        }
        
        // now we know that the block isn't complete, so we tell it to
        // consume a chunk of data
        //log_always_p(LOG, "consume: block processor 0x%x type 0x%x incomplete, "
        //            "calling consume (%zu bytes already buffered)",
        //            info->owner()->block_type(), info->type(),
        //            info->contents().len());
        
        ssize_t cc = info->owner()->consume(bundle, info.get(), data, len);
        if (cc < 0) {
            log_err_p(LOG, "consume: protocol error handling block 0x%x, prev_consumed= %zu len= %zu",
                      info->type(), info->contents().len(), len);
            return -1;
        }

        // decrement the amount that was just handled from the overall
        // total. verify that the block was either completed or
        // consumed all the data that was passed in.
        len  -= cc;
        data += cc;

        //log_always_p(LOG, "consume: consumed %u bytes of block type 0x%x (%s)",
        //            cc, info->type(),
        //            info->complete() ? "complete" : "not complete");

        if (info->complete()) {
            // check if we're done with the bundle
            if (info->owner()->block_type() == PAYLOAD_BLOCK) {
                *last = true;
                break;
            }
                
        } else {
            ASSERT(len == 0);
        }
    }


    //if (len != 0) {
    //    log_always_p(LOG, "consume completed, %zu/%zu bytes consumed %s",
    //                origlen - len, origlen, *last ? "(completed bundle)" : "");
    //}

    return origlen - len;
}

//----------------------------------------------------------------------
bool
BundleProtocolVersion7::validate(Bundle* bundle,
                         BundleProtocol::status_report_reason_t* reception_reason,
                         BundleProtocol::status_report_reason_t* deletion_reason)
{
    //log_debug_p(LOG, "BundleProtocolVersion7::validate begin");

    int primary_blocks = 0, payload_blocks = 0;
    SPtr_BlockInfoVec sptr_recv_blocks = bundle->mutable_recv_blocks();
 
    // a bundle must include at least two blocks (primary and payload)
    if (sptr_recv_blocks->size() < 2) {
        log_err_p(LOG, "bundle fails to contain at least two blocks");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // the first block of a bundle must be a primary block
    if (sptr_recv_blocks->front()->type() != BundleProtocolVersion7::PRIMARY_BLOCK) {
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
            if (blkptr->type() == BundleProtocolVersion7::PRIMARY_BLOCK) {
                primary_blocks++;
            }

            if (blkptr->type() == BundleProtocolVersion7::PAYLOAD_BLOCK) {
                payload_blocks++;
            }
        }
        
        // this is where we validate extension blocks...?
        if (!blkptr->owner()->validate(bundle, sptr_recv_blocks.get(), blkptr.get(), 
                                reception_reason, deletion_reason)) {
            return false;
        }

        // last block must be the Payload block
        if (iter == last_block) {
            if (blkptr->owner()->block_type() != PAYLOAD_BLOCK)
            {
                log_err_p(LOG, "bundle's last block is not a Paylaod Block");
                *deletion_reason
                        = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
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

    return true;
}

//----------------------------------------------------------------------
int
BundleProtocolVersion7::set_timestamp(u_char* ts, size_t len, const BundleTimestamp& tv)
{
    (void) ts;
    (void) len;
    (void) tv;

    return -1;
}

//----------------------------------------------------------------------
u_int64_t
BundleProtocolVersion7::get_timestamp(BundleTimestamp* tv, const u_char* ts, size_t len)
{
    (void) tv;
    (void) ts;
    (void) len;

    return 0;
}

//----------------------------------------------------------------------
size_t
BundleProtocolVersion7::ts_encoding_len(const BundleTimestamp& tv)
{
    (void) tv;
    return 0;
}

//----------------------------------------------------------------------
bool
BundleProtocolVersion7::get_admin_typexx(const Bundle* bundle, admin_record_type_t* type)
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

#undef  CASE
    default:
        return false; // unknown type
    }

    NOTREACHED;
}

} // namespace dtn
