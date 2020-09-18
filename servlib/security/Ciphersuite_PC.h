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

#ifndef _CIPHERSUITE_PC_H_
#define _CIPHERSUITE_PC_H_

#ifdef BSP_ENABLED

#include "gcm/gcm.h"
#include "bundling/Bundle.h"
#include "EVP_PK.h"
#include "BP_Local_CS.h"
#include "Ciphersuite_enc.h"

namespace dtn {

/**
 * Ciphersuite implementation for the payload confidentiality block.
 */
class Ciphersuite_PC : public Ciphersuite_enc {
public:
    Ciphersuite_PC();
    
    virtual u_int16_t cs_num() = 0;
    
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

    virtual int encrypt(
					   std::string       security_dest,
                       const LocalBuffer&       input,
					   LocalBuffer&       result) = 0 ;

    virtual int decrypt(
						std::string   security_dest,
                        const LocalBuffer&   input,
						LocalBuffer&   result) = 0;
    /**
     * Callback for encrypt/decrypt a block. This is normally
     * used for handling the payload contents.
     */
    static bool do_crypt(const Bundle*    bundle,
                         const BlockInfo* caller_block,
                         BlockInfo*       target_block,
                         void*            buf,
                         size_t           len,
                         OpaqueContext*   r);

    static bool should_be_encapsulated(BlockInfo* iter);


    virtual u_int32_t get_key_len() {
        return 128/8;
    };

    const static u_int32_t nonce_len = 12;
    const static u_int32_t salt_len  = 4;
    const static u_int32_t iv_len    = nonce_len - salt_len;
    const static u_int32_t tag_len   = 128/8;
    
    /// @}
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_PC_H_ */
