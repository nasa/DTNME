/*
 *    Copyright 2007 Intel Corporation
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

#include "SessionBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

//----------------------------------------------------------------------
SessionBlockProcessor::SessionBlockProcessor()
    : BlockProcessor(BundleProtocol::SESSION_BLOCK)
{
}

//----------------------------------------------------------------------
int
SessionBlockProcessor::prepare(const Bundle*           bundle,
                               BlockInfoVec*           xmit_blocks,
                               const BlockInfo*        source,
                               const LinkRef&          link,
                               BlockInfo::list_owner_t list)
{
    if (bundle->session_eid().length() == 0) {
        return BP_SUCCESS;
    }
    
    ASSERT(bundle->session_eid() != EndpointID::NULL_EID());
    BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
SessionBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)link;

    ASSERT(bundle->session_eid().length() != 0);

    // add the eid to the dictionary
    block->add_eid(bundle->session_eid());

    size_t length = 1;
    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocol::SESSION_BLOCK,
                      BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |
                      BundleProtocol::BLOCK_FLAG_REPORT_ONERROR |
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);
    u_int8_t* bp = contents->buf() + block->data_offset();
    *bp = bundle->session_flags();

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
SessionBlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                               u_char* buf, size_t len)
{
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if (block->eid_list().size() != 1) {
        log_err_p("/dtn/bundle/protocol",
                  "error parsing session block -- %zu eids in list",
                  block->eid_list().size());
        return -1;
    }

    bundle->mutable_session_eid()->assign(block->eid_list()[0]);
    bundle->set_session_flags(*block->data());

    return cc;
}


} // namespace dtn
