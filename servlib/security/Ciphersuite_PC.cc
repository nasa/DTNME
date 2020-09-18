/*
 *    Copyright 2006-7 SPARTA Inc
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

#include "Ciphersuite_PC.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "openssl/rand.h"
#include "gcm/gcm.h"
#include "security/EVP_PK.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_PC::Ciphersuite_PC()
{
}

//----------------------------------------------------------------------
bool
Ciphersuite_PC::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
	(void)reception_reason;

	//1. do we have security-dest? If yes, get it, otherwise get bundle-dest
	//2. does it match local_eid ??
	//3. if not, return true
	//4. if it does match, parse and validate the block
	//5. the actions must exactly reverse the transforming changes made in finalize()

	Bundle*         deliberate_const_cast_bundle = const_cast<Bundle*>(bundle);
	BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
	EndpointID      local_eid = BundleDaemon::instance()->local_eid();
	size_t          offset;
	size_t          len;
    gcm_ctx_ex      ctx_ex;             // includes OpenSSL context within it
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx_ex);
    LocalBuffer          salt;  
    LocalBuffer          iv; 
    LocalBuffer      encrypted_key;
    u_char          nonce[nonce_len];   // 12 bytes, first 4 are the salt, last 8 are the iv
    LocalBuffer          tag;
    u_char*         key = NULL;
    u_char*         ptr;
    BP_Local_CS*    target_locals = NULL;
    u_int64_t       frag_offset_;       // Offset of fragment in the original bundle
    u_int64_t       orig_length_;       // Length of original bundle
    LocalBuffer      db;

    log_debug_p(log, "Ciphersuite_PC::validate()");
    CS_FAIL_IF_NULL(locals);

    if ( Ciphersuite::destination_is_local_node(bundle, block) )
    {   // yes - this is ours so go to work
    	// we expect this to be the "first" block, and there might or
    	// might not be others. But we should get to this one first and,
    	// during the processing, convert any other C3 blocks to their
    	// unencapsulated form. That is, when this call is over, there
    	// should be no more blocks for us to deal with. Any remaining
    	// C3 block should be for a nested instance

    	// get pieces from params -- salt, iv, range,



    	log_debug_p(log, "Ciphersuite_PC::validate: locals->correlator() 0x%llx", U64FMT(locals->correlator()));
        if(!locals->get_security_param(iv, CS_IV_field)) {
            log_err_p(log, "Ciphersuite_PC::validate: couldn't find iv in block");
            goto fail;
        }
        if(iv_len!=iv.len()) {
            log_err_p(log, "Ciphersuite_PC::validate: iv is the wrong length");
            goto fail;
        }
        if(!locals->get_security_param(salt, CS_salt_field)) {
            log_err_p(log, "Ciphersuite_PC::validate: couldn't find salt in block");
            goto fail;
        }
        if(salt_len != salt.len()){
            log_err_p(log, "Ciphersuite_PC::validate: salt is wrong length");
            goto fail;
        }
        locals->get_fragment_offset(frag_offset_, orig_length_);
        if(!locals->get_security_param(encrypted_key, CS_key_info_field)) {
            log_err_p(log, "Ciphersuite_PC::validate: couldn't find encrypted key in block");
            goto fail;
        }
        log_debug_p(log, "Ciphersuite_PC::validate: key-info: %s", buf2str(encrypted_key).c_str());
    	decrypt(locals->security_dest_host(bundle), encrypted_key, db);
    	if(db.len() != get_key_len()) {
    		goto fail;
    	}
    	key = (u_char *)malloc(get_key_len());
        locals->set_key(db.buf(), get_key_len());
        locals->set_salt(salt.buf(), salt.len());

    	memcpy(key, db.buf(), get_key_len());
    	log_debug_p(log, "Ciphersuite_PC::validate: key: %s", buf2str(key, get_key_len()).c_str());
 
        if(!locals->get_security_result(tag, CS_PC_block_ICV_field)) {
            log_err_p(log, "Ciphersuite_PC::validate: couldn't find icv item");
            goto fail;
        }
        if(tag.len()!= tag_len) {
            log_err_p(log, "Ciphersuite_PC::validate: icv length is wrong");
            goto fail;
        }
    	// prepare context - one time for all usage here
    	gcm_init_and_key(key, get_key_len(), &(ctx_ex.c));
    	ctx_ex.operation = op_decrypt;

    	// we have the necessary pieces from params and result so now
    	// walk all the blocks and do the various processing things needed.
    	// First is to get the iterator to where we are (see note in "generate()"
    	// for why we do this)

    	log_debug_p(log, "Ciphersuite_PC::validate: walk block list");
    	for (BlockInfoVec::iterator iter = block_list->begin();
    			iter != block_list->end();
    			++iter)
    	{
    		// step over all blocks up to and including the one which
    		// prompted this call, pointed at by "block" argument
    		if ( (&*iter) <= block )
    			continue;

    		target_locals = dynamic_cast<BP_Local_CS*>(iter->locals()); //might or might not be valid

    		switch ( iter->type() ) {

    		case BundleProtocol::CONFIDENTIALITY_BLOCK:
    		{
    			log_debug_p(log, "Ciphersuite_PC::validate: PIB block");
    			LocalBuffer    encap_block;
    			CS_FAIL_IF_NULL(target_locals);
    			// even though this isn't our block, the value will have
    			// been set when the block was finished being received
    			// (in Ciphersuite::parse)
    			log_debug_p(log, "Ciphersuite_PC::validate: PIB block owner_cs_num %d", target_locals->owner_cs_num());
    			if ( target_locals->owner_cs_num() != cs_num() )
    				continue;        // only decapsulate C3

    			// it's a C3 block but make sure we own it -- does the
    			// correlator match ??
    			if ( target_locals->correlator() != locals->correlator() )
    				continue;        // not ours

    			// OK - it's ours and we now decapsulate it.
                u_int64_t correlator = locals->correlator();
                if(decapsulate(bundle, &(*iter), locals, false, locals->key().buf(), locals->salt().buf(), correlator) == BP_FAIL) {
                    log_err_p(log, "Ciphersuite_PC::validate: decapsulation failed");
                    goto fail;
                }
    			log_debug_p(log, "Ciphersuite_PC::validate: decapsulation done");
    		}
    		break;

    		case BundleProtocol::PAYLOAD_BLOCK:
    		{
    			log_debug_p(log, "Ciphersuite_PC::validate: PAYLOAD_BLOCK");
    			u_char          tag_calc[tag_len];
    			// nonce is 12 bytes, first 4 are salt (same for all blocks)
    			// and last 8 bytes are per-block IV. The final 4 bytes in
    			// the full block-sized field are, of course, the counter
    			// which is not represented here
    			ptr = nonce;

    			memcpy(ptr, salt.buf(), salt_len);
    			ptr += salt_len;
    			memcpy(ptr, iv.buf(), iv_len);

    			log_debug_p(log, "Ciphersuite_PC::validate: nonce %s", buf2str(nonce, nonce_len).c_str());
    			// prepare context
    			gcm_init_message(nonce, nonce_len, &(ctx_ex.c));

    			offset = iter->data_offset();
    			len = iter->data_length();

    			iter->owner()->mutate( Ciphersuite_PC::do_crypt,
    					deliberate_const_cast_bundle,
    					block,
    					&*iter,
    					offset,
    					len,
    					r );

    			// collect the tag (icv) from the context
    			gcm_compute_tag( tag_calc, tag_len, &(ctx_ex.c) );
    			log_debug_p(log, "Ciphersuite_PC::validate: tag      %s", buf2str(tag).c_str());
    			log_debug_p(log, "Ciphersuite_PC::validate: tag_calc %s", buf2str(tag_calc, tag_len).c_str());
    			if (memcmp(tag.buf(), tag_calc, tag_len) != 0) {
    				log_err_p(log, "Ciphersuite_PC::validate: tag comparison failed");
    				goto fail;
    			}

    		}
    		break;

    		default:
    			continue;

    		}    // end switch
    	}        // end for
    	log_debug_p(log, "Ciphersuite_PC::validate: walk block list done");
    } 

    log_debug_p(log, "Ciphersuite_PC::validate: done");
    if(key != NULL) {
    	free(key);
    }
    return true;

 fail:
    if(key != NULL) {
        free(key);
    }
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;

}

//----------------------------------------------------------------------
int
Ciphersuite_PC::prepare(const Bundle*    bundle,
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
    if(create_bsp_block_from_source(bi, bundle, source,list, cs_num()) == BP_FAIL) {
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

bool Ciphersuite_PC::should_be_encapsulated(BlockInfo* iter) {
 	if (  iter->type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK || iter->type() == BundleProtocol::CONFIDENTIALITY_BLOCK) {
        return true;
	}

    return false;
} 

//----------------------------------------------------------------------
int
Ciphersuite_PC::generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    u_char          		key[get_key_len()]; // use AES-128 or AES-256, depending on the ciphersuite
    u_char          		salt[salt_len];     // GCM "salt" length is 4 bytes
    u_char          		iv[iv_len];         // GCM "iv" length is 8 bytes
    u_int64_t       		correlator = 0LLU;
    BP_Local_CS*    		locals = dynamic_cast<BP_Local_CS*>(block->locals());
    LocalBuffer      		encrypted_key;
    int             		err = 0;

    log_debug_p(log, "Ciphersuite_PC::generate()");

    CS_FAIL_IF_NULL(locals);

    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
        log_debug_p(log, "Ciphersuite_PC::generate: copying block over because it's a received block");
        return Ciphersuite::copy_block_untouched(block, xmit_blocks, last);
    }

    // This block will have a correlator iff there are PSBs or CBs,
    // no correlator if only a payload and no PSBs or CBs
    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
    		iter != xmit_blocks->end();
    		++iter)
    {
    	if ( (&*iter) <= block )
    		continue;
        if (should_be_encapsulated(&*iter)) {
    	    correlator = create_correlator(bundle, xmit_blocks);
    	    correlator |= (int)cs_num() << 16;      // add our ciphersuite number
    	    locals->set_correlator( correlator );
    	    log_debug_p(log, "Ciphersuite_PC::generate() correlator %llx", U64FMT(correlator));
            locals->set_cs_flags(locals->cs_flags() | CS_BLOCK_HAS_CORRELATOR);
            break;
        }

    }

    /* params field will contain
       - salt (4 bytes), plus type and length
       - IV (block-length, 8 bytes), plus type and length
       - encrypted key, plus type and length  
       - fragment offset and length, if a fragment-bundle, plus type and length
       - key-identifier (optional, not implemented yet), plus type and length
     */

    if(!bundle->payload_bek_set()) {
    	// populate salt and IV
    	RAND_bytes(salt, sizeof(salt));
    	RAND_bytes(iv, sizeof(iv));
    	// generate actual key
    	RAND_bytes(key, sizeof(key));
    	const_cast<Bundle *>(bundle)->set_payload_bek(key, get_key_len(), iv, salt);
    } else {
    	memcpy(key, bundle->payload_bek(), sizeof(key));
    	memcpy(salt, bundle->payload_salt(), sizeof(salt));
    	memcpy(iv, bundle->payload_iv(), sizeof(iv));
    }
    // save for finalize()
    locals->set_key(key, sizeof(key));
    locals->set_salt(salt, sizeof(salt));
    locals->set_iv(iv, sizeof(iv));

    log_debug_p(log, "Ciphersuite_PC::generate: salt      %s", buf2str(salt, sizeof(salt)).c_str());
    log_debug_p(log, "Ciphersuite_PC::generate: iv        %s", buf2str(iv, sizeof(iv)).c_str());
    log_debug_p(log, "Ciphersuite_PC::generate: key       %s", buf2str(key, sizeof(key)).c_str());
    log_debug_p(log, "Ciphersuite_PC::generare: logical security_dest %s",locals->security_dest_host(bundle).c_str());

    err = encrypt(locals->security_dest_host(bundle), locals->key(), encrypted_key);

    CS_FAIL_IF(err != 0);
    log_debug_p(log, "Ciphersuite_PC::generate: encrypted_key len = %zu", encrypted_key.len());

    if ( bundle->is_fragment() ) {
    	log_debug_p(log, "Ciphersuite_PC::generate: bundle is fragment");
        locals->add_fragment_param(bundle);
    }
    locals->add_security_param(CS_salt_field, sizeof(salt), salt);
    locals->add_security_param(CS_IV_field, sizeof(iv), iv);
    locals->add_security_param(CS_key_info_field, encrypted_key.len(), encrypted_key.buf());
    locals->add_security_result_space(tag_len);

    log_debug_p(log, "Ciphersuite_PC::generate about to serialize");
    locals->serialize(xmit_blocks, block, last);
    log_debug_p(log, "Ciphersuite_PC::generate() done");


    return BP_SUCCESS;

 fail:
    return BP_FAIL;
}


