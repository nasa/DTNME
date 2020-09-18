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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "APIBlockProcessor.h"

#include "BlockInfo.h"
#include "BundleProtocol.h"

namespace oasys {
    template <> dtn::APIBlockProcessor* oasys::Singleton<dtn::APIBlockProcessor>::instance_ = NULL;
}

namespace dtn {

//----------------------------------------------------------------------
APIBlockProcessor::APIBlockProcessor() :
        BlockProcessor(BundleProtocol::API_EXTENSION_BLOCK)
{
}

//----------------------------------------------------------------------
int64_t
APIBlockProcessor::consume(Bundle* bundle,
                           BlockInfo* block,
                           u_char* buf,
                           size_t len)
{
    (void)bundle;
    (void)block;
    (void)buf;
    (void)len;

    return -1;
}

//----------------------------------------------------------------------
int
APIBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    (void)bundle;
    (void)link;
    
    // there must be a corresponding source block created at the API
    const BlockInfo* source = block->source();
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);

    // source block must include at least a block header, if not actual data
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
    
    u_int8_t flags = source->flags();
    if (last) {
        flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    } else {
        flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    }

    generate_preamble(xmit_blocks, block, source->type(), flags, source->data_length());
    ASSERT(block->data_offset() == source->data_offset());
    ASSERT(block->data_length() == source->data_length());
    
    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->full_length());
    memcpy(contents->buf()          + block->data_offset(),
           source->contents().buf() + block->data_offset(),
           block->data_length());
    contents->set_len(block->full_length());
    
    return BP_SUCCESS;
}

} // namespace dtn



