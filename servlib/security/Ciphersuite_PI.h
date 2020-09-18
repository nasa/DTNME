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

#ifndef _CIPHERSUITE_PI_H_
#define _CIPHERSUITE_PI_H_

#ifdef BSP_ENABLED

#include <oasys/util/ScratchBuffer.h>
#include "bundling/BlockProcessor.h"
#include "PI_BlockProcessor.h"
#include "EVP_PK.h"
#include "Ciphersuite_integ.h"

#include <stdio.h>

namespace dtn {


/**
 * Ciphersuite implementation for the payload integrity block.
 */
class Ciphersuite_PI : public Ciphersuite_integ {
public:
  typedef Ciphersuite::LocalBuffer LocalBuffer;

    Ciphersuite_PI();
    
    virtual u_int16_t cs_num() = 0;
    virtual u_int16_t hash_len();
    
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

    int create_digest(const Bundle* bundle,
                       BlockInfoVec* block_list,
                       BlockInfo*    block,
                       LocalBuffer&   db,
                       int sec_lev);

  private:
    virtual int verify_wrapper(const Bundle*	b,
                               BlockInfoVec* 	block_list,
                               BlockInfo* 		block,
                               const LocalBuffer&			enc_data,
                                               std::string sec_src);

    /**
      * Ciphersuite PI2 needs to know the signature length before generating the signature
      */
    virtual int calculate_signature_length(std::string sec_src,
                                        size_t          digest_len);

    virtual int get_digest_and_sig(const Bundle* 	b,
                 	 	 	 	   BlockInfoVec* 	block_list,
                 	 	 	 	   BlockInfo*       block,
                 	 	 	 	   LocalBuffer&		db_digest,
                 	 	 	 	   LocalBuffer&		db_signed);

    virtual int get_digest_len() {
    	if(hash_len() == 256) {
    		// return 32 (hash output in bytes)
        	return SHA256_DIGEST_LENGTH;
        } else {
        	// return 48 (hash output in bytes)
        	return SHA384_DIGEST_LENGTH;
        }
    };
                             
    
    
    /// @}
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_PI_H_ */
