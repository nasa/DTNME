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

#ifndef _DTPC_PAYLOAD_AGGREGATOR_H_
#define _DTPC_PAYLOAD_AGGREGATOR_H_

#include <map>

#include "oasys/debug/Log.h"
#include "oasys/serialize/Serialize.h"
#include "oasys/thread/SpinLock.h"
#include "oasys/thread/Thread.h"
#include "oasys/util/ScratchBuffer.h"
#include "oasys/thread/Notifier.h"

#include "bundling/BundleEvent.h"
#include "bundling/BundleEventHandler.h"

#include "naming/EndpointID.h"

#include "DtpcTopicAggregator.h"
#include "DtpcProtocolDataUnit.h"

namespace dtn {

class Bundle;
class DtpcPayloadAggregationTimer;
class DtpcApplicationDataItem;
class DtpcPayloadAggregator;
class DtpcProfile;


// Definitions for using maps of DTPC PayloadAggregators
typedef std::map<std::string, DtpcPayloadAggregator*>  DtpcPayloadAggregatorMap;
typedef std::pair<std::string, DtpcPayloadAggregator*> DtpcPayloadAggregatorPair;
typedef DtpcPayloadAggregatorMap::iterator             DtpcPayloadAggregatorIterator;
typedef std::pair<DtpcPayloadAggregatorIterator, bool> DtpcPayloadAggregatorInsertResult;


/**
 * DTPC PayloadAggregator object is instantiated for each unique 
 * Destination EID and Profile ID combination. It manages a list of 
 * topics for transmission and reception.
 */
class DtpcPayloadAggregator: public oasys::SerializableObject,
                             public BundleEventHandler,
                             public oasys::Thread
{
public:


    /**
     * Constructor
     *
     * key is a string consisting of an Endpoint ID and a Profile ID
     */
    DtpcPayloadAggregator(std::string key, 
                          const EndpointID& dest_eid, 
                          const u_int32_t profile_id);

    /**
     * Constructor for deserialization
     */
    DtpcPayloadAggregator(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcPayloadAggregator ();

    /**
     * Initialization routine
     */
    virtual void do_init();

    /**
     * Processing needed after a reload from the data store
     */
    virtual void ds_reload_post_processing();

    /**
     * Delete from the data store
     */
    virtual void del_from_datastore();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);


    /**
     * Return the number of events currently waiting for processing.
     * This may be overridden in a future simulator
     */
    virtual size_t event_queue_size()
    {
    	return eventq_->size();
    }

    /**
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */
   virtual void post(BundleEvent* event);
 
    /**
     * Queues the event at the head of the queue for processing by the
     * daemon thread.
     */
    virtual void post_at_head(BundleEvent* event);
    
    /**
     * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

    /**
     * Adds an Application Data Item to the specified Topic to be sent
     */
    virtual int send_data_item(u_int32_t topic_id, DtpcApplicationDataItem* data_item);

    /**
     * Sends a payload to configured destination using the configured profile
     */
    virtual bool send_payload();

    /**
     * Initializes the common elements of a bundle
     */
    virtual Bundle* init_bundle();

    /**
     * Transmits the PDU
     */
    virtual bool transmit_pdu();

    /**
     * Retransmits the PDU
     */
    virtual bool retransmit_pdu(DtpcProtocolDataUnit* pdu);

    /**
     * Aggregation Timer expired processing
     */
    virtual void timer_expired(u_int64_t seq_ctr);

    /**
     * Process response from an elision function invocation
     */
    virtual void elision_func_response(u_int32_t topic_id, bool modified,
                                       DtpcApplicationDataItemList* data_item_list);

    /**
     * Main event handling function.
     */
    virtual void handle_event(BundleEvent* event);
    virtual void handle_event(BundleEvent* event, bool closeTransaction);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual std::string durable_key() { return key_; }

    /// @{ Accessors
    virtual oasys::Lock&        lock()             { return lock_; }
    virtual u_int32_t           profile_id()       { return profile_id_; }
    virtual const EndpointID&   dest_eid()         { return dest_eid_; }
    virtual u_int64_t           seq_ctr()          { return seq_ctr_; }
    virtual int64_t             size()             { return size_; }

    virtual bool in_datastore()                    { return in_datastore_; }
    virtual bool queued_for_datastore()            { return queued_for_datastore_; }
    virtual bool reloaded_from_ds()                { return reloaded_from_ds_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_profile_id(u_int32_t t)       { profile_id_ = t; }
    virtual EndpointID* mutable_dest_eid()         { return &dest_eid_; }

    virtual void set_in_datastore(bool t)          { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t)  { queued_for_datastore_ = t; }
    virtual void set_reloaded_from_ds()            { reloaded_from_ds_ = true; }
    /// @}

protected:
    /**
     * Main thread function that dispatches events.
     */
    virtual void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    virtual void handle_dtpc_send_data_item(DtpcSendDataItemEvent* event);
    virtual void handle_dtpc_payload_aggregation_timer_expired(DtpcPayloadAggregationTimerExpiredEvent* event);
    /// @}

    /// @{
    virtual void event_handlers_completed(BundleEvent* event);
    /// @}

    /**
     * Add self to the persistent storage
     */
    virtual void add_to_datastore();


    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

protected:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique string consisting of the EID and Profile ID
    std::string key_;

    /// Profile ID in use
    u_int32_t profile_id_;

    /// Payload Counter
    u_int64_t seq_ctr_;

    /// Current size of the aggregating payload
    int64_t size_;

    /// EID - Source or Destination depending on context
    EndpointID dest_eid_;

    /// List of the Topic Aggregators
    DtpcTopicAggregatorMap topic_agg_map_;

    /// Aggregation Timer signals payload should be sent
    DtpcPayloadAggregationTimer* timer_;

    /// Time when the Aggregation Timer should trigger (kept for reload from datastore)
    struct timeval expiration_time_;

    /// buffer in which to construct the payload
    /// DtpcPayloadBuffer is defined in DtpcProtocolDataUnit.h and if
    /// Transport Service is in use then the buf_ is handed off to
    /// a PDU and a new buf_ is instantiated for the next payload
    //oasys::ScratchBuffer<u_int8_t*, 4096>* buf_;
    DtpcPayloadBuffer* buf_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// Reloaded from the datastore flag
    bool reloaded_from_ds_;

    /// Must pause processing events if waiting for an elision function response
    bool wait_for_elision_func_;

    /// Notifier used to signal that elision func responded
    oasys::Notifier elision_func_response_notifier_;

    /// Statistics structure definition
    struct Stats {
        u_int32_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    /// Time value when the last event was handled
    oasys::Time last_event_;


};

} // namespace dtn

#endif /* _DTPC_PAYLOAD_AGGREGATOR_H_ */
