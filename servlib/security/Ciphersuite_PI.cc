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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#define OPENSSL_FIPS    1       /* required for sha256 */

#include "Ciphersuite_PI.h"
#include "Ciphersuite_PC3.h"
#include "Ciphersuite_PC7.h"
#include "Ciphersuite_PC11.h"
#include "Ciphersuite_PI2.h"
#include "Ciphersuite_PI6.h"
#include "Ciphersuite_PI10.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "security/EVP_PK.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";


//----------------------------------------------------------------------
Ciphersuite_PI::Ciphersuite_PI()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_PI::hash_len(void)
{
    return 256;
}

//--------------------------------------------------------------------
int Ciphersuite_PI::verify_wrapper(const Bundle*  b,
								   BlockInfoVec*  block_list,
								   BlockInfo*     block,
								   const LocalBuffer&       enc_data,      // buffer containing the signature
                   std::string   security_source)
{
        LocalBuffer db;
        int err;
        LocalBuffer digest_from_verify;


        err = create_digest(b, block_list, block, db, hash_len());   
        if(err == BP_FAIL) {
            return BP_FAIL;
        }

        // First, verify the RSA signature
        err = EVP_PK::verify(enc_data, digest_from_verify, Ciphersuite::config->get_pub_key_ver(security_source, cs_num()));

        if(err != 0) {
            return BP_FAIL;
        }
        // Now check if the digest from the sig matches what we computed
		// First, check that the lengths are the same
        if(db.len() != digest_from_verify.len()) {
            log_err_p(log, "Ciphersuite_PI::verify_wrapper() digest lengths didn't match "
            		"(we computed %zu but verify returned %zu)", db.len(), digest_from_verify.len());
            return BP_FAIL;
        }
        // Finally, test that the digests match
        if(memcmp(digest_from_verify.buf(), db.buf(), db.len()) != 0) {
            log_err_p(log, "Ciphersuite_PI::verify_wrapper() digests didn't match");
           return BP_FAIL;
        } 
        return BP_SUCCESS;
}

//----------------------------------------------------------------------
int Ciphersuite_PI::calculate_signature_length(string sec_src,
											size_t        	digest_len)
{
    (void) sec_src;
    (void)digest_len;

    // This is only used by Ciphersuite_PI2
    return 1;
}

//----------------------------------------------------------------------
int Ciphersuite_PI::get_digest_and_sig(const Bundle*	b,
									   BlockInfoVec*    block_list,
									   BlockInfo*       block,
									   LocalBuffer&		db_digest,
									   LocalBuffer&		db_signed)
{
    int err;

    // First generate the message digest
    err = create_digest(b, block_list, block, db_digest, hash_len());
    if(err == BP_FAIL) {
        return BP_FAIL;
    }

    // Now compute the RSA signature
    err = EVP_PK::sign(Ciphersuite::config->get_priv_key_sig(dynamic_cast<BP_Local_CS*>(block->locals())->security_src_host(b), cs_num()), db_digest, db_signed);

    if(err != 0) {
        return BP_FAIL;
    }
    return BP_SUCCESS;
}


