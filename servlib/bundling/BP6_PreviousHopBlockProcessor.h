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

#ifndef _BP6_PREVIOUS_HOP_BLOCK_PROCESSOR_H_
#define _BP6_PREVIOUS_HOP_BLOCK_PROCESSOR_H_

#include "BP6_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the previous hop bundle block.
 */
class BP6_PreviousHopBlockProcessor : public BP6_BlockProcessor {
public:
    /// Constructor
    BP6_PreviousHopBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int prepare(const Bundle*    bundle,
                SPtr_BlockInfoVec&    xmit_blocks,
                const SPtr_BlockInfo& source,
                const LinkRef&   link,
                list_owner_t     list) override;
    
    int generate(const Bundle*  bundle,
                 SPtr_BlockInfoVec&  xmit_blocks,
                 SPtr_BlockInfo&     block,
                 const LinkRef& link,
                 bool           last) override;
    
    ssize_t consume(Bundle*    bundle,
                SPtr_BlockInfo& block,
                u_char*    buf,
                size_t     len) override;

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL) override;
    /// @}
};

} // namespace dtn

#endif /* _BP6_PREVIOUS_HOP_BLOCK_PROCESSOR_H_ */
