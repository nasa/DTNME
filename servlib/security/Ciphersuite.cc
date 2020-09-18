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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite.h"
#include "BP_Local_CS.h"
#include "BA_BlockProcessor.h"
#include "Ciphersuite_BA1.h"
#include "Ciphersuite_BA5.h"
#include "Ciphersuite_BA9.h"
#include "PI_BlockProcessor.h"
#include "Ciphersuite_PI2.h"
#include "Ciphersuite_PI6.h"
#include "Ciphersuite_PI10.h"
#include "PC_BlockProcessor.h"
#include "Ciphersuite_PC3.h"
#include "Ciphersuite_PC7.h"
#include "Ciphersuite_PC11.h"
#include "ES_BlockProcessor.h"
#include "Ciphersuite_ES4.h"
#include "Ciphersuite_ES8.h"
#include "Ciphersuite_ES12.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"
#include "bundling/SDNV.h"

namespace dtn {

#define CS_MAX 1024
Ciphersuite* Ciphersuite::ciphersuites_[CS_MAX];
SecurityConfig* Ciphersuite::config=NULL;

static const char* log = "/dtn/bundle/ciphersuite";

bool Ciphersuite::inited = false;

//----------------------------------------------------------------------
Ciphersuite::Ciphersuite()
{
}

//----------------------------------------------------------------------
Ciphersuite::Ciphersuite(SecurityConfig *c) {
    config = c;
}

//----------------------------------------------------------------------
Ciphersuite::~Ciphersuite()
{ 
}

//----------------------------------------------------------------------
void Ciphersuite::shutdown() {
    for(int i=0;i<CS_MAX;i++) {
        if(ciphersuites_[i] != 0) {
            delete ciphersuites_[i];
            ciphersuites_[i] = NULL;
        }
    }
    delete config;
}

//----------------------------------------------------------------------
void
Ciphersuite::register_ciphersuite(Ciphersuite* cs)
{
    log_debug_p(log, "Ciphersuite::register_ciphersuite()");
    u_int16_t    num = cs->cs_num();
    
    if ( num <= 0 || num >= CS_MAX )
        return;            //out of range
        
    // don't override an existing suite
    ASSERT(ciphersuites_[num] == 0);
    ciphersuites_[num] = cs;
}

//----------------------------------------------------------------------
Ciphersuite*
Ciphersuite::find_suite(u_int16_t num)
{
    Ciphersuite* ret = NULL;
    log_debug_p(log, "Ciphersuite::find_suite() ciphersuite number: %d", num);
    

    if ( num > 0 && num < CS_MAX )  // entry for element zero is illegal
        ret = ciphersuites_[num];

    return ret;
}

//----------------------------------------------------------------------
void
Ciphersuite::init_default_ciphersuites()
{
    log_debug_p(log, "Ciphersuite::init_default_ciphersuites()");
    if ( ! inited ) {
        config = new SecurityConfig();
        // register default block processor handlers
        BundleProtocol::register_processor(new BA_BlockProcessor());
        BundleProtocol::register_processor(new PI_BlockProcessor());
        BundleProtocol::register_processor(new PC_BlockProcessor());
        BundleProtocol::register_processor(new ES_BlockProcessor());

        // register mandatory ciphersuites
        register_ciphersuite(new Ciphersuite_BA1());
        register_ciphersuite(new Ciphersuite_PI2());
        register_ciphersuite(new Ciphersuite_PC3());
        register_ciphersuite(new Ciphersuite_ES4());
        register_ciphersuite(new Ciphersuite_BA5());
        register_ciphersuite(new Ciphersuite_PI6());
        register_ciphersuite(new Ciphersuite_PC7());
        register_ciphersuite(new Ciphersuite_ES8());
        register_ciphersuite(new Ciphersuite_BA9());
        register_ciphersuite(new Ciphersuite_PI10());
        register_ciphersuite(new Ciphersuite_PC11());
        register_ciphersuite(new Ciphersuite_ES12());

        inited = true;
    }
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite::cs_num(void)
{

    return 0;
}


//----------------------------------------------------------------------
int
Ciphersuite::consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len)
{
    log_debug_p(log, "Ciphersuite::consume()");
    int cc = block->owner()->consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    // in on-the-fly scenario, process this data for those interested
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if ( block->locals() == NULL ) {      // then we need to parse it
        parse(block);
    }
    
    return cc;
}


//----------------------------------------------------------------------
void
Ciphersuite::parse(BlockInfo* block)
{
    Ciphersuite*    cs_owner = NULL;
    BP_Local_CS*    locals = NULL;
    u_char*         buf;
    size_t          len;
    u_int64_t       cs_flags;
    u_int64_t       suite_num;
    int             sdnv_len;
    u_int64_t       security_correlator    = 0LL;
    u_int64_t       field_length           = 0LL;
    bool            has_source;    
    bool            has_dest;      
    bool            has_params;    
    bool            has_correlator;
    bool            has_result;   
    EndpointIDVector::const_iterator iter;
    
// XXX/pl todo  think about a "const" version of parse() since there
//              are several callers who need parsing but have a const block

    log_debug_p(log, "Ciphersuite::parse() block %p", block);
    ASSERT(block != NULL);
    
    // preamble has already been parsed and stored, so we skip over it here
    buf = block->contents().buf() + block->data_offset();
    len = block->data_length();
    
    // get ciphersuite and flags
    sdnv_len = SDNV::decode(buf,
                            len,
                            &suite_num);
    buf += sdnv_len;
    len -= sdnv_len;

    sdnv_len = SDNV::decode(buf,
                            len,
                            &cs_flags);
    buf += sdnv_len;
    len -= sdnv_len;

    has_source     = (cs_flags & CS_BLOCK_HAS_SOURCE)     != 0;
    has_dest       = (cs_flags & CS_BLOCK_HAS_DEST)       != 0;
    has_params     = (cs_flags & CS_BLOCK_HAS_PARAMS)     != 0;
    has_correlator = (cs_flags & CS_BLOCK_HAS_CORRELATOR) != 0;
    has_result     = (cs_flags & CS_BLOCK_HAS_RESULT)     != 0;
    log_debug_p(log, "Ciphersuite::parse() suite_num %llu cs_flags 0x%llx",
                U64FMT(suite_num), U64FMT(cs_flags));

    cs_owner = dynamic_cast<Ciphersuite*>(find_suite(suite_num));
    
    if ( ciphersuites_[suite_num] != NULL )            // get specific subclass if it's present
        cs_owner = ciphersuites_[suite_num];
    
    if ( block->locals() == NULL ) {
        if ( cs_owner != NULL )
            cs_owner->init_locals(block);                // get owning class to allocate locals
        else
            block->set_locals( new BP_Local_CS );
    }
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    ASSERT ( locals != NULL );

    ASSERT ( suite_num < 65535 );
    locals->set_owner_cs_num(suite_num);

    // set cs_flags
    ASSERT ( cs_flags  < 65535  );
    locals->set_cs_flags(cs_flags);
    
    // get correlator, if present
    if ( has_correlator ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &security_correlator);
        buf += sdnv_len;
        len -= sdnv_len;
    }
    log_debug_p(log, "Ciphersuite::parse() correlator %llu",
                U64FMT(security_correlator));
    locals->set_correlator(security_correlator);
    

