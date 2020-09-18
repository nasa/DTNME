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

#include "BundleEventHandler.h"

namespace dtn {

/** 
 * Dispatch the event by type code to one of the event-specific
 * handler functions below.
 */
void
BundleEventHandler::dispatch_event(BundleEvent* e)
{
    //log_debug("dispatching event (%p) %s", e, e->type_str());
    
    switch(e->type_) {

    case BUNDLE_RECEIVED:
        handle_bundle_received((BundleReceivedEvent*)e);
        break;

    case BUNDLE_TRANSMITTED:
        handle_bundle_transmitted((BundleTransmittedEvent*)e);
        break;

    case BUNDLE_DELIVERED:
        handle_bundle_delivered((BundleDeliveredEvent*)e);
        break;

    case BUNDLE_ACK:
        handle_bundle_acknowledged_by_app((BundleAckEvent*) e);
        break;

    case BUNDLE_EXPIRED:
    	handle_bundle_expired((BundleExpiredEvent*)e);
        break;
        
    case BUNDLE_FREE:
        handle_bundle_free((BundleFreeEvent*)e);
        break;

    case BUNDLE_SEND:
        handle_bundle_send((BundleSendRequest*)e);
        break;

    case BUNDLE_CANCEL:
        handle_bundle_cancel((BundleCancelRequest*)e);
        break;

    case BUNDLE_CANCELLED:
        handle_bundle_cancelled((BundleSendCancelledEvent*)e);
        break;

    case BUNDLE_INJECT:
        handle_bundle_inject((BundleInjectRequest*)e);
        break;

    case BUNDLE_INJECTED:
        handle_bundle_injected((BundleInjectedEvent*)e);
        break;

    case DELIVER_BUNDLE_TO_REG:
        handle_deliver_bundle_to_reg((DeliverBundleToRegEvent*) e);
        break;

    case BUNDLE_DELETE:
        handle_bundle_delete((BundleDeleteRequest*)e);
        break;

    case BUNDLE_TAKE_CUSTODY:
        handle_bundle_take_custody((BundleTakeCustodyRequest*)e);
        break;

    case BUNDLE_CUSTODY_ACCEPTED:
        handle_bundle_custody_accepted((BundleCustodyAcceptedEvent*)e);
        break;

    case BUNDLE_ACCEPT_REQUEST:
        handle_bundle_accept((BundleAcceptRequest*)e);
        break;

    case BUNDLE_QUERY:
        handle_bundle_query((BundleQueryRequest*)e);
        break;

    case BUNDLE_REPORT:
        handle_bundle_report((BundleReportEvent*)e);
        break;
        
    case BUNDLE_ATTRIB_QUERY:
        handle_bundle_attributes_query((BundleAttributesQueryRequest*)e);
        break;

    case PRIVATE:
        handle_private((PrivateEvent*)e);
        break;

    case BUNDLE_ATTRIB_REPORT:
        handle_bundle_attributes_report((BundleAttributesReportEvent*)e);
        break;
        
    case REGISTRATION_ADDED:
        handle_registration_added((RegistrationAddedEvent*)e);
        break;

    case REGISTRATION_REMOVED:
        handle_registration_removed((RegistrationRemovedEvent*)e);
        break;

    case REGISTRATION_EXPIRED:
        handle_registration_expired((RegistrationExpiredEvent*)e);
        break;
 
    case REGISTRATION_DELETE:
        handle_registration_delete((RegistrationDeleteRequest*)e);
        break;

    case STORE_BUNDLE_UPDATE:
        handle_store_bundle_update((StoreBundleUpdateEvent*)e);
        break;

    case STORE_BUNDLE_DELETE:
        handle_store_bundle_delete((StoreBundleDeleteEvent*)e);
        break;

#ifdef ACS_ENABLED 
    case STORE_PENDINGACS_UPDATE:
        handle_store_pendingacs_update((StorePendingAcsUpdateEvent*)e);
        break;

    case STORE_PENDINGACS_DELETE:
        handle_store_pendingacs_delete((StorePendingAcsDeleteEvent*)e);
        break;
#endif // ACS_ENABLED 

    case STORE_REGISTRATION_UPDATE:
        handle_store_registration_update((StoreRegistrationUpdateEvent*)e);
        break;

    case STORE_REGISTRATION_DELETE:
        handle_store_registration_delete((StoreRegistrationDeleteEvent*)e);
        break;

    case STORE_LINK_UPDATE:
        handle_store_link_update((StoreLinkUpdateEvent*)e);
        break;

    case STORE_LINK_DELETE:
        handle_store_link_delete((StoreLinkDeleteEvent*)e);
        break;

   case ROUTE_ADD:
        handle_route_add((RouteAddEvent*)e);
        break;

   case ROUTE_RECOMPUTE:
        handle_route_recompute((RouteRecomputeEvent*)e);
        break;

    case ROUTE_DEL:
        handle_route_del((RouteDelEvent*)e);
        break;

    case ROUTE_QUERY:
        handle_route_query((RouteQueryRequest*)e);
        break;

    case ROUTE_REPORT:
        handle_route_report((RouteReportEvent*)e);
        break;

    case CONTACT_UP:
        handle_contact_up((ContactUpEvent*)e);
        break;

    case CONTACT_DOWN:
        handle_contact_down((ContactDownEvent*)e);
        break;

    case CONTACT_QUERY:
        handle_contact_query((ContactQueryRequest*)e);
        break;

    case CONTACT_REPORT:
        handle_contact_report((ContactReportEvent*)e);
        break;

    case CONTACT_ATTRIB_CHANGED:
        handle_contact_attribute_changed((ContactAttributeChangedEvent*)e);
        break;

    case LINK_CREATED:
        handle_link_created((LinkCreatedEvent*)e);
        break;

    case LINK_DELETED:
        handle_link_deleted((LinkDeletedEvent*)e);
        break;

    case LINK_AVAILABLE:
        handle_link_available((LinkAvailableEvent*)e);
        break;

    case LINK_UNAVAILABLE:
        handle_link_unavailable((LinkUnavailableEvent*)e);
        break;

    case LINK_CHECK_DEFERRED:
        handle_link_check_deferred((LinkCheckDeferredEvent*)e);
        break;

    case LINK_STATE_CHANGE_REQUEST:
        handle_link_state_change_request((LinkStateChangeRequest*)e);
        break;

    case LINK_CANCEL_ALL_BUNDLES_REQUEST:
        handle_link_cancel_all_bundles_request((LinkCancelAllBundlesRequest*)e);
        break;

    case LINK_CREATE:
        handle_link_create((LinkCreateRequest*)e);
        break;

    case LINK_DELETE:
        handle_link_delete((LinkDeleteRequest*)e);
        break;

    case LINK_RECONFIGURE:
        handle_link_reconfigure((LinkReconfigureRequest*)e);
        break;

    case LINK_QUERY:
        handle_link_query((LinkQueryRequest*)e);
        break;

    case LINK_REPORT:
        handle_link_report((LinkReportEvent*)e);
        break;
        
    case LINK_ATTRIB_CHANGED:
        handle_link_attribute_changed((LinkAttributeChangedEvent*)e);
        break;

    case REASSEMBLY_COMPLETED:
        handle_reassembly_completed((ReassemblyCompletedEvent*)e);
        break;

    case CUSTODY_SIGNAL:
        handle_custody_signal((CustodySignalEvent*)e);
        break;

    case CUSTODY_TIMEOUT:
        handle_custody_timeout((CustodyTimeoutEvent*)e);
        break;

    case DAEMON_SHUTDOWN:
        handle_shutdown_request((ShutdownRequest*)e);
        break;

    case DAEMON_STATUS:
        handle_status_request((StatusRequest*)e);
        break;

    case CLA_SET_PARAMS:
        handle_cla_set_params((CLASetParamsRequest*)e);
        break;

    case CLA_PARAMS_SET:
        handle_cla_params_set((CLAParamsSetEvent*)e);
        break;

    case CLA_SET_LINK_DEFAULTS:
        handle_set_link_defaults((SetLinkDefaultsRequest*)e);
        break;

    case CLA_EID_REACHABLE:
        handle_new_eid_reachable((NewEIDReachableEvent*)e);
        break;

    case CLA_BUNDLE_QUEUED_QUERY:
        handle_bundle_queued_query((BundleQueuedQueryRequest*)e);
        break;

    case CLA_BUNDLE_QUEUED_REPORT:
        handle_bundle_queued_report((BundleQueuedReportEvent*)e);
        break;

    case CLA_EID_REACHABLE_QUERY:
        handle_eid_reachable_query((EIDReachableQueryRequest*)e);
        break;

    case CLA_EID_REACHABLE_REPORT:
        handle_eid_reachable_report((EIDReachableReportEvent*)e);
        break;

    case CLA_LINK_ATTRIB_QUERY:
        handle_link_attributes_query((LinkAttributesQueryRequest*)e);
        break;

    case CLA_LINK_ATTRIB_REPORT:
        handle_link_attributes_report((LinkAttributesReportEvent*)e);
        break;

    case CLA_IFACE_ATTRIB_QUERY:
        handle_iface_attributes_query((IfaceAttributesQueryRequest*)e);
        break;

    case CLA_IFACE_ATTRIB_REPORT:
        handle_iface_attributes_report((IfaceAttributesReportEvent*)e);
        break;

    case CLA_PARAMS_QUERY:
        handle_cla_parameters_query((CLAParametersQueryRequest*)e);
        break;

    case CLA_PARAMS_REPORT:
        handle_cla_parameters_report((CLAParametersReportEvent*)e);
        break;

#ifdef ACS_ENABLED
    case AGGREGATE_CUSTODY_SIGNAL:
        handle_aggregate_custody_signal((AggregateCustodySignalEvent*)e);
        break;

    case ISSUE_AGGREGATE_CUSTODY_SIGNAL:
        handle_add_bundle_to_acs((AddBundleToAcsEvent*)e);
        break;

    case ACS_EXPIRED_EVENT:
        handle_acs_expired((AcsExpiredEvent*)e);
        break;
#endif // ACS_ENABLED

    case EXTERNAL_ROUTER_ACS:  // purposely not inside the ACS_ENABLED block
        handle_external_router_acs((ExternalRouterAcsEvent*)e);
        break;

#ifdef DTPC_ENABLED
    case DTPC_TOPIC_REGISTRATION:
        handle_dtpc_topic_registration((DtpcTopicRegistrationEvent*)e);
        break;

    case DTPC_TOPIC_UNREGISTRATION:
        handle_dtpc_topic_unregistration((DtpcTopicUnregistrationEvent*)e);
        break;

    case DTPC_SEND_DATA_ITEM:
        handle_dtpc_send_data_item((DtpcSendDataItemEvent*) e);
        break;

    case DTPC_PAYLOAD_AGGREGATION_EXPIRED:
        handle_dtpc_payload_aggregation_timer_expired((DtpcPayloadAggregationTimerExpiredEvent*) e);
        break;

    case DTPC_PDU_TRANSMITTED:
        handle_dtpc_transmitted_event((DtpcPduTransmittedEvent*) e);
        break;

    case DTPC_PDU_DELETE:
        handle_dtpc_delete_request((DtpcPduDeleteRequest*) e);
        break;

    case DTPC_RETRANSMIT_TIMER_EXPIRED:
        handle_dtpc_retransmit_timer_expired((DtpcRetransmitTimerExpiredEvent*) e);
        break;

    case DTPC_ACK_RECEIVED:
        handle_dtpc_ack_received_event((DtpcAckReceivedEvent*) e);
        break;

    case DTPC_DATA_RECEIVED:
        handle_dtpc_data_received_event((DtpcDataReceivedEvent*) e);
        break;

    case DTPC_DELIVER_PDU_TIMER_EXPIRED:
        handle_dtpc_deliver_pdu_event((DtpcDeliverPduTimerExpiredEvent*) e);
        break;

    case DTPC_TOPIC_EXPIRATION_CHECK:
        handle_dtpc_topic_expiration_check((DtpcTopicExpirationCheckEvent*) e);
        break;

    case DTPC_ELISION_FUNC_RESPONSE:
        handle_dtpc_elision_func_response((DtpcElisionFuncResponse*) e);
        break;
#endif // DTPC_ENABLED

    default:
        PANIC("unimplemented event type %d", e->type_);
    }
}

/**
 * Default event handler for new bundle arrivals.
 */
void
BundleEventHandler::handle_bundle_received(BundleReceivedEvent*)
{
}
    
/**
 * Default event handler when bundles are transmitted.
 */
void
BundleEventHandler::handle_bundle_transmitted(BundleTransmittedEvent*)
{
}

/**
 * Default event handler when bundles are locally delivered.
 */
void
BundleEventHandler::handle_bundle_delivered(BundleDeliveredEvent*)
{
}

/**
 * Default event handler when bundles are acknowledged by apps.
 */
void
BundleEventHandler::handle_bundle_acknowledged_by_app(BundleAckEvent*)
{
}

/**
 * Default event handler when bundles expire.
 */
void
BundleEventHandler::handle_bundle_expired(BundleExpiredEvent*)
{
}

/**
 * Default event handler when bundles are free (i.e. no more
 * references).
 */
void
BundleEventHandler::handle_bundle_free(BundleFreeEvent*)
{
}

/**
 * Default event handler for bundle send requests
 */
void
BundleEventHandler::handle_bundle_send(BundleSendRequest*)
{
}

/**
 * Default event handler for send bundle request cancellations
 */
void
BundleEventHandler::handle_bundle_cancel(BundleCancelRequest*)
{
}

/**
 * Default event handler for bundle cancellations.
 */
void
BundleEventHandler::handle_bundle_cancelled(BundleSendCancelledEvent*)
{
}

/**
 * Default event handler for bundle inject requests
 */
void
BundleEventHandler::handle_bundle_inject(BundleInjectRequest*)
{
}

/**
 * Default event handler for bundle injected events.
 */
void
BundleEventHandler::handle_bundle_injected(BundleInjectedEvent*)
{
}

/**
 * Default event handler for delivery to registration events
 */
void
BundleEventHandler::handle_deliver_bundle_to_reg(DeliverBundleToRegEvent*)
{
}

/**
 * Default event handler for bundle delete requests.
 */
void
BundleEventHandler::handle_bundle_delete(BundleDeleteRequest*)
{
}

/**
 * Default event handler for the take custody of bundle requests.
 */
void
BundleEventHandler::handle_bundle_take_custody(BundleTakeCustodyRequest*)
{
}

/**
 * Default event handler for the take custody of bundle requests.
 */
void
BundleEventHandler::handle_bundle_custody_accepted(BundleCustodyAcceptedEvent*)
{
}

/**
 * Default event handler for a bundle accept request probe.
 */
void
BundleEventHandler::handle_bundle_accept(BundleAcceptRequest*)
{
}

/**
 * Default event handler for bundle query requests.
 */
void
BundleEventHandler::handle_bundle_query(BundleQueryRequest*)
{
}

/**
 * Default event handler for bundle reports.
 */
void
BundleEventHandler::handle_bundle_report(BundleReportEvent*)
{
}

/**
 * Default event handler for bundle attribute query requests.
 */
void
BundleEventHandler::handle_bundle_attributes_query(BundleAttributesQueryRequest*)
{
}

/**
 * Default event handler for bundle attribute reports.
 */
void
BundleEventHandler::handle_bundle_attributes_report(BundleAttributesReportEvent*)
{
}


/**
 * Default event handler for a private event.
 */
void 
BundleEventHandler::handle_private(PrivateEvent*)
{
}


/**
 * Default event handler when a new application registration
 * arrives.
 */
void
BundleEventHandler::handle_registration_added(RegistrationAddedEvent*)
{
}

/**
 * Default event handler when a registration is removed.
 */
void
BundleEventHandler::handle_registration_removed(RegistrationRemovedEvent*)
{
}

/**
 * Default event handler when a registration expires.
 */
void
BundleEventHandler::handle_registration_expired(RegistrationExpiredEvent*)
{
}

/**
 * Default event handler when a registration is to be deleted
 */
void
BundleEventHandler::handle_registration_delete(RegistrationDeleteRequest*)
{
}

/**
 * Default event handler for store bundle update events.
 */
void
BundleEventHandler::handle_store_bundle_update(StoreBundleUpdateEvent*)
{
}

/**
 * Default event handler for store bundle delete events.
 */
void
BundleEventHandler::handle_store_bundle_delete(StoreBundleDeleteEvent*)
{
}

#ifdef ACS_ENABLED 
/**
 * Default event handler for store pendingacs update events.
 */
void
BundleEventHandler::handle_store_pendingacs_update(StorePendingAcsUpdateEvent*)
{
}

/**
 * Default event handler for store pendingacs delete events.
 */
void
BundleEventHandler::handle_store_pendingacs_delete(StorePendingAcsDeleteEvent*)
{
}
#endif // ACS_ENABLED 

/**
 * Default event handler for store registration update events.
 */
void
BundleEventHandler::handle_store_registration_update(StoreRegistrationUpdateEvent*)
{
}

/**
 * Default event handler for store registration delete events.
 */
void
BundleEventHandler::handle_store_registration_delete(StoreRegistrationDeleteEvent*)
{
}

/**
 * Default event handler for store link update events.
 */
void
BundleEventHandler::handle_store_link_update(StoreLinkUpdateEvent*)
{
}

/**
 * Default event handler for store link delete events.
 */
void
BundleEventHandler::handle_store_link_delete(StoreLinkDeleteEvent*)
{
}

/**
 * Default event handler when a new contact is up.
 */
void
BundleEventHandler::handle_contact_up(ContactUpEvent*)
{
}
    
/**
 * Default event handler when a contact is down.
 */
void
BundleEventHandler::handle_contact_down(ContactDownEvent*)
{
}

/**
 * Default event handler for contact query requests.
 */
void
BundleEventHandler::handle_contact_query(ContactQueryRequest*)
{
}

/**
 * Default event handler for contact reports.
 */
void
BundleEventHandler::handle_contact_report(ContactReportEvent*)
{
}

/**
 * Default event handler for contact attribute changes.
 */
void
BundleEventHandler::handle_contact_attribute_changed(ContactAttributeChangedEvent*)
{
}

/**
 * Default event handler when a new link is created.
 */
void
BundleEventHandler::handle_link_created(LinkCreatedEvent*)
{
}
    
/**
 * Default event handler when a link is deleted.
 */
void
BundleEventHandler::handle_link_deleted(LinkDeletedEvent*)
{
}

/**
 * Default event handler when link becomes available
 */
void
BundleEventHandler::handle_link_available(LinkAvailableEvent*)
{
}

/**
 * Default event handler when a link is unavailable
 */
void
BundleEventHandler::handle_link_unavailable(LinkUnavailableEvent*)
{
}

/**
 * Default event handler for the link check deferred bundles event
 */
void
BundleEventHandler::handle_link_check_deferred(LinkCheckDeferredEvent*)
{
}

/**
 * Default event handler for link state change requests.
 */
void
BundleEventHandler::handle_link_state_change_request(LinkStateChangeRequest*)
{
}

/**
* Default event handler for link cancel all bundles request
*/
void
BundleEventHandler::handle_link_cancel_all_bundles_request(LinkCancelAllBundlesRequest*)
{
}

/**
 * Default event handler for link create requests.
 */
void
BundleEventHandler::handle_link_create(LinkCreateRequest*)
{
}

/**
 * Default event handler for link delete requests.
 */
void
BundleEventHandler::handle_link_delete(LinkDeleteRequest*)
{
}

/**
 * Default event handler for link reconfiguration requests.
 */
void
BundleEventHandler::handle_link_reconfigure(LinkReconfigureRequest*)
{
}

/**
 * Default event handler for link query requests.
 */
void
BundleEventHandler::handle_link_query(LinkQueryRequest*)
{
}

/**
 * Default event handler for link reports.
 */
void
BundleEventHandler::handle_link_report(LinkReportEvent*)
{
}

/**
 * Default event handler for link attribute changes.
 */
void
BundleEventHandler::handle_link_attribute_changed(LinkAttributeChangedEvent*)
{
}

/**
 * Default event handler when reassembly is completed.
 */
void
BundleEventHandler::handle_reassembly_completed(ReassemblyCompletedEvent*)
{
}
    
/**
 * Default event handler when a new route is added by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_add(RouteAddEvent*)
{
}

/**
 * Default event handler to check bundle routing after new routes have been added
 */
void
BundleEventHandler::handle_route_recompute(RouteRecomputeEvent*)
{
}

/**
 * Default event handler when a route is deleted by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_del(RouteDelEvent*)
{
}

/**
 * Default event handler for static route query requests.
 */
void
BundleEventHandler::handle_route_query(RouteQueryRequest*)
{
}

/**
 * Default event handler for static route reports.
 */
void
BundleEventHandler::handle_route_report(RouteReportEvent*)
{
}

/**
 * Default event handler when custody signals are received.
 */
void
BundleEventHandler::handle_custody_signal(CustodySignalEvent*)
{
}
    
/**
 * Default event handler when custody transfer timers expire
 */
void
BundleEventHandler::handle_custody_timeout(CustodyTimeoutEvent*)
{
}
    
/**
 * Default event handler for shutdown requests.
 */
void
BundleEventHandler::handle_shutdown_request(ShutdownRequest*)
{
}

/**
 * Default event handler for status requests.
 */
void
BundleEventHandler::handle_status_request(StatusRequest*)
{
}

/**
 * Default event handler for CLA parameter set requests.
 */
void
BundleEventHandler::handle_cla_set_params(CLASetParamsRequest*)
{
}

/**
 * Default event handler for CLA parameters set events.
 */
void
BundleEventHandler::handle_cla_params_set(CLAParamsSetEvent*)
{
}

/**
 * Default event handler for set link defaults requests.
 */
void
BundleEventHandler::handle_set_link_defaults(SetLinkDefaultsRequest*)
{
}

/**
 * Default event handler for new EIDs discovered by CLA.
 */
void
BundleEventHandler::handle_new_eid_reachable(NewEIDReachableEvent*)
{
}

/**
 * Default event handlers for queries to and reports from the CLA.
 */
void
BundleEventHandler::handle_bundle_queued_query(BundleQueuedQueryRequest*)
{
}

void
BundleEventHandler::handle_bundle_queued_report(BundleQueuedReportEvent*)
{
}

void
BundleEventHandler::handle_eid_reachable_query(EIDReachableQueryRequest*)
{
}

void
BundleEventHandler::handle_eid_reachable_report(EIDReachableReportEvent*)
{
}

void
BundleEventHandler::handle_link_attributes_query(LinkAttributesQueryRequest*)
{
}

void
BundleEventHandler::handle_link_attributes_report(LinkAttributesReportEvent*)
{
}

void
BundleEventHandler::handle_iface_attributes_query(IfaceAttributesQueryRequest*)
{
}

void
BundleEventHandler::handle_iface_attributes_report(IfaceAttributesReportEvent*)
{
}

void
BundleEventHandler::handle_cla_parameters_query(CLAParametersQueryRequest*)
{
}

void
BundleEventHandler::handle_cla_parameters_report(CLAParametersReportEvent*)
{
}

#ifdef ACS_ENABLED
/**
 * Default event handler when custody signals are received.
 */
void
BundleEventHandler::handle_aggregate_custody_signal(AggregateCustodySignalEvent*)
{
}

/**
 * Default event handler for issuing an aggregate custody signal
 */
void
BundleEventHandler::handle_add_bundle_to_acs(AddBundleToAcsEvent*)
{
}

/**
 * Default event handler for an aggregate custody signal expiration
 */
void
BundleEventHandler::handle_acs_expired(AcsExpiredEvent*)
{
}
#endif // ACS_ENABLED

/**
 * Default event handler for an External Router ACS -- purposely not in the ACS_ENABLED block
 */
void
BundleEventHandler::handle_external_router_acs(ExternalRouterAcsEvent*)
{
}

#ifdef DTPC_ENABLED
/**
 * Default event handler when a DTPC topic is registered
 */
void
BundleEventHandler::handle_dtpc_topic_registration(DtpcTopicRegistrationEvent*)
{
}

/**
 * Default event handler when a DTPC topic is unregistered
 */
void
BundleEventHandler::handle_dtpc_topic_unregistration(DtpcTopicUnregistrationEvent*)
{
}

/**
 * Default event handler when a DTPC data item is sent
 */
void
BundleEventHandler::handle_dtpc_send_data_item(DtpcSendDataItemEvent*)
{
}

/**
 * Default event handler when DTPC Payload Aggregation Timer expires
 */
void
BundleEventHandler::handle_dtpc_payload_aggregation_timer_expired(DtpcPayloadAggregationTimerExpiredEvent*)
{
}

/**
 * Default event handler when DTPC transmits a PDU
 */
void 
BundleEventHandler::handle_dtpc_transmitted_event(DtpcPduTransmittedEvent*)
{
}

/**
 * Default event handler when DTPC requests deletion of a PDU
 */
void 
BundleEventHandler::handle_dtpc_delete_request(DtpcPduDeleteRequest*)
{
}

/**
 * Default event handler when DTPC Retransmit Timer expires
 */
void
BundleEventHandler::handle_dtpc_retransmit_timer_expired(DtpcRetransmitTimerExpiredEvent*)
{
}

/**
 * Default event handler when DTPC ACK PDUs are received
 */
void 
BundleEventHandler::handle_dtpc_ack_received_event(DtpcAckReceivedEvent*)
{
}

/**
 * Default event handler when DTPC Data PDUs are received
 */
void 
BundleEventHandler::handle_dtpc_data_received_event(DtpcDataReceivedEvent*)
{
}

/**
 * Default event handler when a DTPC Deliver PDU Timer expires
 */
void 
BundleEventHandler::handle_dtpc_deliver_pdu_event(DtpcDeliverPduTimerExpiredEvent*)
{
}

/**
 * Default event handler when a DTPC Topic Expiration Timer expires
 */
void
BundleEventHandler::handle_dtpc_topic_expiration_check(DtpcTopicExpirationCheckEvent*)
{
}


/**
 * Default event handler when a DTPC elision function response is received
 */
void
BundleEventHandler::handle_dtpc_elision_func_response(DtpcElisionFuncResponse*)
{
}

#endif // DTPC_ENABLED

} // namespace dtn
