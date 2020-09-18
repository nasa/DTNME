
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite_enc.h"
#include "bundling/SDNV.h"
#include "openssl/rand.h"
#include "BP_Local_CS.h"
namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

// This actually encrypts the block in place, so don't expect to use
// the block itself again.
int Ciphersuite_enc::encrypt_contents(u_char *salt, u_char *iv, gcm_ctx_ex ctx_ex, BlockInfo *block, LocalBuffer &store_result_here, u_char *tag) {
    u_char nonce[nonce_len];
    u_char *ptr=nonce;

    memcpy(ptr, salt, nonce_len-iv_len);
    ptr+=nonce_len-iv_len;
    memcpy(ptr, iv, iv_len);

    gcm_init_message(nonce, nonce_len, &(ctx_ex.c));
        gcm_encrypt_message(nonce,
                nonce_len,
                NULL,
                0,
                block->writable_contents()->buf(),
                block->full_length(),
                tag,
                tag_len,
                &(ctx_ex.c));
    log_debug_p(log, "Ciphersuite_enc::encapsulate: gcm_encrypt_message returned, encap_block=%s tag=%s",  buf2str(block->contents()).c_str(), buf2str(tag,tag_len).c_str());

        log_debug_p(log, "Ciphersuite_enc::encapsulate: body is encrypted");

        // copy encrypted block before it gets overwritten
        store_result_here.reserve(block->full_length());
        store_result_here.set_len(block->full_length());
        ptr = store_result_here.buf();
        memcpy(ptr, block->contents().buf(), block->full_length());

        log_debug_p(log, "Ciphersuite_enc::encapsulate: copied body");

        return BP_SUCCESS;
}

int Ciphersuite_enc::add_sec_src_dest_to_eid_refs(BlockInfo *iter, string sec_src, string sec_dest, u_int16_t *cs_flags) {
    EndpointIDVector::const_iterator it;
    log_debug_p(log, "Ciphersuite_enc::encapsulate, security_src()=%s", sec_src.c_str());
    log_debug_p(log, "Ciphersuite_enc::encapsulate, security_dest()=%s", sec_dest.c_str());
    EndpointIDVector temp;
    if(sec_src.length() > 0) {
        temp.push_back(sec_src);
        *cs_flags |= CS_BLOCK_HAS_SOURCE;
    }
    if(sec_dest.length() > 0) {
        temp.push_back(sec_dest);
        *cs_flags |= CS_BLOCK_HAS_DEST;
    }
    for(it=iter->eid_list().begin();it!=iter->eid_list().end();it++) {
        log_debug_p(log, "Ciphersuite_enc::encapsulate: have encapsulated EID %s", it->str().c_str());
        temp.push_back(*it);
    }
    iter->set_eid_list(temp); 
    return BP_SUCCESS;
}

int Ciphersuite_enc::encapsulate_subsequent(BlockInfo *iter, BlockInfoVec *xmit_blocks, BP_Local_CS *src_locals, int block_type, gcm_ctx_ex ctx_ex) {
    return encapsulate(iter, xmit_blocks, src_locals, block_type, ctx_ex, LocalBuffer(), false, true);
}

