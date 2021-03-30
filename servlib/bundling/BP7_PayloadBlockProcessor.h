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

#ifndef _BP7_PAYLOAD_BLOCK_PROCESSOR_H_
#define _BP7_PAYLOAD_BLOCK_PROCESSOR_H_

#include "BP7_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP7_PayloadBlockProcessor : public BP7_BlockProcessor {
public:
    /// Constructor
    BP7_PayloadBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                SPtr_BlockInfo& block,
                u_char*    buf,
                size_t     len) override;
    
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
    
    void produce(const Bundle*    bundle,
                 const SPtr_BlockInfo& block,
                 u_char*          buf,
                 size_t           offset,
                 size_t           len) override;

    void process(process_func*    func,
                 const Bundle*    bundle,
                 const SPtr_BlockInfo& caller_block,
                 const SPtr_BlockInfo& target_block,
                 size_t           offset,            
                 size_t           len,
                 OpaqueContext*   context) override;
    
    bool mutate(mutate_func*     func,
                Bundle*          bundle,
                const SPtr_BlockInfo& caller_block,
                SPtr_BlockInfo&       target_block,
                size_t           offset,
                size_t           len,
                OpaqueContext*   context) override;

    int format(oasys::StringBuffer* buf, BlockInfo *b = nullptr) override;
    /// @}
    //

protected:

    int64_t encode_cbor(uint8_t* buf, uint64_t buflen, const Bundle* bundle, uint64_t block_num, uint64_t block_flgas, int64_t& encoded_len);
    int64_t encode_payload_len(uint8_t* buf, uint64_t buflen, uint64_t in_use, size_t payload_len, int64_t& encoded_len);

    int64_t decode_payload_header(SPtr_BlockInfo& block, uint8_t*  buf, size_t buflen, size_t& payload_len);

};

} // namespace dtn

#endif /* _BP7_PAYLOAD_BLOCK_PROCESSOR_H_ */
