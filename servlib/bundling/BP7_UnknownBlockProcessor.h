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

#ifndef _BP7_UNKNOWN_BLOCK_PROCESSOR_H_
#define _BP7_UNKNOWN_BLOCK_PROCESSOR_H_

#include <third_party/oasys/util/Singleton.h>

#include "BP7_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP7_UnknownBlockProcessor : public BP7_BlockProcessor,
                                  public oasys::Singleton<BP7_UnknownBlockProcessor> {
public:
    /// Constructor
    BP7_UnknownBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                SPtr_BlockInfo& block,
                u_char*    buf,
                size_t     len) override;
    
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
    
    bool validate(const Bundle*           bundle,
                  SPtr_BlockInfoVec&           block_list,
                  SPtr_BlockInfo&              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason) override;
    
    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL) override;
    /// @}
    //

protected:

    int64_t decode_cbor(SPtr_BlockInfo& block, uint8_t*  buf, size_t buflen);

};

} // namespace dtn

#endif /* _BP7_UNKNOWN_BLOCK_PROCESSOR_H_ */