//----------------------------------------------------------------------
bool
Ciphersuite_PI::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    (void)reception_reason;

    BP_Local_CS*    					locals = NULL;
    u_int16_t       					cs_flags;
    int             					err = 0;
    LocalBuffer      					db;
    LocalBuffer                          sig;

    log_debug_p(log, "Ciphersuite_PI::validate()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();
    
    if ( destination_is_local_node(bundle, block) )
    {  //yes - this is ours so go to work
            
        if ( !(cs_flags & CS_BLOCK_HAS_RESULT) ) {
            log_err_p(log, "Ciphersuite_PI::validate() block has no security_result");
            goto fail;
        }
        
        if(!locals->get_security_result(sig, CS_signature_field)) {
            log_err_p(log, "Ciphersuite_PI::validate() couldn't find signature field in block");
        }
        err = verify_wrapper(bundle, block_list, block, sig, locals->security_src_host(bundle));
        if ( err == BP_FAIL ) {
            log_err_p(log, "Ciphersuite_PI::validate() CS_signature_field validation failed");
            goto fail;
        }

    }
        
    log_debug_p(log, "Ciphersuite_PI::validate() done");
    
    return true;
    
    
    
 fail:    
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI::prepare(const Bundle*    bundle,
                         BlockInfoVec*    xmit_blocks,
                         const BlockInfo* source,
                         const LinkRef&   link,
                         list_owner_t     list)
{
    (void)bundle;
    (void)link;

    // This should never happen.  If it does, we got an API or EXT
    // list block that lists us as sec dest.  Received blocks are
    // handled in the block processor.
    if(Ciphersuite::we_are_block_sec_dest(source)) {
        return BP_SUCCESS; // Don't forward BSP blocks where we're the security dest.
    }

    //Received blocks should be handled by the block processor prepare
    //methods.  We are never called for these.
    ASSERT(list != BlockInfo::LIST_RECEIVED);
    
    BlockInfo bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::CONFIDENTIALITY_BLOCK), source);        // NULL source is OK here
    if(create_bsp_block_from_source(bi, bundle, source, list,cs_num()) == BP_FAIL) {
        goto fail;
    }
    
    // We should already have the primary block in the list.
    // We'll insert this after the primary and any BA blocks
    // and before everything else
    if ( xmit_blocks->size() > 0 ) {
        BlockInfoVec::iterator iter = xmit_blocks->begin();
        
        while ( iter != xmit_blocks->end()) {
            switch (iter->type()) {
            case BundleProtocol::PRIMARY_BLOCK:
            case BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK:
                ++iter;
                continue;
                
            default:
                break;
            }
            xmit_blocks->insert(iter, bi);
            break;
        }
    } else {
        // it's weird if there are no other blocks but, oh well ...
        xmit_blocks->push_back(bi);
    }
    
    return BP_SUCCESS;
    
 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI::generate(const Bundle*  bundle,
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block,
                          const LinkRef& link,
                          bool           last)
{
    (void)link;
    (void)xmit_blocks;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    LocalBuffer      encrypted_key;
    size_t          digest_len;
    size_t sig_len;
        
    log_debug_p(log, "Ciphersuite_PI::generate() %p", block);
    CS_FAIL_IF_NULL(locals);
    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
        return Ciphersuite::copy_block_untouched(block, xmit_blocks, last);
    }    /**************  forwarding done  **************/
    
    
    if ( bundle->is_fragment() ) {
        locals->add_fragment_param(bundle);
        log_debug_p(log, "Ciphersuite_PI::generate() bundle is fragment");
    }
   
    digest_len = get_digest_len();

    sig_len = calculate_signature_length(locals->security_src_host(bundle), digest_len);
    locals->add_security_result_space(sig_len); 
    locals->serialize(xmit_blocks, block, last);

    return BP_SUCCESS;

 fail:
    return BP_FAIL;
}

   
 