    // get cs params length, and data
    if ( has_params ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &field_length);
        buf += sdnv_len;
        len -= sdnv_len;
        BP_Local_CS::parse_params_or_result(locals->security_params_, buf, field_length);
        // make sure the buffer has enough space, copy data in
        //locals->writable_security_params()->reserve(field_length);
        //memcpy( locals->writable_security_params()->end(), buf, field_length);
        //locals->writable_security_params()->set_len(field_length);
        buf += field_length;
        len -= field_length;
        log_debug_p(log, "Ciphersuite::parse() security_params len %llu",
                    U64FMT(field_length));
    }
    

    // get sec-src length and data
    log_debug_p(log, "Ciphersuite::parse() eid_list().size() %zu has_source %u has_dest %u", 
                block->eid_list().size(), has_source, has_dest);
    
    // XXX/pl - temp fix for blocks loaded from store
    if ( block->eid_list().size() > 0 ) {
        iter = block->eid_list().begin();
        if ( has_source ) {    
            locals->set_security_src( iter->str() );
            iter++;
        }
        

        // get sec-dest length and data
        if ( has_dest ) {    
            locals->set_security_dest( iter->str() );
        }
    }
    

    // get sec-result length and data, if present
    if ( has_result ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &field_length);
        buf += sdnv_len;
        len -= sdnv_len;
        BP_Local_CS::parse_params_or_result(locals->security_result_, buf, field_length);
        // make sure the buffer has enough space, copy data in
        //locals->writable_security_result()->reserve(field_length);
        //memcpy( locals->writable_security_result()->end(), buf, field_length);
        //locals->writable_security_result()->set_len(field_length);
        buf += field_length;
        len -= field_length;
        
    }
    



}


