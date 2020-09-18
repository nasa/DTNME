/*
 *    Copyright 2010-2011 Trinity College Dublin
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include <oasys/util/ScratchBuffer.h>

#include "BPQResponse.h"
#include "BundleTimestamp.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/bpq";

//----------------------------------------------------------------------
bool
BPQResponse::create_bpq_response(Bundle*      new_response,
                                 Bundle*      query,
                                 Bundle*      cached_response,
                                 EndpointID&  local_eid)
{
    log_debug_p(LOG, "BPQResponse::create_bpq_response");

	typedef oasys::ScratchBuffer<u_char*, 128> local_buf_t;
	local_buf_t *temp_buf = new local_buf_t();
	size_t data_length;

    // init metadata
    cached_response->copy_metadata(new_response);

    // include info not copied over in prev function
    new_response->set_orig_length(cached_response->orig_length());
    new_response->set_frag_offset(cached_response->frag_offset());

    // set EIDs
    new_response->mutable_source()->assign(local_eid);
    new_response->mutable_dest()->assign(query->replyto());
    new_response->mutable_replyto()->assign(cached_response->replyto());

    // set expiry
    new_response->set_expiration(query->expiration());      // TODO: check this is ok

    // reset creation timestamp (seconds = now, seqno = id)
    BundleTimestamp* ts = new BundleTimestamp(BundleTimestamp::get_current_time(),
                                              new_response->bundleid());
    new_response->set_creation_ts(*ts);

    // set payload
    log_debug_p(LOG, "Copy response payload");
    new_response->mutable_payload()->
        replace_with_file(cached_response->payload().filename().c_str());

    // copy API blocks
    BlockInfoVec* api_blocks = cached_response->api_blocks();

    for (BlockInfoVec::iterator iter = api_blocks->begin();
         iter != api_blocks->end();
         ++iter)
    {
        BlockInfo current_bi = *iter;    
        BlockProcessor* new_bp = BundleProtocol::find_processor(
                                    current_bi.owner()->block_type());

        // take a pointer to the data in the buffer
        // making sure the buffer is big enough
        data_length = current_bi.data_length();
        const u_char* data = current_bi.writable_contents()->
                             buf(current_bi.full_length()) +
                             current_bi.data_offset();

        // Watch for BPQ block and mutate kind PUBLISH to kind RESPONSE
        if (current_bi.type() == BundleProtocol::QUERY_EXTENSION_BLOCK) {
        	temp_buf->reserve(data_length);
        	temp_buf->set_len(data_length);
        	memcpy(temp_buf->buf(), data, data_length);
        	if (*data == BPQBlock::KIND_PUBLISH) {
        		*(temp_buf->buf()) = BPQBlock::KIND_RESPONSE;
        		data = temp_buf->buf();
        	}
        }

        BlockInfo* new_bi = new_response->api_blocks()->append_block(new_bp);
        new_bp->init_block( new_bi,
                            new_response->api_blocks(),
                            NULL, // Already have original source and creation_ts in extension data
                            current_bi.type(),
                            current_bi.flags(),
                            data,
                            data_length );
    }

    // copy RECV blocks
    BlockInfoVec* recv_blocks = cached_response->mutable_recv_blocks();

    int n=0;
    for (BlockInfoVec::iterator iter = recv_blocks->begin();
         iter != recv_blocks->end();
         ++iter)
    {
        BlockInfo current_bi = *iter;       
        log_debug_p(LOG, "RECV Block[%d] is of type: %d", 
                    n++, current_bi.owner()->block_type());

        BlockProcessor* new_bp = BundleProtocol::find_processor(
                                    current_bi.owner()->block_type());

        // take a pointer to the data in the buffer
        // making sure the buffer is big enough
        data_length = current_bi.data_length();
        const u_char* data = current_bi.writable_contents()->
                             buf(current_bi.full_length())
                           + current_bi.data_offset();

        // Watch for BPQ block and mutate kind PUBLISH to kind RESPONSE
        if (current_bi.type() == BundleProtocol::QUERY_EXTENSION_BLOCK) {
        	temp_buf->reserve(data_length);
        	temp_buf->set_len(data_length);
        	memcpy(temp_buf->buf(), data, data_length);
        	if (*data == BPQBlock::KIND_PUBLISH) {
        		*(temp_buf->buf()) = BPQBlock::KIND_RESPONSE;
        		data = temp_buf->buf();
        	}
        }

        BlockInfo* new_bi = new_response->mutable_recv_blocks()->append_block(new_bp);
        new_bp->init_block( new_bi,
                            new_response->mutable_recv_blocks(),
                            NULL, // Already have original source and creation_ts in extension data
                            current_bi.type(),
                            current_bi.flags(),
                            data,
                            data_length );
        new_bi->set_complete(current_bi.complete());
   }
    
   delete temp_buf;
   return true;
}

} // namespace dtn

#endif /* BPQ_ENABLED */

