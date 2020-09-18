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


#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include "EhsEventHandler.h"

namespace dtn {

/** 
 * Dispatch the event by type code to one of the event-specific
 * handler functions below.
 */
void
EhsEventHandler::dispatch_event(EhsEvent* e)
{
    log_debug("dispatching event (%p) %s", e, e->type_str());
    
    switch(e->type_) {

    case EHS_BPA_RECEIVED:
        handle_bpa_received((EhsBpaReceivedEvent*)e);
        break;

    case EHS_FREE_BUNDLE_REQ:
        handle_free_bundle_req((EhsFreeBundleReq*)e);
        break;

    case EHS_ROUTE_BUNDLE_REQ:
        handle_route_bundle_req((EhsRouteBundleReq*)e);
        break;

    case EHS_ROUTE_BUNDLE_LIST_REQ:
        handle_route_bundle_list_req((EhsRouteBundleListReq*)e);
        break;

    case EHS_DELETE_BUNDLE_REQ:
        handle_delete_bundle_req((EhsDeleteBundleReq*)e);
        break;

    case EHS_RECONFIGURE_LINK_REQ:
        handle_reconfigure_link_req((EhsReconfigureLinkReq*)e);
        break;

    case EHS_BUNDLE_TRANSMITTED:
        handle_bundle_transmitted_event((EhsBundleTransmittedEvent*) e);
        break;

    default:
        PANIC("unimplemented event type %d", e->type_);
    }
}

void EhsEventHandler::handle_bpa_received(EhsBpaReceivedEvent*) { }

void EhsEventHandler::handle_free_bundle_req(EhsFreeBundleReq*) { }

void EhsEventHandler::handle_route_bundle_req(EhsRouteBundleReq*) { }

void EhsEventHandler::handle_route_bundle_list_req(EhsRouteBundleListReq*) { }

void EhsEventHandler::handle_delete_bundle_req(EhsDeleteBundleReq*) { }

void EhsEventHandler::handle_reconfigure_link_req(EhsReconfigureLinkReq*) { }

void EhsEventHandler::handle_bundle_transmitted_event(EhsBundleTransmittedEvent*) { }

} // namespace dtn

#endif // defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#endif // EHSROUTER_ENABLED