//----------------------------------------------------------------------
int
Ciphersuite::reload_post_process(Bundle*       bundle,
                                 BlockInfoVec* block_list,
                                 BlockInfo*    block)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    
    // Received blocks might be stored and reloaded and
    // some fields aren't reset.
    // This allows BlockProcessors to reestablish what they
    // need. The default implementation does nothing.
    // In general that's appropriate, as the BlockProcessor
    // will have called parse() and that's the main need. 
    // Individual ciphersuites can override this if their 
    // usage requires it.
    
    block->set_reloaded(false);
    return 0;

}

//----------------------------------------------------------------------
void
Ciphersuite::generate_preamble(BlockInfoVec* xmit_blocks,
                               BlockInfo*    block,
                               u_int8_t      type,
                               u_int64_t     flags,
                               u_int64_t     data_length)
{
    block->owner()->generate_preamble(xmit_blocks, block, type,
                                      flags, data_length);
}

EndpointID Ciphersuite::base_portion(const EndpointID &in) {
    // this is a very clunky way to get the "base" portion of the eid
    std::string the_str = in.uri().scheme() + "://" + in.uri().host();
    return EndpointID(the_str);
}


//----------------------------------------------------------------------
bool
Ciphersuite::destination_is_local_node(const Bundle*    bundle,
                                       const BlockInfo* block)
{
    u_int16_t       cs_flags = 0;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals;
    bool            result = false;     //default is "no" even in case of errors

    log_debug_p(log, "Ciphersuite::destination_is_local_node()");
    if ( block == NULL )
        return false;
        
    if ( block->locals() == NULL )       // then the block is broken
        return false;
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    if ( locals == NULL )
        return false;

    cs_flags = locals->cs_flags();
    
    EndpointID        dest_node = base_portion(bundle->dest());
    
    
    // if this is security-dest, or there isn't one and this is bundle dest
    if (  (  (cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) && (local_eid == locals->security_dest())) ||
          ( !(cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) && (local_eid == dest_node)              )    ) 
    {  //yes - this is ours 
        result = true;
    }
    log_debug_p(log, "Ciphersuite::destination_is_local_node local_eid=%s\n", local_eid.str().c_str());
    if( cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) {
        log_debug_p(log, "Ciphersuite::destination_is_local_node locals->security_dest()=%s", locals->security_dest().c_str());
    } else {
        log_debug_p(log, "Ciphersuite::destination_is_local_node dest_node=%s", dest_node.str().c_str());
    }


    
    return result;
}

EndpointID Ciphersuite::find_last_secdestp(const Bundle *bundle) {
    EndpointID        dest = base_portion(bundle->dest());
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;
    for ( iter = bundle->recv_blocks().begin();
          iter != bundle->recv_blocks().end();
          ++iter)
    {
        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
    cs_flags = locals->cs_flags();
        if (Ciphersuite::config->get_block_type(locals->owner_cs_num()) == BundleProtocol::PAYLOAD_SECURITY_BLOCK ||
            Ciphersuite::config->get_block_type(locals->owner_cs_num()) == BundleProtocol::CONFIDENTIALITY_BLOCK ) {
            
            continue;
            if( cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) {
                return EndpointID(locals->security_dest());
            } else {
                return dest;
            }
        }
    }
    return dest;
}
EndpointID Ciphersuite::find_last_secdeste(const Bundle *bundle) {
    EndpointID        dest = base_portion(bundle->dest());
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;


    for ( iter = bundle->recv_blocks().begin();
          iter != bundle->recv_blocks().end();
          ++iter)
    {
        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
    cs_flags = locals->cs_flags();
        if (Ciphersuite::config->get_block_type(locals->owner_cs_num()) == BundleProtocol::EXTENSION_SECURITY_BLOCK ) {
            
            continue;
            if( cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) {
                return EndpointID(locals->security_dest());
            } else {
                return dest;
            }
        }
    }
    return dest;

}

