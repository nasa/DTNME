/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _FORWARDINGINFO_H_
#define _FORWARDINGINFO_H_

#include <string>
#include <sys/time.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/debug/Log.h>
#include <oasys/util/Time.h>
#include "CustodyTimer.h"

namespace dtn {

/**
 * Class to encapsulate bundle forwarding information. This is created
 * when a bundle is forwarded to log a record of the forwarding event,
 * along with any route-specific information about the action, such as
 * the custody timer.
 *
 * Routing algorithms consult this log to determine their course of
 * action, for instance if they don't want to retransmit to the same
 * next hop twice.
 */
class ForwardingInfo : public oasys::SerializableObject,
                       public oasys::Logger {
public:
    /**
     * The forwarding action type codes.
     */
    typedef enum {
        INVALID_ACTION = 0,	///< Invalid action
        FORWARD_ACTION = 1 << 0,///< Forward the bundle to only this next hop
        COPY_ACTION    = 1 << 1,///< Forward a copy of the bundle
    } action_t;
    
    /**
     * Convenience flag to specify any forwarding action for use in
     * searching the log.
     */
    static const unsigned int ANY_ACTION = 0xffffffff;

    static inline const char* action_to_str(action_t action)
    {
        switch(action) {
        case INVALID_ACTION:	return "INVALID";
        case FORWARD_ACTION:	return "FORWARD";
        case COPY_ACTION:	return "COPY";
        default:
            NOTREACHED;
        }
    }

    /**
     * The forwarding log state codes.
     */
    typedef enum {
        NONE             = 0,       ///< Return value for no entry
        QUEUED           = 1 << 0,  ///< Currently queued or being sent
        TRANSMITTED      = 1 << 1,  ///< Successfully transmitted
        TRANSMIT_FAILED  = 1 << 2,  ///< Transmission failed
        CANCELLED        = 1 << 3,  ///< Transmission cancelled
        CUSTODY_TIMEOUT  = 1 << 4,  ///< Custody transfer timeout
        PENDING_DELIVERY = 1 << 5,  ///< Pending delivery to local registration
        DELIVERED        = 1 << 6,  ///< Delivered to local registration
        SUPPRESSED       = 1 << 7,  ///< Transmission suppressed
        RECEIVED         = 1 << 10, ///< Where the bundle came from
    } state_t;

    /**
     * Convenience flag to specify any forwarding state for use in
     * searching the log.
     */
    static const unsigned int ANY_STATE = 0xffffffff;

    static const char* state_to_str(state_t state)
    {
        switch(state) {
        case NONE:      	return "NONE";
        case QUEUED: 		return "QUEUED";
        case TRANSMITTED:      	return "TRANSMITTED";
        case TRANSMIT_FAILED:  	return "TRANSMIT_FAILED";
        case CANCELLED: 	return "CANCELLED";
        case CUSTODY_TIMEOUT:	return "CUSTODY_TIMEOUT";
        case PENDING_DELIVERY: 	return "PENDING_DELIVERY";
        case DELIVERED:      	return "DELIVERED";
        case SUPPRESSED:      	return "SUPPRESSED";
        case RECEIVED:      	return "RECEIVED";

        default:
            NOTREACHED;
        }
    }

    /**
     * Default constructor.
     */
    ForwardingInfo()
        : Logger("ForwardingInfo", "/dtn/bundle/forwardingInfo"),
          state_(NONE),
          action_(INVALID_ACTION),
          link_name_(""),
          regid_(0xffffffff),
          remote_eid_(),
          custody_spec_() {}

    /*
     * Constructor for serialization.
     */
    ForwardingInfo(const oasys::Builder& builder)
        : Logger("ForwardingInfo", "/dtn/bundle/forwardingInfo"),
          state_(NONE),
          action_(INVALID_ACTION),
          link_name_(""),
          regid_(0xffffffff),
          remote_eid_(builder),
          custody_spec_() {}
    
    /**
     * Constructor used for new entries.
     */
    ForwardingInfo(state_t                 state,
                   action_t                action,
                   const std::string&      link_name,
                   u_int32_t               regid,
                   const EndpointID&       remote_eid,
                   const CustodyTimerSpec& custody_spec)
        : Logger("ForwardingInfo", "/dtn/bundle/forwardingInfo"),
         state_(NONE),
          action_(action),
          link_name_(link_name),
          regid_(regid),
          remote_eid_(remote_eid),
          custody_spec_(custody_spec)
    {
        set_state(state);
    }

    /**
     * Set the state and update the timestamp.
     */
    void set_state(state_t new_state)
    {
        state_ = new_state;
        timestamp_.get_time();
    }

    virtual void serialize(oasys::SerializeAction *a);

    /// @{ Accessors
    state_t                 state()        const { return static_cast<state_t>(state_); }
    action_t                action()       const { return static_cast<action_t>(action_); }
    const std::string&      link_name()    const { return link_name_; }
    u_int32_t               regid()        const { return regid_; }
    const EndpointID&       remote_eid()   const { return remote_eid_; }
    const oasys::Time&      timestamp()    const { return timestamp_; }
    const CustodyTimerSpec& custody_spec() const { return custody_spec_; }
    /// @}
    
private:
    u_int32_t        state_;            ///< State of the transmission
    u_int32_t        action_;           ///< Forwarding action
    std::string      link_name_;        ///< The name of the link
    u_int32_t        regid_;            ///< The regid (DELIVERED/PENDINGDELIVERY only)
    EndpointID       remote_eid_;       ///< The EID of the next hop node/reg
    oasys::Time      timestamp_;        ///< Timestamp of last state update
    CustodyTimerSpec custody_spec_;     ///< Custody timer information 
};

} // namespace dtn

#endif /* _FORWARDINGINFO_H_ */
