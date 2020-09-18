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

#include "Ciphersuite_ES.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "openssl/rand.h"
#include "gcm/gcm.h"
#include "security/EVP_PK.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_ES::Ciphersuite_ES()
{
}


//----------------------------------------------------------------------
bool
Ciphersuite_ES::validate(const Bundle*           bundle,
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

    BP_Local_CS*    locals = NULL;
    BP_LocalRef     localsref("Ciphersuite_ES::validate"); // we need to convince oasys not to garbage collect our locals
    u_char      key[get_key_len()];
    u_char      salt[nonce_len-iv_len];
    u_int64_t        correlator=0;


    log_debug_p(log, "Ciphersuite_ES::validate()");

    for (BlockInfoVec::iterator iter = block_list->begin();
            iter!= block_list->end();
            ++iter) {
        log_debug_p(log, "We have block of type = %d csnum=%d", iter->type(), iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK ? dynamic_cast<BP_Local_CS *>(iter->locals())->owner_cs_num() : 0) ;
    }
    log_debug_p(log, "---------------------");
	
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    localsref = locals;
    CS_FAIL_IF_NULL(locals);

    if ( Ciphersuite::destination_is_local_node(bundle, block) )
    {  //yes - this is ours so go to work
        set<BlockInfo*>to_be_revalidated;
        BSPProtectionInfo bsp;
        bsp.csnum = cs_num();
        bsp.secsrc = locals->security_src_eid(bundle);
        bsp.secdest = locals->security_dest_eid(bundle);
        if(decapsulate(bundle, block, locals, true, key, salt, correlator) == BP_FAIL) {
            log_err_p(log, "Ciphersuite_ES::validate: decapsulation failed");
            goto fail;
        }

        log_debug_p(log, "Ciphersuite_ES::validate: adding %d BSP metadata entries from the encapsulated block to the list for the decapsulated block", block->bsp.size());
        block->bsp.push_back(bsp);
        log_debug_p(log, "Ciphersuite_ES::validate: adding 1 more BSP metadata entry for this BSP instance to the list for the decapsulated block");
        log_debug_p(log, "Ciphersuite_ES::validate: decapsulation done");
        if(block->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
            to_be_revalidated.insert(block);
        }
     
        log_debug_p(log, "Ciphersuite_ES::validate: looking for correlated blocks to decapsulate");
        for(BlockInfoVec::iterator iter=block_list->begin();iter!= block_list->end();iter++) {
            log_debug_p(log, "Ciphersuite_ES::validate: found a block of type %d", iter->type());
            if(iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
                log_debug_p(log, "Ciphersuite_ES::validate has correlator %llu (ours is %llu)", dynamic_cast<BP_Local_CS*>(iter->locals())->correlator(), correlator);
                if(dynamic_cast<BP_Local_CS*>(iter->locals())->correlator() == correlator) {
                    if(decapsulate(bundle, &*iter, locals, false, key, salt, correlator) == BP_FAIL) {
                        goto fail;
                    }
                    if(iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) { // Decapsulation may have changed this!
                        to_be_revalidated.insert(&*iter);
                    }
                    iter->bsp.push_back(bsp);
                }
            }
        }
        BlockProcessor *bp = BundleProtocol::find_processor(block->type());
       if(bp == NULL) {
            log_err_p(log, "Ciphersuite_ES::validate: couldn't find ESB block processor to validate decapsulated ESB block");
            goto fail;
        }
 
        for(set<BlockInfo*>::iterator it=to_be_revalidated.begin(); it!= to_be_revalidated.end();it++) {
            bp->validate(bundle, block_list, *it, reception_reason, deletion_reason);
        }

    }

    log_debug_p(log, "Ciphersuite_ES::validate: done");
    return true;

    fail:
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
Ciphersuite_ES::prepare(const Bundle*    bundle,
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
    
    BlockInfo bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::EXTENSION_SECURITY_BLOCK), source);        // NULL source is OK here
    if(create_bsp_block_from_source(bi, bundle, source, list, cs_num()) == BP_FAIL) {
        goto fail;
    }

    // This is how we will recognize our dummy block.
    dynamic_cast<BP_Local_CS*>(bi.locals())->set_list_owner(BlockInfo::LIST_NONE);
    

    // We should already have the primary block in the list.
    // We'll insert the ESB block after the primary
    // any other security blocks, but before anything else
    // Most importantly, this check is structured to make sure the ESB
    // block comes before any extension blocks which ESB will need to
    // encapsulate
    if ( xmit_blocks->size() > 0 ) {
        BlockInfoVec::iterator iter = xmit_blocks->begin();

        while ( iter != xmit_blocks->end()) {

            switch (iter->type()) {
            case BundleProtocol::PRIMARY_BLOCK:
            case BundleProtocol::PAYLOAD_BLOCK:
            case BundleProtocol::CONFIDENTIALITY_BLOCK:
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
Ciphersuite_ES::generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    LocalBuffer      encrypted_key;

    log_debug_p(log, "Ciphersuite_ES::generate()");

    CS_FAIL_IF_NULL(locals);

    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
        return Ciphersuite::copy_block_untouched(block, xmit_blocks, last);
    }

    locals->serialize(xmit_blocks, block, last);

    return BP_SUCCESS;

    fail:
    return BP_FAIL;
}
bool Ciphersuite_ES::esbable(int type) {
    if( type == BundleProtocol::PRIMARY_BLOCK || 
        type == BundleProtocol::PAYLOAD_BLOCK ||
        type == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK ||
        type == BundleProtocol::CONFIDENTIALITY_BLOCK ||
        type == BundleProtocol::PAYLOAD_SECURITY_BLOCK ||
        type == BundleProtocol::PREVIOUS_HOP_BLOCK ) {
        return false;
    }
    return true;
}

bool Ciphersuite_ES::should_be_encapsulated(BlockInfo* iter) {
    log_debug_p(log, "We have block of type = %d csnum=%d offset = %d len=%d", iter->type(), iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK ? dynamic_cast<BP_Local_CS *>(iter->locals())->owner_cs_num() : 0, iter->data_offset(), iter->writable_contents()->len()) ;

    if(iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK && dynamic_cast<BP_Local_CS*>(iter->locals())->list_owner() == BlockInfo::LIST_NONE) {
        log_debug_p(log, "Skipping placeholder ESB block");
        return false;
    }
    if(!esbable(iter->type())) {
        log_debug_p(log, "Skipping non-ESBable block");
        return false;
    }
    return true;

}

//----------------------------------------------------------------------
int
Ciphersuite_ES::finalize(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     old_ESB_block,
                         const LinkRef& link)
{
    (void) link;
	gcm_ctx_ex      ctx_ex;               // includes OpenSSL context within it
    u_char          key[get_key_len()];   // use AES-128 or AES-256, depending on the ciphersuite
    BP_Local_CS*    locals = NULL;
	EndpointID      local_eid = BundleDaemon::instance()->local_eid();
	LocalBuffer      encrypted_key;
	int             err = 0;
    BP_LocalRef     localsref("Ciphersuite_ES::finalize"); // we need to convince oasys not to garbage collect our locals
    int             number_of_blocks_to_encapsulate=0;
    bool            first_block=true;
    u_char salt[nonce_len-iv_len];

	log_debug_p(log, "Ciphersuite_ES::finalize()");
	locals = dynamic_cast<BP_Local_CS*>(old_ESB_block->locals());
    localsref = locals;
	CS_FAIL_IF_NULL(locals);

	// if this is a received block then we're done
	if ( locals->list_owner() == BlockInfo::LIST_RECEIVED )
		return BP_SUCCESS;

	// delete the ESB block (we'll add a new ESB block for each extension block)
	log_debug_p(log, "Ciphersuite_ES::finalize: walk block list");
	for (BlockInfoVec::iterator iter = xmit_blocks->begin();
			iter != xmit_blocks->end();
			++iter) {
		if (&(*iter) == old_ESB_block) { // iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
			xmit_blocks->erase(iter);
		}
        if(should_be_encapsulated(&(*iter))) {
            number_of_blocks_to_encapsulate++;
        }
	}
    if(number_of_blocks_to_encapsulate > 1) {
    	u_int64_t correlator = create_correlator(bundle, xmit_blocks);
    	correlator |= (int)cs_num() << 16;      // add our ciphersuite number
    	locals->set_correlator( correlator );
    	log_debug_p(log, "Ciphersuite_ES::finalize() correlator %llx", U64FMT(correlator));
        locals->set_cs_flags(locals->cs_flags() | CS_BLOCK_HAS_CORRELATOR);
    }
    RAND_bytes(salt, nonce_len-iv_len);
    locals->set_salt(salt, nonce_len-iv_len);
 
	// generate actual key
	RAND_bytes(key, sizeof(key));


	log_debug_p(log, "Ciphersuite_ES::finalize: key %s", buf2str(key, sizeof(key)).c_str());

	// encrypt the key with RSA
    {
    locals->set_key(key, sizeof(key));
	err = encrypt(locals->security_dest_host(bundle), locals->key(), encrypted_key);
    }
	CS_FAIL_IF(err != 0);

	// prepare context - one time for all usage here
    log_debug_p(log, "Ciphersuite_ES::finalize: calling gcm_init_and_key with key=%s", buf2str(key,get_key_len()).c_str());
	gcm_init_and_key(key, get_key_len(), &(ctx_ex.c));
	ctx_ex.operation = op_encrypt;

    log_debug_p(log, "Ciphersuite_ES::finalize: walk block list");
    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
        iter!= xmit_blocks->end();
        ++iter) {
    log_debug_p(log, "We have block of type = %d offset = %d len = %d csnum=%d", iter->type(), iter->data_offset(), iter->writable_contents()->len(), iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK ? dynamic_cast<BP_Local_CS *>(iter->locals())->owner_cs_num() : 0) ;
    }
    log_debug_p(log, "============================");


    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
            iter != xmit_blocks->end();
            ++iter)
    {
        if(should_be_encapsulated(&(*iter))) {
            log_debug_p(log, "Ciphersuite_ES::finalize: replace this block with ESB block, len %u eid_ref_count %zu",
                    iter->full_length(), iter->eid_list().size());

            if(encapsulate(&(*iter), xmit_blocks, locals, BundleProtocol::EXTENSION_SECURITY_BLOCK, ctx_ex,encrypted_key, first_block, number_of_blocks_to_encapsulate > 1) == BP_FAIL) {
                goto fail;
            }
            first_block=false;  // We're now moving on to subsequent blocks that won't need the key
            // This allows us to distinguish these from placeholder
            // blocks
            dynamic_cast<BP_Local_CS*>(iter->locals())->set_list_owner(BlockInfo::LIST_XMIT);
        }
    }   // end for
	log_debug_p(log, "Ciphersuite_ES::finalize: done");


	return BP_SUCCESS;

	fail:
	return BP_FAIL;
}

} // namespace dtn

#endif /* BSP_ENABLED */