//----------------------------------------------------------------------
bool
Ciphersuite::source_is_local_node(const Bundle* bundle, const BlockInfo* block)
{
    u_int16_t       cs_flags = 0;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals;
    bool            result = false;     //default is "no" even in case of errors

    log_debug_p(log, "Ciphersuite::source_is_local_node()");
    if ( block == NULL )
        return false;
        
    if ( block->locals() == NULL )       // then the block is broken
        return false;
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    if ( locals == NULL )
        return false;

    cs_flags = locals->cs_flags();
    
    EndpointID        src_node= base_portion(bundle->source());
    
    
    // if this is security-src, or there isn't one and this is bundle source
    if (  (  (cs_flags & Ciphersuite::CS_BLOCK_HAS_SOURCE) && (local_eid == locals->security_src())) ||
          ( !(cs_flags & Ciphersuite::CS_BLOCK_HAS_SOURCE) && (local_eid == src_node)              )    ) 
    {  //yes - this is ours 
        result = true;
    }
    
    return result;
}

// Helper to check_validation.  This checks whether the bsp metadata
// for the block pointed to by iter (which contains information about 
// esb blocks that were previously encapsulating the one pointed to by
// iter) match the given security src, dest, and csnum, (passed as src,
// dest, csnum).
bool Ciphersuite::check_esb_metadata(BlockInfoVec::const_iterator iter, EndpointIDPatternNULL src, EndpointIDPatternNULL dest, const Bundle *b, int csnum) {
            oasys::SerializableVector<BSPProtectionInfo>::const_iterator it;
    for(it=iter->bsp.begin();it!=iter->bsp.end();it++) {
        log_debug_p(log, "Ciphersuite::check_validation: considering BSP metadata entry");
        bool esb_block_meets_rule=true;
        if(!it->src_matches(src, b)) {
            esb_block_meets_rule= false;
        }
        if(!it->dest_matches(dest, b)) {
            esb_block_meets_rule=false;
        }
        if(csnum != it->csnum)
            esb_block_meets_rule=false;
        if(esb_block_meets_rule) {
            return true;
        }

    }
    return false;
}
// Helper to check_validation.  This checks whether the bsp
// information in locals match the given security src, dest, (passed as src,
// dest). 
bool Ciphersuite::check_src_dest_match(BP_Local_CS* locals, EndpointIDPatternNULL src, EndpointIDPatternNULL dest, const Bundle *bundle) {
    if(!locals->security_dest_matches(dest, bundle)) {
        log_debug_p(log, "Ciphersuite::check_validation() security dest doesn't match");
        return false;
    }
    if(!locals->security_src_matches(src, bundle)) {
        log_debug_p(log, "Ciphersuite::check_validation() security src doesn't match");
        return false;
    }
    return true;
}

 
//----------------------------------------------------------------------
bool
Ciphersuite::check_validation(const Bundle*       bundle,
                              const BlockInfoVec* block_list,
                              u_int16_t           num,
                              EndpointIDPatternNULL &src,
                              EndpointIDPatternNULL &dest)
{
    (void)bundle;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;

    bool is_esb = (Ciphersuite::config->get_block_type(num) == dtn::BundleProtocol::EXTENSION_SECURITY_BLOCK);
    bool is_bab = (Ciphersuite::config->get_block_type(num) == dtn::BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK);

    log_debug_p(log, "Ciphersuite::check_validation(%hu)", num);
    if ( block_list == NULL )
        return false;

    if(is_esb) {
        log_debug_p(log, "Ciphersuite::check_validation: it's an ESB rule, so we'll do different checks!");
        for ( iter = block_list->begin();
            iter != block_list->end();
            ++iter) {
            log_debug_p(log, "Ciphersuite::check_validation: considering block of type %d", iter->owner()->block_type());
            bool block_meets_rule=false;
            if(Ciphersuite_ES::esbable(iter->owner()->block_type())) {
                log_debug_p(log, "Ciphersuite::check_validation: it's an ESBable block, but not an ESB block");
                block_meets_rule=check_esb_metadata(iter, src, dest, bundle,num);
            } else if(iter->owner()->block_type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
                block_meets_rule=check_esb_metadata(iter, src, dest, bundle, num);

                BP_Local_CS *locals = dynamic_cast<BP_Local_CS*>(iter->locals());
                if(locals != NULL) {
                    block_meets_rule |= check_src_dest_match(locals, src, dest, bundle);
                }
            } else {
                log_debug_p(log, "Ciphersuite::check_validation: not an ESBable block");
                block_meets_rule = true;
            }
            if(block_meets_rule == false) {
                return false;
            }
        }
        return true;
    } else if (is_bab) {
        log_debug_p(log, "Ciphersuite::check_validation: it's a BAB rule - only one needs to verify!");
        for ( iter = block_list->begin();
              iter != block_list->end();
              ++iter)
        {

            if ( iter->locals() == NULL )       // then of no interest
                continue;
        
            locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            if ( locals == NULL )
                continue;
    
            if (locals->owner_cs_num() != num ) {
                log_debug_p(log, "Ciphersuite::check_validation: found a block, but the cs_num is wrong (%d)", locals->owner_cs_num());
                continue;
            }

            // check if src and dest pattern match here
            if(check_src_dest_match(locals, src, dest, bundle)) {
                if (iter->owner()->validate_security_result(bundle, block_list, (BlockInfo*) &*iter)) {
                    log_debug_p(log, "Ciphersuite::check_validation: found a validated BAB security result");
                    return true;
                }
                log_debug_p(log, "Ciphersuite::check_validation: found a BAB security result that did not validate - continuing");
            } else {
                log_debug_p(log, "Ciphersuite::check_validation: found a BAB block for a different secdest");
            }
        }

        return false;
    }

       
    for ( iter = block_list->begin();
          iter != block_list->end();
          ++iter)
    {

        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
        if (locals->owner_cs_num() != num ) {
            log_debug_p(log, "Ciphersuite::check_validation: found a block, but the cs_num is wrong (%d)", locals->owner_cs_num());
            continue;
        }

        // check if src and dest pattern match here
        if(!check_src_dest_match(locals, src, dest, bundle)) {
            log_debug_p(log, "Ciphersuite::check_validation: found a block with the right ciphersuite, but the sec src/dest don't match");
            continue;
        }
        
        return true;
    }
    
    log_debug_p(log, "Ciphersuite::check_validation(%hu) returning false because we didn't find a matching block", num);
    
    return false;   // no blocks of wanted type
}

