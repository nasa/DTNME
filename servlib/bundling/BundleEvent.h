/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _BUNDLE_EVENT_H_
#define _BUNDLE_EVENT_H_

#ifdef ACS_ENABLED
#include "AggregateCustodySignal.h"
#endif // ACS_ENABLED

#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleRef.h"
#include "BundleList.h"
#include "CustodySignal.h"
#include "contacts/Link.h"
#include "contacts/NamedAttribute.h"
#include "GbofId.h"
#include "reg/APIRegistration.h"

#ifdef DTPC_ENABLED
#    include "dtpc/DtpcApplicationDataItem.h"
#    include "dtpc/DtpcProtocolDataUnit.h"
#    include "dtpc/DtpcRegistration.h"
#endif // DTPC_ENABLED


namespace dtn {

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
class Contact;
class Interface;
class Registration;
class RouteEntry;

/**
 * Processor codes for events / requests identifies which thread handles the event
 */
typedef enum {
  EVENT_PROCESSOR_MAIN = 0x1,
  EVENT_PROCESSOR_INPUT,
  EVENT_PROCESSOR_STORAGE,
  EVENT_PROCESSOR_ACS,
  EVENT_PROCESSOR_DTPC,
  EVENT_PROCESSOR_OUTPUT

} event_processor_t;
/**
 * Conversion function from a processor to a string.
 */
inline const char*
event_processor_to_str(event_processor_t proc)
{
    switch(proc) {

    case EVENT_PROCESSOR_MAIN:       return "EVENT_PROCESSOR_MAIN";
    case EVENT_PROCESSOR_INPUT:      return "EVENT_PROCESSOR_INPUT";
    case EVENT_PROCESSOR_STORAGE:    return "EVENT_PROCESSOR_STORAGE";
    case EVENT_PROCESSOR_ACS:        return "EVENT_PROCESSOR_ACS";
    case EVENT_PROCESSOR_DTPC:       return "EVENT_PROCESSOR_DTPC";
    case EVENT_PROCESSOR_OUTPUT:     return "EVENT_PROCESSOR_OUTPUT";

    default:                   return "(invalid event processor)";
        
    }
}


/**
 * Type codes for events / requests.
 */
typedef enum {
    BUNDLE_RECEIVED = 0x1,      ///< New bundle arrival
    BUNDLE_TRANSMITTED,         ///< Bundle or fragment successfully sent
    BUNDLE_DELIVERED,           ///< Bundle locally delivered
    BUNDLE_DELIVERY,            ///< Bundle delivery (with payload)
    BUNDLE_EXPIRED,             ///< Bundle expired
    BUNDLE_NOT_NEEDED,          ///< Bundle no longer needed
    BUNDLE_FREE,                ///< No more references to the bundle
    BUNDLE_FORWARD_TIMEOUT,     ///< A Mapping timed out
    BUNDLE_SEND,                ///< Send a bundle
    BUNDLE_CANCEL,              ///< Cancel a bundle transmission
    BUNDLE_CANCELLED,           ///< Bundle send cancelled
    BUNDLE_INJECT,              ///< Inject a bundle
    BUNDLE_INJECTED,            ///< A bundle was injected
    BUNDLE_ACCEPT_REQUEST,      ///< Request acceptance of a new bundle
    BUNDLE_DELETE,              ///< Request deletion of a bundle
    BUNDLE_QUERY,               ///< Bundle query
    BUNDLE_REPORT,              ///< Response to bundle query
    BUNDLE_ATTRIB_QUERY,        ///< Query for a bundle's attributes
    BUNDLE_ATTRIB_REPORT,       ///< Report with bundle attributes
    BUNDLE_ACK,                 ///< Receipt acked by app
    BUNDLE_TAKE_CUSTODY,        ///< ExternalRouter request to take custody of a bundle
    BUNDLE_CUSTODY_ACCEPTED,    ///< Informs ExternalRouter that custody of bundle was accepted

    CONTACT_UP,                 ///< Contact is up
    CONTACT_DOWN,               ///< Contact abnormally terminated
    CONTACT_QUERY,              ///< Contact query
    CONTACT_REPORT,             ///< Response to contact query
    CONTACT_ATTRIB_CHANGED,     ///< An attribute changed

    LINK_CREATED,               ///< Link is created into the system
    LINK_DELETED,               ///< Link is deleted from the system
    LINK_AVAILABLE,             ///< Link is available
    LINK_UNAVAILABLE,           ///< Link is unavailable
    LINK_BUSY,                  ///< Link is busy 
    LINK_CREATE,                ///< Create and open a new link
    LINK_DELETE,                ///< Delete a link
    LINK_RECONFIGURE,           ///< Reconfigure a link
    LINK_QUERY,                 ///< Link query
    LINK_REPORT,                ///< Response to link query
    LINK_ATTRIB_CHANGED,        ///< An attribute changed
    LINK_CHECK_DEFERRED,        ///< Link deferred bundles ready to send?

    LINK_STATE_CHANGE_REQUEST,        ///< Link state should be changed
    LINK_CANCEL_ALL_BUNDLES_REQUEST, ///<All bundles on link should be cancelled


    REASSEMBLY_COMPLETED,       ///< Reassembly completed

    PRIVATE,                    ///< A private event occured

    REGISTRATION_ADDED,         ///< New registration arrived
    REGISTRATION_REMOVED,       ///< Registration removed
    REGISTRATION_EXPIRED,       ///< Registration expired
    REGISTRATION_DELETE,	///< Registration to be deleted

    DELIVER_BUNDLE_TO_REG,      ///< Deliver bundle to registation

    STORE_BUNDLE_UPDATE,        ///< Add/Update bundle in BundleStore
    STORE_BUNDLE_DELETE,        ///< Delete bundle from BundleStore
    STORE_PENDINGACS_UPDATE,    ///< Add/Update pending ACS in PendingAcsStore
    STORE_PENDINGACS_DELETE,    ///< Delete pending ACS from PendingAcsStore
    STORE_REGISTRATION_UPDATE,  ///< Add/Update API Reg in RegistrationStore
    STORE_REGISTRATION_DELETE,  ///< Delete API Reg from RegistrationStore
    STORE_LINK_UPDATE,          ///< Add/Update Link in LinkStore
    STORE_LINK_DELETE,          ///< Delete Link from LinkStore

    ROUTE_ADD,                  ///< Add a new entry to the route table
    ROUTE_RECOMPUTE,            ///< Force bundles to be rechecked for routing (use after new routes added)
    ROUTE_DEL,                  ///< Remove an entry from the route table
    ROUTE_QUERY,                ///< Static route query
    ROUTE_REPORT,               ///< Response to static route query

    CUSTODY_SIGNAL,             ///< Custody transfer signal received
    CUSTODY_TIMEOUT,            ///< Custody transfer timer fired
    AGGREGATE_CUSTODY_SIGNAL,   ///< Aggregate Custody transfer signal received
    ISSUE_AGGREGATE_CUSTODY_SIGNAL,  ///< Issue an Aggregate Custody Signal for a bundle
    ACS_EXPIRED_EVENT,          ///< Aggregate Custody Signal Timer Expired
    EXTERNAL_ROUTER_ACS,        ///< ACS data to send to an external router

    DAEMON_SHUTDOWN,            ///< Shut the daemon down cleanly
    DAEMON_STATUS,              ///< No-op event to check the daemon

    CLA_SET_PARAMS,             ///< Set CLA configuration
    CLA_PARAMS_SET,             ///< CLA configuration changed
    CLA_SET_LINK_DEFAULTS,      ///< Set defaults for new links
    CLA_EID_REACHABLE,          ///< A new EID has been discovered

    CLA_BUNDLE_QUEUED_QUERY,    ///< Query if a bundle is queued at the CLA
    CLA_BUNDLE_QUEUED_REPORT,   ///< Report if a bundle is queued at the CLA
    CLA_EID_REACHABLE_QUERY,    ///< Query if an EID is reachable by the CLA
    CLA_EID_REACHABLE_REPORT,   ///< Report if an EID is reachable by the CLA
    CLA_LINK_ATTRIB_QUERY,      ///< Query CLA for a link's attributes
    CLA_LINK_ATTRIB_REPORT,     ///< Report from CLA with link attributes
    CLA_IFACE_ATTRIB_QUERY,     ///< Query CLA for an interface's attributes
    CLA_IFACE_ATTRIB_REPORT,    ///< Report from CLA with interface attributes
    CLA_PARAMS_QUERY,           ///< Query CLA for config parameters
    CLA_PARAMS_REPORT,          ///< Report from CLA with config paramters

    DTPC_TOPIC_REGISTRATION,    ///< DTPC Topic Registration request received
    DTPC_TOPIC_UNREGISTRATION,  ///< DTPC Topic Unregistration requested
    DTPC_SEND_DATA_ITEM,        ///< Request to send a DTPC data item
    DTPC_DATA_ITEM_RECEIVED,    ///< DTPC Data Item was received
    DTPC_PAYLOAD_AGGREGATION_EXPIRED, ///<DTPC Payload Aggregation Timer Expired
    DTPC_PDU_TRANSMITTED,       ///< DTPC PDU was transmitted
    DTPC_PDU_DELETE,            ///< DTPC PDU deletion request
    DTPC_RETRANSMIT_TIMER_EXPIRED, ///<DTPC Retransmit Timer Expired
    DTPC_ACK_RECEIVED,          ///<DTPC ACK PDU received
    DTPC_DATA_RECEIVED,         ///<DTPC Data PDU received
    DTPC_DELIVER_PDU_TIMER_EXPIRED, ///<DTPC Deliver PDU Timer Expired
    DTPC_TOPIC_EXPIRATION_CHECK, ///<DTPC Topic Expiration Timer Expired
    DTPC_ELISION_FUNC_RESPONSE,  ///<DTPC Elision Function Response 
} event_type_t;

/**
 * Conversion function from an event to a string.
 */
inline const char*
event_to_str(event_type_t event)
{
    switch(event) {

    case BUNDLE_RECEIVED:       return "BUNDLE_RECEIVED";
    case BUNDLE_TRANSMITTED:    return "BUNDLE_TRANSMITTED";
    case BUNDLE_DELIVERED:      return "BUNDLE_DELIVERED";
    case BUNDLE_DELIVERY:       return "BUNDLE_DELIVERY";
    case BUNDLE_EXPIRED:        return "BUNDLE_EXPIRED";
    case BUNDLE_FREE:           return "BUNDLE_FREE";
    case BUNDLE_NOT_NEEDED:     return "BUNDLE_NOT_NEEDED";
    case BUNDLE_FORWARD_TIMEOUT:return "BUNDLE_FORWARD_TIMEOUT";
    case BUNDLE_SEND:           return "BUNDLE_SEND";
    case BUNDLE_CANCEL:         return "BUNDLE_CANCEL";
    case BUNDLE_CANCELLED:      return "BUNDLE_CANCELLED";
    case BUNDLE_INJECT:         return "BUNDLE_INJECT";
    case BUNDLE_INJECTED:       return "BUNDLE_INJECTED";
    case BUNDLE_ACCEPT_REQUEST: return "BUNDLE_ACCEPT_REQUEST";
    case BUNDLE_DELETE:         return "BUNDLE_DELETE";
    case BUNDLE_QUERY:          return "BUNDLE_QUERY";
    case BUNDLE_REPORT:         return "BUNDLE_REPORT";
    case BUNDLE_ATTRIB_QUERY:   return "BUNDLE_ATTRIB_QUERY";
    case BUNDLE_ATTRIB_REPORT:  return "BUNDLE_ATTRIB_REPORT";
    case BUNDLE_ACK:            return "BUNDLE_ACK_BY_APP";
    case BUNDLE_TAKE_CUSTODY:   return "BUNDLE_TAKE_CUSTODY";
    case BUNDLE_CUSTODY_ACCEPTED: return "BUNDLE_CUSTODY_ACCEPTED";

    case CONTACT_UP:            return "CONTACT_UP";
    case CONTACT_DOWN:          return "CONTACT_DOWN";
    case CONTACT_QUERY:         return "CONTACT_QUERY";
    case CONTACT_REPORT:        return "CONTACT_REPORT";
    case CONTACT_ATTRIB_CHANGED:return "CONTACT_ATTRIB_CHANGED";

    case LINK_CREATED:          return "LINK_CREATED";
    case LINK_DELETED:          return "LINK_DELETED";
    case LINK_AVAILABLE:        return "LINK_AVAILABLE";
    case LINK_UNAVAILABLE:      return "LINK_UNAVAILABLE";
    case LINK_BUSY:             return "LINK_BUSY";
    case LINK_CREATE:           return "LINK_CREATE";
    case LINK_DELETE:           return "LINK_DELETE";
    case LINK_RECONFIGURE:      return "LINK_RECONFIGURE";
    case LINK_QUERY:            return "LINK_QUERY";
    case LINK_REPORT:           return "LINK_REPORT";
    case LINK_ATTRIB_CHANGED:   return "LINK_ATTRIB_CHANGED";
    case LINK_CHECK_DEFERRED:   return "LINK_CHECK_DEFERRED";

    case LINK_STATE_CHANGE_REQUEST:return "LINK_STATE_CHANGE_REQUEST";
    case LINK_CANCEL_ALL_BUNDLES_REQUEST: return "LINK_CANCEL_ALL_BUNDLES_REQUEST";

    case REASSEMBLY_COMPLETED:  return "REASSEMBLY_COMPLETED";

    case PRIVATE:               return "PRIVATE";

    case REGISTRATION_ADDED:    return "REGISTRATION_ADDED";
    case REGISTRATION_REMOVED:  return "REGISTRATION_REMOVED";
    case REGISTRATION_EXPIRED:  return "REGISTRATION_EXPIRED";
    case REGISTRATION_DELETE:   return "REGISTRATION_DELETE";

    case DELIVER_BUNDLE_TO_REG: return "DELIVER_BUNDLE_TO_REG";

    case STORE_BUNDLE_UPDATE:        return "STORE_BUNDLE_UPDATE";
    case STORE_BUNDLE_DELETE:        return "STORE_BUNDLE_DELETE";
    case STORE_PENDINGACS_UPDATE:    return "STORE_PENDINGACS_UPDATE";
    case STORE_PENDINGACS_DELETE:    return "STORE_PENDINGACS_DELETE";
    case STORE_REGISTRATION_UPDATE:  return "STORE_REGISTRATION_UPDATE";
    case STORE_REGISTRATION_DELETE:  return "STORE_REGISTRATION_DELETE";
    case STORE_LINK_UPDATE:          return "STORE_LINK_UPDATE";
    case STORE_LINK_DELETE:          return "STORE_LINK_DELETE";

    case ROUTE_ADD:             return "ROUTE_ADD";
    case ROUTE_RECOMPUTE:       return "ROUTE_RECOMPUTE";
    case ROUTE_DEL:             return "ROUTE_DEL";
    case ROUTE_QUERY:           return "ROUTE_QUERY";
    case ROUTE_REPORT:          return "ROUTE_REPORT";

    case CUSTODY_SIGNAL:        return "CUSTODY_SIGNAL";
    case CUSTODY_TIMEOUT:       return "CUSTODY_TIMEOUT";
    case AGGREGATE_CUSTODY_SIGNAL: return "AGGREGATE_CUSTODY_SIGNAL";
    case ISSUE_AGGREGATE_CUSTODY_SIGNAL: return "ISSUE_AGGREGATE_CUSTODY_SIGNAL";
    case ACS_EXPIRED_EVENT:     return "ACS_EXPIRED_EVENT";
    case EXTERNAL_ROUTER_ACS:   return "EXTERNAL_ROUTER_ACS";
    
    case DAEMON_SHUTDOWN:       return "SHUTDOWN";
    case DAEMON_STATUS:         return "DAEMON_STATUS";
        
    case CLA_SET_PARAMS:        return "CLA_SET_PARAMS";
    case CLA_PARAMS_SET:        return "CLA_PARAMS_SET";
    case CLA_SET_LINK_DEFAULTS: return "CLA_SET_LINK_DEFAULTS";
    case CLA_EID_REACHABLE:     return "CLA_EID_REACHABLE";

    case CLA_BUNDLE_QUEUED_QUERY:  return "CLA_BUNDLE_QUEUED_QUERY";
    case CLA_BUNDLE_QUEUED_REPORT: return "CLA_BUNDLE_QUEUED_REPORT";
    case CLA_EID_REACHABLE_QUERY:  return "CLA_EID_REACHABLE_QUERY";
    case CLA_EID_REACHABLE_REPORT: return "CLA_EID_REACHABLE_REPORT";
    case CLA_LINK_ATTRIB_QUERY:    return "CLA_LINK_ATTRIB_QUERY";
    case CLA_LINK_ATTRIB_REPORT:   return "CLA_LINK_ATTRIB_REPORT";
    case CLA_IFACE_ATTRIB_QUERY:   return "CLA_IFACE_ATTRIB_QUERY";
    case CLA_IFACE_ATTRIB_REPORT:  return "CLA_IFACE_ATTRIB_REPORT";
    case CLA_PARAMS_QUERY:         return "CLA_PARAMS_QUERY";
    case CLA_PARAMS_REPORT:        return "CLA_PARAMS_REPORT";

    case DTPC_TOPIC_REGISTRATION:  return "DTPC_TOPIC_REGISTRATION";
    case DTPC_TOPIC_UNREGISTRATION: return "DTPC_TOPIC_UNREGISTRATION";
    case DTPC_SEND_DATA_ITEM:      return "DTPC_SEND_DATA_ITEM";
    case DTPC_DATA_ITEM_RECEIVED:  return "DTPC_DATA_ITEM_RECEIVED";
    case DTPC_PAYLOAD_AGGREGATION_EXPIRED: return "DTPC_PAYLOAD_AGGREGATION_EXPIRED";
    case DTPC_PDU_TRANSMITTED:     return "DTPC_PDU_TRANSMITTED";
    case DTPC_PDU_DELETE:          return "DTPC_PDU_DELETE";
    case DTPC_RETRANSMIT_TIMER_EXPIRED: return "DTPC_RETRANSMIT_TIMER_EXPIRED";
    case DTPC_ACK_RECEIVED:        return "DTPC_ACK_RECEIVED";
    case DTPC_DATA_RECEIVED:       return "DTPC_DATA_RECEIVED";
    case DTPC_DELIVER_PDU_TIMER_EXPIRED: return "DTPC_DELIVER_PDU_TIMER_EXPIRED";
    case DTPC_TOPIC_EXPIRATION_CHECK: return "DTPC_TOPIC_EXPIRATION_CHECK";
    case DTPC_ELISION_FUNC_RESPONSE: return "DTPC_ELISION_FUNC_RESPONSE";

    default:                   return "(invalid event type)";
        
    }
}

/**
 * Possible sources for events.
 */
typedef enum {
    EVENTSRC_PEER   = 1,        ///< a peer dtn forwarder
    EVENTSRC_APP    = 2,        ///< a local application
    EVENTSRC_STORE  = 3,        ///< the data store
    EVENTSRC_ADMIN  = 4,        ///< the admin logic
    EVENTSRC_FRAGMENTATION = 5, ///< the fragmentation engine
    EVENTSRC_ROUTER = 6,        ///< the routing logic
    EVENTSRC_CACHE  = 7         ///< the BPQ cache
} event_source_t;

/**
 * Conversion function from a source to a string
 * suitable for use with plug-in arch XML messaging.
 */
inline const char*
source_to_str(event_source_t source)
{
    switch(source) {

    case EVENTSRC_PEER:             return "peer";
    case EVENTSRC_APP:              return "application";
    case EVENTSRC_STORE:            return "dataStore";
    case EVENTSRC_ADMIN:            return "admin";
    case EVENTSRC_FRAGMENTATION:    return "fragmentation";
    case EVENTSRC_ROUTER:           return "router";
    case EVENTSRC_CACHE:            return "cache";

    default:                        return "(invalid source type)";
    }
}

class MetadataBlockRequest {
public:
    enum QueryType { QueryByIdentifier, QueryByType, QueryAll };

