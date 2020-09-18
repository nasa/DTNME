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

#ifndef _EHS_EVENT_H_
#define _EHS_EVENT_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <oasys/util/Time.h>

#include "EhsBundleRef.h"

#include "router-custom.h"

namespace dtn {

class EhsBundle;
class EhsBundlePriorityQueue;


/**
 * All signaling from various components to the routing layer is done
 * via the Ehs Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

/**
 * Type codes for events / requests.
 */
typedef enum {
    EHS_BPA_RECEIVED = 0x1,     ///< BPA message received from DTNME ExternalRouter
    EHS_FREE_BUNDLE_REQ,        ///< Free bundle request
    EHS_ROUTE_BUNDLE_REQ,       ///< Route bundle request
    EHS_ROUTE_BUNDLE_LIST_REQ,  ///< Route bundle list request
    EHS_DELETE_BUNDLE_REQ,      ///< Delete bundle request
    EHS_RECONFIGURE_LINK_REQ,   ///< Update Next Hops request
    EHS_BUNDLE_TRANSMITTED,     ///< Bundle transmitted event
} event_type_t;

/**
 * Conversion function from an event to a string.
 */
inline const char*
event_to_str(event_type_t event)
{
    switch(event) {

    case EHS_BPA_RECEIVED:            return "EHS_BPA_RECEIVED";
    case EHS_FREE_BUNDLE_REQ:         return "EHS_FREE_BUNDLE_REQ";
    case EHS_ROUTE_BUNDLE_REQ:        return "EHS_ROUTE_BUNDLE_REQ";
    case EHS_ROUTE_BUNDLE_LIST_REQ:   return "EHS_ROUTE_BUNDLE_LIST_REQ";
    case EHS_DELETE_BUNDLE_REQ:       return "EHS_DELETE_BUNDLE_REQ";
    case EHS_RECONFIGURE_LINK_REQ:    return "EHS_RECONFIGURE_LINK_REQ";
    case EHS_BUNDLE_TRANSMITTED:      return "EHS_BUNDLE_TRANSMITTED";

    default: return "(invalid event type)";
        
    }
}

/**
 * Event base class.
 */
class EhsEvent {
public:
    /**
     * The event type code.
     */
    const event_type_t type_;

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
    virtual ~EhsEvent() {}

protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    EhsEvent(event_type_t type)
        : type_(type) {}
};

/**
 * Event class for new bundle arrivals.
 */
class EhsBpaReceivedEvent : public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsBpaReceivedEvent(std::unique_ptr<rtrmessage::bpa>& bpa_ptr)
        : EhsEvent(EHS_BPA_RECEIVED),
          bpa_ptr_(bpa_ptr.release())
    {
    }

    /// The pointer to the bpa message
    std::unique_ptr<rtrmessage::bpa> bpa_ptr_;
};

/**
 * Event class for bundle transmission event
 */
class EhsBundleTransmittedEvent: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsBundleTransmittedEvent(std::string link_id)
        : EhsEvent(EHS_BUNDLE_TRANSMITTED),
          link_id_(link_id)
    {
    }

    /// Link ID on which bundle was transmitted
    std::string link_id_;
};

/**
 * Event class for freeing a bundle
 */
class EhsFreeBundleReq: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsFreeBundleReq(EhsBundle* bundle)
        : EhsEvent(EHS_FREE_BUNDLE_REQ),
          bundle_(bundle)
    {
    }

    /// The pointer to the bundle
    EhsBundle* bundle_;
};

/**
 * Event class for routing a bundle
 */
class EhsRouteBundleReq: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsRouteBundleReq(EhsBundleRef bref)
        : EhsEvent(EHS_ROUTE_BUNDLE_REQ),
          bref_(bref)
    {
    }

    /// A BundleRef pointer to the bundle
    EhsBundleRef bref_;
};

/**
 * Event class for routing a bundle
 */
class EhsRouteBundleListReq: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsRouteBundleListReq(EhsBundlePriorityQueue* blist)
        : EhsEvent(EHS_ROUTE_BUNDLE_LIST_REQ),
          blist_(blist)
    {
    }

    /// The list of bundles to route
    EhsBundlePriorityQueue* blist_;
};

/**
 * Event class for deleting a bundle from our lists
 */
class EhsDeleteBundleReq: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsDeleteBundleReq(EhsBundleRef bref)
        : EhsEvent(EHS_DELETE_BUNDLE_REQ),
          bref_(bref)
    {
    }

    /// A BundleRef pointer to the bundle
    EhsBundleRef bref_;
};

/**
 * Event class for signaling EhsRouter to update Next Hops
 */
class EhsReconfigureLinkReq: public EhsEvent {
public:
    /**
     * Constructor
     */
    EhsReconfigureLinkReq()
        : EhsEvent(EHS_RECONFIGURE_LINK_REQ)
    {
    }
};

} // namespace dtn

#endif // XERCES_C_ENABLED && EHS_DP_ENABLED

#endif // EHSROUTER_ENABLED

#endif /* _EHS_EVENT_H_ */
