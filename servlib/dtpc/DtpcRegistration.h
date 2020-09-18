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

#ifndef _DTPC_REGISTRATION_H_
#define _DTPC_REGISTRATION_H_

#include <list>

#include "oasys/thread/SpinLock.h"

#include "reg/APIRegistration.h"

#include "DtpcTopicAggregatorList.h"
#include "DtpcTopicCollectorList.h"

namespace dtn {


class DtpcRegistration;


/**
 * DTPC Registration object is instantiated for each unique 
 * Topic within a DTPC PayloadCollector. It manages a list of 
 * data PDUs for transmission and reception.
 */
class DtpcRegistration: public APIRegistration
{
public:
    /**
     * Constructor
     */
    DtpcRegistration(u_int32_t topic_id, bool has_elision_func);

    /**
     * Constructor for deserialization
     */
    DtpcRegistration(const oasys::Builder& bldr);

    /**
     * Destructor.
     */
    virtual ~DtpcRegistration ();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);


    /**
     * Add the Topic Collector to the list to deliver data item(s)
     */
    virtual void deliver_topic_collector(DtpcTopicCollector* collector);

    /**
     * Add the Topic Aggregator to the list to invoc elision func
     */
    virtual void add_topic_aggregator(DtpcTopicAggregator* aggregator);

    /// @{ Accessors
    virtual u_int32_t                        topic_id()         { return regid(); }
    virtual bool                             has_elision_func() { return has_elision_func_; }
    virtual BlockingDtpcTopicCollectorList*  collector_list()   { return collector_list_; }
    virtual BlockingDtpcTopicAggregatorList* aggregator_list()  { return aggregator_list_; }
    /// @}

    /// @{ Setters and mutable accessors
    /// @}
protected:
    /// List of Topic Collectors with data items to be delivered
    BlockingDtpcTopicCollectorList* collector_list_;

    /// Topic registration has an elision function indication
    bool has_elision_func_;

    /// List of Topic Aggregators needing elision function invocations
    BlockingDtpcTopicAggregatorList* aggregator_list_;
};


/**
 * Typedef for a list of DtpcRegistrations.
 */
class DtpcRegistrationList : public std::list<DtpcRegistration*> {};
typedef DtpcRegistrationList::iterator  DtpcRegistrationIterator;


} // namespace dtn

#endif /* _DTPC_REGISTRATION_H_ */
