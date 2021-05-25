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

#ifndef _BP6_PAYLOAD_BLOCK_PROCESSOR_H_
#define _BP6_PAYLOAD_BLOCK_PROCESSOR_H_

#include "BP6_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP6_PayloadBlockProcessor : public BP6_BlockProcessor {
public:
    /// Constructor
    BP6_PayloadBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len) override;
    
    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last) override;
    
    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason) override;
    
    void produce(const Bundle*    bundle,
                 const BlockInfo* block,
                 u_char*          buf,
                 size_t           offset,
                 size_t           len) override;

    void process(process_func*    func,
                 const Bundle*    bundle,
                 const BlockInfo* caller_block,
                 const BlockInfo* target_block,
                 size_t           offset,            
                 size_t           len,
                 OpaqueContext*   context) override;
    
    bool mutate(mutate_func*     func,
                Bundle*          bundle,
                const BlockInfo* caller_block,
                BlockInfo*       target_block,
                size_t           offset,
                size_t           len,
                OpaqueContext*   context) override;

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL) override;
    /// @}
};

} // namespace dtn

#endif /* _BP6_PAYLOAD_BLOCK_PROCESSOR_H_ */
