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

#ifndef _PREVIOUS_HOP_BLOCK_PROCESSOR_H_
#define _PREVIOUS_HOP_BLOCK_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the previous hop bundle block.
 */
class PreviousHopBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    PreviousHopBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const BlockInfo* source,
                const LinkRef&   link,
                list_owner_t     list);
    
    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last);
    
    int64_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len);

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL);
    /// @}
};

} // namespace dtn

#endif /* _PREVIOUS_HOP_BLOCK_PROCESSOR_H_ */
