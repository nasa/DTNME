/*
 *    Copyright 2006 SPARTA Inc
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

#ifndef _AS_BLOCK_PROCESSOR_H_
#define _AS_BLOCK_PROCESSOR_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "Ciphersuite.h"
#include "BP_Local_CS.h"

namespace dtn {

/**
 * Block processor implementation for Abstract Security blocks (this
 * is just boilerplate for the other security block processors).
 */
class AS_BlockProcessor : public BlockProcessor {
public:
    /// Constructor
    AS_BlockProcessor(int block_type);

    virtual int format(oasys::StringBuffer* buf, BlockInfo *block);
    
    /// @{ Virtual from BlockProcessor
    virtual int consume(Bundle*    bundle,
                        BlockInfo* block,
                        u_char*    buf,
                        size_t     len);

    virtual int reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block);

    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason);

    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list);
    
    virtual int generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last);
    
    virtual int finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link);
    /// @}
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _AS_BLOCK_PROCESSOR_H_ */
