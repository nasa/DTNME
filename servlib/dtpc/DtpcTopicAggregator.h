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

#ifndef _DTPC_TOPIC_AGGREGATOR_H_
#define _DTPC_TOPIC_AGGREGATOR_H_

#include <map>

#include "oasys/thread/SpinLock.h"

#include "DtpcApplicationDataItem.h"
#include "DtpcProtocolDataUnit.h"

namespace dtn {


class DtpcPayloadAggregator;
class DtpcTopicAggregator;

// Definitions for using maps of DTPC TopicAggregators
typedef std::map<u_int32_t, DtpcTopicAggregator*>    DtpcTopicAggregatorMap;
typedef std::pair<u_int32_t, DtpcTopicAggregator*>   DtpcTopicAggregatorPair;
typedef DtpcTopicAggregatorMap::iterator             DtpcTopicAggregatorIterator;
typedef std::pair<DtpcTopicAggregatorIterator, bool> DtpcTopicAggregatorInsertResult;


/**
 * DTPC TopicAggregator object is instantiated for each unique 
 * Topic within a DTPC PayloadAggregator. It manages a list of 
 * data PDUs for transmission and reception.
 */
class DtpcTopicAggregator: public oasys::Logger,
                           public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    DtpcTopicAggregator(u_int32_t topic_id, 
                        DtpcPayloadAggregator* payload_agg);

    /**
     * Constructor for deserialization
     */
    DtpcTopicAggregator(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcTopicAggregator ();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /**
     * Add an Application Data Item to the list to be sent
     */
    virtual int64_t send_data_item(DtpcApplicationDataItem* data_item, bool optimize, 
                                    bool* wait_for_elision_func);

    /**
     * Process the results of an elision function invocation
     */
    virtual int64_t elision_func_response(bool modified, 
                                          DtpcApplicationDataItemList* new_data_item_list);
    /**
     * Fill the buffer with the data items and return the length added
     */
    virtual int64_t load_payload(DtpcPayloadBuffer* buf);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual u_int32_t durable_key() { return topic_id_; }

    /// @{ Accessors
    virtual oasys::Lock&                 lock()        { return lock_; }
    virtual const u_int32_t&             topic_id()    { return topic_id_; }
    virtual DtpcPayloadAggregator*       payload_agg() { return payload_aggregator_; }
    virtual const u_int32_t&             item_count()  { return item_count_; }
    virtual DtpcApplicationDataItemList* adi_list()    { return &data_item_list_; }

    virtual const bool&         in_datastore()   { return in_datastore_; }
    virtual const bool& queued_for_datastore()   { return queued_for_datastore_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_topic_id(u_int32_t t)                 { topic_id_ = t; }
    virtual void set_payload_agg(DtpcPayloadAggregator* t) { payload_aggregator_ = t; }

    virtual void set_in_datastore(bool t)         { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t) { queued_for_datastore_ = t; }
    /// @}
protected:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// Topic ID
    u_int32_t topic_id_;

    /// Parent payload aggregator
    DtpcPayloadAggregator* payload_aggregator_;

    /// List of Application Data Items being aggregated
    DtpcApplicationDataItemList data_item_list_;

    /// Current size (bytes) of the data_item_list
    int64_t size_;

    /// Pending size (bytes) of the data_item_list if 
    /// not modified by an elision function
    int64_t pending_size_delta_;

    /// Current count of items on the data_item_list
    u_int32_t item_count_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

};

} // namespace dtn

#endif /* _DTPC_TOPIC_AGGREGATOR_H_ */
