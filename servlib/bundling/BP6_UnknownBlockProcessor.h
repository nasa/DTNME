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

#ifndef _BP6_UNKNOWN_BLOCK_PROCESSOR_H_
#define _BP6_UNKNOWN_BLOCK_PROCESSOR_H_

#include <third_party/oasys/util/Singleton.h>

#include "BP6_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for any unknown bundle blocks.
 */
class BP6_UnknownBlockProcessor : public BP6_BlockProcessor,
                              public oasys::Singleton<BP6_UnknownBlockProcessor> {
public:
    /// Constructor
    BP6_UnknownBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const SPtr_BlockInfo source,
                const LinkRef&   link,
                list_owner_t     list) override;
    
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

    /// @}
};

} // namespace dtn

#endif /* _BP6_UNKNOWN_BLOCK_PROCESSOR_H_ */
