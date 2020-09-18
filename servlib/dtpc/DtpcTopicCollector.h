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

#ifndef _DTPC_TOPIC_COLLECTOR_H_
#define _DTPC_TOPIC_COLLECTOR_H_

#include <list>

#include "oasys/thread/SpinLock.h"

#include "DtpcTopicAggregator.h"

namespace dtn {


class DtpcTopicCollector;


/**
 * DTPC TopicCollector object is instantiated for each unique 
 * Topic within a DTPC PayloadCollector. It manages a list of 
 * data PDUs for transmission and reception.
 */
class DtpcTopicCollector: public DtpcTopicAggregator
{
public:
    /**
     * Constructor
     */
    DtpcTopicCollector(u_int32_t topic_id);

    /**
     * Constructor for deserialization
     */
    DtpcTopicCollector(const oasys::Builder& bldr);

    /**
     * Destructor.
     */
    virtual ~DtpcTopicCollector ();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /**
     * Return (and remove) the first ApplicationDataItem from the list
     */
    virtual DtpcApplicationDataItem* pop_data_item();

    /**
     * Push a data item on the list to be delivered
     */
    virtual void deliver_data_item(DtpcApplicationDataItem* data_item);

    /// @{ Accessors
    virtual const EndpointID&   remote_eid()     { return remote_eid_; }
    virtual time_t              expiration_ts()  { return expiration_ts_; }
    virtual bool                expired();
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_remote_eid(const EndpointID& eid)  { remote_eid_.assign(eid); }
    virtual void set_expiration_ts(u_int64_t t)         { expiration_ts_ = t; }
    /// @}
protected:
    /// EID - Remote EID - Source EID from received bundle
    EndpointID remote_eid_;

    /// Expiration time (seconds since 1/1/1970 - actual time, not a lifetime)
    time_t expiration_ts_;
};

} // namespace dtn

#endif /* _DTPC_TOPIC_COLLECTOR_H_ */