//----------------------------------------------------------------------
int
Ciphersuite_PI::finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link)
{
    (void)link;

    BP_Local_CS*    					locals = NULL;
    LocalBuffer      					db_digest;
    LocalBuffer 							db_signed;
        
    log_debug_p(log, "Ciphersuite_PI::finalize()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
        
    // if this is a received block then we're done
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
        return BP_SUCCESS;
  
    if(BP_FAIL== get_digest_and_sig(bundle, xmit_blocks, block, db_digest, db_signed)) {
        goto fail;
    }
    locals->reset_security_result();
    locals->add_security_result(CS_signature_field, db_signed.len(), db_signed.buf());
    // It's unfortunate we have to do this twice, but everything may
    // move when we change the security result length and thus the
    // main block length.
    locals->serialize(xmit_blocks, block, block->last_block());
    log_debug_p(log, "Ciphersuite_PI::finalize() done");
    
    return BP_SUCCESS;

 fail:
    log_err_p(log, "Ciphersuite_PI::finalize() failed");
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI::create_digest(const Bundle*  bundle, 
                               BlockInfoVec*  block_list,
                               BlockInfo*     block,
                               LocalBuffer&    db,
                               int hash_length = 256)
{
    EVP_MD_CTX      ctx;
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx);
    char*           dict;
    size_t          digest_len;
    u_char          ps_digest[EVP_MAX_MD_SIZE];
    u_int32_t       rlen = 0;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    BlockInfoVec::iterator iter;
    int             err = 0;
        
    log_debug_p(log, "Ciphersuite_PI::create_digest()");
        
    // prepare context 
    EVP_MD_CTX_init(&ctx);
    if (hash_length == 384) {
        err = EVP_DigestInit_ex(&ctx, EVP_sha384(), NULL);
    } else {
		// SHA-256 is the default hash function
        err = EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL);
    }
    if(err == 0) {
        log_err_p(log, "Ciphersuite_PI::create_digest: Error initing sha digest");
        return BP_FAIL;
    }
    digest_len = EVP_MD_CTX_size(&ctx);
    // XXX-pl  check error -- zero is failure
        
    // Walk the list and process each of the blocks.
    // We only digest PS, C3 and the payload data,
    // all others are ignored
    
    // Note that we can only process PSBs and C3s that follow this block
    // as doing otherwise would mean that there would be a
    // correlator block preceding its parent
    
    // There can also be tunnelling issues, depending upon the
    // exact sequencing of blocks. It seems best to add C blocks
    // as early as possible in order to mitigate this problem.
    // That has its own drawbacks unfortunately
    
    iter = block_list->begin();     //primary
    
    mutable_canonicalization_primary(bundle, block, &*iter, r,&dict);
    ++iter;     //primary is done now
    
    log_debug_p(log, "Ciphersuite_PI::create_digest() walk block list");
    for ( ;
          iter != block_list->end();
          ++iter)
    {
        // Advance the iterator to our current position.
        if(&*iter <= block) {
            continue;
        }


        switch ( iter->type() ) {
        case BundleProtocol::PAYLOAD_SECURITY_BLOCK:
        case BundleProtocol::CONFIDENTIALITY_BLOCK:
        {
                    
            log_debug_p(log, "Ciphersuite_PI::create_digest() digest this block, len %u eid_list().size() %zu", 
                        iter->full_length(), iter->eid_list().size());
            if(mutable_canonicalization_extension(bundle, block, &*iter, r,dict) == BP_FAIL) {
                log_err_p(log, "Ciphersuite_PI::create_digest: failed to digest extension block");
                return BP_FAIL;
            }
        }
        break;  //break from switch, continue for "for" loop
            
        case BundleProtocol::PAYLOAD_BLOCK:
        {
            if(mutable_canonicalization_payload(bundle, block, &*iter, r,dict) == BP_FAIL) {
                log_err_p(log, "Ciphersuite_PI::create_digest: failed to digest payload");
                return BP_FAIL;
            }
        }
        break;  //break from switch, continue for "for" loop
                
        default:
            continue;
        
        }   // end of switch  
    }       // end of loop-through-all-the-blocks
    
    
    err = EVP_DigestFinal_ex(&ctx, ps_digest, &rlen);
    if(err == 0) {
        log_err_p(log, "Ciphersuite_PI::create_digest: failed to finalize digest");
        return BP_FAIL;
    }
    
    log_debug_p(log, "Ciphersuite_PI::create_digest() digest      %s", buf2str(ps_digest, 20).c_str());

    EVP_MD_CTX_cleanup(&ctx);
    
    db.reserve(digest_len);
    db.set_len(digest_len);
    memcpy(db.buf(), ps_digest, digest_len);
    log_debug_p(log, "Ciphersuite_PI::create_digest() done");
    
    return BP_SUCCESS;
}


//----------------------------------------------------------------------

} // namespace dtn

#endif /* BSP_ENABLED */
