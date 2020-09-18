/*
 *    Copyright 2010-2012 Trinity College Dublin
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

#ifdef BPQ_ENABLED

#include <inttypes.h>

#include <oasys/util/ScratchBuffer.h>

#include "BPQBlockProcessor.h"
#include "SDNV.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/bpq";

namespace oasys {
    template <> BPQBlockProcessor* oasys::Singleton<BPQBlockProcessor>::instance_ = NULL;
}



//----------------------------------------------------------------------
BPQBlockProcessor::BPQBlockProcessor() :
        BlockProcessor(BundleProtocol::QUERY_EXTENSION_BLOCK)
{
    log_info_p(LOG, "BPQBlockProcessor::BPQBlockProcessor()");
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::consume(Bundle* bundle,
                           BlockInfo* block,
                           u_char* buf,
                           size_t len)
{
    log_info_p(LOG, "BPQBlockProcessor::consume() start");

    int cc;

    if ( (cc = BlockProcessor::consume(bundle, block, buf, len)) < 0) {
        log_err_p(LOG, "BPQBlockProcessor::consume(): error handling block 0x%x",
                        BundleProtocol::QUERY_EXTENSION_BLOCK);
        return cc;
    }

    // If we don't finish processing the block, return the number of bytes
    // consumed. (Error checking done in the calling function?)
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    BPQBlock* bpq_block = new BPQBlock(block->writable_contents()->buf() +
									   block->data_offset(),
									   block->data_length());
    log_info_p(LOG, "     BPQBlock:");
    log_info_p(LOG, "         kind: %d", bpq_block->kind());
    log_info_p(LOG, "matching rule: %d", bpq_block->matching_rule());
    log_info_p(LOG, "    query_len: %d", bpq_block->query_len());
    log_info_p(LOG, "    query_val: %s", bpq_block->query_val());

    // record bpq_block in BlockInfo for future use
    block->set_locals(bpq_block);

    log_info_p(LOG, "BPQBlockProcessor::consume() end");
    
    return cc;
}

//----------------------------------------------------------------------

int 
BPQBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list)
{
    log_info_p(LOG, "BPQBlockProcessor::prepare()");

    (void)bundle;
    (void)link;
    (void)list; 

    if (source != NULL) {
        log_debug_p(LOG, "prepare(): data_length() = %u", source->data_length());
        log_debug_p(LOG, "prepare(): data_offset() = %u", source->data_offset());
        log_debug_p(LOG, "prepare(): full_length() = %u", source->full_length());
    }

    // Received blocks are added to the end of the list (which
    // maintains the order they arrived in) but API blocks 
    // are added after the primary block (that is, before the
    // payload and the received blocks). This places them "outside"
    // the original blocks.

    if ( list == BlockInfo::LIST_API ) {
        log_info_p(LOG, "Adding BPQBlock found in API Block Vec to xmit_blocks");

        ASSERT((*xmit_blocks)[0].type() == BundleProtocol::PRIMARY_BLOCK);
        xmit_blocks->insert(xmit_blocks->begin() + 1, BlockInfo(this, source));
        ASSERT(xmit_blocks->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK));

        return BP_SUCCESS;

    } else if ( list == BlockInfo::LIST_RECEIVED ) {

        log_info_p(LOG, "Adding BPQBlock found in Recv Block Vec to xmit_blocks");

        xmit_blocks->append_block(this, source);
        ASSERT(xmit_blocks->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK));

        return BP_SUCCESS;


    } else {

        log_err_p(LOG, "BPQBlock not found in bundle");
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS;
    }
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    log_info_p(LOG, "BPQBlockProcessor::generate() starting");

    (void)xmit_blocks;    
    (void)link;
    (void)bundle;

    ASSERT (block->type() == BundleProtocol::QUERY_EXTENSION_BLOCK);

    // set flags
    u_int8_t flags = block->source()->flags();
    if (last) {
        flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    } else {
        flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    }

    size_t length = block->source()->data_length();
    generate_preamble(xmit_blocks,
                      block,
                      BundleProtocol::QUERY_EXTENSION_BLOCK,
                      flags,
                      length );


    // The process of storing the value into the block. We'll create a
    // `DataBuffer` object and `reserve` the length of our BPQ data and
    // update the length of the `DataBuffer`.

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    // Set our pointer to the right offset.
    u_char* buf = contents->buf() + block->data_offset();
    
    // Copy across the data
    memcpy(buf, block->source()->contents().buf() +
    		    block->source()->data_offset(),
    		    length);

    // Don't need to make a BPQBlock and store it in block locals for xmit blocks.

    log_info_p(LOG, "BPQBlockProcessor::generate() ending");
    return BP_SUCCESS;
}
//----------------------------------------------------------------------
bool
BPQBlockProcessor::validate(const Bundle*           bundle,
                            BlockInfoVec*           block_list,
                            BlockInfo*              block,
                            status_report_reason_t* reception_reason,
                            status_report_reason_t* deletion_reason)
{
    if ( ! BlockProcessor::validate(bundle, 
                                    block_list,
                                    block,reception_reason,
                                    deletion_reason) ) {
        log_err_p(LOG, "BlockProcessor validation failed");
        return false;
    }
    
    if ( block->data_offset() + block->data_length() != block->full_length() ) {
        
        log_err_p(LOG, "offset (%u) + data len (%u) is not equal to the full len (%u)",
                  block->data_offset(), block->data_length(), block->full_length() );
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    if ( block->contents().buf_len() < block->full_length() ) {

        log_err_p(LOG, "block buffer len (%zu) is less than the full len (%u)",
                  block->contents().buf_len(), block->full_length() );
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    BPQBlock* bpq_block = dynamic_cast<BPQBlock *>(block->locals());

    if (!bpq_block->validated()) {
        log_err_p(LOG, "BPQ extension blocks contents are invalid");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block)
{
    (void)bundle;
    (void)block_list;

    BPQBlock* bpq_block = new BPQBlock(block->writable_contents()->buf() +
									   block->data_offset(),
									   block->data_length());

    block->set_locals(bpq_block);

    block->set_reloaded(true);
    return 0;
}

//----------------------------------------------------------------------
void
BPQBlockProcessor::init_block(BlockInfo*    block,
                              BlockInfoVec* block_list,
                              Bundle*       bundle,
                              u_int8_t      type,
                              u_int8_t      flags,
                              const u_char* bp,
                              size_t        len)
{
	BPQBlock* bpq_block = new BPQBlock(bp, len);
	if (!bpq_block->validated()) {
		log_err_p("/bundling/extblock/BPQBlockProcessor",
				  "Supplied BPQ block data was invalid");
	}
	// If a bundle is supplied as a parameter use its creation_ts and source as the
	// original values for this BPQBlock - otherwise assume they are right already.
	if (bundle != NULL) {
		bpq_block->set_creation_ts(bundle->creation_ts());
		bpq_block->set_source(bundle->source());
	}
	size_t new_len= bpq_block->length();
	typedef oasys::ScratchBuffer<u_char*, 128> local_buf_t;
	local_buf_t *temp_buf = new local_buf_t();
	temp_buf->reserve(new_len);
	bpq_block->write_to_buffer(temp_buf->buf(), new_len);

	// Hand off to base class to initialize block
	BlockProcessor::init_block(block, block_list, bundle, type,
							   flags, temp_buf->buf(), new_len);

	// Remember BPQBlock in BP_Locals for block
	block->set_locals(bpq_block);

	delete temp_buf;
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *block)
{
	u_char* content = NULL;
	int len = 0;
	BundleTimestamp creation_ts;
	u_int64_t item_len;
	u_int64_t frag_count;
	u_int64_t frag_offset;
	u_int64_t frag_len;

	if (block!=NULL) {
		content = block->data();
		len = block->data_length();
	}

	buf->append("Query Extension (BPQ):\n");
	int i=0;
	u_int j=0;
	int k;
	int q_decoding_len, f_decoding_len, decoding_len;

	// BPQ-kind             1-byte
	if (i < len) {
		buf->append("    Kind: ");
		j = (u_int)(content[i++]);
		switch ((BPQBlock::kind_t)j) {
		case BPQBlock::KIND_QUERY:
			buf->append("query\n");
			break;
		case BPQBlock::KIND_RESPONSE:
			buf->append("response\n");
			break;
		case BPQBlock::KIND_RESPONSE_DO_NOT_CACHE_FRAG:
			buf->append("response (do not cache fragments)\n");
			break;
		case BPQBlock::KIND_PUBLISH:
			buf->append("publish\n");
			break;
		default:
			buf->appendf("Unknown kind: %u\n", j);
			break;
		}
	} else {
		buf->append("Block too short");
		return 0;
	}

	// matching rule type   1-byte
	if (i < len) {
		buf->appendf("    Matching rule: %d\n",(int) (content[i++]));
	} else {
		buf->append("Block too short\n");
		return 0;
	}

	// Creation time-stamp sec     SDNV
	if ( (q_decoding_len = SDNV::decode (&(content[i]),
										len - i,
										&(creation_ts.seconds_))) == -1 ) {
		buf->append("Error decoding creation time-stamp sec\n");
		return 0;
	}
	i += q_decoding_len;
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}

	// Creation time-stamp seq     SDNV
	if ( (q_decoding_len = SDNV::decode (&(content[i]),
										len - i,
										&(creation_ts.seqno_))) == -1 ) {
		buf->append("Error decoding creation time-stamp seqno\n");
		return 0;
	}
	i += q_decoding_len;
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}
	buf->appendf("    Original creation ts: %" PRIu64 ".%" PRIu64 "\n", creation_ts.seconds_, creation_ts.seqno_);

	// Source EID length     SDNV
	if ( (q_decoding_len = SDNV::decode (&(content[i]),
										len - i,
										&item_len)) == -1 ) {
		buf->append("Error decoding source EID length\n");
		return 0;
	}
	i += q_decoding_len;
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}

	// Source EID            n-bytes
	if (i < len) {
		buf->append("    Source EID: ");
		buf->append((char*)&(content[i]), item_len);
		buf->appendf(" (length %" PRIu64 ")\n", item_len);
		i += item_len;
	} else {
		buf->append("Error copying source EID\n");
		return 0;
	}
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}

	// BPQ query value length     SDNV
	if ( (q_decoding_len = SDNV::decode (&(content[i]),
										len - i,
										&item_len)) == -1 ) {
		buf->append("Error decoding BPQ query value length\n");
		return 0;
	}
	i += q_decoding_len;
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}

	// BPQ query value            n-bytes
	if (i < len) {
		buf->append("    Query value: ");
		buf->append((char*)&(content[i]), item_len - 1);
		if (content[i+item_len] == '\0') {
			buf->append("<nul>");
		} else {
			buf->appendf("<0x%x>", (unsigned int)(content[i+item_len]));
		}
		buf->appendf(" (length %" PRIu64 ")\n", item_len);
		i += item_len;
	} else {
		buf->append("Error copying BPQ query value \n");
		return 0;
	}
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}

	// number of fragments  SDNV
	if ( (f_decoding_len = SDNV::decode (&(content[i]),
										len - i,
										&frag_count)) == -1 ) {
		buf->append("Error decoding number of fragments\n");
		return 0;
	}
	i += f_decoding_len;
	if (i > len) {
		buf->append("Block too short\n");
		return 0;
	}
	buf->appendf("    Number of fragments: %" PRIu64 "\n", frag_count);

	for (k = 0; k < len && (u_int64_t)k < frag_count; ++k) {

		// fragment offset     SDNV
		if ( (decoding_len = SDNV::decode (&(content[i]),
										  len - i,
										  &frag_offset)) == -1 ) {
			buf->appendf("Error decoding fragment offset for fragment #%d\n", k);
			return 0;
		}
		i += decoding_len;
		if (i > len) {
			buf->append("Block too short\n");
			return 0;
		}

		// fragment offsets     SDNV
		if ( (decoding_len = SDNV::decode (&(content[i]),
										  len - i,
										  &frag_len)) == -1 ) {
			buf->appendf("Error decoding fragment offset for fragment #%u\n", j);
			return 0;
		}
		i += decoding_len;
		if (i > len) {
			buf->append("Block too short\n");
			return 0;
		}
		buf->appendf("      [%d] Offset: %" PRIu64 ", Length: %" PRIu64 "\n", k, frag_offset, frag_len);
	}

	if (i != len) {
		buf->appendf("Block is too long - expected %u, actual %u\n", i, len);
		return 0;
	}
	buf->append("\n");
	return(0);
}


} // namespace dtn

#endif /* BPQ_ENABLED */
