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
 *    Copyright 2015 United States Government as represented by NASA
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite_BA.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "KeyDB.h"
#include "openssl/hmac.h"

// Need quad versions of hton for manipulating full-length (unpacked) SDNV values
#if defined(WORDS_BIGENDIAN) && (WORDS_BIGENDIAN == 1)
#define htonq( x ) (x)
#define ntohq( x ) (x)
#else

inline u_int64_t htonq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = htonl( hi );
    res = htonl( lo );
    res = res << 32 | hi;

    return res;
}

inline u_int64_t ntohq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = ntohl( hi );
    res = ntohl( lo );
    res = res << 32 | hi;

    return res;
}
#endif

namespace dtn {

static const char* log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_BA::Ciphersuite_BA()
{
}

int Ciphersuite_BA::create_digest(const Bundle *bundle, BlockInfo* block, const BlockInfoVec *recv_blocks, const KeyDB::Entry* key_entry, u_char *result) {
    HMAC_CTX        	ctx;
    u_int32_t       	rlen = 0;
    OpaqueContext*   	r = reinterpret_cast<OpaqueContext*>(&ctx);
    size_t         		offset;
    size_t          	len;
    size_t          	rem;
    u_int64_t       	cs_flags;
    u_int64_t       	suite_num;
    int             	sdnv_len = 0;        // use an int to handle -1 return values

        HMAC_CTX_init(&ctx);
        if (result_len() == 20) {
        	HMAC_Init_ex(&ctx, key_entry->key(), key_entry->key_len(), EVP_sha1(), NULL);
        } else if (result_len() == 32) {
        	HMAC_Init_ex(&ctx, key_entry->key(), key_entry->key_len(), EVP_sha256(), NULL);
        } else if (result_len() == 48) {
        	HMAC_Init_ex(&ctx, key_entry->key(), key_entry->key_len(), EVP_sha384(), NULL);
        } else {
        	log_err_p(log, "Ciphersuite_BA::validate: Invalid value for hash length (bytes): %zu (should be 20, 32 or 48)", result_len());
        	goto fail;
        }

        // walk the list and process each of the blocks
        for ( BlockInfoVec::const_iterator iter = recv_blocks->begin();
              iter != recv_blocks->end();
              ++iter)
        {
            offset = 0;
            len = iter->full_length();
            if (iter->type() != BundleProtocol::PRIMARY_BLOCK) {
                uint64_t flags = iter->flags();
                u_char *cont = iter->contents().buf();
                uint64_t totallen = iter->contents().len();
                uint8_t type = *cont;
                if(type != (uint16_t)iter->type()) {
                    log_err_p(log, "Ciphersuite_BA::validate: there's something wrong with the type");
                }
                digest(bundle, block, &*iter, cont, 1,r);
                offset++;
                len--;
                cont ++;
                totallen --;

                uint64_t flags2;
                sdnv_len = SDNV::decode(cont, totallen, &flags2);
                if(flags != flags2) {
                    log_err_p(log, "Ciphersuite_BA::validate: there's something wrong with the flags");
                }
                flags &= (0xFFFFFFFFFFFFFFFF-0x20);
                flags = htonq(flags);
                digest(bundle, block, &*iter, &flags, sizeof(flags), r);
                offset += sdnv_len;
                len -= sdnv_len;
            }


            
            if ( iter->type() == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK ) {
                // This is a BA block but might or might not be BA.
                // So we need to see if there is a security-result field
                // which needs exclusion
                
                // ciphersuite number and flags
                u_char* ptr = iter->data();
                rem = iter->full_length();

                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &suite_num);
                ptr += sdnv_len;
                rem -= sdnv_len;

                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &cs_flags);
                ptr += sdnv_len;
                rem -= sdnv_len;

                if ( cs_flags & CS_BLOCK_HAS_RESULT ) {
                    // if there's a security-result we have to ease up to it
                    
                    sdnv_len =  SDNV::len(ptr);        //step over correlator
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    sdnv_len =  SDNV::len(ptr);        //step over security-result-length field
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    len = ptr - iter->contents().buf() - offset;  //this is the length to use
                }
            }
            
            iter->owner()->process( Ciphersuite_BA::digest,
                                    bundle,
                                    block,
                                    &*iter,
                                    offset,
                                    len,
                                    r);
        }
        
        // finalize the digest
        HMAC_Final(&ctx, result, &rlen);
        HMAC_cleanup(&ctx);
        ASSERT(rlen == result_len());
    return BP_SUCCESS;
  fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
Ciphersuite_BA::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    (void)reception_reason;
    (void)deletion_reason;
    // any structural elements that could be checked?
    return true;
}

