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

#include "BA_BlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

static const char* log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
BA_BlockProcessor::BA_BlockProcessor()
    : AS_BlockProcessor(BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK)
{
}

//----------------------------------------------------------------------
int BA_BlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *block) {
    buf->append("BAB ");
    return AS_BlockProcessor::format(buf, block);
}
//----------------------------------------------------------------------
bool
BA_BlockProcessor::validate(const Bundle*           bundle,
                            BlockInfoVec*           block_list,
                            BlockInfo*              block,
                            status_report_reason_t* reception_reason,
                            status_report_reason_t* deletion_reason)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    (void)reception_reason;

    Ciphersuite*    p = NULL;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    bool            result = false;

    CS_FAIL_IF_NULL(locals);

    log_debug_p(log, "BA_BlockProcessor::validate: %p ciphersuite %d",
                block, locals->owner_cs_num());
    
    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->validate(bundle, block_list, block,
                             reception_reason, deletion_reason);
    } else {
        log_err_p(log, "BA_BlockProcessor::validate: block failed security validation BA_BlockProcessor");
        *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    }

    return result;


 fail:
    
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;


    if ( list == BlockInfo::LIST_NONE || source == NULL ) {
    	return BP_SUCCESS;
    }
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(source->locals());
    CS_FAIL_IF_NULL(locals);

    log_debug_p(log, "BA_BlockProcessor::prepare: ciphersuite %d",
                locals->owner_cs_num());

    switch ( list ) {
    case BlockInfo::LIST_API:
    case BlockInfo::LIST_EXT:
        // we expect a specific ciphersuite, rather than the
        // generic block processor. See if we have one, and fail
        // if we do not
            
        p = Ciphersuite::find_suite(locals->owner_cs_num());
        if ( p == NULL ) {
            log_err_p(log, "BA_BlockProcessor::prepare: Couldn't find ciphersuite in registration!");
            return result;
        }
            
        // Now we know the suite, get it to prepare its own block
        result = p->prepare( bundle, xmit_blocks, source, link, list );
        if(result == BP_FAIL) {
            log_err_p(log, "BA_BlockProcessor::prepare: The ciphersuite prepare returned BP_FAIL");
        }
        break;
        
    default:
        log_debug_p(log, "BA_BlockProcessor::prepare: We landed in the default case");
        return BP_SUCCESS;
        break;
            
    }
    
    
    
    return result;

 fail:
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)block;
    (void)last;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->generate(bundle, xmit_blocks, block, link, last);
    } else {
        log_err_p(log, "BA_BlockProcessor::generate: - ciphersuite %d is missing",
                  locals->owner_cs_num());
    }
    
    return result;

 fail:
    return BP_FAIL;
}

} // namespace dtn

#endif /* BSP_ENABLED */
