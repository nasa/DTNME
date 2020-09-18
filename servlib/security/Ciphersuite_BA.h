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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _CIPHERSUITE_BA_H_
#define _CIPHERSUITE_BA_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "KeyDB.h"
#include "BA_BlockProcessor.h"

namespace dtn {

/**
 * Ciphersuite implementation for the bundle authentication block.
 */
class Ciphersuite_BA : public Ciphersuite {
public:
    Ciphersuite_BA();
    
    virtual u_int16_t   cs_num() = 0;
    virtual size_t      result_len()=0;  // length (in bytes) of the HMAC
    
    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason);

    virtual bool validate_security_result(const Bundle*           bundle,
                                          BlockInfoVec*           block_list,
                                          BlockInfo*              block);

    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        BlockInfo::list_owner_t list);
    
    virtual int generate(const Bundle* 	bundle,
                         BlockInfoVec*     xmit_blocks,
                         BlockInfo*    	block,
                         const LinkRef&	link,
                         bool          	last);
    
    virtual int finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link);

    int create_digest(const Bundle *bundle, BlockInfo *block, const BlockInfoVec *recv_blocks, const KeyDB::Entry* key_entry, u_char *result);

    static void digest(const Bundle*    bundle,
                       const BlockInfo* caller_block,
                       const BlockInfo* target_block,
                       const void*      buf,
                       size_t           len,
                       OpaqueContext*   r);
    
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_BA_H_ */