//----------------------------------------------------------------------
int
Ciphersuite_PC::finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block, 
                         const LinkRef& link)
{
    (void)link;

    Bundle*         deliberate_const_cast_bundle = const_cast<Bundle*>(bundle);
    size_t          offset;
    size_t          len;
    gcm_ctx_ex      ctx_ex;    // includes OpenSSL context within it
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx_ex);
    u_char          key[get_key_len()]; // use AES-128 or AES-256, depending on the ciphersuite
    u_char          nonce[nonce_len];   // 12 bytes, first 4 are the salt, last 8 are the iv
    u_char          tag[tag_len];       // GCM ICV, 16 bytes
    BP_Local_CS*    locals = NULL;
    BP_Local_CS*    target_locals = NULL;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
        
    log_debug_p(log, "Ciphersuite_PC::finalize()");
     

    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    // if this is a received block then we're done
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) {
        log_debug_p(log, "Ciphersuite_PC::finalize returning BP_SUCCESS because this is a received block");
    	return BP_SUCCESS;
    }

    // NEED-BIT-SECURITY-LEVEL
    // prepare context - one time for all usage here
    memcpy(key, locals->key().buf(), get_key_len());
    gcm_init_and_key(key, get_key_len(), &(ctx_ex.c));
    ctx_ex.operation = op_encrypt;
    if(locals->cs_flags() & CS_BLOCK_HAS_DEST) {
        log_debug_p(log, "Ciphersuite::finalize running with destination %s", locals->security_dest().c_str());
    } else {
        log_debug_p(log, "Ciphersuite::finalize running with destination %s", bundle->dest().str().c_str());
    }

    // Walk the list and process each of the blocks.
    // We only change PS, C3 and the payload data,
    // all others are unmodified

    // Note that we can only process PSBs and C3s that follow this block
    // as doing otherwise would mean that there would be a
    // correlator block preceding its parent

    // However this causes a problem if the PS is a two-block scheme,
    // as we'll convert the second, correlated block to C and then
    // the PS processor won't have its second block.

    // There can also be tunnelling issues, depending upon the
    // exact sequencing of blocks. It seems best to add C blocks
    // as early as possible in order to mitigate this problem.
    // That has its own drawbacks unfortunately


    log_debug_p(log, "Ciphersuite_PC::finalize: walk block list");
    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
    		iter != xmit_blocks->end();
    		++iter)
    {
    	// Advance the iterator to our current position.
    	if ( (&*iter) <= block ) {
    		continue;
    	}

        if(should_be_encapsulated(&*iter)) {
    		target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
    		CS_FAIL_IF_NULL(target_locals);
    		log_debug_p(log, "Ciphersuite_PC::finalize: encapsulate this block, len %u eid_ref_count %zu",
    				iter->full_length(), iter->eid_list().size());
            if(encapsulate_subsequent(&(*iter), xmit_blocks, dynamic_cast<BP_Local_CS*>(block->locals()), BundleProtocol::CONFIDENTIALITY_BLOCK, ctx_ex) == BP_FAIL) {
                goto fail;
            }

    

    	} else if(iter->type() == BundleProtocol::PAYLOAD_BLOCK) {
    		u_char*           ptr;
    		if(!bundle->payload_encrypted()) {
    			// prepare context -- key supplied already
    			// nonce is 12 bytes, first 4 are salt (same for all blocks)
    			// and last 8 bytes are per-block IV. The final 4 bytes in
    			// the full block-sized field are, of course, the counter
    			// which is not represented here
    			ptr = nonce;

    			log_debug_p(log, "Ciphersuite_PC::finalize: PAYLOAD_BLOCK");
    			memcpy(ptr, locals->salt().buf(), salt_len);
    			ptr += salt_len;
    			// NEED-BIT-SECURITY-LEVEL
    			memcpy(ptr, locals->iv().buf(), iv_len);

    			// prepare context
    			log_debug_p(log, "Ciphersuite_PC::finalize: nonce    %s", buf2str(nonce,12).c_str());
    			gcm_init_message(nonce, nonce_len, &(ctx_ex.c));

    			offset = iter->data_offset();
    			len = iter->data_length();
    			iter->owner()->mutate( Ciphersuite_PC::do_crypt,
    					deliberate_const_cast_bundle,
    					block,
    					&*iter,
    					offset,
    					len,
    					r);

    			// collect the tag (icv) from the context
    					gcm_compute_tag( tag, tag_len, &(ctx_ex.c) );
    			log_debug_p(log, "Ciphersuite_PC::finalize: tag      %s", buf2str(tag, 16).c_str());

    			const_cast<Bundle *>(bundle)->set_payload_tag(tag);
    			const_cast<Bundle *>(bundle)->set_payload_encrypted();

    		}   else {
    			memcpy(tag, bundle->payload_tag(), tag_len);
    			log_debug_p(log, "Ciphersuite_PC::finalize: tag      %s", buf2str(tag, 16).c_str());
    		}
            locals->add_missing_security_result(block, CS_PC_block_ICV_field, tag_len, tag);
    		log_debug_p(log, "Ciphersuite_PC::finalize: PAYLOAD_BLOCK done");
    	}


    }
    log_debug_p(log, "Ciphersuite_PC::finalize() done");

    return BP_SUCCESS;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
Ciphersuite_PC::do_crypt(const Bundle*    bundle,
                         const BlockInfo* caller_block,
                         BlockInfo*       target_block,
                         void*            buf,
                         size_t           len,
                         OpaqueContext*   r)
{    
    (void) bundle;
    (void) caller_block;
    (void) target_block;
    gcm_ctx_ex* pctx = reinterpret_cast<gcm_ctx_ex*>(r);
    
    log_debug_p(log, "Ciphersuite_PC::do_crypt: operation %hhu len %zu", pctx->operation, len);
    if (pctx->operation == op_encrypt)
        gcm_encrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );
    else    
        gcm_decrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );

    return (len > 0) ? true : false;
}

} // namespace dtn

#endif /* BSP_ENABLED */
