/*
 *    Copyright 2005-2006 Intel Corporation
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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#include "BundleEventHandler.h"

namespace dtn {

/** 
 * Dispatch the event by type code to one of the event-specific
 * handler functions below.
 */
void
BundleEventHandler::dispatch_event(SPtr_BundleEvent& sptr_event)
{
    //log_debug("dispatching event (%p) %s", e, e->type_str());
    
    switch(sptr_event->type_) {

    case BUNDLE_RECEIVED:
        handle_bundle_received(sptr_event);
        break;

    case BUNDLE_TRANSMITTED:
        handle_bundle_transmitted(sptr_event);
        break;

    case BUNDLE_RESTAGED:
        handle_bundle_restaged(sptr_event);
        break;

    case BUNDLE_DELIVERED:
        handle_bundle_delivered(sptr_event);
        break;

    case BUNDLE_ACK:
        handle_bundle_acknowledged_by_app(sptr_event);
        break;

    case BUNDLE_EXPIRED:
    	handle_bundle_expired(sptr_event);
        break;
        
    case BUNDLE_FREE:
        handle_bundle_free(sptr_event);
        break;

    case BUNDLE_SEND:
        handle_bundle_send(sptr_event);
        break;

    case BUNDLE_CANCEL:
        handle_bundle_cancel(sptr_event);
        break;

    case BUNDLE_CANCELLED:
        handle_bundle_cancelled(sptr_event);
        break;

    case BUNDLE_INJECT:
        handle_bundle_inject(sptr_event);
        break;

    case BUNDLE_INJECTED:
        handle_bundle_injected(sptr_event);
        break;

    case DELIVER_BUNDLE_TO_REG:
        handle_deliver_bundle_to_reg(sptr_event);
        break;

    case BUNDLE_DELETE:
        handle_bundle_delete(sptr_event);
        break;

    case BUNDLE_TAKE_CUSTODY:
        handle_bundle_take_custody(sptr_event);
        break;

    case BUNDLE_CUSTODY_ACCEPTED:
        handle_bundle_custody_accepted(sptr_event);
        break;

    case BUNDLE_TRY_DELETE:
        handle_bundle_try_delete_request(sptr_event);
        break;

    case BUNDLE_ACCEPT_REQUEST:
        handle_bundle_accept(sptr_event);
        break;

    case BUNDLE_QUERY:
        handle_bundle_query(sptr_event);
        break;

    case BUNDLE_REPORT:
        handle_bundle_report(sptr_event);
        break;
        
    case BUNDLE_ATTRIB_QUERY:
        handle_bundle_attributes_query(sptr_event);
        break;

    case PRIVATE:
        handle_private(sptr_event);
        break;

    case BUNDLE_ATTRIB_REPORT:
        handle_bundle_attributes_report(sptr_event);
        break;
        
    case REGISTRATION_ADDED:
        handle_registration_added(sptr_event);
        break;

    case REGISTRATION_REMOVED:
        handle_registration_removed(sptr_event);
        break;

    case REGISTRATION_EXPIRED:
        handle_registration_expired(sptr_event);
        break;
 
    case REGISTRATION_DELETE:
        handle_registration_delete(sptr_event);
        break;

    case STORE_BUNDLE_UPDATE:
        handle_store_bundle_update(sptr_event);
        break;

    case STORE_BUNDLE_DELETE:
        handle_store_bundle_delete(sptr_event);
        break;

    case STORE_PENDINGACS_UPDATE:
        handle_store_pendingacs_update(sptr_event);
        break;

    case STORE_PENDINGACS_DELETE:
        handle_store_pendingacs_delete(sptr_event);
        break;

    case STORE_REGISTRATION_UPDATE:
        handle_store_registration_update(sptr_event);
        break;

    case STORE_REGISTRATION_DELETE:
        handle_store_registration_delete(sptr_event);
        break;

    case STORE_LINK_UPDATE:
        handle_store_link_update(sptr_event);
        break;

    case STORE_LINK_DELETE:
        handle_store_link_delete(sptr_event);
        break;

    case STORE_IMC_RECS_UPDATE:
        handle_store_imc_recs_update(sptr_event);
        break;

    case STORE_IMC_RECS_DELETE:
        handle_store_imc_recs_delete(sptr_event);
        break;

   case ROUTE_ADD:
        handle_route_add(sptr_event);
        break;

   case ROUTE_RECOMPUTE:
        handle_route_recompute(sptr_event);
        break;

    case ROUTE_DEL:
        handle_route_del(sptr_event);
        break;

    case ROUTE_QUERY:
        handle_route_query(sptr_event);
        break;

    case ROUTE_REPORT:
        handle_route_report(sptr_event);
        break;

    case ROUTE_IMC_BUNDLE:
        handle_route_imc_bundle(sptr_event);
        break;

    case CONTACT_UP:
        handle_contact_up(sptr_event);
        break;

    case CONTACT_DOWN:
        handle_contact_down(sptr_event);
        break;

    case CONTACT_QUERY:
        handle_contact_query(sptr_event);
        break;

    case CONTACT_REPORT:
        handle_contact_report(sptr_event);
        break;

    case CONTACT_ATTRIB_CHANGED:
        handle_contact_attribute_changed(sptr_event);
        break;

    case LINK_CREATED:
        handle_link_created(sptr_event);
        break;

    case LINK_DELETED:
        handle_link_deleted(sptr_event);
        break;

    case LINK_AVAILABLE:
        handle_link_available(sptr_event);
        break;

    case LINK_UNAVAILABLE:
        handle_link_unavailable(sptr_event);
        break;

    case LINK_CHECK_DEFERRED:
        handle_link_check_deferred(sptr_event);
        break;

    case LINK_STATE_CHANGE_REQUEST:
        handle_link_state_change_request(sptr_event);
        break;

    case LINK_CANCEL_ALL_BUNDLES_REQUEST:
        handle_link_cancel_all_bundles_request(sptr_event);
        break;

    case LINK_CREATE:
        handle_link_create(sptr_event);
        break;

    case LINK_DELETE:
        handle_link_delete(sptr_event);
        break;

    case LINK_RECONFIGURE:
        handle_link_reconfigure(sptr_event);
        break;

    case LINK_QUERY:
        handle_link_query(sptr_event);
        break;

    case LINK_REPORT:
        handle_link_report(sptr_event);
        break;
        
    case LINK_ATTRIB_CHANGED:
        handle_link_attribute_changed(sptr_event);
        break;

    case REASSEMBLY_COMPLETED:
        handle_reassembly_completed(sptr_event);
        break;

    case CUSTODY_SIGNAL:
        handle_custody_signal(sptr_event);
        break;

    case CUSTODY_TIMEOUT:
        handle_custody_timeout(sptr_event);
        break;

    case DAEMON_SHUTDOWN:
        handle_shutdown_request(sptr_event);
        break;

    case DAEMON_STATUS:
        handle_status_request(sptr_event);
        break;

    case CLA_SET_PARAMS:
        handle_cla_set_params(sptr_event);
        break;

    case CLA_PARAMS_SET:
        handle_cla_params_set(sptr_event);
        break;

    case CLA_SET_LINK_DEFAULTS:
        handle_set_link_defaults(sptr_event);
        break;

    case CLA_EID_REACHABLE:
        handle_new_eid_reachable(sptr_event);
        break;

    case CLA_BUNDLE_QUEUED_QUERY:
        handle_bundle_queued_query(sptr_event);
        break;

    case CLA_BUNDLE_QUEUED_REPORT:
        handle_bundle_queued_report(sptr_event);
        break;

    case CLA_EID_REACHABLE_QUERY:
        handle_eid_reachable_query(sptr_event);
        break;

    case CLA_EID_REACHABLE_REPORT:
        handle_eid_reachable_report(sptr_event);
        break;

    case CLA_LINK_ATTRIB_QUERY:
        handle_link_attributes_query(sptr_event);
        break;

    case CLA_LINK_ATTRIB_REPORT:
        handle_link_attributes_report(sptr_event);
        break;

    case CLA_IFACE_ATTRIB_QUERY:
        handle_iface_attributes_query(sptr_event);
        break;

    case CLA_IFACE_ATTRIB_REPORT:
        handle_iface_attributes_report(sptr_event);
        break;

    case CLA_PARAMS_QUERY:
        handle_cla_parameters_query(sptr_event);
        break;

    case CLA_PARAMS_REPORT:
        handle_cla_parameters_report(sptr_event);
        break;

    case AGGREGATE_CUSTODY_SIGNAL:
        handle_aggregate_custody_signal(sptr_event);
        break;

    case ISSUE_AGGREGATE_CUSTODY_SIGNAL:
        handle_add_bundle_to_acs(sptr_event);
        break;

    case ACS_EXPIRED_EVENT:
        handle_acs_expired(sptr_event);
        break;

    case EXTERNAL_ROUTER_ACS:
        handle_external_router_acs(sptr_event);
        break;

#ifdef DTPC_ENABLED
    case DTPC_TOPIC_REGISTRATION:
        handle_dtpc_topic_registration(sptr_event);
        break;

    case DTPC_TOPIC_UNREGISTRATION:
        handle_dtpc_topic_unregistration(sptr_event);
        break;

    case DTPC_SEND_DATA_ITEM:
        handle_dtpc_send_data_item(sptr_event);
        break;

    case DTPC_PAYLOAD_AGGREGATION_EXPIRED:
        handle_dtpc_payload_aggregation_timer_expired(sptr_event);
        break;

    case DTPC_PDU_TRANSMITTED:
        handle_dtpc_transmitted_event(sptr_event);
        break;

    case DTPC_PDU_DELETE:
        handle_dtpc_delete_request(sptr_event);
        break;

    case DTPC_RETRANSMIT_TIMER_EXPIRED:
        handle_dtpc_retransmit_timer_expired(sptr_event);
        break;

    case DTPC_ACK_RECEIVED:
        handle_dtpc_ack_received_event(sptr_event);
        break;

    case DTPC_DATA_RECEIVED:
        handle_dtpc_data_received_event(sptr_event);
        break;

    case DTPC_DELIVER_PDU_TIMER_EXPIRED:
        handle_dtpc_deliver_pdu_event(sptr_event);
        break;

    case DTPC_TOPIC_EXPIRATION_CHECK:
        handle_dtpc_topic_expiration_check(sptr_event);
        break;

    case DTPC_ELISION_FUNC_RESPONSE:
        handle_dtpc_elision_func_response(sptr_event);
        break;
#endif // DTPC_ENABLED

    default:
        PANIC("unimplemented event type %d", sptr_event->type_);
    }
}

