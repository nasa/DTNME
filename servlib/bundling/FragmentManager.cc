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

#include <list>

#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "BundleRef.h"
#include "FragmentManager.h"
#include "FragmentState.h"
#include "BlockInfo.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"
#include "BP7_BlockProcessor.h"
#include "bundling/SDNV.h"

namespace dtn {
    
class BlockInfoPointerList : public std::list<BlockInfo*> { };

//----------------------------------------------------------------------
FragmentManager::FragmentManager()
    : Logger("FragmentManager", "/dtn/bundle/fragmentation")
{
}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment(Bundle* bundle,
                                 const BlockInfoVec* blocks,
                                 const BlockInfoVec* xmit_blocks,
                                 size_t offset,
                                 size_t length,
                                 bool first,
                                 bool last)
{
    if (bundle->is_bpv6())
        return create_fragment_bp6(bundle, blocks, xmit_blocks, offset, length, first, last);
    else
        return create_fragment_bp7(bundle, blocks, xmit_blocks, offset, length, first, last);

}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment_bp6(Bundle* bundle,
                                 const BlockInfoVec* blocks,
                                 const BlockInfoVec* xmit_blocks,
                                 size_t offset,
                                 size_t length,
                                 bool first,
                                 bool last)
{

    (void) blocks;

    Bundle* fragment = new Bundle(BundleProtocol::BP_VERSION_6);


    // copy the metadata into the new fragment (which can be further fragmented)
    bundle->copy_metadata(fragment);
    fragment->set_is_fragment(true);
    fragment->set_do_not_fragment(false);
    fragment->set_frag_created_from_bundleid(bundle->bundleid());
    

    // copy all blocks that follow the payload, and all those before
    // the payload that are marked with the "must be replicated in every
    // fragment" bit
    BlockInfoVec::const_iterator iter;
    bool found_payload = false;
    size_t ext_block_len = 0;
    for(iter = xmit_blocks->begin(); iter!=xmit_blocks->end();iter++) {
        SPtr_BlockInfo blkptr = *iter;

        int type = blkptr->type();
        SPtr_BlockInfo src_block = blkptr->source();
        if ((type == BundleProtocolVersion6::PRIMARY_BLOCK)
            || (type == BundleProtocolVersion6::PAYLOAD_BLOCK)
            || (!found_payload && first)
            || (found_payload && last)
            || (src_block == nullptr)
            || (blkptr->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_REPLICATE)) {

            // the src_block should be in the recv_blocks list
            if (src_block != nullptr) {
                fragment->mutable_recv_blocks()->push_back(src_block);
            }

            if (type == BundleProtocolVersion6::PAYLOAD_BLOCK) {
                found_payload = true;
            } else {
                ext_block_len += blkptr->contents().len();
                length -= blkptr->contents().len();
            }
        }
    }
    log_debug("FragmentManager::create_fragment_bp6 setting payload lengthto %zu based on block lengths retrieved from xmit_blocks", length);
    log_debug("FragmentManager::create_fragment_bp6 extension blocks total %zu in length", ext_block_len);
    log_debug("FragmentManager::create_fragment_bp6 current payload length %zu", bundle->payload().length());
    // initialize the fragment's orig_length and figure out the offset
    // into the payload
    if (! bundle->is_fragment()) {
        fragment->set_orig_length(bundle->payload().length());
        fragment->set_frag_offset(offset);
        // Need to correct for the fact the new primary block will
        // contain the offset and orig length (which the original
        // unfragmented bundle didn't)
        length -= SDNV::encoding_len(bundle->payload().length());
        length -= SDNV::encoding_len(offset);
    } else {
        fragment->set_orig_length(bundle->orig_length());
        fragment->set_frag_offset(bundle->frag_offset() + offset);
        length -= (SDNV::encoding_len(bundle->frag_offset() + offset) - SDNV::encoding_len(bundle->frag_offset()));
    }
    // also allow for some slop to encode the payload block info
    length -= 12;


    // check for overallocated length
    // If these are equal, the assignemt will do nothing, but we still
    // want to subtract one from the length if this is the first
    // fragment
    if ((offset + length) >= bundle->payload().length()) {
        length = bundle->payload().length()- offset;
        // This catches the case that the first fragment could contain
        // the entire payload, but the trailing extension blocks won't
        // fit.
        if(first) {
           //length -= 1;
           if(length % 2 == 0) {
               length = length/2;
           } else {
               length = length/2+1;
           }
        } 
    }
    log_debug("FragmentManager::create_fragment_bp6 After check for overallocated length, length=%zu", length);


    // initialize payload
    fragment->mutable_payload()->set_length(length);
    fragment->mutable_payload()->write_data(bundle->payload(), offset, length, 0);

    // set the frag length in the GbofId
    fragment->set_frag_length(length);

    // if original bundle is in custody then take custody of this fragment
    if (bundle->local_custody()) {
        fragment->mutable_custodian()->assign(EndpointID::NULL_EID());
        BundleDaemon::instance()->accept_custody(fragment);
    }

    log_debug("Created BP6 fragment: %s", fragment->gbofid_str().c_str()); //dz debug

    return fragment;
}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment_bp7(Bundle* bundle,
                                 const BlockInfoVec* blocks,
                                 const BlockInfoVec* xmit_blocks,
                                 size_t offset,
                                 size_t length,
                                 bool first,
                                 bool last)
{
    (void) blocks;

    Bundle* fragment = new Bundle();


    // copy the metadata into the new fragment (which can be further fragmented)
    bundle->copy_metadata(fragment);
    fragment->set_is_fragment(true);
    fragment->set_do_not_fragment(false);
    fragment->set_frag_created_from_bundleid(bundle->bundleid());
    

    // copy all blocks that follow the payload, and all those before
    // the payload that are marked with the "must be replicated in every
    // fragment" bit
    BlockInfoVec::const_iterator iter;
    bool found_payload = false;
    size_t ext_block_len=0;
    for(iter = xmit_blocks->begin(); iter!=xmit_blocks->end();iter++) {
        SPtr_BlockInfo blkptr = *iter;
        int type = blkptr->type();
        SPtr_BlockInfo src_block = blkptr->source();

        if ((type == BundleProtocolVersion7::PRIMARY_BLOCK)
            || (type == BundleProtocolVersion7::PAYLOAD_BLOCK)
            || (!found_payload && first)
            || (found_payload && last)
            || (src_block == nullptr)
            || (blkptr->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_REPLICATE)) {

            // the src_block should be in the recv_blocks list
            if (src_block != nullptr) {
                fragment->mutable_recv_blocks()->push_back(src_block);
            }

            if (type == BundleProtocolVersion7::PAYLOAD_BLOCK) {
                found_payload = true;
            } else {
                ext_block_len += blkptr->contents().len();
                length -= blkptr->contents().len();
            }
        }
    }
    // initialize the fragment's orig_length and figure out the offset
    // into the payload
    size_t extra_bytes = 0;
    if (! bundle->is_fragment()) {
        fragment->set_orig_length(bundle->payload().length());
        fragment->set_frag_offset(offset);
        // Need to correct for the fact the new primary block will
        // contain the offset and orig length (which the original
        // unfragmented bundle didn't)
        extra_bytes = BP7_BlockProcessor::uint64_encoding_len(bundle->payload().length()) * 2;
    } else {
        fragment->set_orig_length(bundle->orig_length());
        fragment->set_frag_offset(bundle->frag_offset() + offset);
        // allow for a larger offset size - a little bit of wasted space 
        // (also subtract length required to encode the payload length)
        extra_bytes = BP7_BlockProcessor::uint64_encoding_len(bundle->orig_length());
    }
    length -= extra_bytes;
    // also allow for some slop to encode the payload block info
    length -= 12;

    log_debug("FragmentManager::create_fragment_bp7 extension blocks total %zu in length", ext_block_len);
    log_debug("FragmentManager::create_fragment_bp7 current payload length %zu", bundle->payload().length());
    log_debug("FragmentManager::create_fragment_bp7 subtracting extra_bytes for fragment values in primary block: %zu", extra_bytes);
    log_debug("FragmentManager::create_fragment_bp7 setting payload lengthto %zu based on block lengths retrieved from xmit_blocks", length);


    // check for overallocated length
    // If these are equal, the assignemt will do nothing, but we still
    // want to subtract one from the length if this is the first
    // fragment
    if ((offset + length) >= bundle->payload().length()) {
        length = bundle->payload().length()- offset;
        // This catches the case that the first fragment could contain
        // the entire payload, but the trailing extension blocks won't
        // fit.
        if(first) {
           //length -= 1;
           if(length % 2 == 0) {
               length = length/2;
           } else {
               length = length/2+1;
           }
        } 
    }
    log_debug("FragmentManager::create_fragment_bp7 After check for overallocated length, length=%zu", length);


    // initialize payload
    fragment->mutable_payload()->set_length(length);
    fragment->mutable_payload()->write_data(bundle->payload(), offset, length, 0);

    // set the frag length in the GbofId
    fragment->set_frag_length(length);

    // if original bundle is in custody then take custody of this fragment
    if (bundle->local_custody()) {
        fragment->mutable_custodian()->assign(EndpointID::NULL_EID());
        BundleDaemon::instance()->accept_custody(fragment);
    }

    log_debug("Created BP7 fragment: %s - orig length: %zu", 
               fragment->gbofid_str().c_str(), fragment->orig_length());

    return fragment;
}

//----------------------------------------------------------------------
bool
FragmentManager::try_to_convert_to_fragment(Bundle* bundle)
{
    SPtr_BlockInfo payload_block
        = bundle->recv_blocks()->find_block(BundleProtocolVersion6::PAYLOAD_BLOCK);
    if (!payload_block) {
        return false; // can't do anything
    }
    if (payload_block->data_offset() == 0) {
        return false; // there is not even enough data for the preamble
    }

    if (bundle->do_not_fragment()) {
        return false; // can't do anything
    }

    // the payload is already truncated to the length that was received
    size_t payload_len  = payload_block->data_length();
    size_t payload_rcvd = bundle->payload().length();

    // A fragment cannot be created with only one byte of payload
    // available.
    if (payload_len <= 1) {
        return false;
    }

    if (payload_rcvd >= payload_len) {
        ASSERT(payload_block->complete() || payload_len == 0);

        // Payload block is always last block in BPv7
        if (bundle->is_bpv7() || payload_block->last_block()) {
            return false; // nothing to do - whole bundle present
        }

        // If the payload block is not the last block, there are extension
        // blocks following it. See if they all appear to be present.
        BlockInfoVec::const_iterator last_block =
            bundle->recv_blocks()->end() - 1;

        SPtr_BlockInfo blkptr = *last_block;
        
        if ((blkptr->data_offset() != 0) && blkptr->complete() && blkptr->last_block()) {
            return false; // nothing to do - whole bundle present
        }

        // At this point the payload is complete but the bundle is not,
        // so force the creation of a fragment by dropping a byte.
        payload_rcvd--;
        bundle->mutable_payload()->truncate(payload_rcvd);
    }
    
    log_debug("partial bundle *%p, making reactive fragment of %zu bytes",
              bundle, payload_rcvd);
        
    if (! bundle->is_fragment()) {
        bundle->set_is_fragment(true);
        bundle->set_orig_length(payload_len);
        bundle->set_frag_offset(0);
    } else {
        // if it was already a fragment, the fragment headers are
        // already correct
    }
    bundle->set_fragmented_incoming(true);
    
    return true;
}

//----------------------------------------------------------------------
void
FragmentManager::get_hash_key(const Bundle* bundle, std::string* key)
{
    char buf[128];
    snprintf(buf, 128, "%" PRIu64 ".%" PRIu64,
             bundle->creation_ts().seconds_,
             bundle->creation_ts().seqno_);
    
    key->append(buf);
    key->append(bundle->source().c_str());
    key->append(bundle->dest().c_str());
}

//----------------------------------------------------------------------
FragmentState*
FragmentManager::proactively_fragment(Bundle* bundle, 
                                      const LinkRef& link,
                                      size_t max_length)
{
    if (bundle->is_bpv6())
        return proactively_fragment_bp6(bundle, link, max_length);
    else if (bundle->is_bpv7())
        return proactively_fragment_bp7(bundle, link, max_length);
    else
        return nullptr;

}

//----------------------------------------------------------------------
FragmentState*
FragmentManager::proactively_fragment_bp6(Bundle* bundle, 
                                      const LinkRef& link,
                                      size_t max_length)
{
    size_t payload_len = bundle->payload().length();
    
    Bundle* fragment;
    FragmentState* state = nullptr; 
    if (!bundle->is_fragment()) {
        state = new FragmentState(bundle);
    } else {
        state = get_fragment_state(bundle);
    }
    //FragmentState* state = new FragmentState();
    
    size_t todo = payload_len;
    if (todo <= 1) {
        log_debug("FragmentManager::proactively_fragment_bp6: we can't fragment a payload of size <= 1");
        return nullptr;
    }
    size_t offset = 0;
    size_t count = 0;

    BlockInfoVec::const_iterator iter;
    bool found_payload = false;
    size_t first_len=0;
    size_t last_len=0;
    for (iter = bundle->xmit_blocks()->find_blocks(link)->begin(); 
         iter != bundle->xmit_blocks()->find_blocks(link)->end(); 
         iter++) {
        SPtr_BlockInfo blkptr = *iter;

        int type = blkptr->type();
        SPtr_BlockInfo src_block = blkptr->source();
        if (type == BundleProtocolVersion6::PRIMARY_BLOCK) {
            first_len += blkptr->contents().len();
            last_len += blkptr->contents().len();
        } else if (type == BundleProtocolVersion6::PAYLOAD_BLOCK) {
            found_payload = true;
        } else if (blkptr->block_flags() & BundleProtocolVersion6::BLOCK_FLAG_REPLICATE || src_block == nullptr) {
            first_len+= blkptr->contents().len();
            last_len += blkptr->contents().len();
        } else if (found_payload) {
            last_len += blkptr->contents().len();
        } else {
            first_len += blkptr->contents().len();
        }
    }
    if (bundle->is_fragment()) {
        // and some slop to allow for the bytes to encode the payload block header
        first_len += SDNV::encoding_len(bundle->orig_length()) + 12;
        last_len += SDNV::encoding_len(bundle->orig_length()) + 12;
    } else {
        // and some slop to allow for the bytes to encode the payload block header
        first_len += SDNV::encoding_len(bundle->payload().length()*2) + 12;
        last_len += SDNV::encoding_len(bundle->payload().length()*2) + 12;
    }
    

    if (first_len >= max_length || last_len >= max_length) {
        log_debug("FragmentManager::proactively_fragment_bp6 can't fragment because the extension blocks are too large");
        return nullptr;
    }

    //dz debug - adjust max_length because a calc is off somewhere -- frags are bigger than the mtu also
    if (max_length > 10) max_length -= 10;

   
    bool first = true; 
    bool last = false;
    SPtr_BlockInfoVec xmit_blocks = bundle->xmit_blocks()->find_blocks(link);
    do {
        if ( !first && ((todo+last_len) <= max_length)) {
            last = true;
        }

        fragment = create_fragment(bundle, bundle->recv_blocks().get(), xmit_blocks.get(), offset, max_length, first, last);
        ASSERT(fragment);

        first = false;
        log_debug("FragmentManager::proactively_fragment_bp6: just created fragment #%zu with payload len %zu",count+1, fragment->payload().length());
        
        state->add_fragment(fragment);
        offset += fragment->payload().length();
        todo -= fragment->payload().length();
        ++count;
        
    } while (todo > 0);
    
    log_info("proactively BP6 fragmenting "
            "%zu byte payload into %zu <=%zu byte fragments",
            payload_len, count, max_length);
    
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    fragment_table_[hash_key] = state;

    return state;
}

//----------------------------------------------------------------------
FragmentState*
FragmentManager::proactively_fragment_bp7(Bundle* bundle, 
                                      const LinkRef& link,
                                      size_t max_length)
{
    size_t payload_len = bundle->payload().length();
    
    Bundle* fragment;
    FragmentState* state = nullptr; 
    if (!bundle->is_fragment()) {
        state = new FragmentState(bundle);
    } else {
        state = get_fragment_state(bundle);
    }
    //FragmentState* state = new FragmentState();
    
    size_t todo = payload_len;
    if (todo <= 1) {
        log_debug("FragmentManager::proactively_fragment_bp7: we can't fragment a payload of size <= 1");
        return nullptr;
    }
    size_t offset = 0;
    size_t count = 0;

    BlockInfoVec::const_iterator iter;
    bool found_payload = false;
    size_t first_len = 2; // allow for 2 byte CBOR overhead per bundle
    size_t last_len = 2;
    for (iter = bundle->xmit_blocks()->find_blocks(link)->begin(); 
         iter != bundle->xmit_blocks()->find_blocks(link)->end(); 
         iter++) {
        SPtr_BlockInfo blkptr = *iter;

        int type = blkptr->type();
        const SPtr_BlockInfo src_block = blkptr->source();
        if (type == BundleProtocolVersion7::PRIMARY_BLOCK) {
            first_len += blkptr->contents().len();
            last_len += blkptr->contents().len();
        } else if (type == BundleProtocolVersion7::PAYLOAD_BLOCK) {
            found_payload = true;
        } else if ((blkptr->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_REPLICATE) || src_block == nullptr) {
            first_len += blkptr->contents().len();
            last_len += blkptr->contents().len();
        } else if (found_payload) {
            last_len += blkptr->contents().len();
        } else {
            first_len += blkptr->contents().len();
        }
    }


    if (bundle->is_fragment()) {
        // allow for increase in the fragment offset value 
        // and some slop to allow for the bytes to encode the payload block header
        int64_t extra_bytes = BP7_BlockProcessor::uint64_encoding_len(bundle->orig_length()) + 12;
        first_len += extra_bytes;
        last_len += extra_bytes;
    } else {
        // allow for the two new fragment related fields in the Primary Block
        // and some slop to allow for the bytes to encode the payload block header
        int64_t extra_bytes = BP7_BlockProcessor::uint64_encoding_len(bundle->payload().length()) * 2 + 12;
        first_len += extra_bytes;
        last_len += extra_bytes;
    }
    

    if (first_len >= max_length || last_len >= max_length) {
        log_debug("FragmentManager::proactively_fragment_bp7 can't fragment because the extension blocks are too large");
        return nullptr;
    }

   
    bool first = true; 
    bool last = false;
    SPtr_BlockInfoVec xmit_blocks = bundle->xmit_blocks()->find_blocks(link);
    do {
        if( !first && ((todo+last_len) <= max_length)) {
            last = true;
        }
        fragment = create_fragment(bundle, bundle->recv_blocks().get(), xmit_blocks.get(), offset, max_length, first, last);
        ASSERT(fragment);

        first = false;
        //dzdebug
        log_always("FragmentManager::proactively_fragment_bp7: just created fragment #%zu with %zu blocks and payload len %zu",
                  count+1, fragment->recv_blocks()->size(), fragment->payload().length());
        
        state->add_fragment(fragment);
        offset += fragment->payload().length();
        todo -= fragment->payload().length();
        ++count;
        
    } while (todo > 0);
    
    log_info("proactively BP7 fragmenting "
            "%zu byte payload into %zu <=%zu byte fragments",
            payload_len, count, max_length);
    
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    fragment_table_[hash_key] = state;

    return state;
}

//----------------------------------------------------------------------
FragmentState*
FragmentManager::get_fragment_state(Bundle* bundle)
{
    std::string hash_key;
    get_hash_key(bundle, &hash_key);
    FragmentTable::iterator iter = fragment_table_.find(hash_key);

    if (iter == fragment_table_.end()) {
        return nullptr;
    } else {
        return iter->second;
    }
}

//----------------------------------------------------------------------
void
FragmentManager::erase_fragment_state(FragmentState* state)
{
    std::string hash_key;
    get_hash_key(state->bundle().object(), &hash_key);
    fragment_table_.erase(hash_key);
}

//----------------------------------------------------------------------
bool
FragmentManager::try_to_reactively_fragment(Bundle* bundle,
                                            BlockInfoVec* blocks,
                                            size_t  bytes_sent)
{
    if (bundle->do_not_fragment()) {
        return false; // can't do anything
    }

    size_t payload_offset = BundleProtocolVersion6::payload_offset(blocks);
    size_t total_length = BundleProtocol::total_length(bundle, blocks);

    if (bytes_sent <= payload_offset) {
        return false; // can't do anything
    }

    if (bytes_sent >= total_length) {
        return false; // nothing to do
    }
    
    const SPtr_BlockInfo payload_block
        = blocks->find_block(BundleProtocolVersion6::PAYLOAD_BLOCK);

    size_t payload_len  = bundle->payload().length();
    size_t payload_sent = std::min(payload_len, bytes_sent - payload_offset);

    // A fragment cannot be created with only one byte of payload
    // available.
    if (payload_len <= 1) {
        return false;
    }

    size_t frag_off, frag_len;

    if (payload_sent >= payload_len) {
        // this means some but not all data after the payload was transmitted
        ASSERT(! payload_block->last_block());

        // keep a byte to put with the trailing blocks
        frag_off = payload_len - 1;
        frag_len = 1;
    }
    else {
        frag_off = payload_sent;
        frag_len = payload_len - payload_sent;
    }

    log_debug("creating reactive fragment (offset %zu len %zu/%zu)",
              frag_off, frag_len, payload_len);
    
    Bundle* tail = create_fragment(bundle, blocks, blocks, frag_off, frag_len, false, true);

    // treat the new fragment as if it just arrived
    BundleDaemon::post_at_head(
        new BundleReceivedEvent(tail, EVENTSRC_FRAGMENTATION));

    return true;
}

//----------------------------------------------------------------------
void
FragmentManager::process_for_reassembly(Bundle* fragment)
{
    FragmentState* state;
    FragmentTable::iterator iter;

    ASSERT(fragment->is_fragment());

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = fragment_table_.find(hash_key);

    log_debug("processing bundle fragment id=%" PRIbid " hash=%s %d",
              fragment->bundleid(), hash_key.c_str(),
              fragment->is_fragment());

    if (iter == fragment_table_.end()) {
        log_debug("no reassembly state for key %s -- creating new state",
                  hash_key.c_str());
        state = new FragmentState();

        // copy the metadata from the first fragment to arrive, but
        // make sure we mark the bundle that it's not a fragment (or
        // at least won't be for long)
        fragment->copy_metadata(state->bundle().object());
        state->bundle()->set_is_fragment(false);
        state->bundle()->mutable_payload()->
            set_length(fragment->orig_length());
        fragment_table_[hash_key] = state;
    } else {
        state = iter->second;
        log_debug("found reassembly state for key %s (%zu fragments)",
                  hash_key.c_str(), state->fragment_list().size());
    }

    // stick the fragment on the reassembly list
    state->add_fragment(fragment);
    
    // store the fragment data in the partially reassembled bundle file
    size_t fraglen = fragment->payload().length();
    
    log_debug("write_data: length_=%zu src_offset=%u dst_offset=%u len %zu",
              state->bundle()->payload().length(), 
              0, fragment->frag_offset(), fraglen);

    state->bundle()->mutable_payload()->
        write_data(fragment->payload(), 0, fraglen,
                   fragment->frag_offset());
    
    // XXX/jmmikkel this ensures that we have a set of blocks in the
    // reassembled bundle, but eventually reassembly will have to do much more
    if (fragment->frag_offset() == 0)
    {
        BlockInfoVec::iterator dest_iter = state->bundle()->mutable_recv_blocks()->begin();
        BlockInfoVec::const_iterator block_i;
        for (block_i =  fragment->recv_blocks()->begin();
             block_i != fragment->recv_blocks()->end(); ++block_i)
        {
            SPtr_BlockInfo blkptr_i = *block_i;
            SPtr_BlockInfo blkptr_dest = std::make_shared<BlockInfo>(blkptr_i);

            dest_iter = state->bundle()->mutable_recv_blocks()->
                insert(dest_iter, blkptr_dest) +1;


            if(blkptr_i->type() == BundleProtocolVersion6::PAYLOAD_BLOCK) {
                break;
            }
        }
    }

    if(fragment->frag_offset() + fraglen == fragment->orig_length()) {
        BlockInfoVec::const_iterator block_i;
        bool seen_payload = false;
        for (block_i =  fragment->recv_blocks()->begin();
             block_i != fragment->recv_blocks()->end(); ++block_i)
        {
            SPtr_BlockInfo blkptr_i = *block_i;

            if (seen_payload) {
                SPtr_BlockInfo blkptr_dest = std::make_shared<BlockInfo>(blkptr_i);
                state->bundle()->mutable_recv_blocks()->push_back(blkptr_dest);
            }
            if (blkptr_i->type() == BundleProtocolVersion6::PAYLOAD_BLOCK) {
                seen_payload = true;
            }
        }
    }

    
    // check see if we're done
    if (! state->check_completed() ) {
        return;
    }

    BundleDaemon::post_at_head
        (new ReassemblyCompletedEvent(state->bundle().object(),
                                      &state->fragment_list()));
    ASSERT(state->fragment_list().size() == 0); // moved into the event
    fragment_table_.erase(hash_key);
    delete state;
}

//----------------------------------------------------------------------
void
FragmentManager::delete_obsoleted_fragments(Bundle* bundle)
{
    FragmentState* state;
    FragmentTable::iterator iter;
    
    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(bundle, &hash_key);
    iter = fragment_table_.find(hash_key);

    log_debug("checking for obsolete fragments id=%" PRIbid " hash=%s...",
              bundle->bundleid(), hash_key.c_str());
    
    if (iter == fragment_table_.end()) {
        log_debug("no reassembly state for key %s",
                  hash_key.c_str());
        return;
    }

    state = iter->second;
    log_debug("found reassembly state... deleting %zu fragments",
              state->num_fragments());

    BundleRef fragment("FragmentManager::delete_obsoleted_fragments");
    BundleList::iterator i;
    oasys::ScopeLock l(state->fragment_list().lock(),
                       "FragmentManager::delete_obsoleted_fragments");
    while (! state->fragment_list().empty()) {
        BundleDaemon::post(new BundleDeleteRequest(state->fragment_list().pop_back(),
                                                   BundleProtocol::REASON_NO_ADDTL_INFO));
    }

    ASSERT(state->fragment_list().size() == 0); // moved into events
    l.unlock();
    fragment_table_.erase(hash_key);
    delete state;
}

//----------------------------------------------------------------------
void
FragmentManager::delete_fragment(Bundle* fragment)
{
    FragmentState* state;
    FragmentTable::iterator iter;

    ASSERT(fragment->is_fragment());

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = fragment_table_.find(hash_key);

    // no reassembly state, simply return
    if (iter == fragment_table_.end()) {
        return;
    }

    state = iter->second;

    // remove the fragment from the reassembly list
    bool erased = state->erase_fragment(fragment);

    // fragment was not in reassembly list, simply return
    if (!erased) {
        return;
    }

    // note that the old fragment data is still kept in the
    // partially-reassembled bundle file, but there won't be metadata
    // to indicate as such
    
    // delete reassembly state if no fragments now exist
    if (state->num_fragments() == 0) {
        fragment_table_.erase(hash_key);
        delete state;
    }
}

} // namespace dtn