int Ciphersuite_enc::encapsulate(BlockInfo *iter, BlockInfoVec *xmit_blocks, BP_Local_CS *src_locals, int block_type, gcm_ctx_ex ctx_ex, LocalBuffer encrypted_key, bool first_block, bool use_correlator) {
    LocalBuffer encap_block;
    u_int16_t       cs_flags;
    u_char iv[iv_len];
    u_char tag[tag_len];
    bool last = iter->last_block();
    BP_Local_CS *new_target_locals = new BP_Local_CS(); 

    RAND_bytes(iv, iv_len);
    encrypt_contents(src_locals->salt().buf(), iv, ctx_ex, iter, encap_block, tag);
    new_target_locals->set_owner_cs_num(src_locals->owner_cs_num());
    cs_flags = 0;
    if(use_correlator) {
        cs_flags |= CS_BLOCK_HAS_CORRELATOR;
        new_target_locals->set_correlator(src_locals->correlator());
    } 
    if(first_block) {
        add_sec_src_dest_to_eid_refs(&*iter, src_locals->security_src(), src_locals->security_dest(), &cs_flags);
    }
    new_target_locals->set_cs_flags(cs_flags);

    iter->set_owner(BundleProtocol::find_processor(block_type));            // "steal this block"

    log_debug_p(log, "Ciphersuite_enc::encapsulate: owner()->block_type() %u buf()[0] %hhu",
            iter->owner()->block_type(), iter->contents().buf()[0]);

    new_target_locals->add_security_param(CS_IV_field, iv_len, iv);
    if(first_block) {
        new_target_locals->add_security_param(CS_salt_field, nonce_len -iv_len, src_locals->salt().buf());
        new_target_locals->add_security_param(CS_key_info_field, encrypted_key.len(), encrypted_key.buf());
    }

    log_debug_p(log, "Ciphersuite_enc::encapsulate encapsulated block encrypted data len=%d tag_len=%d", encap_block.len(), tag_len);
    new_target_locals->add_encapsulated_block(encap_block.len(), encap_block.buf(), tag_len, tag);

    iter->set_locals(new_target_locals);    //will also decrement ref for old one
    log_debug_p(log, "Ciphersuite_enc::encapsulate set locals");
    new_target_locals->serialize(xmit_blocks, &(*iter), last);
    log_debug_p(log, "Ciphersuite_enc:encapsulate encapsulation done");
    return BP_SUCCESS;
}

