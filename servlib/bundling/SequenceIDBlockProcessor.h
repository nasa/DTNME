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

#ifndef _SEQUENCE_BLOCK_ID_PROCESSOR_H_
#define _SEQUENCE_BLOCK_ID_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for sequence id blocks and obsoletes
 * blocks since they use the same wire format (with a different block
 * type code).
 */
class SequenceIDBlockProcessor : public BlockProcessor {
public:
    /// Constructor. The sa
    SequenceIDBlockProcessor(u_int8_t block_type);
    
    /// @{ Virtual from BlockProcessor
    int prepare(const Bundle* bundle, BlockInfoVec* xmit_blocks, 
                const BlockInfo* source, const LinkRef& link,
                BlockInfo::list_owner_t list);
    
    int generate(const Bundle* bundle, BlockInfoVec* xmit_blocks,
                 BlockInfo* block, const LinkRef& link, bool last);
    
    int64_t consume(Bundle* bundle, BlockInfo* block, u_char* buf, size_t len);
    /// @}
};

} // namespace dtn

#endif /* _SEQUENCE_BLOCK_ID_PROCESSOR_H_ */
