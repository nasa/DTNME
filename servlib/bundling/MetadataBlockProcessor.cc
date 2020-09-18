/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <ctype.h>
#include <inttypes.h>

#include "MetadataBlockProcessor.h"
#include "MetadataBlock.h"
#include "Bundle.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
MetadataBlockProcessor::MetadataBlockProcessor()
    : BlockProcessor(BundleProtocol::METADATA_BLOCK)
{
}

//----------------------------------------------------------------------
int64_t
MetadataBlockProcessor::consume(Bundle*    bundle,
                                BlockInfo* block,
                                u_char*    buf,
                                size_t     len)
{
    ASSERT(bundle != NULL);
    ASSERT(block != NULL);

    int cc = BlockProcessor::consume(bundle, block, buf, len);
 
    if (cc == -1) {
        return -1; // protocol error
    }
 
    if (!block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    parse_metadata(bundle, block);

    return cc;
}

int
MetadataBlockProcessor::reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block)
{
    //static const char* log = "/dtn/bundle/protocol/MetadataBlockProcessor";

    log_debug_p(log, "MetadataBlockProcessor::reload_post_process");
    BlockProcessor::reload_post_process(bundle, block_list, block);
    parse_metadata(bundle, block);
    return(0);
}

//----------------------------------------------------------------------
bool
MetadataBlockProcessor::validate(const Bundle*           bundle,
                                 BlockInfoVec*           block_list,
                                 BlockInfo*              block,
                                 status_report_reason_t* reception_reason,
                                 status_report_reason_t* deletion_reason)
{
    static const char* log = "/dtn/bundle/protocol/MetadataBlockProcessor";
    (void)log;
    
    ASSERT(bundle != NULL);
    ASSERT(block != NULL);
    ASSERT(block->owner() == this);
    ASSERT(block->type() == BundleProtocol::METADATA_BLOCK);

    MetadataBlock *metablock = dynamic_cast<MetadataBlock*>(block->locals());
    ASSERT(metablock != NULL);

    if (metablock->error()) {
        log_debug_p(log, "MetadataBlockProcessor::validate: "
                         "error in metadata block preamble");
        return handle_error(block, reception_reason, deletion_reason);
    }

    // Check for generic block errors.
    if (!BlockProcessor::validate(bundle, block_list, block,
                                  reception_reason, deletion_reason)) {
        metablock->set_block_error();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
int
MetadataBlockProcessor::prepare(const Bundle*    bundle,
                                BlockInfoVec*    xmit_blocks,
                                const BlockInfo* source,
                                const LinkRef&   link,
                                list_owner_t     list)
{
    static const char* log = "/dtn/bundle/protocol";
    (void)log;

    ASSERT(bundle != NULL);
    ASSERT(xmit_blocks != NULL);

    // Do not include metadata unless there is a received source block.
    if (source == NULL) {
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS; 
    }
    
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);
    ASSERT(source->type() == BundleProtocol::METADATA_BLOCK);

    MetadataBlock* source_metadata =
        dynamic_cast<MetadataBlock*>(source->locals());

    // if the source metadata locals is null just return 
    // XXX this indicates a bug in the Ref class or a race elsewhere
    if (source_metadata == NULL) {
        log_debug_p(log, "MetadataBlockProcessor::prepare: "
                         "invalid NULL source metadata");
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS; 
    }

    oasys::ScopeLock metadata_lock(source_metadata->lock(), 
                                   "MetadataBlockProcessor::prepare");

    // Do not include invalid metadata if block flags indicate as such.
    if (source_metadata->error() &&
       (source->flags() & BundleProtocol::BLOCK_FLAG_DISCARD_BLOCK_ONERROR)) {
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS; 
    }

    // Do not include metadata that has been marked for removal.
    if (source_metadata->metadata_removed(link)) {
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS; 
    }

    BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
MetadataBlockProcessor::prepare_generated_metadata(Bundle*        bundle,
                                                   BlockInfoVec*  blocks,
                                                   const LinkRef& link)
{
    ASSERT(bundle != NULL);
    ASSERT(blocks != NULL);
    
    oasys::ScopeLock bundle_lock(bundle->lock(), 
                      "MetadataBlockProcessor::prepare_generated_metadata");

    // Include metadata generated specifically for the outgoing link.
    const MetadataVec* metadata =
        bundle->generated_metadata().find_blocks(link);
    if (metadata != NULL) {
        MetadataVec::const_iterator iter;
        for (iter = metadata->begin(); iter != metadata->end(); ++iter) {
            if ((*iter)->metadata_len() > 0) {
                blocks->push_back(BlockInfo(this));
                blocks->back().set_locals(iter->object());
            }
        }
    }

    // Include metadata generated for all outgoing links.
    LinkRef null_link("MetadataBlockProcessor::prepare_generated_metadata");
    const MetadataVec*
        nulldata = bundle->generated_metadata().find_blocks(null_link);

    if (nulldata != NULL) {
        MetadataVec::const_iterator iter;
        for (iter = nulldata->begin(); iter != nulldata->end(); ++iter) {
            bool link_specific = false;
            if (metadata != NULL) {
                MetadataVec::const_iterator liter = metadata->begin();
                for ( ; liter != metadata->end(); ++liter) {
                    if ((*liter)->source() &&
                       ((*liter)->source_id() == (*iter)->id())) {
                        link_specific = true;
                        break;
                    }
                }
            }

            if (link_specific) {
                continue;
            }
	    
            blocks->push_back(BlockInfo(this));
            blocks->back().set_locals(iter->object());
        }
    }
}

//----------------------------------------------------------------------
int
MetadataBlockProcessor::generate(const Bundle*  bundle,
                                 BlockInfoVec*  xmit_blocks,
                                 BlockInfo*     block,
                                 const LinkRef& link,
                                 bool           last)
{
    (void)xmit_blocks;

    ASSERT(bundle != NULL);
    ASSERT(block != NULL);
    ASSERT(block->owner() == this);

    // Determine if the outgoing metadata block was received in the
    // bundle or newly generated; however, both should not be true.
    MetadataBlock* metadata = NULL;
    bool received_block = false;
    bool generated_block = false;
    
    if (block->source() != NULL) {
        ASSERT(block->source()->owner() == this);
        received_block = true;
        metadata = dynamic_cast<MetadataBlock*>(block->source()->locals());
        ASSERT(metadata != NULL);
    }

    if (block->locals() != NULL) {
        generated_block = true;
        metadata = dynamic_cast<MetadataBlock*>(block->locals());
        ASSERT(metadata != NULL);
    }

    ASSERT(received_block || generated_block);
    ASSERT(!(received_block && generated_block));
    ASSERT(metadata != NULL);
    
    oasys::ScopeLock metadata_lock(metadata->lock(),
                                   "MetadataBlockProcessor::generate");

    if (received_block && metadata->error()) {
        ASSERT((block->source()->flags() &
                BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) == 0);
        ASSERT((block->source()->flags() &
                BundleProtocol::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) == 0);
    }

    // Buffer to the metadata block ontology-specific data.
    u_char* buf = NULL;
    u_int32_t len = 0;

    // Determine if the outgoing metadata block was modified.
    // NOTE: metadata_modified() will set buf and len if it returns true.
    bool modified = (received_block ?
                     metadata->metadata_modified(link, &buf, len) : false);

    // Determine the outgoing metadata block length.
    size_t block_data_len;
    if (received_block && !modified) {
        block_data_len = block->source()->data_length();

    } else {
        if (!modified) {
            buf = metadata->metadata();
            len = metadata->metadata_len();
        }
        
        // If it is modified, len and buf were set by metadata_modified().

        block_data_len = SDNV::encoding_len(metadata->ontology()) +
                         SDNV::encoding_len(len) + len;
    }

    // Determine the preamble flags for the outgoing metadata block.
    u_int8_t flags = 0;
    if (received_block) {
        flags = block->source()->flags();
    } else {
        flags |= metadata->flags();
    }
    if (last) {
        flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    } else {
        flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    }

    // Generate the generic block preamble and reserve
    // buffer space for the block-specific data.
    generate_preamble(xmit_blocks, block, block_type(), flags, block_data_len);
    //block->writable_contents()->reserve(block->data_offset() + block_data_len);
    block->writable_contents()->reserve(block->data_offset() + block_data_len+100);
    block->writable_contents()->set_len(block->data_offset() + block_data_len);

    // Simply copy the incoming metadata to the outgoing buffer
    // if the block was originally received with the bundle and
    // subsequently not modified.
    if (received_block && !modified) {
        memcpy(block->writable_contents()->buf() + block->data_offset(),
               block->source()->contents().buf() + block->data_offset(),
               block->data_length());
        return BP_SUCCESS;
    }

    // Write the metadata block to the outgoing buffer.
    u_char* outgoing_buf   = block->writable_contents()->buf() + 
                             block->data_offset();
    u_int32_t outgoing_len = block_data_len;

    // Write the ontology type.
    size_t sdnv_len = SDNV::encode(metadata->ontology(),
                                   outgoing_buf, outgoing_len);
    ASSERT(sdnv_len > 0);
    outgoing_buf += sdnv_len;
    outgoing_len -= sdnv_len;

    // Write the ontology data length.
    sdnv_len = SDNV::encode(len, outgoing_buf, outgoing_len);
    ASSERT(sdnv_len > 0);
    outgoing_buf += sdnv_len;
    outgoing_len -= sdnv_len;

    // Write the ontology data.
    ASSERT(block->contents().nfree() >= len);
    memcpy(outgoing_buf, buf, outgoing_len);

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
bool
MetadataBlockProcessor::parse_metadata(Bundle* bundle, BlockInfo* block)
{
    static const char* log = "/dtn/bundle/protocol";

    ASSERT(bundle != NULL);
    ASSERT(block != NULL);
    ASSERT(block->owner() == this);
    ASSERT(block->type() == BundleProtocol::METADATA_BLOCK);
    ASSERT(block->complete());
    ASSERT(block->data_offset() > 0);

    // Generate metadata block state that is maintained by
    // the bundle and referenced by the generic block state.
    MetadataBlock* metadata = new MetadataBlock(block);
    bundle->mutable_recv_metadata()->push_back(metadata);

    block->set_locals(metadata);

    // Parse the metadata block.
    u_char *  buf = block->data();
    u_int32_t len = block->data_length();

    // Read the metadata block ontology.
    int ontology_len = 0;
    u_int64_t ontology = 0;
    if ((ontology_len = SDNV::decode(buf, len, &ontology)) < 0) {
        log_err_p(log, "MetadataBlockProcessor::parse_metadata_ontology: "
                       "invalid ontology field length");
        metadata->set_block_error();
        return false;
    }
    buf += ontology_len;
    len -= ontology_len;

    metadata->set_ontology(ontology);

    // XXX/demmer this doesn't seem to conform to the spec... instead
    // the length should be whatever is left in len after the SDNV for
    // the ontology
    
    // Read the metadata block data length.
    int length_len = 0;
    u_int64_t length = 0;
    if ((length_len = SDNV::decode(buf, len, &length)) < 0) {
        log_err_p(log, "MetadataBlockProcessor::parse_metadata_ontology: "
                       "invalid ontology length field length");
        metadata->set_block_error();
        return false;
    }
    buf += length_len;
    len -= length_len;

    if (len != length) {
        log_err_p(log, "MetadataBlockProcessor::parse_metadata_ontology: "
                       "ontology length(%u) fails to match remaining block length(%" PRIu64 ")",
                       len, length);
        metadata->set_block_error();
        return false;
    }

    // Set the offset within the buffer to the metadata block ontology data.
    metadata->set_metadata(buf, len);

    return true;
}

//----------------------------------------------------------------------
bool
MetadataBlockProcessor::handle_error(const BlockInfo*        block,
                                     status_report_reason_t* reception_reason,
                                     status_report_reason_t* deletion_reason)
{
    if (block->flags() & BundleProtocol::BLOCK_FLAG_REPORT_ONERROR) {
        *reception_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
    }

    if (block->flags() & BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) {
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
void
MetadataBlockProcessor::delete_generated_metadata(Bundle*        bundle,
                                                  const LinkRef& link)
{
    ASSERT(bundle != NULL);

    bundle->mutable_generated_metadata()->delete_blocks(link);

    MetadataVec::const_iterator iter;
    for (iter = bundle->recv_metadata().begin();
         iter != bundle->recv_metadata().end(); ++iter)
    {
        (*iter)->delete_outgoing_metadata(link);
    }
}

//----------------------------------------------------------------------
int
MetadataBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *block)
{
	int i;
	u_char* content = NULL;
	int len = 0;

	if (block!=NULL) {
		content = block->data();
		len = block->data_length();
	}

	buf->append("Metadata");

	buf->append(": ");
	for ( i=0; i<10 && i<len; i++ ) {
		buf->appendf("%c", toascii(content[i]));
	}
	return(0);
}

} // namespace dtn
