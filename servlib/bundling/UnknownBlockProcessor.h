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

#ifndef _UNKNOWN_BLOCK_PROCESSOR_H_
#define _UNKNOWN_BLOCK_PROCESSOR_H_

#include <oasys/util/Singleton.h>

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for any unknown bundle blocks.
 */
class UnknownBlockProcessor : public BlockProcessor,
                              public oasys::Singleton<UnknownBlockProcessor> {
public:
    /// Constructor
    UnknownBlockProcessor();
    
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

    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason);

    using BlockProcessor::format;
    int format(oasys::StringBuffer* buf);
    /// @}
};

} // namespace dtn

#endif /* _UNKNOWN_BLOCK_PROCESSOR_H_ */
