/*
 *    Copyright 2010-2011 Trinity College Dublin
 *    Copyright 2010-2012 Folly Consulting Ltd
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

#include "BPQBlock.h"
#include "BPQFragmentList.h"
#include "Bundle.h"
#include "BundleProtocol.h"
#include "SDNV.h"

namespace dtn {

BPQBlock::BPQBlock(const Bundle* bundle)
		: BP_Local(),
		  Logger("BPQBlock", "/dtn/bundle/bpq"),
		  query_val_(NULL),
		  fragments_("fragments"),
		  validated_(false)
{
    log_debug("constructor()");

    // Note validation_ is set to false if the data supplied for initialization is invalid

    if( bundle->recv_blocks().
        has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {

        log_debug("BPQBlock found in Recv Block Vec => created remotely");
        initialise_from_block( const_cast<BlockInfo*> (bundle->recv_blocks().
							   find_block(BundleProtocol::QUERY_EXTENSION_BLOCK)),
							   false, bundle);

    } else if( const_cast<Bundle*>(bundle)->api_blocks()->
               has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {

        log_debug("BPQBlock found in API Block Vec => created locally");
        initialise_from_block( const_cast<BlockInfo*> (const_cast<Bundle*>(bundle)->api_blocks()->
							   find_block(BundleProtocol::QUERY_EXTENSION_BLOCK)),
							   true, bundle);

    } else {
        log_err("BPQ Block not found in bundle");
    }

    log_debug("leaving constructor");
}

//----------------------------------------------------------------------
BPQBlock::BPQBlock(const u_char* buf, u_int buf_len)
		: BP_Local(),
		  Logger("BPQBlock", "/dtn/bundle/bpq"),
		  query_val_(NULL),
		  fragments_("fragments"),
		  validated_(false)
{
	log_debug("Creating BPQBlock from data buffer (length %d)", buf_len);
    // Note validation_ is set to false if the data supplied for initialization is invalid

	initialise_from_buf(buf, buf_len);
}

//----------------------------------------------------------------------
BPQBlock::~BPQBlock()
{
    log_debug("BPQBlock: destructor");
    if ( query_val_ != NULL ){
        free(query_val_);
        query_val_ = NULL;
    }
}

//----------------------------------------------------------------------
bool
BPQBlock::validate(u_char* buf, size_t len)
{
	ASSERT(buf != NULL);
	ASSERT(len > 0);
	size_t used_len = 0;
	int sdnv_len;
	u_int64_t sdnv_val;
	u_int64_t frag_count;

#define TEST_ITEM(fn, item_len, msg_item) 								\
    if(!fn) { 															\
    	log_err_p("/bundling/BPQBlock", 								\
				  "Validating BPQBlock buffer failed: %s", msg_item);	\
        return false; 													\
    } else if ((used_len + item_len) > len) {							\
    	log_err_p("/bundling/BPQBlock",									\
				  "Validating BPQBlock buffer failed: Buffer too short");	\
    	return false;													\
	}																	\
	used_len += item_len;												\
	buf += item_len;

	// Kind
	u_int kindv = (u_int)*buf;
	TEST_ITEM(((kindv == BPQBlock::KIND_QUERY) || (kindv == BPQBlock::KIND_RESPONSE) ||
			   (kindv == BPQBlock::KIND_RESPONSE_DO_NOT_CACHE_FRAG) ||
			   (kindv == BPQBlock::KIND_PUBLISH)), 1,
			   "kind is not a valid value");
	// Matching rule
	u_int matching_rule_val = (u_int)*buf;
	TEST_ITEM((matching_rule_val == 0), 1, "matching rule is not a valid value");
	// Timestamp - seconds
	sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
	TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for timestamp seconds");
	// Timestamp - seqno
	sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
	TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for timestamp seqno");
	// Source EID length
	sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
	TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for source EID length");
	// Source EID buffer
	TEST_ITEM((((u_int64_t)used_len + sdnv_val) > len), (int)sdnv_val,
			   "source EID longer than remaining buffer");
	// Query length
	sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
	TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for query length");
	// Query value buffer
	TEST_ITEM((((u_int64_t)used_len + sdnv_val) > len), (int)sdnv_val,
			   "query value longer than remaining buffer");
	// Fragment count
	sdnv_len = SDNV::decode(buf, (len - used_len), &frag_count);
	TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for fragment count");
	// Fragment specifiers
	while (frag_count-- > 0) {
		// Fragment offset
		sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
		TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for fragment offset");
		// Fragment offset
		sdnv_len = SDNV::decode(buf, (len - used_len), &sdnv_val);
		TEST_ITEM((sdnv_len < 0), sdnv_len, "cannot decode SDNV for fragment offset");
	}
	if (used_len != len) {
		log_err_p("/bundling/BPQBlock", "Validating BPQBlock buffer failed: buffer too long");
		return false;
	}
	return true;
}
//----------------------------------------------------------------------
int
BPQBlock::write_to_buffer(u_char* buf, size_t len)
{
    int encoding_len=0; 
    u_int i=0, j=0;

    // BPQ-kind             1-byte
    if ( i < len ) 
        buf[i++] = (u_char) kind_;
    else
        return -1;

    // matching rule type   1-byte
    if ( i < len )
        buf[i++] = (u_char) matching_rule_;
    else
        return -1;

    // timestamp secs		SDNV
    if ( i < len &&
        (encoding_len = SDNV::encode (creation_ts_.seconds_, &(buf[i]), len -i)) >= 0 ) {
        i += encoding_len;
    } else {
        log_err("Error encoding BPQ creation timestamp secs");
        return -1;
    }

    // timestamp seqno		SDNV
    if ( i < len &&
        (encoding_len = SDNV::encode (creation_ts_.seqno_, &(buf[i]), len -i)) >= 0 ) {
        i += encoding_len;
    } else {
        log_err("Error encoding BPQ creation timestamp seqno");
        return -1;
    }

    // source eid length	SDNV
    if ( i < len &&
        (encoding_len = SDNV::encode (source_.length(), &(buf[i]), len -i)) >= 0 ) {
        i += encoding_len;
    } else {
        log_err("Error encoding BPQ source EID length");
        return -1;
    }

    // source eid           n-bytes
    const char* src_eid = source_.c_str();
    for (j=0; src_eid != NULL && i < len && j < source_.length(); i++, j++)
        buf[i] = src_eid[j];

    // query-length         SDNV
    if ( i < len &&
        (encoding_len = SDNV::encode (query_len_, &(buf[i]), len -i)) >= 0 ) {
        i += encoding_len;
    } else {
        log_err("Error encoding BPQ query length");
        return -1;
    }

    // query-value           n-bytes
    for (j=0; query_val_ != NULL && i < len && j < query_len_; i++, j++)
        buf[i] = query_val_[j];    

    // fragment-length		SDNV
    if ( i < len &&
        (encoding_len = SDNV::encode (frag_len(), &(buf[i]), len -i)) >= 0 ) {
        i += encoding_len;
    } else {
        log_err("Error encoding BPQ fragment length");
        return -1;
    }

    // fragment-values		SDNV
    BPQFragmentList::const_iterator iter;
    for (iter  = fragments_.begin();
    	 iter != fragments_.end();
    	 ++iter) {

    	BPQFragment* fragment = *iter;

        if ( i < len &&
            (encoding_len = SDNV::encode (fragment->offset(), &(buf[i]), len -i)) >= 0 ) {
            i += encoding_len;
        } else {
            log_err("Error encoding BPQ individual fragment offset");
            return -1;
        }

        if ( i < len &&
            (encoding_len = SDNV::encode (fragment->length(), &(buf[i]), len -i)) >= 0 ) {
            i += encoding_len;
        } else {
            log_err("Error encoding BPQ individual fragment length");
            return -1;
        }
    }

    ASSERT ( i == this->length())

    return i;
}

//----------------------------------------------------------------------
u_int
BPQBlock::length() const
{
    // initial size {kind, matching rule}
    u_int len = 2; 
        
    len += SDNV::encoding_len(creation_ts_.seconds_);
    len += SDNV::encoding_len(creation_ts_.seqno_);
    len += SDNV::encoding_len(source_.length());
    len += source_.length();

    len += SDNV::encoding_len(query_len_);
    len += query_len_;

    len += SDNV::encoding_len(frag_len());
    BPQFragmentList::const_iterator iter;
	for (iter  = fragments_.begin();
		 iter != fragments_.end();
		 ++iter) {

		BPQFragment* fragment = *iter;

		len += SDNV::encoding_len(fragment->offset());
		len += SDNV::encoding_len(fragment->length());
	}

    return len;
}

//----------------------------------------------------------------------
bool
BPQBlock::match(const BPQBlock* other) const
{
    return matching_rule_ == other->matching_rule() &&
    	   query_len_ == other->query_len() 		&&
           strncmp( (char*)query_val_, (char*)other->query_val(),
                     query_len_ ) == 0;
}

//----------------------------------------------------------------------
void
BPQBlock::add_fragment(BPQFragment* new_fragment)
{
    fragments_.insert_sorted(new_fragment);
}

//----------------------------------------------------------------------
int
BPQBlock::initialise_from_block(BlockInfo* block, bool created_locally, const Bundle* bundle)
{
	ASSERT ( block != NULL);
	ASSERT ( !created_locally || (bundle != NULL));
	int ret;
	log_block_info(block);
	ret = initialise_from_buf(block->data(), block->data_length());

	// Overwrite values (usually read in from API) if bundle created locally
	if ((ret == BP_SUCCESS) && created_locally) {
		log_debug("BPQBlock::initialise_from_block: bundle was locally created");
		log_debug("\t timestamp seconds = %" PRIu64, creation_ts_.seconds_);
		log_debug("\t timestamp sequence number = %" PRIu64, creation_ts_.seqno_);
		log_debug("\t Source EID length = %zu", source_.length());
		log_debug("\t Source EID = %s", source_.c_str());

		creation_ts_.seconds_ 	= bundle->creation_ts().seconds_;
		creation_ts_.seqno_		= bundle->creation_ts().seqno_;
		source_.assign(bundle->source());
	}

	return ret;

}

//----------------------------------------------------------------------
int
BPQBlock::initialise_from_buf(const u_char* buf, u_int buf_length)
{
	validated_ = false;
#define TRY(fn, msg) 									\
    if(fn != BP_SUCCESS) { 								\
    	log_err("Decoding BPQBlock buffer failed: %s", msg);	\
        return BP_FAIL; 								\
    }
	u_int buf_index = 0;

	TRY (extract_kind(buf, &buf_index, buf_length), "Error in 'kind'");
	TRY (extract_matching_rule(buf, &buf_index, buf_length), "Error in 'matching rule'");

	TRY (extract_creation_ts(buf, &buf_index, buf_length),"Error in 'creation_ts'");
	TRY (extract_source(buf, &buf_index, buf_length), "Error in 'Source EID'");

	TRY (extract_query(buf, &buf_index, buf_length), "Error in 'Query'");
	TRY (extract_fragments(buf, &buf_index, buf_length), "Error in 'Fragment specifiers'");

	validated_ = true;
	return BP_SUCCESS;

#undef TRY
}

//----------------------------------------------------------------------
void
BPQBlock::log_block_info(BlockInfo* block)
{
    ASSERT ( block != NULL);

    log_debug("block: data_length() = %d", block->data_length());
    log_debug("block: data_offset() = %d", block->data_offset());
    log_debug("block: full_length() = %d", block->full_length());
    log_debug("block: complete() = %s",
        (block->complete()) ? "true" : "false" );

    log_debug("block: reloaded() = %s",
        (block->reloaded()) ? "true" : "false" );

    if ( block->source() != NULL ) {
        BlockInfo* block_src = const_cast<BlockInfo*>(block->source());

        log_debug("block_src: data_length() = %d", block_src->data_length());
        log_debug("block_src: data_offset() = %d", block_src->data_offset());
        log_debug("block_src: full_length() = %d", block_src->full_length());
        log_debug("block_src: complete() = %s",
            (block_src->complete()) ? "true" : "false" );

        log_debug("block_src: reloaded() = %s",
            (block_src->reloaded()) ? "true" : "false" );
    }

    if ( block->full_length() != block->data_offset() + block->data_length() ) {
        log_err("BPQBlock: full_len != offset + length");
    }

    if ( block->writable_contents()->buf_len() < block->full_length() ){
        log_err("BPQBlock:  buf_len() < full_len");
        log_err("BPQBlock:  buf_len() = %zu",
            block->writable_contents()->buf_len());
    }

    if ( *(block->data()) != KIND_QUERY 		&&
    	 *(block->data()) != KIND_RESPONSE 		&&
    	 *(block->data()) != KIND_RESPONSE_DO_NOT_CACHE_FRAG &&
    	 *(block->data()) != KIND_PUBLISH ) {
        log_err("BPQBlock: block->data() = %d (should be 0|1|2|3)",
            *(block->data()));
    }
}

//----------------------------------------------------------------------
int
BPQBlock::extract_kind (const u_char* buf, u_int* buf_index, u_int buf_length)
{
	if ( *buf_index < buf_length ) {
	    kind_ = (kind_t) buf[(*buf_index)++];
	    log_debug("BPQBlock::extract_kind: kind = %d", kind_);
	    return BP_SUCCESS;

	} else {
	    log_err("Error decoding kind");
	    return BP_FAIL;
	}
}

//----------------------------------------------------------------------
int
BPQBlock::extract_matching_rule (const u_char* buf, u_int* buf_index, u_int buf_length)
{
	if ( *buf_index < buf_length ) {
	    matching_rule_ = (u_int) buf[(*buf_index)++];
	    log_debug("BPQBlock::extract_matching_rule: matching rule = %d", matching_rule_);
	    return BP_SUCCESS;

	} else {
	    log_err("Error decoding matching rule");
	    return BP_FAIL;
	}
}

//----------------------------------------------------------------------
int
BPQBlock::extract_creation_ts (	const u_char* buf,
								u_int* buf_index,
								u_int buf_length)
{
	int decoding_len = 0;

	if (*buf_index < buf_length &&
		(decoding_len = SDNV::decode(	&(buf[*buf_index]),
				 	 	 	 	 	 	buf_length - (*buf_index),
				 	 	 	 	 	 	&(creation_ts_.seconds_)) ) >= 0 ) {
		*buf_index += decoding_len;
		log_debug("BPQBlock::extract_creation_ts: timestamp seconds = %" PRIu64,
				creation_ts_.seconds_);
	} else {
		log_err("Error decoding timestamp seconds");
		return BP_FAIL;
	}


	if (*buf_index < buf_length &&
		(decoding_len = SDNV::decode(	&(buf[*buf_index]),
										buf_length - (*buf_index),
										&(creation_ts_.seqno_)) ) >= 0 ) {
		*buf_index += decoding_len;
		log_debug("BPQBlock::extract_creation_ts: timestamp sequence number = %" PRIu64,
				creation_ts_.seqno_);
	} else {
		log_err("Error decoding timestamp sequence number");
		return BP_FAIL;
	}

	return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BPQBlock::extract_source (	const u_char* buf,
							u_int* buf_index,
							u_int buf_length)
{
	int decoding_len = 0;
	u_int src_eid_len = 0;

	if (*buf_index < buf_length &&
		(decoding_len = SDNV::decode(	&(buf[*buf_index]),
										buf_length - (*buf_index),
										&src_eid_len)) >= 0 ) {
		(*buf_index) += decoding_len;
		log_debug("BPQBlock::extract_source: Source EID length = %u", src_eid_len);
	} else {
		log_err("Error decoding Source EID length");
		return BP_FAIL;
	}

    if (((*buf_index) + src_eid_len) < buf_length  &&
        source_.assign((const char*) &(buf[*buf_index]), src_eid_len) ) {

    	(*buf_index) += src_eid_len;
    	log_debug("BPQBlock::extract_source: Source EID = %s", source_.c_str());

    } else {
        log_err("Error extracting Source EID");
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BPQBlock::extract_query (const u_char* buf, u_int* buf_index, u_int buf_length)
{
	int decoding_len = 0;

	if (*buf_index < buf_length &&
		(decoding_len = SDNV::decode(	&(buf[*buf_index]),
										buf_length - (*buf_index),
										&query_len_)) >= 0 ) {
		(*buf_index) += decoding_len;
		log_debug("BPQBlock::extract_query: query length = %u", query_len_);
    } else {
        log_err("Error decoding BPQ query length");
        return BP_FAIL;
	}

	if (query_len_ <= 0) {
		log_warn("BPQBlock::extract_query: No query found in block");
	}

    if (((*buf_index) + query_len_) < buf_length) {

    	query_val_ = (u_char*) malloc ( sizeof(u_char) * query_len_ );

    	memcpy(query_val_, &(buf[*buf_index]), query_len_);

    	(*buf_index) += query_len_;
    	log_debug("BPQBlock::extract_query: query value = %s", query_val_);

    } else {
        log_err("Error extracting query value");
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BPQBlock::extract_fragments (const u_char* buf, u_int* buf_index, u_int buf_length)
{
	int decoding_len = 0;
	u_int i;
	u_int frag_count = 0;
	u_int frag_off = 0;
	u_int frag_len = 0;

	if (*buf_index < buf_length &&
		(decoding_len = SDNV::decode(	&(buf[*buf_index]),
										buf_length - (*buf_index),
										&frag_count)) >= 0 ) {
		(*buf_index) += decoding_len;
		log_debug("BPQBlock::extract_fragments: number of fragments = %u", frag_count);
    } else {
        log_err("Error decoding number of fragments");
        return BP_FAIL;
	}

	for (i=0; i < frag_count; i++) {

		if (*buf_index < buf_length &&
			(decoding_len = SDNV::decode(	&(buf[*buf_index]),
											buf_length - (*buf_index),
											&frag_off)) >= 0 ) {
			(*buf_index) += decoding_len;
			log_debug("BPQBlock::extract_fragments: fragment [%u] offset = %u", i, frag_off);
	    } else {
	        log_err("Error decoding fragment [%u] offset", i);
	        return BP_FAIL;
		}

		if (*buf_index < buf_length &&
			(decoding_len = SDNV::decode(	&(buf[*buf_index]),
											buf_length - (*buf_index),
											&frag_len)) >= 0 ) {
			(*buf_index) += decoding_len;
			log_debug("BPQBlock::extract_fragments: fragment [%u] length = %u", i, frag_len);
	    } else {
	        log_err("Error decoding fragment [%u] length", i);
	        return BP_FAIL;
		}


		add_fragment(new BPQFragment(frag_off, frag_len));
	}

	return BP_SUCCESS;
}

} // namespace dtn

#endif /* BPQ_ENABLED */