    MetadataBlockRequest(QueryType query_type, unsigned int query_value) :
        _query_type(query_type), _query_value(query_value) { }

    int          query_type()  const { return _query_type; }
    unsigned int query_value() const { return _query_value; }

private:
    QueryType    _query_type;
    unsigned int _query_value;
};
typedef std::vector<MetadataBlockRequest> MetaBlockRequestVector;

/**
 * Event base class.
 */
class BundleEvent {
public:
    /**
     * The event type code.
     */
    const event_type_t type_;

    /**
     * The processor that needs to process the event
     */
    event_processor_t event_processor_;

    /**
     * Bit indicating whether this event is for the daemon only or if
     * it should be propagated to other components (i.e. the various
     * routers).
     */
    bool daemon_only_;

    /**
     * Slot for a notifier to indicate that the event was processed.
     */
    oasys::Notifier* processed_notifier_;

    /**
     * Slot to record the time that the event was put into the queue.
     */
    oasys::Time posted_time_;

    /**
     * Used for printing
     */
    const char* type_str() {
        return event_to_str(type_);
    }

    /**
     * Need a virtual destructor to make sure all the right bits are
     * cleaned up.
     */
    virtual ~BundleEvent() {}

protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    BundleEvent(event_type_t type, event_processor_t proc)
        : type_(type),
          event_processor_(proc),
          daemon_only_(false),
          processed_notifier_(NULL) {}
};

/**
 * Event classes for new bundle arrivals.
 */
class BundleReceivedEvent : public BundleEvent {
public:
    /*
     * Constructor for bundles arriving from a peer, named by the
     * prevhop and optionally marked with the link it arrived on.
     */
    BundleReceivedEvent(Bundle*           bundle,
                        event_source_t    source,
                        uint64_t bytes_received,
                        const EndpointID& prevhop,
                        Link*             originator = NULL)

