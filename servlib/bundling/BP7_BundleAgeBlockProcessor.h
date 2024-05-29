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

#ifndef _BP7_BUNDLE_AGE_BLOCK_PROCESSOR_H_
#define _BP7_BUNDLE_AGE_BLOCK_PROCESSOR_H_

#include <third_party/oasys/util/Singleton.h>

#include "BP7_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP7_BundleAgeBlockProcessor : public BP7_BlockProcessor {
public:
    /// Constructor
    BP7_BundleAgeBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len) override;
    
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
    
    /// @}
    //

protected:

    int64_t encode_block_data(uint8_t* buf, uint64_t buflen, uint64_t age,
                              const Bundle* bundle, int64_t& encoded_len);

    int64_t decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle);

};

} // namespace dtn

#endif /* _BP7_BUNDLE_AGE_BLOCK_PROCESSOR_H_ */