// First arg is the decrypted block as a buffer.  The second is the
// block we are recreating into (the formerly encrypted block).
// The third argument is the bundle (we need this so we can call
// consume). 
// The fourth argument is the number of eid refs in the outer eid ref
// list that belong to the encapsulating block, not the encapsulated
// block
int Ciphersuite_enc::recreate_block(LocalBuffer encap_block, BlockInfo* iter, const Bundle *bundle, int skip_eid_refs) {

    Bundle*         deliberate_const_cast_bundle = const_cast<Bundle*>(bundle);
    size_t sdnv_len;
    u_char *data;
    size_t len;
    int32_t         rem;                // use signed value
    int cc;
    size_t preamble_size;
    u_char *ptr;
    LocalBuffer preamble;

    // encap_block is the raw data of the encapsulated block
    // and now we have to reconstitute it the way it used to be :)

    // Parse the content as would be done for a newly-received block
    // using the owner's consume() method

    // We need to stitch up the EID lists as the list in the block is broken.
    // The way to do this is to create a slightly-synthetic preamble
    // with the appropriate eid-offsets in it. The pre-existing list has been
    // preserved and carried along. But the offsets contained in the preamble
    // refer to an outdated image of the dictionary. So we copy the offsets
    // from the *current* block into the synthetic preamble.
    // The list will then have the correct pointers into the dictionary,
    // as those will have been updated at all the intermediate nodes.
    // The remainder of the preamble comes from the encapsulated block.

    data = encap_block.buf();
    len = encap_block.len();

    BlockInfo info(BundleProtocol::find_processor(*data));
    u_int64_t eid_ref_count = 0LLU;
    u_int64_t current_eid_count;
    u_int64_t flags;
    u_int64_t content_length = 0LLU;

    preamble.reserve(iter->full_length());    //can't be bigger
    // do set_len() later

    // copy bits and pieces from the decrypted block
    ptr = preamble.buf();
    rem = iter->full_length();

    *ptr++ = *data++;                // block type
    rem--;
    len--;
    sdnv_len = SDNV::decode(data, len, &flags);        // block processing flags (SDNV)
    data += sdnv_len;
    len -= sdnv_len;
    log_debug_p(log, "Ciphersuite_enc::decapsulate: target block type %hhu flags 0x%llx", *(preamble.buf()), U64FMT(flags));

    // EID list is next, starting with the count
    if  ( flags & BundleProtocol::BLOCK_FLAG_EID_REFS ) {
        sdnv_len = SDNV::decode(data, len, &eid_ref_count);
        data += sdnv_len;
        len -= sdnv_len;

        current_eid_count = iter->eid_list().size();

        if ( eid_ref_count+skip_eid_refs != current_eid_count ) {
            log_err_p(log, "Ciphersuite_PC::validate: eid_ref_count %lld  != current_eid_count %lld",
                    U64FMT(eid_ref_count), U64FMT(current_eid_count));
            goto fail;        // block is broken somehow
        }
    }

    // each ref is a pair of SDNVs, so step over 2 * eid_ref_count
    if ( eid_ref_count > 0 ) {
        for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
            sdnv_len = SDNV::len(data);
            data += sdnv_len;
            len -= sdnv_len;
        }
    }        // now we're positioned after the broken refs, if any

    sdnv_len = SDNV::decode(data, len, &content_length);
    data += sdnv_len;
    len -= sdnv_len;
    log_debug_p(log, "Ciphersuite_enc::decapsulate: target data content size %llu", U64FMT(content_length));

    // fix up last-block flag
    // this probably isn't the last block, but who knows ? :)
    if ( iter->flags() & BundleProtocol::BLOCK_FLAG_LAST_BLOCK )
        flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    else
        flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;

    // put flags into the adjusted block
    sdnv_len = SDNV::encode(flags, ptr, rem);
    ptr += sdnv_len;
    rem -= sdnv_len;

    // copy the offsets from the current block
    if ( eid_ref_count > 0 ) {
        u_char*        cur_ptr = iter->contents().buf();
        size_t        cur_len = iter->full_length();

        cur_ptr++;    //type field
        cur_len--;
        sdnv_len = SDNV::len(cur_ptr);    //flags
        cur_ptr += sdnv_len;
        cur_len -= sdnv_len;

        sdnv_len = SDNV::len(cur_ptr);    //eid ref count
        cur_ptr += sdnv_len;
        cur_len -= sdnv_len;

        // put eid_count into the adjusted block
        log_debug_p(log, "Ciphersuite_enc::decapsulate: eid_ref_count %lld", U64FMT(eid_ref_count));
        sdnv_len = SDNV::encode(eid_ref_count, ptr, rem);
        ptr += sdnv_len;
        rem -= sdnv_len;


       for(int i=0;i<2*skip_eid_refs;i++) {
           uint32_t foo;
           SDNV::decode(cur_ptr, cur_len, &foo);
            log_debug_p(log, "Ciphersuite_ES::validate: skipping over sdnv value=%d", foo);
            sdnv_len = SDNV::len(cur_ptr);
            cur_ptr += sdnv_len;
            cur_len -= sdnv_len;
        }

        // now copy the reference pairs

        for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
            uint32_t foo;
           SDNV::decode(cur_ptr, cur_len, &foo);
            log_debug_p(log, "Ciphersuite_ES::validate: copying sdnv value=%d", foo);
            sdnv_len = SDNV::len(cur_ptr);
            memcpy(ptr, cur_ptr, sdnv_len);
            cur_ptr += sdnv_len;
            cur_len -= sdnv_len;
            ptr += sdnv_len;
            rem -= sdnv_len;
        }
    }

    // length of data content in block
    sdnv_len = SDNV::encode(content_length, ptr, rem);
    ptr += sdnv_len;
    rem -= sdnv_len;

    // we now have a preamble in "preamble" and the rest of the data at *data
    preamble_size = ptr - preamble.buf();
    preamble.set_len(preamble_size);
    log_debug_p(log, "Ciphersuite_enc::decapsulate: target preamble_size %zu", preamble_size);

    {
        // we're reusing the existing BlockInfo but we need to clean it first
        oasys::SerializableVector<BSPProtectionInfo> bsp(iter->bsp);
        iter->~BlockInfo();
        /* we'd like to reinitilize the block thusly
         *      iter->BlockInfo(type);
         * but C++ gets bent so we have to achieve the desired result
         * in a more devious fashion using placement-new.
         */

        log_debug_p(log, "Ciphersuite_enc::decapsulate: re-init target");
        BlockInfo* bp = &*iter;
        bp = new (bp) BlockInfo(BundleProtocol::find_processor(*(preamble.buf())));
        bp->bsp = bsp;
        log_debug_p(log, "Ciphersuite_enc::decapsulate: copied %d bsp metadata entries to newly decapsulated block", bsp.size());
        CS_FAIL_IF_NULL(bp);
    }

    // process preamble
    log_debug_p(log, "Ciphersuite_enc::decapsulate: process target preamble");
    cc = iter->owner()->consume(deliberate_const_cast_bundle, &*iter, preamble.buf(), preamble_size);
    if (cc < 0) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: consume failed handling encapsulated preamble 0x%x, cc = %d",
                info.type(), cc);
        goto fail;
    }

    // process the main part of the encapsulated block
    log_debug_p(log, "Ciphersuite_enc::decapsulate: process target content");
    cc = iter->owner()->consume(deliberate_const_cast_bundle, &*iter, data, len);
    if (cc < 0) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: consume failed handling encapsulated block 0x%x, cc = %d",
                info.type(), cc);
        goto fail;
    }
    log_debug_p(log, "Ciphersuite_enc::decapsulate: decapsulation done");
    if(iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
    log_debug_p(log, "As a sanity check, dynamic_cast<BP_Local_CS*>iter->locals()->security_src().c_str()=%s", dynamic_cast<BP_Local_CS*>(iter->locals())->security_src().c_str());
    log_debug_p(log, "As asanity check, dynamic_cast<BP_Local_CS*>iter->locals()->security_dest().c_str()=%s", dynamic_cast<BP_Local_CS*>(iter->locals())->security_dest().c_str());
    }
    return BP_SUCCESS;
