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

#include "BP6_PreviousHopBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "naming/IPNScheme.h"

namespace dtn {

//----------------------------------------------------------------------
BP6_PreviousHopBlockProcessor::BP6_PreviousHopBlockProcessor()
    : BP6_BlockProcessor(BundleProtocolVersion6::PREVIOUS_HOP_BLOCK)
{
	block_name_ = "BP6_PreviousHop";
}

//----------------------------------------------------------------------
int
BP6_PreviousHopBlockProcessor::prepare(const Bundle*    bundle,
                                   BlockInfoVec*    xmit_blocks,
                                   const SPtr_BlockInfo source,
                                   const LinkRef&   link,
                                   list_owner_t     list)
{
    if (link == nullptr || !link->params().prevhop_hdr_) {
        return BP_SUCCESS;  // no block to include - not a failure
    }
    
    return BP6_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//----------------------------------------------------------------------
int
BP6_PreviousHopBlockProcessor::generate(const Bundle*  bundle,
                                    BlockInfoVec*  xmit_blocks,
                                    BlockInfo*     block,
                                    const LinkRef& link,
                                    bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    // XXX/demmer this is not the right protocol spec'd format since
    // it's supposed to use the dictionary
    

    // if the flag isn't set, we shouldn't have a block
    ASSERT(link->params().prevhop_hdr_);

    BundleDaemon* bd = BundleDaemon::instance();
    size_t length;  //dz debug - adding null char for compatibility with ION (both wrong)
    if (bd->params_.announce_ipn_) {
        length = bd->local_eid_ipn()->length() + 1;
    } else {
        length = bd->local_eid()->length() + 1;
    }
    
    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocolVersion6::PREVIOUS_HOP_BLOCK,
                      BundleProtocolVersion6::BLOCK_FLAG_DISCARD_BLOCK_ONERROR |
                        (last ? BundleProtocolVersion6::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    if (bd->params_.announce_ipn_) {
        memcpy(contents->buf() + block->data_offset(),
               bd->local_eid_ipn()->data(), 3);
        memset(contents->buf() + block->data_offset() + 3, 0, 1);
        memcpy(contents->buf() + block->data_offset() + 4,
               bd->local_eid_ipn()->data() + 4, length-4);
    } else {
        memcpy(contents->buf() + block->data_offset(),
               bd->local_eid()->data(), 3);
        memset(contents->buf() + block->data_offset() + 3, 0, 1);
        memcpy(contents->buf() + block->data_offset() + 4,
               bd->local_eid()->data() + 4, length-4);
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
ssize_t
BP6_PreviousHopBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    ssize_t cc = BP6_BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (ssize_t)len);
        return cc;
    }

    //XXX/dz  TODO: add IPN support to oasys::URI
    if (3 == strlen((const char*) block->data())) {
        bundle->mutable_prevhop() = BD_MAKE_EID_SCHEME((const char*)block->data(), (const char*)block->data()+4);
    } 
    else if (0 == strncmp("ipn:", (const char*)block->data(), 4)) 
    {
        if (strlen((const char*)block->data()) > 4) {
            std::string ssp = (const char*)block->data() + 4;
            bundle->mutable_prevhop() = BD_MAKE_EID_SCHEME("ipn", ssp.c_str());

//            u_int64_t node;
//            u_int64_t service;
//            if (IPNScheme::parse(ssp, &node, &service)) {
//                IPNScheme::format(bundle->mutable_prevhop(), node, service);
//            } else {
//                log_err_p("/dtn/bundle/protocol",
//                          "error parsing previous hop eid IPN scheme: '%.*s",
//                          (int)block->data_length(), block->data());
//                return -1;
//            }
        } else {
            log_err_p("/dtn/bundle/protocol",
                      "error parsing previous hop eid IPN scheme: '%.*s",
                      (int)block->data_length(), block->data());
            return -1;
        }
    }
    else 
    {
        std::string eid_str((char*)block->data(), block->data_length());
        bundle->mutable_prevhop() = BD_MAKE_EID(eid_str);
        if (! bundle->mutable_prevhop()->valid())
        {
            log_err_p("/dtn/bundle/protocol",
                      "error parsing previous hop eid '%.*s",
                      (int)block->data_length(), block->data());
            return -1;
        }
    }
    
    return cc;
}

} // namespace dtn