//----------------------------------------------------------------------
bool
Ciphersuite_BA::validate_security_result(const Bundle*            bundle,
                                          BlockInfoVec*           block_list,
                                          BlockInfo*              block)
{

    u_char          	result[EVP_MAX_MD_SIZE];
    BP_Local_CS*    	locals = NULL;

    log_debug_p(log, "Ciphersuite_BA::validate()");
    // if first block
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    if ( !(locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT) ) {            
        const KeyDB::Entry* key_entry = KeyDB::find_key(locals->security_src_host(bundle).c_str(),cs_num());
        if (key_entry == NULL) {
            log_warn_p(log, "Ciphersuite_BA::validate: unable to find verification key for this block");
            goto fail;
        }
        ASSERT(key_entry->key_len() == result_len());
        
        // dump key_entry to debugging output
        // oasys::StringBuffer ksbuf;
        // key_entry->dump(&ksbuf);
        // log_debug_p(log, "Ciphersuite_BA::validate(): using key entry:\n%s",
        // ksbuf.c_str());
        
        // prepare the digest in "result"
        if(create_digest(bundle, block, &(bundle->recv_blocks()), key_entry, result) == BP_FAIL) {
            goto fail;
        }

        // check the digest in the result - in the *second* block
        // walk the list to find it
        for (BlockInfoVec::iterator iter = block_list->begin();
             iter != block_list->end();
             ++iter)
        {
            BP_Local_CS* target_locals;
            if ( iter->type() != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK )
                continue;
            
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);
            if ( target_locals->owner_cs_num() != cs_num() )
                continue;
            
            if (target_locals->correlator() != locals->correlator() )
                continue;
            
            // Now we're at the block which is ...
            //   1. BA block
            //   2. BA ciphersuite
            //   3. same correlator as the main one        
            
            if ( target_locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT ) {
                LocalBuffer target_result;
                if(!target_locals->get_security_result(target_result, Ciphersuite::CS_signature_field)) {
                    log_err_p(log, "Ciphersuite_BA::validate: didn't find digest");
                    goto fail;
                }
                if(result_len()!=target_result.len()) {
                    log_err_p(log, "Ciphersuite_BA::validate: digest was wrong length");
                    goto fail;
                }
                
                if ( memcmp(target_result.buf(), result, result_len()) != 0) {
                    log_err_p(log, "Ciphersuite_BA::validate: block failed security validation");
                    goto fail;
                } else {
                    log_debug_p(log, "Ciphersuite_BA::validate: block passed security validation");
                    return true;
                }
            }
            else 
            {
                continue;
            }
        }
        log_err_p(log, "Ciphersuite_BA::validate: block failed security validation - result is missing");
        goto fail;
    }
    else    
    {
        log_debug_p(log, "BABlockProcessor::validate: no check on this block");
    }

    return true;

 fail:
    return false;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA::prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list)
{
    (void)bundle;
    (void)link;

    u_int64_t       correlator = 0;     //also need to add a low-order piece
    u_int16_t       flags = CS_BLOCK_HAS_CORRELATOR;
    BP_Local_CS*    locals = NULL;

    log_debug_p(log, "Ciphersuite_BA::prepare()");
    //Received blocks should be handled by the block processor prepare
    //methods.  We are never called for these.
    ASSERT(list != BlockInfo::LIST_RECEIVED);
        
    // Need to add two blocks, one at the start, one after payload
    // It's simpler to fill in the pieces and then insert them.
    BlockInfo       bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK), source);

    // initialize the first block
    BundleDaemon* bd = BundleDaemon::instance();
    bi.set_locals(new BP_Local_CS);
    locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    CS_FAIL_IF_NULL(locals);
    locals->set_owner_cs_num(cs_num());


    // if there is a security-src and/or -dest, use it -- might be specified by API
    if ( source != NULL && source->locals() != NULL)  {
        locals->set_security_src(dynamic_cast<BP_Local_CS*>(source->locals())->security_src());
        locals->set_security_dest(dynamic_cast<BP_Local_CS*>(source->locals())->security_dest());
    }
        
           


    if ( locals->security_src().length() == 0 ) {
        locals->set_security_src(bd->local_eid().str());
    }
    log_debug_p(log, "Ciphersuite_BA::prepare: add security_src EID <%s>", locals->security_src().c_str());
    flags |= CS_BLOCK_HAS_SOURCE;
    locals->set_cs_flags(flags);
    bi.add_eid(locals->security_src());

     if ( locals->security_dest().length() > 0 ) {
        log_debug_p(log, "Ciphersuite_BA::prepare: add security_dest EID <%s>", locals->security_dest().c_str());
        flags |= CS_BLOCK_HAS_DEST;
        locals->set_cs_flags(flags);
        bi.add_eid(locals->security_dest());
    }
        

    correlator = create_correlator(bundle, xmit_blocks);
    correlator |= (int)cs_num() << 16;      // add our ciphersuite number
    locals->set_correlator( correlator );
    locals->set_correlator_sequence( 0 );
    
    
    // We should already have the primary block in the list.
    // If primary is there then insert after it.
    // If not, insert first in the list.
    // If list is empty then just add to back
    //   -- this will be troublesome later but we have no choice
    if ( xmit_blocks->size() > 0 ) {
        BlockInfoVec::iterator iter = xmit_blocks->begin();
        if ( iter->type() == BundleProtocol::PRIMARY_BLOCK)
            ++iter;
        xmit_blocks->insert(iter, bi);
    } else {
        xmit_blocks->push_back(bi);
    }
    
    // initialize the second (trailing) block
    bi.set_locals(new BP_Local_CS);
    locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    CS_FAIL_IF_NULL(locals);
    locals->set_owner_cs_num(cs_num());
    locals->set_correlator( correlator );       // same one created above, obviously
    locals->set_cs_flags(flags);
    locals->set_correlator_sequence( 1 );
    xmit_blocks->push_back(bi);

    return BP_SUCCESS;
    
 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA::generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    BP_Local_CS*    		locals = dynamic_cast<BP_Local_CS*>(block->locals());
            
    log_debug_p(log, "Ciphersuite_BA::generate()");

    CS_FAIL_IF_NULL(locals);
  
    if(locals->correlator_sequence() == 1) {
       locals->add_security_result_space(result_len()); 
    }
    locals->serialize(xmit_blocks, block, last);
       
    return BP_SUCCESS;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA::finalize(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link)
{
    (void)link;
    
    u_char          digest_result[EVP_MAX_MD_SIZE];
    BP_Local_CS*    locals = NULL;

    log_debug_p(log, "Ciphersuite_BA::finalize()");
    
    /* The processing for BundleAuthentication takes place
     * when finalize() is called for the "front" block, even though
     * the result itself goes into the trailing block, after the payload.
     * It is an error to calculate the digest during the finalize() call
     * for the trailing block itself, as other needed results have not
     * been created at that time. Remember that the finalize() processing
     * is a reverse iteration over all the blocks.
     */
     
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    if ( locals->correlator_sequence() == 0 ) {       // front block is zero
        // fetch key
        const KeyDB::Entry* key_entry = KeyDB::find_key(locals->security_dest_host(bundle, link).c_str(),cs_num());
        // XXX/ngoffee -- fix this ASSERT later, but it's what we have
        // to do until the prepare()/generate()/finalize() interface
        // is changed to allow more subtle return codes.
        CS_FAIL_IF(key_entry == NULL);
        CS_FAIL_IF(key_entry->key_len() != result_len());
        
        // dump key_entry to debugging output
        // oasys::StringBuffer ksbuf;
        // key_entry->dump(&ksbuf);
        // log_debug_p(log, "Ciphersuite_BA::finalize(): using key entry:\n%s", ksbuf.c_str());
        
        // prepare the digest in "digest_result"
        create_digest(bundle, block, xmit_blocks, key_entry, digest_result);
       // place the digest into the block - it goes into the *second* block
        // walk the list to find it
        for (BlockInfoVec::iterator iter = xmit_blocks->begin();
             iter != xmit_blocks->end();
             ++iter)
        {

            BP_Local_CS* target_locals;
            if ( iter->type() != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK )
                continue;
            
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);
            if ( target_locals->owner_cs_num() != cs_num() )
                continue;
            
            if (target_locals->correlator() != locals->correlator() )
                continue;
            
            if (target_locals->correlator_sequence() != 1 )
                continue;
            
            // Now we're at the block which is ...
            //   1. BA block
            //   2. BA ciphersuite
            //   3. same correlator as the main one
            //   4. correlator sequence is 1, which means second block

            target_locals->add_missing_security_result(&(*iter), Ciphersuite::CS_signature_field, result_len(), digest_result);
        }
    }
    
    return BP_SUCCESS;

 fail:
    return BP_FAIL;
}


// This method takes these arguments so that it can be passed to
// BlockProcessor::process.  When we are digesting the payload, it may
// be stored on disk rather than in the block contents.  process knows
// how to handle this.  Otherwise we could just walk through the block
// contents ourself.
//----------------------------------------------------------------------
void
Ciphersuite_BA::digest(const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        const void*      buf,
                        size_t           len,
                        OpaqueContext*   r)
{
    (void)bundle;
    (void)caller_block;
    (void)target_block;
    
    log_debug_p(log, "Ciphersuite_BA::digest() %zu", len);

    HMAC_CTX*       pctx = reinterpret_cast<HMAC_CTX*>(r);
    
    HMAC_Update( pctx, reinterpret_cast<const u_char*>(buf), len );
}

} // namespace dtn

#endif /* BSP_ENABLED */