        : BundleEvent(BUNDLE_RECEIVED, EVENT_PROCESSOR_INPUT),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source),
          bytes_received_(bytes_received),
          link_(originator, "BundleReceivedEvent"),
          prevhop_(prevhop),
          registration_(NULL)
    {
        ASSERT(source == EVENTSRC_PEER);
    }

    /*
     * Constructor for bundles arriving from a local application
     * identified by the given Registration.
     */
    BundleReceivedEvent(Bundle*           bundle,
                        event_source_t    source,
                        Registration*     registration)
        : BundleEvent(BUNDLE_RECEIVED, EVENT_PROCESSOR_INPUT),
          bundleref_(bundle, "BundleReceivedEvent2"),
          source_(source),
          bytes_received_(0),
          link_("BundleReceivedEvent"),
          prevhop_(EndpointID::NULL_EID()),
          registration_(registration)
    {
    }

    /*
     * Constructor for other "arriving" bundles, including reloading
     * from storage and generated signals.
     */
    BundleReceivedEvent(Bundle*        bundle,
                        event_source_t source)
        : BundleEvent(BUNDLE_RECEIVED, EVENT_PROCESSOR_INPUT),
          bundleref_(bundle, "BundleReceivedEvent3"),
          source_(source),
          bytes_received_(0),
          link_("BundleReceivedEvent"),
          prevhop_(EndpointID::NULL_EID()),
          registration_(NULL)
    {
    }

    /// The newly arrived bundle
    BundleRef bundleref_;

    /// The source of the bundle
    int source_;

    /// The total bytes actually received
    uint64_t bytes_received_;

    /// Link from which bundle was received, if applicable
    LinkRef link_;

    /// Previous hop endpoint id
    EndpointID prevhop_;

    /// Registration where the bundle arrived
    Registration* registration_;

    /// Duplicate Bundle
    Bundle* duplicate_;
};

// This variant of the class is processed by the BundleDaemonOutput
class BundleReceivedEvent_OutputProcessor : public BundleReceivedEvent {
public:
    /*
     * Constructor for bundles arriving from a peer, named by the
     * prevhop and optionally marked with the link it arrived on.
     */
    BundleReceivedEvent_OutputProcessor(Bundle*           bundle,
                                        event_source_t    source,
                                        uint64_t         bytes_received,
                                        const EndpointID& prevhop,
                                        Link*             originator = NULL)

        : BundleReceivedEvent(bundle, source, bytes_received, prevhop, originator)
    {
        event_processor_ = EVENT_PROCESSOR_OUTPUT;
    }

    /*
     * Constructor for bundles arriving from a local application
     * identified by the given Registration.
     */
    BundleReceivedEvent_OutputProcessor(Bundle*           bundle,
                                        event_source_t    source,
                                        Registration*     registration)
        : BundleReceivedEvent(bundle, source, registration)
    {
        event_processor_ = EVENT_PROCESSOR_OUTPUT;
    }

    /*
     * Constructor for other "arriving" bundles, including reloading
     * from storage and generated signals.
     */
    BundleReceivedEvent_OutputProcessor(Bundle*        bundle,
                                        event_source_t source)
        : BundleReceivedEvent(bundle, source)
    {
        event_processor_ = EVENT_PROCESSOR_OUTPUT;
    }
};

// This variant of the class is processed by the BundleDaemon
class BundleReceivedEvent_MainProcessor : public BundleReceivedEvent {
public:
    /*
     * Constructor for bundles arriving from a peer, named by the
     * prevhop and optionally marked with the link it arrived on.
     */
    BundleReceivedEvent_MainProcessor(Bundle*           bundle,
                                      event_source_t    source,
                                      uint64_t         bytes_received,
                                      const EndpointID& prevhop,
                                      Link*             originator = NULL)

        : BundleReceivedEvent(bundle, source, bytes_received, prevhop, originator)
    {
        event_processor_ = EVENT_PROCESSOR_MAIN;
    }

    /*
     * Constructor for bundles arriving from a local application
     * identified by the given Registration.
     */
    BundleReceivedEvent_MainProcessor(Bundle*           bundle,
                                      event_source_t    source,
                                      Registration*     registration)
        : BundleReceivedEvent(bundle, source, registration)
    {
        event_processor_ = EVENT_PROCESSOR_MAIN;
    }

    /*
     * Constructor for other "arriving" bundles, including reloading
     * from storage and generated signals.
     */
    BundleReceivedEvent_MainProcessor(Bundle*        bundle,
                                      event_source_t source)
        : BundleReceivedEvent(bundle, source)
    {
        event_processor_ = EVENT_PROCESSOR_MAIN;
        daemon_only_ = true;
    }
};




/**
 * Event class for bundle or fragment transmission.
 */
class BundleTransmittedEvent : public BundleEvent {
public:
    BundleTransmittedEvent(Bundle* bundle, const ContactRef& contact,
                           const LinkRef& link, uint64_t  bytes_sent,
                           uint64_t  reliably_sent,
                           bool success=true)
        : BundleEvent(BUNDLE_TRANSMITTED, EVENT_PROCESSOR_OUTPUT),
          bundleref_(bundle, "BundleTransmittedEvent"),
          contact_(contact.object(), "BundleTransmittedEvent"),
          bytes_sent_(bytes_sent),
          reliably_sent_(reliably_sent),
          link_(link.object(), "BundleTransmittedEvent"),
          success_(success) {}

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact where the bundle was sent
    ContactRef contact_;

    /// Total number of bytes sent
    uint64_t  bytes_sent_;

    /// If the convergence layer that we sent on is reliable, this is
    /// the count of the bytes reliably sent, which must be less than
    /// or equal to the bytes transmitted
    uint64_t  reliably_sent_;

    /// The link over which the bundle was sent
    /// (may not have a contact when the transmission result is reported)
    LinkRef link_;

    /// Flag indicating success or failure
    bool success_;

};

// This variant of the class is processed by the BundleDaemon
class BundleTransmittedEvent_MainProcessor : public BundleTransmittedEvent {
public:
    /*
     * Constructor for bundles arriving from a peer, named by the
     * prevhop and optionally marked with the link it arrived on.
     */
    BundleTransmittedEvent_MainProcessor(Bundle* bundle, const ContactRef& contact,
                           const LinkRef& link, uint64_t bytes_sent,
                           uint64_t  reliably_sent,
                           bool success=true)

        : BundleTransmittedEvent(bundle, contact, link, bytes_sent, reliably_sent, success)
    {
        event_processor_ = EVENT_PROCESSOR_MAIN;
    }
};


/**
 * Event class for local bundle delivery.
 */
