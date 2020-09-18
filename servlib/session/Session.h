/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <oasys/debug/Formatter.h>
#include <oasys/debug/Logger.h>

#include "Subscriber.h"
#include "bundling/BundleList.h"
#include "bundling/SequenceID.h"
#include "naming/EndpointID.h"
#include "routing/RouteEntry.h"

namespace oasys {
class Timer;
}

namespace dtn {

/**
 * Class to manage a session.
 */
class Session : public oasys::Formatter,
                public oasys::Logger,
                public RouteEntryInfo
{
public:
    class ResubscribeTimer;
    
    /**
     * Type for session-related flags. XXX/demmer should split these
     * into two enums.
     */
    typedef enum {
        NONE             = 0,
        SUBSCRIBE        = 1<<1,
        RESUBSCRIBE      = 1<<2,
        UNSUBSCRIBE      = 1<<3,
        SUBSCRIPTION_ACK = 1<<4,
        PUBLISH          = 1<<5,
        CUSTODY          = 1<<6
    } flags_t;

    /**
     * Pretty print function for session flags.
     */
    static const char* flag_str(u_int flags);

    Session(const EndpointID& eid);
    virtual ~Session();

    /// virtual from Formatter
    virtual int format(char* buf, size_t sz) const;

    void set_upstream(const Subscriber& subscriber);
    void add_subscriber(const Subscriber& subscriber);
    void set_resubscribe_timer(oasys::Timer* t) { resubscribe_timer_ = t; }
    
    u_int32_t             id()              const { return id_; }
    const EndpointID&     eid()             const { return eid_;}
    const Subscriber&     upstream()        const { return upstream_; }
    const SubscriberList& subscribers()     const { return subscribers_; }
    BundleList*           bundles()               { return &bundles_; }
    SequenceID*           sequence_id()           { return &sequence_id_; }
    oasys::Timer*         resubscribe_timer()     { return resubscribe_timer_; }

protected:
    u_int32_t  id_;              ///< Unique session id (for logging)
    EndpointID eid_;	         ///< EndpointID naming the session
    Subscriber upstream_;        ///< Subscriber in direction of a custodian
    SubscriberList subscribers_; ///< Local + downstream subscribers
    BundleList bundles_;         ///< Currently-resident session bundles
    SequenceID sequence_id_;     ///< SequenceID that covers all bundles
    oasys::Timer* resubscribe_timer_; ///< Timer to re-send subscribe message
};

} // namespace dtn

#endif /* _SESSION_H_ */
