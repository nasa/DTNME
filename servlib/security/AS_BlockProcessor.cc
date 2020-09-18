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

#include "AS_BlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
AS_BlockProcessor::AS_BlockProcessor(int block_type)
    : BlockProcessor(block_type)
{
}

int AS_BlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *block) {
    BP_Local_CS* locals = dynamic_cast<BP_Local_CS*>(block->locals());
    
    if(locals!= NULL) {
        buf->appendf("(%d) ", locals->owner_cs_num());
    if(locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_SOURCE) {
        buf->appendf("src = %s ", locals->security_src().c_str());
    }
    if(locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_DEST) {
        buf->appendf("dest = %s ", locals->security_dest().c_str());
    }

    }
    return 0;
}

//----------------------------------------------------------------------
int
AS_BlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                           u_char* buf, size_t len)
{
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    
    // in on-the-fly scenario, process this data for those interested
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if ( block->locals() == NULL ) {      // then we need to parse it
        Ciphersuite::parse(block);
    }
    
    return cc;
}

//----------------------------------------------------------------------
int
AS_BlockProcessor::reload_post_process(Bundle*       bundle,
                                       BlockInfoVec* block_list,
                                       BlockInfo*    block)
{

    // Received blocks might be stored and reloaded and
    // some fields aren't reset.
    // This allows BlockProcessors to reestablish what they
    // need
    
    Ciphersuite* p = NULL;
    int          err = 0;
    int          type = 0;
    BP_Local_CS* locals;
    
    if ( ! block->reloaded() )
        return 0;
        
    type = block->type();
    log_debug_p(log, "AS_BlockProcessor::reload block type %d", type);
    
    Ciphersuite::parse(block);
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) 
        err = p->reload_post_process(bundle, block_list, block);
    
    block->set_reloaded(false);
    return err;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
AS_BlockProcessor::validate(const Bundle*           bundle,
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

    Ciphersuite* p = NULL;
    EndpointID   local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS* locals = dynamic_cast<BP_Local_CS*>(block->locals());
    bool         result = false;

    CS_FAIL_IF_NULL(locals);

    log_debug_p(log, "AS_BlockProcessor::validate() %p ciphersuite %d",
                block, locals->owner_cs_num());
    
    if ( Ciphersuite::destination_is_local_node(bundle, block) )
    {  //yes - this is ours 
        log_debug_p(log, "AS_BlockProcessor::validate() this is destined for us, so we're looking for a ciphersuite validate to run on it.");
        
        p = Ciphersuite::find_suite( locals->owner_cs_num() );
        if ( p != NULL ) {
            result = p->validate(bundle, block_list, block,
                                 reception_reason, deletion_reason);
            return result;
        } else {
            log_err_p(log, "block failed security validation AS_BlockProcessor");
            *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
            return false;
        }
    }

    return true;

 fail:
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;

}

//----------------------------------------------------------------------
int
AS_BlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           BlockInfo::list_owner_t list)
{
    (void)bundle;
    (void)link;
    (void)list;

    Ciphersuite* p = NULL;
    int          result = BP_FAIL;
    BP_Local_CS* source_locals = NULL;
    
    if ( list == BlockInfo::LIST_RECEIVED ) {
        

// XXX/pl  review this to see how much is actually needed
        ASSERT(source != NULL);

        if ( Ciphersuite::destination_is_local_node(bundle, source) )
            return BP_SUCCESS;     //don't forward if it's for here

        xmit_blocks->push_back(BlockInfo(this, source));
        BlockInfo* bp = &(xmit_blocks->back());
        CS_FAIL_IF_NULL( source->locals() );      // then the block is broken

        source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
        Ciphersuite::create_bsp_block_from_source(*bp, bundle, source, list, source_locals->owner_cs_num());
        result = BP_SUCCESS;
    } else {
        if ( source != NULL ) {
            source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
            CS_FAIL_IF_NULL(source_locals);    
            p = Ciphersuite::find_suite( source_locals->owner_cs_num() );
            if ( p != NULL ) {
                result = p->prepare(bundle, xmit_blocks, source, link, list);
            } else {
                log_err_p(log, "AS_BlockProcessor::prepare() - ciphersuite %d is missing",
                          source_locals->owner_cs_num());
            }
        }  // no msg if "source" is NULL, as BundleProtocol calls all BPs that way once
    }
    return result;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
AS_BlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    log_debug_p(log, "AS_BlockProcessor::generate()");
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->generate(bundle, xmit_blocks, block, link, last);
    } else {
        // generate the preamble and copy the data.
        result = Ciphersuite::copy_block_untouched(block, xmit_blocks, last);
    }
    return result;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
AS_BlockProcessor::finalize(const Bundle*  bundle, 
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block, 
                            const LinkRef& link)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)block;
    
    Ciphersuite*    p = NULL;
    int             result = BP_SUCCESS; // If result isn't set by p->finalize, it means we don't know about this cs, and so we're just passing it
                                         // along unchanged.  So, we
                                         // just return BP_SUCCESS
    log_debug_p(log, "AS_BlockProcessor::finalize()");
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        log_debug_p(log, "AS_BlockProcessor::finalize: ciphersuite successfully found (not NULL)");
        log_debug_p(log, "AS_BlockProcessor::finalize: p->cs_num()=%d", p->cs_num());
        result = p->finalize(bundle, xmit_blocks, block, link);
    } 
    log_debug_p(log, "AS_BlockProcessor::finalize(): returning %d",result);
    return result;

 fail:
    log_debug_p(log, "AS_BlockProcessor::finalize(): returning BP_FAIL");
    return BP_FAIL;
}

} // namespace dtn

#endif /* BSP_ENABLED */