class BundleDeliveredEvent : public BundleEvent {
public:
    BundleDeliveredEvent(Bundle* bundle, Registration* registration)
        : BundleEvent(BUNDLE_DELIVERED, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleDeliveredEvent"),
          registration_(registration) {}

    /// The delivered bundle
    BundleRef bundleref_;

    /// The registration that got it
    Registration* registration_;
};

/**
 * Event class for acknowledgement of bundle reciept by app.
 */
class BundleAckEvent : public BundleEvent {
public:
    BundleAckEvent(u_int reg, std::string source, u_quad_t secs, u_quad_t seq)
        : BundleEvent(BUNDLE_ACK, EVENT_PROCESSOR_MAIN),
          regid_(reg),
          sourceEID_(source),
          creation_ts_(secs, seq) {}

    u_int regid_;
    EndpointID sourceEID_;
    BundleTimestamp creation_ts_;
};

/**
 * Event class for local bundle delivery.
 * Processing of this event causes the bundle to be delivered
 * to the particular registration (application).
 */
class DeliverBundleToRegEvent : public BundleEvent {
public:
    DeliverBundleToRegEvent(Bundle* bundle,
                            u_int32_t regid)
        : BundleEvent(DELIVER_BUNDLE_TO_REG, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleToRegEvent"),
          regid_(regid) { }

    /// The bundle to be delivered
    BundleRef bundleref_;

    u_int32_t regid_;
};

/**
 * Event class for bundle expiration.
 */
class BundleExpiredEvent : public BundleEvent {
public:
    BundleExpiredEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_EXPIRED, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleExpiredEvent") {}

    /// The expired bundle
    BundleRef bundleref_;
};

/**
 * Event class for bundles that have no more references to them.
 */
class BundleFreeEvent : public BundleEvent {
public:
    BundleFreeEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_FREE, EVENT_PROCESSOR_MAIN),
          bundle_(bundle)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The freed bundle
    Bundle* bundle_;
};

/**
 * Abstract class for the subset of events related to contacts and
 * links that defines a reason code enumerated type.
 */
class ContactEvent : public BundleEvent {
public:
    /**
     * Reason codes for contact state operations
     */
    typedef enum {
        INVALID = 0,    ///< Should not be used
        NO_INFO,        ///< No additional info
        USER,           ///< User action (i.e. console / config)
        BROKEN,         ///< Unexpected session interruption
        DISCOVERY,      ///< Dynamically discovered link
        CL_ERROR,       ///< Convergence layer protocol error
        CL_VERSION,     ///< Convergence layer version mismatch
        SHUTDOWN,       ///< Clean connection shutdown
        RECONNECT,      ///< Re-establish link after failure
        IDLE,           ///< Idle connection shut down by the CL
        TIMEOUT,        ///< Scheduled link ended duration
    } reason_t;

    /**
     * Reason to string conversion.
     */
    static const char* reason_to_str(int reason) {
        switch(reason) {
        case INVALID:   return "INVALID";
        case NO_INFO:   return "no additional info";
        case USER:      return "user action";
        case DISCOVERY: return "link discovery";
        case SHUTDOWN:  return "peer shut down";
        case BROKEN:    return "connection broken";
        case CL_ERROR:  return "cl protocol error";
        case CL_VERSION:return "cl version mismatch";
        case RECONNECT: return "re-establishing connection";
        case IDLE:      return "connection idle";
        case TIMEOUT:   return "schedule timed out";
        }
        NOTREACHED;
    }

    /// Constructor
    ContactEvent(event_type_t type, reason_t reason = NO_INFO)
        : BundleEvent(type, EVENT_PROCESSOR_MAIN), reason_(reason) {}

    int reason_;        ///< reason code for the event
};

/**
 * Event class for contact up events
 */
class ContactUpEvent : public ContactEvent {
public:
    ContactUpEvent(const ContactRef& contact)
        : ContactEvent(CONTACT_UP),
          contact_(contact.object(), "ContactUpEvent") {}

    /// The contact that is up
    ContactRef contact_;
};

/**
 * Event class for contact down events
 */
class ContactDownEvent : public ContactEvent {
public:
    ContactDownEvent(const ContactRef& contact, reason_t reason)
        : ContactEvent(CONTACT_DOWN, reason),
          contact_(contact.object(), "ContactDownEvent") {}

    /// The contact that is now down
    ContactRef contact_;
};

/**
 * Event classes for contact queries and responses
 */
class ContactQueryRequest: public BundleEvent {
public:
    ContactQueryRequest() : BundleEvent(CONTACT_QUERY, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class ContactReportEvent : public BundleEvent {
public:
    ContactReportEvent() : BundleEvent(CONTACT_REPORT, EVENT_PROCESSOR_MAIN) {}
};

/**
 * Event class for a change in contact attributes.
 */
class ContactAttributeChangedEvent: public ContactEvent {
public:
    ContactAttributeChangedEvent(const ContactRef& contact, reason_t reason)
        : ContactEvent(CONTACT_ATTRIB_CHANGED, reason),
          contact_(contact.object(), "ContactAttributeChangedEvent") {}

    ///< The link/contact that changed
    ContactRef contact_;
};

/**
 * Event class for link creation events
 */
class LinkCreatedEvent : public ContactEvent {
public:
    LinkCreatedEvent(const LinkRef& link, reason_t reason = ContactEvent::USER)
        : ContactEvent(LINK_CREATED, reason),
          link_(link.object(), "LinkCreatedEvent") {}

    /// The link that was created
    LinkRef link_;
};

/**
 * Event class for link deletion events
 */
class LinkDeletedEvent : public ContactEvent {
public:
    LinkDeletedEvent(const LinkRef& link, reason_t reason = ContactEvent::USER)
        : ContactEvent(LINK_DELETED, reason),
          link_(link.object(), "LinkDeletedEvent") {}

    /// The link that was deleted
    LinkRef link_;
};

/**
 * Event class for link available events
 */
class LinkAvailableEvent : public ContactEvent {
public:
    LinkAvailableEvent(const LinkRef& link, reason_t reason)
        : ContactEvent(LINK_AVAILABLE, reason),
          link_(link.object(), "LinkAvailableEvent") {}

    /// The link that is available
    LinkRef link_;
};

/**
 * Event class for link unavailable events
 */
class LinkUnavailableEvent : public ContactEvent {
public:
    LinkUnavailableEvent(const LinkRef& link, reason_t reason)
        : ContactEvent(LINK_UNAVAILABLE, reason),
          link_(link.object(), "LinkUnavailableEvent") {}

    /// The link that is unavailable
    LinkRef link_;
};

/**
 * Event class for link unavailable events
 */
class LinkCheckDeferredEvent : public BundleEvent {
public:
    LinkCheckDeferredEvent(Link* link)
        : BundleEvent(LINK_CHECK_DEFERRED, EVENT_PROCESSOR_MAIN), 
          linkref_(link, "LinkCheckDeferredEvent") {}

    /// The link for which to check the deferred bundles
    LinkRef linkref_;
};


/**
 * Request class for link state change requests that are sent to the
 * daemon thread for processing. This includes requests to open or
 * close the link, and changing its status to available or
 * unavailable.
 *
 * When closing a link, a reason must be given for the event.
 */
class LinkStateChangeRequest : public ContactEvent {
public:
    /// Shared type code for state_t with Link
    typedef Link::state_t state_t;

    LinkStateChangeRequest(const LinkRef& link, state_t state, reason_t reason)
        : ContactEvent(LINK_STATE_CHANGE_REQUEST, reason),
          link_(link.object(), "LinkStateChangeRequest"),
          state_(state), contact_("LinkStateChangeRequest")
    {
        daemon_only_ = true;
        
        contact_   = link->contact();
        old_state_ = link->state();
    }

    LinkStateChangeRequest(const oasys::Builder&,
                           state_t state, reason_t reason)
        : ContactEvent(LINK_STATE_CHANGE_REQUEST, reason),
          state_(state), contact_("LinkStateChangeRequest")
    {
        daemon_only_ = true;
    }

    /// The link to be changed
    LinkRef link_;

    /// Requested state
    int state_;
    
    /// The active Contact when the request was made
    ContactRef contact_;

    /// State when the request was made
    int old_state_;
};


/**
 * Request to clear all bundles queued and inflight on a link
 */
class LinkCancelAllBundlesRequest : public BundleEvent {
public:
    LinkCancelAllBundlesRequest(const LinkRef& link)
        : BundleEvent(LINK_CANCEL_ALL_BUNDLES_REQUEST, EVENT_PROCESSOR_OUTPUT), 
          link_(link.object(), "LinkCancelAllBundlesRequest") {}

    /// The link to be cleared
    LinkRef link_;
};


/*
 * Private Event used for components to post an event for themselves
 * which will be processed synchronously in the BundleDaemon Event Queue
 */
class PrivateEvent : public BundleEvent {
 public:
  PrivateEvent(int sub_type, void* context) 
    :  BundleEvent(PRIVATE, EVENT_PROCESSOR_MAIN), sub_type_(sub_type), context_(context) {}

  int   sub_type_;
  void* context_;
};


/**
 * Event class for new registration arrivals.
 */
class RegistrationAddedEvent : public BundleEvent {
public:
    RegistrationAddedEvent(Registration* reg, event_source_t source)
        : BundleEvent(REGISTRATION_ADDED, EVENT_PROCESSOR_MAIN), registration_(reg),
          source_(source) {}

    /// The newly added registration
    Registration* registration_;

    /// Why is the registration added
    int source_;
};

/**
 * Event class for registration removals.
 */
class RegistrationRemovedEvent : public BundleEvent {
public:
    RegistrationRemovedEvent(Registration* reg)
        : BundleEvent(REGISTRATION_REMOVED, EVENT_PROCESSOR_MAIN), registration_(reg) {}

    /// The to-be-removed registration
    Registration* registration_;
};

/**
 * Event class for registration expiration.
 */
class RegistrationExpiredEvent : public BundleEvent {
public:
    RegistrationExpiredEvent(Registration* registration)
        : BundleEvent(REGISTRATION_EXPIRED, EVENT_PROCESSOR_MAIN),
          registration_(registration) {}
    
    /// The to-be-removed registration 
    Registration* registration_;
};

/**
 * Daemon-only event class used to delete a registration after it's
 * removed or expired.
 */
class RegistrationDeleteRequest : public BundleEvent {
public:
    RegistrationDeleteRequest(Registration* registration)
        : BundleEvent(REGISTRATION_DELETE, EVENT_PROCESSOR_MAIN),
          registration_(registration)
    {
        daemon_only_ = true;
    }

    /// The registration to be deleted
    Registration* registration_;
};

/**
 * Event class for adding/updating a bundle in the BundleStore
 */
class StoreBundleUpdateEvent: public BundleEvent {
public:
    StoreBundleUpdateEvent(Bundle* bundle)
        : BundleEvent(STORE_BUNDLE_UPDATE, EVENT_PROCESSOR_STORAGE),
//dz debug          bundleref_(bundle, "StoreBundleUpdateEvent")
          bundleid_(bundle->bundleid())
    {
        // should be processed only by the daemon
        daemon_only_ = true;
        ASSERT(processed_notifier_ == NULL);
    }

    /// The Bundle to add or update in the database
//dz debug    const BundleRef bundleref_;
    bundleid_t bundleid_;
};

/**
 * Event class for deleting a bundle from the BundleStore
 */
class StoreBundleDeleteEvent: public BundleEvent {
public:
    StoreBundleDeleteEvent(bundleid_t bundleid, int64_t size_and_flag)
        : BundleEvent(STORE_BUNDLE_DELETE, EVENT_PROCESSOR_STORAGE),
          bundleid_(bundleid),
          size_and_flag_(size_and_flag)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
        ASSERT(processed_notifier_ == NULL);
    }

    /// The freed bundle
    bundleid_t bundleid_;

    /// Durable size of the bundle being deleted 
    /// (negative indicates not written to DB yet)
    int64_t size_and_flag_;
};

#ifdef ACS_ENABLED
/**
 * Event class for adding/updating a pending ACS in the PendingAcsStore
 */
class StorePendingAcsUpdateEvent: public BundleEvent {
public:
    StorePendingAcsUpdateEvent(PendingAcs* pacs)
        : BundleEvent(STORE_PENDINGACS_UPDATE, EVENT_PROCESSOR_STORAGE),
          pacs_(pacs)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The Bundle to add or update in the database
    PendingAcs* pacs_;
};

/**
 * Event class for deleting a pending ACS from the PendingAcsStore
 */
class StorePendingAcsDeleteEvent: public BundleEvent {
public:
    StorePendingAcsDeleteEvent(PendingAcs* pacs)
        : BundleEvent(STORE_PENDINGACS_DELETE, EVENT_PROCESSOR_STORAGE),
          pacs_(pacs)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The freed bundle
    PendingAcs* pacs_;
};
#endif // ACS_ENABLED 

/**
 * Event class for adding/updating a registration in the RegistrationStore
 */
class StoreRegistrationUpdateEvent: public BundleEvent {
public:
    StoreRegistrationUpdateEvent(APIRegistration* apireg)
        : BundleEvent(STORE_REGISTRATION_UPDATE, EVENT_PROCESSOR_STORAGE),
          apireg_(apireg)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The API Registration to add or update in the database
    APIRegistration* apireg_;
};

/**
 * Event class for deleting a registration from the RegistrationStore
 */
class StoreRegistrationDeleteEvent: public BundleEvent {
public:
    StoreRegistrationDeleteEvent(u_int32_t regid, APIRegistration* apireg)
        : BundleEvent(STORE_REGISTRATION_DELETE, EVENT_PROCESSOR_STORAGE),
          regid_(regid),
          apireg_(apireg)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The API Registration to delete from the database
    u_int32_t  regid_;