/**
 * Default event handler for new bundle arrivals.
 */
void
BundleEventHandler::handle_bundle_received(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler when bundles are transmitted.
 */
void
BundleEventHandler::handle_bundle_transmitted(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when bundles are restaged
 */
void
BundleEventHandler::handle_bundle_restaged(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when bundles are locally delivered.
 */
void
BundleEventHandler::handle_bundle_delivered(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when bundles are acknowledged by apps.
 */
void
BundleEventHandler::handle_bundle_acknowledged_by_app(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when bundles expire.
 */
void
BundleEventHandler::handle_bundle_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when bundles are free (i.e. no more
 * references).
 */
void
BundleEventHandler::handle_bundle_free(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle send requests
 */
void
BundleEventHandler::handle_bundle_send(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for send bundle request cancellations
 */
void
BundleEventHandler::handle_bundle_cancel(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle cancellations.
 */
void
BundleEventHandler::handle_bundle_cancelled(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle inject requests
 */
void
BundleEventHandler::handle_bundle_inject(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle injected events.
 */
void
BundleEventHandler::handle_bundle_injected(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for delivery to registration events
 */
void
BundleEventHandler::handle_deliver_bundle_to_reg(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle delete requests.
 */
void
BundleEventHandler::handle_bundle_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for the take custody of bundle requests.
 */
void
BundleEventHandler::handle_bundle_take_custody(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for the take custody of bundle requests.
 */
void
BundleEventHandler::handle_bundle_custody_accepted(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for request to try to delete a bundle
 */
void
BundleEventHandler::handle_bundle_try_delete_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for a bundle accept request probe.
 */
void
BundleEventHandler::handle_bundle_accept(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle query requests.
 */
void
BundleEventHandler::handle_bundle_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle reports.
 */
void
BundleEventHandler::handle_bundle_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle attribute query requests.
 */
void
BundleEventHandler::handle_bundle_attributes_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for bundle attribute reports.
 */
void
BundleEventHandler::handle_bundle_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}


/**
 * Default event handler for a private event.
 */
void 
BundleEventHandler::handle_private(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}


/**
 * Default event handler when a new application registration
 * arrives.
 */
void
BundleEventHandler::handle_registration_added(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a registration is removed.
 */
void
BundleEventHandler::handle_registration_removed(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a registration expires.
 */
void
BundleEventHandler::handle_registration_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a registration is to be deleted
 */
void
BundleEventHandler::handle_registration_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store bundle update events.
 */
void
BundleEventHandler::handle_store_bundle_update(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store bundle delete events.
 */
void
BundleEventHandler::handle_store_bundle_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store pendingacs update events.
 */
void
BundleEventHandler::handle_store_pendingacs_update(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store pendingacs delete events.
 */
void
BundleEventHandler::handle_store_pendingacs_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store registration update events.
 */
void
BundleEventHandler::handle_store_registration_update(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store registration delete events.
 */
void
BundleEventHandler::handle_store_registration_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store link update events.
 */
void
BundleEventHandler::handle_store_link_update(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store link delete events.
 */
void
BundleEventHandler::handle_store_link_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store IMC Recs update events.
 */
void
BundleEventHandler::handle_store_imc_recs_update(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for store IMC Recs delete events.
 */
void
BundleEventHandler::handle_store_imc_recs_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}


/**
 * Default event handler when a new contact is up.
 */
void
BundleEventHandler::handle_contact_up(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler when a contact is down.
 */
void
BundleEventHandler::handle_contact_down(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for contact query requests.
 */
void
BundleEventHandler::handle_contact_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for contact reports.
 */
void
BundleEventHandler::handle_contact_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for contact attribute changes.
 */
void
BundleEventHandler::handle_contact_attribute_changed(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a new link is created.
 */
void
BundleEventHandler::handle_link_created(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler when a link is deleted.
 */
void
BundleEventHandler::handle_link_deleted(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when link becomes available
 */
void
BundleEventHandler::handle_link_available(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a link is unavailable
 */
void
BundleEventHandler::handle_link_unavailable(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for the link check deferred bundles event
 */
void
BundleEventHandler::handle_link_check_deferred(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link state change requests.
 */
void
BundleEventHandler::handle_link_state_change_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
* Default event handler for link cancel all bundles request
*/
void
BundleEventHandler::handle_link_cancel_all_bundles_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link create requests.
 */
void
BundleEventHandler::handle_link_create(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link delete requests.
 */
void
BundleEventHandler::handle_link_delete(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link reconfiguration requests.
 */
void
BundleEventHandler::handle_link_reconfigure(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link query requests.
 */
void
BundleEventHandler::handle_link_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link reports.
 */
void
BundleEventHandler::handle_link_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for link attribute changes.
 */
void
BundleEventHandler::handle_link_attribute_changed(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when reassembly is completed.
 */
void
BundleEventHandler::handle_reassembly_completed(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler when a new route is added by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_add(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler to check bundle routing after new routes have been added
 */
void
BundleEventHandler::handle_route_recompute(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a route is deleted by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_del(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for static route query requests.
 */
void
BundleEventHandler::handle_route_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for static route reports.
 */
void
BundleEventHandler::handle_route_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for route IMC bundle events
 */
void 
BundleEventHandler::handle_route_imc_bundle(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when custody signals are received.
 */
void
BundleEventHandler::handle_custody_signal(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler when custody transfer timers expire
 */
void
BundleEventHandler::handle_custody_timeout(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}
    
/**
 * Default event handler for shutdown requests.
 */
void
BundleEventHandler::handle_shutdown_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for status requests.
 */
void
BundleEventHandler::handle_status_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for CLA parameter set requests.
 */
void
BundleEventHandler::handle_cla_set_params(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for CLA parameters set events.
 */
void
BundleEventHandler::handle_cla_params_set(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for set link defaults requests.
 */
void
BundleEventHandler::handle_set_link_defaults(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for new EIDs discovered by CLA.
 */
void
BundleEventHandler::handle_new_eid_reachable(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handlers for queries to and reports from the CLA.
 */
void
BundleEventHandler::handle_bundle_queued_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_bundle_queued_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_eid_reachable_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_eid_reachable_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_link_attributes_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_link_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_iface_attributes_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_iface_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_cla_parameters_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

void
BundleEventHandler::handle_cla_parameters_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when custody signals are received.
 */
void
BundleEventHandler::handle_aggregate_custody_signal(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for issuing an aggregate custody signal
 */
void
BundleEventHandler::handle_add_bundle_to_acs(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for an aggregate custody signal expiration
 */
void
BundleEventHandler::handle_acs_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler for an External Router ACS
 */
void
BundleEventHandler::handle_external_router_acs(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

#ifdef DTPC_ENABLED
/**
 * Default event handler when a DTPC topic is registered
 */
void
BundleEventHandler::handle_dtpc_topic_registration(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a DTPC topic is unregistered
 */
void
BundleEventHandler::handle_dtpc_topic_unregistration(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a DTPC data item is sent
 */
void
BundleEventHandler::handle_dtpc_send_data_item(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC Payload Aggregation Timer expires
 */
void
BundleEventHandler::handle_dtpc_payload_aggregation_timer_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC transmits a PDU
 */
void 
BundleEventHandler::handle_dtpc_transmitted_event(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC requests deletion of a PDU
 */
void 
BundleEventHandler::handle_dtpc_delete_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC Retransmit Timer expires
 */
void
BundleEventHandler::handle_dtpc_retransmit_timer_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC ACK PDUs are received
 */
void 
BundleEventHandler::handle_dtpc_ack_received_event(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when DTPC Data PDUs are received
 */
void 
BundleEventHandler::handle_dtpc_data_received_event(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a DTPC Deliver PDU Timer expires
 */
void 
BundleEventHandler::handle_dtpc_deliver_pdu_event(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

/**
 * Default event handler when a DTPC Topic Expiration Timer expires
 */
void
BundleEventHandler::handle_dtpc_topic_expiration_check(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}


/**
 * Default event handler when a DTPC elision function response is received
 */
void
BundleEventHandler::handle_dtpc_elision_func_response(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

#endif // DTPC_ENABLED
} // namespace dtn