//----------------------------------------------------------------------
u_int64_t
Ciphersuite::create_correlator(const Bundle*       bundle,
                               const BlockInfoVec* block_list)
{
    (void)bundle;
    u_int64_t       result = 0LLU;
    u_int16_t       high_val = 0;
    u_int16_t       value;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;

    log_debug_p(log, "Ciphersuite::create_correlator()");
    if ( bundle == NULL )
        return 1LLU;        // and good luck :)
        
    if ( block_list == NULL )
        return 1LLU;
        
    if ( bundle->is_fragment() ) {
        result = bundle->frag_offset() << 24;
    }
    
    for ( iter = block_list->begin();
          iter != block_list->end();
          ++iter)
    {
        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
        value = locals->correlator();       // only low-order 16 bits
        log_debug_p(log, "Ciphersuite::create_correlator() encoutered an existing correlator with low order 16 bits = 0x%x", value);
        
        high_val = value > high_val ? value : high_val;
    }
    
    result |= (high_val+1);     // put high_val into low-order two bytes
   
    log_debug_p(log, "Ciphersuite::create_correlator() returning 0x%llx", result); 
    return result;
}

//----------------------------------------------------------------------
void
Ciphersuite::init_locals(BlockInfo* block)
{
    /* Create new locals block but do not overwrite
     * if one already exists.
     * Derived classes may wish to change this behavior
     * and map old-to-new, or whatever
     */
     
    if ( block->locals() == NULL )
        block->set_locals( new BP_Local_CS );
    
}