    /// The API Registration to add or update in the database
    APIRegistration* apireg_;
};

/**
 * Event class for adding/updating a link in the LinkStore
 */
class StoreLinkUpdateEvent: public BundleEvent {
public:
    StoreLinkUpdateEvent(Link* link)
        : BundleEvent(STORE_LINK_UPDATE, EVENT_PROCESSOR_STORAGE),
          linkref_(link, "StoreLinkUpdateEvent")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The Link to add or update in the database
    LinkRef linkref_;
};

/**
 * Event class for deleting a link from the LinkStore
 */
class StoreLinkDeleteEvent: public BundleEvent {
public:
    StoreLinkDeleteEvent(const std::string& linkname)
        : BundleEvent(STORE_LINK_DELETE, EVENT_PROCESSOR_STORAGE),
          linkname_(linkname)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The Link to delete from the database
    const std::string linkname_;
};

/**
 * Event class for route add events
 */
class RouteAddEvent : public BundleEvent {
public:
    RouteAddEvent(RouteEntry* entry)
        : BundleEvent(ROUTE_ADD, EVENT_PROCESSOR_MAIN), entry_(entry) {}

    /// The route table entry to be added
    RouteEntry* entry_;
};

/**
 * Event class to force a recompute_routes
 */
class RouteRecomputeEvent : public BundleEvent {
public:
    RouteRecomputeEvent()
        : BundleEvent(ROUTE_RECOMPUTE, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

/**
 * Event class for route delete events
 */
class RouteDelEvent : public BundleEvent {
public:
    RouteDelEvent(const EndpointIDPattern& dest)
        : BundleEvent(ROUTE_DEL, EVENT_PROCESSOR_MAIN), dest_(dest) {}

    /// The destination eid to be removed
    EndpointIDPattern dest_;
};

/**
 * Event classes for static route queries and responses
 */
class RouteQueryRequest: public BundleEvent {
public:
    RouteQueryRequest() : BundleEvent(ROUTE_QUERY, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class RouteReportEvent : public BundleEvent {
public:
    RouteReportEvent() : BundleEvent(ROUTE_REPORT, EVENT_PROCESSOR_MAIN) {}
};

/**
 * Event class for reassembly completion.
 */
class ReassemblyCompletedEvent : public BundleEvent {
public:
    ReassemblyCompletedEvent(Bundle* bundle, BundleList* fragments)
        : BundleEvent(REASSEMBLY_COMPLETED, EVENT_PROCESSOR_MAIN),
          bundle_(bundle, "ReassemblyCompletedEvent"),
          fragments_("ReassemblyCompletedEvent")
    {
        fragments->move_contents(&fragments_);
    }

    /// The newly reassembled bundle
    BundleRef bundle_;

    /// The list of bundle fragments
    BundleList fragments_;
};

/**
 * Event class for custody transfer signal arrivals.
 */
class CustodySignalEvent : public BundleEvent {
public:
    CustodySignalEvent(const CustodySignal::data_t& data,
                       bundleid_t bundle_id)
        : BundleEvent(CUSTODY_SIGNAL, EVENT_PROCESSOR_MAIN), 
          data_(data),
          bundle_id_(bundle_id) {} 

    /// The parsed data from the custody transfer signal
    CustodySignal::data_t data_;

    ///< The bundle that was transferred
    bundleid_t bundle_id_;
};

/**
 * Event class for custody transfer timeout events
 */
class CustodyTimeoutEvent : public BundleEvent {
public:
    CustodyTimeoutEvent(Bundle* bundle, const LinkRef& link)
        : BundleEvent(CUSTODY_TIMEOUT, EVENT_PROCESSOR_MAIN),
          bundle_id_(bundle->bundleid()),
          link_(link.object(), "CustodyTimeoutEvent") {}

    ///< The bundle whose timer fired
    bundleid_t bundle_id_;

    ///< The link it was sent on
    LinkRef link_;
};

//dz debug class CustodyTimeoutEvent : public BundleEvent {
//dz debug public:
//dz debug     CustodyTimeoutEvent(Bundle* bundle, const LinkRef& link)
//dz debug         : BundleEvent(CUSTODY_TIMEOUT, EVENT_PROCESSOR_MAIN),
//dz debug           bundle_(bundle, "CustodyTimeoutEvent"),
//dz debug           link_(link.object(), "CustodyTimeoutEvent") {}
//dz debug 
//dz debug     ///< The bundle whose timer fired
//dz debug     BundleRef bundle_;
//dz debug 
//dz debug     ///< The link it was sent on
//dz debug     LinkRef link_;
//dz debug };



/**
 * Event class for shutting down a daemon. The daemon closes and
 * deletes all links, then cleanly closes the various data stores,
 * then calls exit().
 */
class ShutdownRequest : public BundleEvent {
public:
    ShutdownRequest() : BundleEvent(DAEMON_SHUTDOWN, EVENT_PROCESSOR_MAIN)
    {
        daemon_only_ = false;
    }
};

/**
 * Event class for checking that the daemon is still running.
 */
class StatusRequest : public BundleEvent {
public:
    StatusRequest() : BundleEvent(DAEMON_STATUS, EVENT_PROCESSOR_MAIN)
    {
        daemon_only_ = true;
    }
};

/**
 * Event classes for sending a bundle
 */
class BundleSendRequest: public BundleEvent {
public:
    BundleSendRequest() : BundleEvent(BUNDLE_SEND, EVENT_PROCESSOR_OUTPUT)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Used by External Router 
    BundleSendRequest(const BundleRef& bundle,
                      const std::string& link,
                      int action)
        : BundleEvent(BUNDLE_SEND, EVENT_PROCESSOR_OUTPUT),
          bundle_(bundle.object(), "BundleSendRequest"),
          link_(link),
          action_(action)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
 

    // Used internally by LTPConvergenceLayer
    BundleSendRequest(const BundleRef& bundle,
                      const LinkRef& linkref)
        : BundleEvent(BUNDLE_SEND, EVENT_PROCESSOR_OUTPUT),
          bundle_(bundle.object(), "BundleSendRequest"),
          link_(""),
          link_ref_(linkref),
          action_(0)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
 
    ///< Bundle to be sent
    BundleRef bundle_;

    ///< Link on which to send the bundle
    std::string link_;

    ///< Link on which to send the bundle
    LinkRef link_ref_;

    ///< Forwarding action to use when sending bundle
    int action_;
};

//dz class BundleSendRequest_MainProcessor: public BundleSendRequest {
//dz public:
//dz     BundleSendRequest_MainProcessor()
//dz     {
//dz         // should be processed only by the daemon
//dz         daemon_only_ = true;
//dz     }
//dz 
//dz     // Used by External Router 
//dz     BundleSendRequest_MainProcessor(const BundleRef& bundle,
//dz                       const std::string& link,
//dz                       int action)
//dz         : BundleSendRequest(bundle, link, action)
//dz     {
//dz         // should be processed only by the daemon
//dz         daemon_only_ = true;
//dz     }
//dz  
//dz 
//dz     // Used internally by LTPConvergenceLayer
//dz     BundleSendRequest_MainProcessor(const BundleRef& bundle,
//dz                                     const LinkRef& linkref)
//dz         : BundleSendRequest(bundle, linkref)
//dz     {
//dz         // should be processed only by the daemon
//dz         daemon_only_ = true;
//dz     }
//dz };

/**
 * Event class for canceling a bundle transmission
 */
class BundleCancelRequest: public BundleEvent {
public:
    BundleCancelRequest() : BundleEvent(BUNDLE_CANCEL, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleCancelRequest(const BundleRef& bundle, const std::string& link) 
        : BundleEvent(BUNDLE_CANCEL, EVENT_PROCESSOR_MAIN),
          bundle_(bundle.object(), "BundleCancelRequest"),
          link_(link)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Bundle to be cancelled
    BundleRef bundle_;

    ///< Link where the bundle is destined
    std::string link_;
};

/**
 * Event class for succesful cancellation of a bundle send.
 */
class BundleSendCancelledEvent : public BundleEvent {
public:
    BundleSendCancelledEvent(Bundle* bundle, const LinkRef& link)
        : BundleEvent(BUNDLE_CANCELLED, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleSendCancelledEvent"),
          link_(link.object(), "BundleSendCancelledEvent") {}

    /// The cancelled bundle
    BundleRef bundleref_;

    /// The link it was queued on
    LinkRef link_;
};

/**
 * Event class for injecting a bundle
 */
class BundleInjectRequest: public BundleEvent {
public:
    BundleInjectRequest() : BundleEvent(BUNDLE_INJECT, EVENT_PROCESSOR_INPUT)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    // Bundle properties
    std::string src_;
    std::string dest_;
    std::string replyto_;
    std::string custodian_;
    u_int8_t    priority_;
    u_int32_t   expiration_;
    std::string payload_file_;

    // Outgoing link
    std::string link_;

    // Forwarding action
    int action_;

    // Request ID for the event, to identify corresponding BundleInjectedEvent
    std::string request_id_;
};

/**
 * Event classes for a succesful bundle injection
 */
class BundleInjectedEvent: public BundleEvent {
public:
    BundleInjectedEvent(Bundle *bundle, const std::string &request_id)
        : BundleEvent(BUNDLE_INJECTED, EVENT_PROCESSOR_OUTPUT),
          bundleref_(bundle, "BundleInjectedEvent"),
          request_id_(request_id)
    {
    }

    /// The injected bundle
    BundleRef bundleref_;

    // Request ID from the inject request
    std::string request_id_;
};

class BundleInjectedEvent_MainProcessor: public BundleInjectedEvent {
public:
    BundleInjectedEvent_MainProcessor(Bundle *bundle, const std::string &request_id)
        : BundleInjectedEvent(bundle, request_id)
    {
        event_processor_ = EVENT_PROCESSOR_MAIN;
        daemon_only_ = true;
    }
};

/**
 * Event class for requestion deletion of a bundle
 */
class BundleDeleteRequest: public BundleEvent {
public:
    BundleDeleteRequest() : BundleEvent(BUNDLE_DELETE, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleDeleteRequest(Bundle* bundle,
                        BundleProtocol::status_report_reason_t reason)
        : BundleEvent(BUNDLE_DELETE, EVENT_PROCESSOR_MAIN),
          bundle_(bundle, "BundleDeleteRequest"),
          reason_(reason)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleDeleteRequest(const BundleRef& bundle,
                        BundleProtocol::status_report_reason_t reason)
        : BundleEvent(BUNDLE_DELETE, EVENT_PROCESSOR_MAIN),
          bundle_(bundle.object(), "BundleDeleteRequest"),
          reason_(reason)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Bundle to be deleted
    BundleRef bundle_;

    /// The reason code
    BundleProtocol::status_report_reason_t reason_;
};

/**
 * Event class for requesting taking custody of a bundle
 */
class BundleTakeCustodyRequest: public BundleEvent {
public:
    BundleTakeCustodyRequest() : BundleEvent(BUNDLE_TAKE_CUSTODY, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleTakeCustodyRequest(Bundle* bundle)
        : BundleEvent(BUNDLE_TAKE_CUSTODY, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleTakeCustodyRequest")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleTakeCustodyRequest(const BundleRef& bref)
        : BundleEvent(BUNDLE_TAKE_CUSTODY, EVENT_PROCESSOR_MAIN),
          bundleref_(bref.object(), "BundleTakeCustodyRequest")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Bundle to be to take custody of
    BundleRef bundleref_;
};

/**
 * Event class for requesting taking custody of a bundle
 */
class BundleCustodyAcceptedEvent: public BundleEvent {
public:
    BundleCustodyAcceptedEvent() : BundleEvent(BUNDLE_CUSTODY_ACCEPTED, EVENT_PROCESSOR_MAIN)
    {
    }

    BundleCustodyAcceptedEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_CUSTODY_ACCEPTED, EVENT_PROCESSOR_MAIN),
          bundleref_(bundle, "BundleCustodyAcceptedEvent")
    {
    }

    BundleCustodyAcceptedEvent(const BundleRef& bref)
        : BundleEvent(BUNDLE_TAKE_CUSTODY, EVENT_PROCESSOR_MAIN),
          bundleref_(bref.object(), "BundleCustodyAcceptedEvent")
    {
    }

    ///< Bundle taken into custody
    BundleRef bundleref_;
};

/**
 * Event class to optionally probe if a bundle can be accepted by the
 * system before a BundleReceivedEvent is posted. Currently used for
 * backpressure via the API.
 *
 * Note that the bundle may not be completely constructed when this
 * event is posted. In particular, the payload need not be filled in
 * yet, and other security fields may not be present. At a minimum
 * though, the fields from the primary block and the payload length
 * must be known.
 */
class BundleAcceptRequest : public BundleEvent {
public:
    BundleAcceptRequest(const BundleRef& bundle,
                        event_source_t   source,
                        bool*            result,
                        int*             reason)
        : BundleEvent(BUNDLE_ACCEPT_REQUEST, EVENT_PROCESSOR_INPUT),
          bundle_(bundle.object(), "BundleAcceptRequest"),
          source_(source),
          result_(result),
          reason_(reason)
    {
    }
    
    /// The newly arrived bundle
    BundleRef bundle_;

    /// The source of the event
    int source_;

    /// Pointer to the expected result
    bool* result_;

    /// Pointer to the reason code
    int* reason_;
};

/**
 * Event classes for bundle queries and responses
 */
class BundleQueryRequest: public BundleEvent {
public:
    BundleQueryRequest() : BundleEvent(BUNDLE_QUERY, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class BundleReportEvent : public BundleEvent {
public:
    BundleReportEvent() : BundleEvent(BUNDLE_REPORT, EVENT_PROCESSOR_MAIN) {}
};

class BundleAttributesQueryRequest: public BundleEvent {
public:
    BundleAttributesQueryRequest(const std::string& query_id,
                                 const BundleRef& bundle,
                                 const AttributeNameVector& attribute_names)
        : BundleEvent(BUNDLE_ATTRIB_QUERY, EVENT_PROCESSOR_MAIN),
          query_id_(query_id),
          bundle_(bundle.object(), "BundleAttributesQueryRequest"),
          attribute_names_(attribute_names) {}

    /// Query Identifier
    std::string query_id_;

    ///< Bundle being queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// metadata blocks requested by type/identifier
    MetaBlockRequestVector metadata_blocks_;
};

class BundleAttributesReportEvent: public BundleEvent {
public:
    BundleAttributesReportEvent(const std::string& query_id,
                                const BundleRef& bundle,
                                const AttributeNameVector& attribute_names,
                                const MetaBlockRequestVector& metadata_blocks)
        : BundleEvent(BUNDLE_ATTRIB_REPORT, EVENT_PROCESSOR_MAIN),
          query_id_(query_id),
          bundle_(bundle.object(), "BundleAttributesReportEvent"),
          attribute_names_(attribute_names),
          metadata_blocks_(metadata_blocks) {}

    /// Query Identifier
    std::string query_id_;

    /// Bundle that was queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// metadata blocks requested by type/identifier
    MetaBlockRequestVector metadata_blocks_;
};

/**
 * Event class for creating and opening a link
 */
class LinkCreateRequest: public BundleEvent {
public:
    LinkCreateRequest(const std::string &name, Link::link_type_t link_type,
                      const std::string &endpoint,
                      ConvergenceLayer *cla, AttributeVector &parameters)
        : BundleEvent(LINK_CREATE, EVENT_PROCESSOR_MAIN),
          name_(name),
          endpoint_(endpoint),
          link_type_(link_type),
          cla_(cla),
          parameters_(parameters)
        
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Identifier for the link
    std::string name_;

    ///< Next hop EID
    std::string endpoint_;

    ///< Type of link
    Link::link_type_t link_type_;

    ///< CL to use
    ConvergenceLayer *cla_;

    ///< An optional set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for reconfiguring an existing link.
 */
class LinkReconfigureRequest: public BundleEvent {
public:
    LinkReconfigureRequest(const LinkRef& link,
                           AttributeVector &parameters)
        : BundleEvent(LINK_RECONFIGURE, EVENT_PROCESSOR_MAIN),
          link_(link.object(), "LinkReconfigureRequest"),
          parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< The link to be changed
    LinkRef link_;

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for requesting deletion of a link.
 */
class LinkDeleteRequest: public BundleEvent {
public:
    LinkDeleteRequest(const LinkRef& link) :
        BundleEvent(LINK_DELETE, EVENT_PROCESSOR_MAIN),
        link_(link.object(), "LinkDeleteRequest")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< The link to be deleted
    LinkRef link_;
};

/**
 * Event class for a change in link attributes.
 */
class LinkAttributeChangedEvent: public ContactEvent {
public:
    LinkAttributeChangedEvent(const LinkRef& link,
                              AttributeVector attributes,
                              reason_t reason = ContactEvent::NO_INFO)
        : ContactEvent(LINK_ATTRIB_CHANGED, reason),
          link_(link.object(), "LinkAttributeChangedEvent"),
          attributes_(attributes)
    {
    }

    ///< The link that changed
    LinkRef link_;

    ///< Attributes that changed
    AttributeVector attributes_;
};

/**
 * Event classes for link queries and responses
 */
class LinkQueryRequest: public BundleEvent {
public:
    LinkQueryRequest() : BundleEvent(LINK_QUERY, EVENT_PROCESSOR_MAIN)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class LinkReportEvent : public BundleEvent {
public:
    LinkReportEvent() : BundleEvent(LINK_REPORT, EVENT_PROCESSOR_MAIN) {}
};

/**
 * Event class for DP-originated CLA parameter change requests.
 */
class CLASetParamsRequest : public BundleEvent {
public:
    CLASetParamsRequest(ConvergenceLayer *cla, AttributeVector &parameters)
        : BundleEvent(CLA_SET_PARAMS, EVENT_PROCESSOR_MAIN), cla_(cla), parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< CL to change
    ConvergenceLayer *cla_;

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for CLA parameter change request completion events.
 */
class CLAParamsSetEvent : public BundleEvent {
public:
    CLAParamsSetEvent(ConvergenceLayer *cla, std::string name)
        : BundleEvent(CLA_PARAMS_SET, EVENT_PROCESSOR_MAIN), cla_(cla), name_(name) {}

    ///< CL that changed
    ConvergenceLayer *cla_;

    ///< Name of CL (if cla_ is External)
    std::string name_;
};

/**
 * Event class for DP-originated requests to set link defaults.
 */
class SetLinkDefaultsRequest : public BundleEvent {
public:
    SetLinkDefaultsRequest(AttributeVector &parameters)
        : BundleEvent(CLA_SET_LINK_DEFAULTS, EVENT_PROCESSOR_MAIN), parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for discovery of a new EID.
 */
class NewEIDReachableEvent: public BundleEvent {
public:
    NewEIDReachableEvent(Interface* iface, const std::string &endpoint)
        : BundleEvent(CLA_EID_REACHABLE, EVENT_PROCESSOR_MAIN),
          iface_(iface),
          endpoint_(endpoint) {}

    ///< The interface the peer was discovered on
    Interface* iface_;

    ///> The EID of the discovered peer
    std::string endpoint_;
};

/**
 *  Event classes for queries to and reports from the CLA.
 */

class CLAQueryReport: public BundleEvent {
public:

    /// Query Identifier
    std::string query_id_;

protected:

    /**
     * Constructor; protected because this class should only be
     * instantiated via a subclass.
     */
    CLAQueryReport(event_type_t type,
                   const std::string& query_id,
                   bool daemon_only = false)
        : BundleEvent(type, EVENT_PROCESSOR_MAIN),
          query_id_(query_id)
    {
        daemon_only_ = daemon_only;
    }
};

class BundleQueuedQueryRequest: public CLAQueryReport {
public:
    BundleQueuedQueryRequest(const std::string& query_id,
                             Bundle* bundle,
                             const LinkRef& link)
        : CLAQueryReport(CLA_BUNDLE_QUEUED_QUERY, query_id, true),
          bundle_(bundle, "BundleQueuedQueryRequest"),
          link_(link.object(), "BundleQueuedQueryRequest") {}

    /// Bundle to be checked for queued status.
    BundleRef bundle_;

    /// Link on which to check if the given bundle is queued.
    LinkRef link_;
};

class BundleQueuedReportEvent: public CLAQueryReport {
public:
    BundleQueuedReportEvent(const std::string& query_id,
                            bool is_queued)
        : CLAQueryReport(CLA_BUNDLE_QUEUED_REPORT, query_id),
          is_queued_(is_queued) {}

    /// True if the queried bundle was queued on the given link;
    /// otherwise false.
    bool is_queued_;
};

class EIDReachableQueryRequest: public CLAQueryReport {
public:
    EIDReachableQueryRequest(const std::string& query_id,
                             Interface* iface,
                             const std::string& endpoint)
        : CLAQueryReport(CLA_EID_REACHABLE_QUERY, query_id, true),
          iface_(iface),
          endpoint_(endpoint) {}

    /// Interface on which to check if the given endpoint is reachable.
    Interface* iface_;

    /// Endpoint to be checked for reachable status.
    std::string endpoint_;
};

class EIDReachableReportEvent: public CLAQueryReport {
public:
    EIDReachableReportEvent(const std::string& query_id,
                            bool is_reachable)
        : CLAQueryReport(CLA_EID_REACHABLE_REPORT, query_id),
          is_reachable_(is_reachable) {}

    /// True if the queried endpoint is reachable via the given interface;
    /// otherwise false.
    bool is_reachable_;
};

class LinkAttributesQueryRequest: public CLAQueryReport {
public:
    LinkAttributesQueryRequest(const std::string& query_id,
                               const LinkRef& link,
                               const AttributeNameVector& attribute_names)
        : CLAQueryReport(CLA_LINK_ATTRIB_QUERY, query_id, true),
          link_(link.object(), "LinkAttributesQueryRequest"),
          attribute_names_(attribute_names) {}

    /// Link for which the given attributes are requested.
    LinkRef link_;

    /// Link attributes requested by name.
    AttributeNameVector attribute_names_;
};

class LinkAttributesReportEvent: public CLAQueryReport {
public:
    LinkAttributesReportEvent(const std::string& query_id,
                              const AttributeVector& attributes)
        : CLAQueryReport(CLA_LINK_ATTRIB_REPORT, query_id),
          attributes_(attributes) {}

    /// Link attribute values given by name.
    AttributeVector attributes_;
};

class IfaceAttributesQueryRequest: public CLAQueryReport {
public:
    IfaceAttributesQueryRequest(const std::string& query_id,
                                Interface* iface,
                                const AttributeNameVector& attribute_names)
        : CLAQueryReport(CLA_IFACE_ATTRIB_QUERY, query_id, true),
          iface_(iface),
          attribute_names_(attribute_names) {}

    /// Interface for which the given attributes are requested.
    Interface* iface_;

    /// Link attributes requested by name.
    AttributeNameVector attribute_names_;
};

class IfaceAttributesReportEvent: public CLAQueryReport {
public:
    IfaceAttributesReportEvent(const std::string& query_id,
                               const AttributeVector& attributes)
        : CLAQueryReport(CLA_IFACE_ATTRIB_REPORT, query_id),
          attributes_(attributes) {}

    /// Interface attribute values by name.
    AttributeVector attributes_;
};

class CLAParametersQueryRequest: public CLAQueryReport {
public:
    CLAParametersQueryRequest(const std::string& query_id,
                              ConvergenceLayer* cla,
                              const AttributeNameVector& parameter_names)
        : CLAQueryReport(CLA_PARAMS_QUERY, query_id, true),
          cla_(cla),
          parameter_names_(parameter_names) {}

    /// Convergence layer for which the given parameters are requested.
    ConvergenceLayer* cla_;

    /// Convergence layer parameters requested by name.
    AttributeNameVector parameter_names_;
};

class CLAParametersReportEvent: public CLAQueryReport {
public:
    CLAParametersReportEvent(const std::string& query_id,
                             const AttributeVector& parameters)
        : CLAQueryReport(CLA_PARAMS_REPORT, query_id),
          parameters_(parameters) {}

    /// Convergence layer parameter values by name.
    AttributeVector parameters_;
};

#ifdef ACS_ENABLED
/**
 * Event class for aggregate custody transfer signal arrivals.
 * (processed by BundleDaemon)
 */
class AggregateCustodySignalEvent : public BundleEvent {
public:
    AggregateCustodySignalEvent(const AggregateCustodySignal::data_t& data)
        : BundleEvent(AGGREGATE_CUSTODY_SIGNAL, EVENT_PROCESSOR_MAIN), data_(data)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The parsed data from the custody transfer signal
    AggregateCustodySignal::data_t data_;
};

/**
 * Event class for issuing an aggregate custody signal for a bundle
 * (ACS internal event processed by BundleDaemonACS)
 */
class AddBundleToAcsEvent : public BundleEvent {
public:
    AddBundleToAcsEvent(EndpointID* custody_eid, bool succeeded, 
                        BundleProtocol::custody_signal_reason_t reason, 
                        uint64_t custody_id)
        : BundleEvent(ISSUE_AGGREGATE_CUSTODY_SIGNAL, EVENT_PROCESSOR_ACS),
          custody_eid_(custody_eid),
          succeeded_(succeeded),
          reason_(reason),
          custody_id_(custody_id)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// The Endpoint ID of the previous custody holder to which we are sending the ACS
    const EndpointID* custody_eid_;

    /// Whether or not the signal is success or fail
    bool succeeded_;

    /// Reason code for the signal
    BundleProtocol::custody_signal_reason_t reason_;

    /// The Custody ID of the reference bundle 
    uint64_t custody_id_;
};

/**
 * Event class for expiration of an Aggregate Custody Signal
 * (ACS internal event processed by BundleDaemonACS)
 */
class AcsExpiredEvent : public BundleEvent {
public:
    AcsExpiredEvent(std::string key, uint32_t pacs_id)
        : BundleEvent(ACS_EXPIRED_EVENT, EVENT_PROCESSOR_ACS),
          pacs_key_(key),
          pacs_id_(pacs_id)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    /// key value into the map structure
    std::string pacs_key_;

    /// Unique Pending ACS ID
    u_int32_t pacs_id_;
};
#endif // ACS_ENABLED

/**
 * Event class for external routers to receive a copy of the
 * Aggregate Custody Signal Data
 * (processed by BundleDaemon - passed through to the router)
 *
 * NOTE: Not in the ACS_ENABLED ifdef since it is not dependent
 *       on any ACS headers and won't be used unless enabled
 */
class ExternalRouterAcsEvent : public BundleEvent {
public:
    ExternalRouterAcsEvent(const char* buf, size_t len)
        : BundleEvent(EXTERNAL_ROUTER_ACS, EVENT_PROCESSOR_MAIN), 
          acs_data_(buf, len)
    {
    }
    
    /// The payload data from the ACS as a string
    std::string acs_data_;
};


#ifdef DTPC_ENABLED
/**
 * Event class for registering a topic 
 */
class DtpcTopicRegistrationEvent : public BundleEvent {
public:
    DtpcTopicRegistrationEvent(u_int32_t topic_id,
                               bool      has_elision_func,
                               int*      result,
                               DtpcRegistration** registration)
        : BundleEvent(DTPC_TOPIC_REGISTRATION, EVENT_PROCESSOR_DTPC), 
          topic_id_(topic_id),
          has_elision_func_(has_elision_func),
          result_(result),
          registration_(registration)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The topic ID to register for
    u_int32_t topic_id_;

    /// Ellision function defined for the topic
    bool has_elision_func_;

    /// Pointer to the expected result
    int* result_;

    /// Pointer to the newly created DtpcRegistration object 
    DtpcRegistration** registration_;
};

/**
 * Event class for unregistering a topic 
 */
class DtpcTopicUnregistrationEvent : public BundleEvent {
public:
    DtpcTopicUnregistrationEvent(u_int32_t topic_id,
                                 DtpcRegistration*     registration,
                                 int*      result)
        : BundleEvent(DTPC_TOPIC_UNREGISTRATION, EVENT_PROCESSOR_DTPC), 
          topic_id_(topic_id),
          registration_(registration),
          result_(result)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The topic ID to unregister
    u_int32_t topic_id_;

    /// The DtpcRegistration object to be unregistered
    DtpcRegistration* registration_;

    /// Pointer to the expected result
    int* result_;
};

/**
 * Event class for sending a Data Item
 */
class DtpcSendDataItemEvent : public BundleEvent {
public:
    DtpcSendDataItemEvent(u_int32_t topic_id,
                          DtpcApplicationDataItem* data_item,
                          const EndpointID& dest_eid,
                          u_int32_t profile_id,
                          int*      result)
        : BundleEvent(DTPC_SEND_DATA_ITEM, EVENT_PROCESSOR_DTPC), 
          topic_id_(topic_id),
          data_item_(data_item),
          dest_eid_(dest_eid),
          profile_id_(profile_id),
          result_(result)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The topic ID
    u_int32_t topic_id_;

    /// The Application Data Item
    DtpcApplicationDataItem* data_item_;

    /// Destination endpoint id
    EndpointID dest_eid_;

    /// The transmission profile ID
    u_int32_t profile_id_;

    /// Pointer to the expected result
    int* result_;
};

/**
 * Event class for expiration of a DTPC Payload Aggregation
 */
class DtpcPayloadAggregationTimerExpiredEvent : public BundleEvent {
public:
    DtpcPayloadAggregationTimerExpiredEvent(std::string key, u_int64_t seq_ctr)
        : BundleEvent(DTPC_PAYLOAD_AGGREGATION_EXPIRED, EVENT_PROCESSOR_DTPC), 
          key_(key),
          seq_ctr_(seq_ctr)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The key to find the Payload Aggregator
    std::string key_;

    /// sequence counter when the timer was started
    u_int64_t seq_ctr_;    
};

/**
 * Event class for a DTPC PDU Transmission
 */
class DtpcPduTransmittedEvent: public BundleEvent {
public:
    DtpcPduTransmittedEvent(DtpcProtocolDataUnit* pdu)
        : BundleEvent(DTPC_PDU_TRANSMITTED, EVENT_PROCESSOR_DTPC), 
          pdu_(pdu)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The DTPC PDU
    DtpcProtocolDataUnit* pdu_;
};

/**
 * Event class for a DTPC Delete request
 */
class DtpcPduDeleteRequest: public BundleEvent {
public:
    DtpcPduDeleteRequest(DtpcProtocolDataUnit* pdu)
        : BundleEvent(DTPC_PDU_DELETE, EVENT_PROCESSOR_DTPC), 
          pdu_(pdu)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The DTPC PDU
    DtpcProtocolDataUnit* pdu_;
};

/**
 * Event class for expiration of a Retransmit Timer
 */
class DtpcRetransmitTimerExpiredEvent : public BundleEvent {
public:
    DtpcRetransmitTimerExpiredEvent(std::string key)
        : BundleEvent(DTPC_RETRANSMIT_TIMER_EXPIRED, EVENT_PROCESSOR_DTPC), 
          key_(key)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The key to find the Payload Aggregator
    std::string key_;
};

/**
 * Event class for a DTPC ACK received event
 */
class DtpcAckReceivedEvent : public BundleEvent {
public:
    DtpcAckReceivedEvent(DtpcProtocolDataUnit* pdu)
        : BundleEvent(DTPC_ACK_RECEIVED, EVENT_PROCESSOR_DTPC), 
          pdu_(pdu)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The DTPC PDU
    DtpcProtocolDataUnit* pdu_;
};

/**
 * Event class for a DTPC Data PDU received event
 */
class DtpcDataReceivedEvent : public BundleEvent {
public:
    DtpcDataReceivedEvent(DtpcProtocolDataUnit* pdu,
                          BundleRef& bref)
        : BundleEvent(DTPC_DATA_RECEIVED, EVENT_PROCESSOR_DTPC), 
          pdu_(pdu),
          rcvd_bref_(bref.object(), "DtpcDataReceivedEvent")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The DTPC PDU
    DtpcProtocolDataUnit* pdu_;

    /// The Bundle that delivered the PDU
    BundleRef rcvd_bref_;
};

/**
 * Event class for expiration of a DTPC Payload Aggregation
 */
class DtpcDeliverPduTimerExpiredEvent : public BundleEvent {
public:
    DtpcDeliverPduTimerExpiredEvent(std::string key, u_int64_t seq_ctr)
        : BundleEvent(DTPC_DELIVER_PDU_TIMER_EXPIRED, EVENT_PROCESSOR_DTPC), 
          key_(key),
          seq_ctr_(seq_ctr)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The key to find the Data PDU Collector
    std::string key_;

    /// sequence counter when the timer was started
    u_int64_t seq_ctr_;    
};

/**
 * Event class for expiration of a Retransmit Timer
 */
class DtpcTopicExpirationCheckEvent: public BundleEvent {
public:
    DtpcTopicExpirationCheckEvent(u_int32_t topic_id)
        : BundleEvent(DTPC_TOPIC_EXPIRATION_CHECK, EVENT_PROCESSOR_DTPC), 
          topic_id_(topic_id)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// Topic ID
    u_int32_t topic_id_;
};

/**
 * Event class for elision function response
 */
class DtpcElisionFuncResponse : public BundleEvent {
public:
    DtpcElisionFuncResponse(u_int32_t topic_id,
                          DtpcApplicationDataItemList* data_item_list,
                          const EndpointID& dest_eid,
                          u_int32_t profile_id,
                          bool modified)
        : BundleEvent(DTPC_ELISION_FUNC_RESPONSE, EVENT_PROCESSOR_DTPC), 
          topic_id_(topic_id),
          data_item_list_(data_item_list),
          dest_eid_(dest_eid),
          profile_id_(profile_id),
          modified_(modified)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The topic ID
    u_int32_t topic_id_;

    /// The List of Application Data Items
    DtpcApplicationDataItemList* data_item_list_;

    /// Destination endpoint id
    EndpointID dest_eid_;

    /// The transmission profile ID
    u_int32_t profile_id_;

    /// Indication if the ADI list was modified by the elision function
    bool modified_;
};


#endif // DTPC_ENABLED

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */
