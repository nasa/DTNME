/*
 *    Copyright 2008 Intel Corporation
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

#include "SequenceIDBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
SequenceIDBlockProcessor::SequenceIDBlockProcessor(u_int8_t block_type)
    : BlockProcessor(block_type)
{
    ASSERT(block_type == BundleProtocol::SEQUENCE_ID_BLOCK ||
           block_type == BundleProtocol::OBSOLETES_ID_BLOCK);
}

//----------------------------------------------------------------------
int
SequenceIDBlockProcessor::prepare(const Bundle*           bundle,
                                  BlockInfoVec*           xmit_blocks,
                                  const BlockInfo*        source,
                                  const LinkRef&          link,
                                  BlockInfo::list_owner_t list)
{
    const SequenceID& sequence_id =
        (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
        bundle->sequence_id() :
        bundle->obsoletes_id();

    if (sequence_id.empty()) {
        return BP_SUCCESS;
    }
    
    BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
SequenceIDBlockProcessor::generate(const Bundle*  bundle,
                                   BlockInfoVec*  xmit_blocks,
                                   BlockInfo*     block,
                                   const LinkRef& link,
                                   bool           last)
{
    (void)link;
    
    const SequenceID& sequence_id =
        (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
        bundle->sequence_id() :
        bundle->obsoletes_id();

    size_t length = 0;

    // add all eids into the reference vector and add up the lengths
    // for the counters / identifiers
    SequenceID::const_iterator iter;
    for (iter = sequence_id.begin(); iter != sequence_id.end(); ++iter)
    {
        block->add_eid(iter->eid_);

        length += 1; // type bit
        
        if (iter->type_ == SequenceID::COUNTER) {
            length += SDNV::encoding_len(iter->counter_);
        } else if (iter->type_ == SequenceID::IDENTIFIER) {
            length += SDNV::encoding_len(iter->identifier_.length());
            length += iter->identifier_.length();
        } else {
            NOTREACHED; // bogus type
        }
    }
    
    generate_preamble(xmit_blocks, 
                      block,
                      block_type(),
                      BundleProtocol::BLOCK_FLAG_REPLICATE |
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    u_int8_t* bp = contents->buf() + block->data_offset();
    for (iter = sequence_id.begin(); iter != sequence_id.end(); ++iter)
    {
        // add the type value by just taking the enum value from the entry
        *bp++ = static_cast<u_int8_t>(iter->type_);
        length--;
        
        if (iter->type_ == SequenceID::COUNTER)
        {
            int len = SDNV::encode(iter->counter_, bp, length);
            ASSERT(len > 0);
            bp     += len;
            length -= len;
        }
        else
        {
            size_t id_len = iter->identifier_.length();
            int sdnv_len = SDNV::encode(id_len, bp, length);
            ASSERT(sdnv_len > 0);
            bp     += sdnv_len;
            length -= sdnv_len;

            ASSERT(length >= id_len);
            memcpy(bp, iter->identifier_.data(), id_len);
            bp     += id_len;
            length -= id_len;
        }
    }

    ASSERT(length == 0);
    
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
SequenceIDBlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                                  u_char* buf, size_t len)
{
    SequenceID* mutable_sequence_id =
        (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
        bundle->mutable_sequence_id() :
        bundle->mutable_obsoletes_id();

    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    size_t count = block->eid_list().size();
    if (count == 0) {
        log_err_p("/dtn/bundle/protocol",
                  "error parsing sequence id block -- no eids in list");
        return -1;
    }

    size_t length = block->data_length();
    u_char* bp    = block->data();

    for (size_t i = 0; i < count; ++i)
    {
        u_int8_t type = *bp++;
        length--;

        if (type == SequenceID::COUNTER)
        {
            u_int64_t counter;
            int sdnv_len = SDNV::decode(bp, length, &counter);
            if (sdnv_len == -1) {
                return -1; // protocol error;
            }

            bp     += sdnv_len;
            length -= sdnv_len;

            log_debug_p("/dtn/bundle/protocol",
                        "parsed %s id entry %s %llu",
                        (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
                        "sequence" : "obsoletes",
                        block->eid_list()[i].c_str(), U64FMT(counter));
            
            mutable_sequence_id->add(block->eid_list()[i], counter);
        }
        else if (type == SequenceID::IDENTIFIER)
        {
            u_int64_t id_len;
            int sdnv_len = SDNV::decode(bp, length, &id_len);
            if (sdnv_len == -1) {
                return -1; // protocol error;
            }

            bp     += sdnv_len;
            length -= sdnv_len;

            std::string id_str(reinterpret_cast<const char*>(bp),
                               static_cast<size_t>(id_len));

            log_debug_p("/dtn/bundle/protocol",
                        "parsed %s id entry %s %s",
                        (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
                        "sequence" : "obsoletes",
                        block->eid_list()[i].c_str(), id_str.c_str());
            
            mutable_sequence_id->add(block->eid_list()[i], id_str);

            bp     += id_len;
            length -= id_len;
        }
        else
        {
            log_warn_p("/dtn/bundle/protocol",
                       "invalid type code %u for %s block entry", type, 
                       (block_type() == BundleProtocol::SEQUENCE_ID_BLOCK) ?
                       "sequence" : "obsoletes");
            return -1;
        }
    }
    
    return cc;
}

} // namespace dtn