int Ciphersuite::copy_block_untouched(BlockInfo *block, BlockInfoVec *xmit_blocks, bool last) {
        // generate the preamble and copy the data.
        size_t length = block->source()->data_length();
        int block_type = block->source()->type();
        
        generate_preamble(xmit_blocks, 
                          block,
                          block_type,
                          //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                          (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                          length);

        BlockInfo::DataBuffer* contents = block->writable_contents();
        contents->reserve(block->data_offset() + length);
        contents->set_len(block->data_offset() + length);
        memcpy(contents->buf() + block->data_offset(),
               block->source()->data(), length);
        return BP_SUCCESS;
}

bool Ciphersuite::we_are_block_sec_dest(const BlockInfo *source) {
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    
    //XXXpl - fix this test
    if ( (source != NULL)  &&
         (dynamic_cast<BP_Local_CS*>(source->locals())->security_dest() == local_eid.data()) ) {
        log_debug_p(log, "Ciphersuite::we_are_block_set_dest: not being forwarded");
        return true; 
    }
    return false;
}

int Ciphersuite::create_bsp_block_from_source(BlockInfo &bi,const Bundle *bundle, const BlockInfo *source, BlockInfo::list_owner_t list, int csnum) {
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals = NULL;
    
    // initialize the block
    log_debug_p(log, "Ciphersuite::create_bsp_block_from_source: begin");
    log_debug_p(log, "Ciphersuite::create_bsp_block_from_source: local_eid %s bundle->source_ %s", local_eid.c_str(), bundle->source().c_str());
    bi.set_locals(new BP_Local_CS);
    CS_FAIL_IF_NULL(bi.locals());

    if(list == BlockInfo::LIST_RECEIVED) {
        bi.set_eid_list(source->eid_list());
    }
    locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    CS_FAIL_IF_NULL(locals);
    locals->set_owner_cs_num(csnum);
    locals->set_list_owner(list);
    
    // if there is a security-src and/or -dest, use it -- might be specified by API
    if ( source != NULL && source->locals() != NULL)  {
        locals->set_security_src(dynamic_cast<BP_Local_CS*>(source->locals())->security_src());
        locals->set_security_dest(dynamic_cast<BP_Local_CS*>(source->locals())->security_dest());
    }
    
    // if not, and we didn't create the bundle, specify ourselves as sec-src
    if ( list != BlockInfo::LIST_RECEIVED && (locals->security_src().length() == 0) && (local_eid != bundle->source())) {
        locals->set_security_src(local_eid.str());
    }
    
    // if we now have one, add it to list, etc
    if ( locals->security_src().length() > 0 ) {
        log_debug_p(log, "Ciphersuite::create_bsp_block_from_source: add security_src EID %s", locals->security_src().c_str());
        cs_flags |= CS_BLOCK_HAS_SOURCE;
        if(list != BlockInfo::LIST_RECEIVED)
            bi.add_eid(locals->security_src());
    }
    
    if ( locals->security_dest().length() > 0 ) {
        log_debug_p(log, "Ciphersuite::create_bsp_block_from_source: add security_dest EID %s", locals->security_dest().c_str());
        cs_flags |= CS_BLOCK_HAS_DEST;
        if(list != BlockInfo::LIST_RECEIVED)
            bi.add_eid(locals->security_dest());
    }
        
    locals->set_cs_flags(cs_flags);
    return BP_SUCCESS;
  fail:
    return BP_FAIL;
} 

string Ciphersuite::buf2str(const u_char *buf, size_t len) {
    char *hrbytes = (char *)malloc(len*2+1);
    for(unsigned int i=0;i<len;i++) {
        snprintf(hrbytes+2*i, 3, "%02x", ((unsigned char *)buf)[i]);
    }
    hrbytes[2*len] = '\0';
    string foo(hrbytes);
    free(hrbytes);
    return foo;
 
}
string Ciphersuite::buf2str(const LocalBuffer buf) {
    return buf2str(buf.buf(), buf.len());
}
string Ciphersuite::buf2str(const BlockInfo::DataBuffer buf) {
    return buf2str(buf.buf(), buf.len());
}


} // namespace dtn

#endif /* BSP_ENABLED */