fail:
    return BP_FAIL;
}


int Ciphersuite_enc::decapsulate(const Bundle *bundle, BlockInfo* iter, BP_Local_CS* locals, bool first_block, u_char *key, u_char *salt, u_int64_t &correlator) {
    BP_Local_CS *target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
    u_char tag_encap[tag_len];
    u_char nonce[nonce_len];
    LocalBuffer target_iv;
    LocalBuffer saltlb;
    LocalBuffer encrypted_key;
    LocalBuffer      db;
    LocalBuffer      encap_block;        // Holds the encapsulated (encrypted) extension block
    u_char *ptr;
    gcm_ctx_ex ctx_ex;
    ret_type        ret = 0;
    u_int16_t       cs_flags;
    int toskip;

   



    CS_FAIL_IF_NULL(target_locals);

    if(!target_locals->get_security_param(target_iv, CS_IV_field)) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: iv not found in block");
        goto fail;
    } 
    if(target_iv.len()!= iv_len) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: iv was wrong length");
        goto fail;
    }
    if(first_block) {
        if(!target_locals->get_security_param(saltlb, CS_salt_field)) {
            log_err_p(log, "Ciphersuite_enc::decapsulate: salt not found in block");
            goto fail;
        } 
        if(saltlb.len()!= salt_len) {
            log_err_p(log, "Ciphersuite_enc::decapsulate: salt was wrong length");
            goto fail;
        }
        memcpy(salt, saltlb.buf(), salt_len);
        if(!target_locals->get_security_param(encrypted_key, CS_key_info_field)) {
            log_err_p(log, "Ciphersuite_enc::decapsulate: key-info not found in block");
        }
        log_debug_p(log, "Ciphersuite_enc::decapsulate: key-info: %s", buf2str(encrypted_key).c_str());
        // decrypt the encrypted Bundle-Encryption Key (BEK)
        if(decrypt(locals->security_dest_host(bundle), encrypted_key, db)!=0) {
            log_err_p(log, "Ciphersuite_enc::decapsulate: failed to decrypt the symmetric key");
            goto fail;
        }
        memcpy(key, db.buf(), get_key_len());
		log_debug_p(log, "Ciphersuite_enc::decapsulate: key: %s", buf2str(key, get_key_len()).c_str());
        correlator = target_locals->correlator();
        log_debug_p(log, "Ciphersuite_enc::decapsulate: found correlator of %llu", correlator);
    }

    if(!target_locals->get_security_result(encap_block, CS_encap_block_field)) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: didn't find ecapsulated block in security result");
        goto fail;
    }
    memcpy(tag_encap, encap_block.buf()+encap_block.len()-tag_len, tag_len);
    encap_block.set_len(encap_block.len()-tag_len);
    log_debug_p(log, "Ciphersuite_enc::decapsulate: tag: %s", buf2str(tag_encap, tag_len).c_str());
    // nonce is 12 bytes, first 4 are salt (same for all blocks)
    // and last 8 bytes are per-block IV. The final 4 bytes in
    // the full block-sized field are, of course, the counter
    // which is not represented here
    //ptr = nonce;
 
    // We need to get the salt from locals->salt() if this is
    // PCB, and the appropriate field if it's ESB.
    ptr = nonce;
    memcpy(ptr, salt, salt_len);
    ptr += salt_len;
    memcpy(ptr, target_iv.buf(), iv_len);
    log_debug_p(log, "Ciphersuite_enc::decapsulate: combined salt and iv into nonce");

    // prepare context
    log_debug_p(log, "Ciphersuite_enc::decapsulate: calling gcm_init_and_key with key=%s", buf2str(key, get_key_len()).c_str());
    gcm_init_and_key(key, get_key_len(), &(ctx_ex.c));
    log_debug_p(log, "Ciphersuite_enc::decapsulate: prepared context");

    // decrypt message
    log_debug_p(log, "Ciphersuite_enc::decapsulate: calling gcm_Decrypt_message with nonce=%s, encap_block=%s, tag=%s", buf2str(nonce, nonce_len).c_str(), buf2str(encap_block).c_str(), buf2str(tag_encap, tag_len).c_str());
    ret = gcm_decrypt_message(nonce,
            nonce_len,
            NULL,
            0,
            encap_block.buf(),
            encap_block.len(),
            tag_encap,                // tag is input, for validation against calculated tag
            tag_len,
            &(ctx_ex.c));

    // check return value that the block was OK
    if ( ret != 0 ) {
        log_err_p(log, "Ciphersuite_enc::decapsulate: gcm_decrypt_message failed, ret = %d", ret);
        goto fail;
    }
    log_debug_p(log, "Ciphersuite_enc::decapsulate: decrypted block");

    // Also see if there are EID refs, and if there will be any in
    // the resultant block
    toskip=0;
    cs_flags = locals->cs_flags();

    if(first_block) {
        // We need to skip up to the first two eid refs,
        // which actually belong to the encapsulating ESB
        // block.
        if(cs_flags & Ciphersuite::CS_BLOCK_HAS_SOURCE) {
            toskip++;
            log_debug_p(log,"Ciphersuite_enc::decapsulate: skipping one eid ref because block has a sec src");
        }
        if(cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) {
            toskip++;
            log_debug_p(log,"Ciphersuite_enc::decapsulate: skipping one eid ref because block has a sec dest");
        }
    }
 
    if(recreate_block(encap_block, iter, bundle, toskip) == BP_FAIL) {
        log_err_p(log, "Ciphersuite_enc::decapuslate couldn't reconsitute block");
        goto fail;
    }

        return BP_SUCCESS;
     fail:
        return BP_FAIL;
}
}
#endif
