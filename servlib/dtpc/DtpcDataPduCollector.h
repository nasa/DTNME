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

#ifndef _DTPC_DATA_PDU_COLLECTOR_H_
#define _DTPC_DATA_PDU_COLLECTOR_H_

#include <map>

#include "oasys/debug/Log.h"
#include <oasys/serialize/Serialize.h>
#include "oasys/util/ScratchBuffer.h"
#include "oasys/thread/SpinLock.h"

#include "bundling/BundleRef.h"
#include "naming/EndpointID.h"

#include "DtpcDeliverPduTimer.h"
#include "DtpcProtocolDataUnit.h"
#include "DtpcTopicCollector.h"

namespace dtn {

class Bundle;
class DtpcPayloadAggregationTimer;
class DtpcApplicationDataItem;
class DtpcDataPduCollector;
class DtpcProfile;


// Definitions for using maps of DTPC DataPduCollectors
typedef std::map<std::string, DtpcDataPduCollector*>  DtpcDataPduCollectorMap;
typedef std::pair<std::string, DtpcDataPduCollector*> DtpcDataPduCollectorPair;
typedef DtpcDataPduCollectorMap::iterator             DtpcDataPduCollectorIterator;
typedef std::pair<DtpcDataPduCollectorIterator, bool> DtpcDataPduCollectorInsertResult;


/**
 * DTPC DataPduCollector object is instantiated for each unique 
 * Destination EID and Profile ID combination. It manages a list of 
 * topics for transmission and reception.
 */
class DtpcDataPduCollector: public oasys::Logger,
                            public oasys::SerializableObject
{
public:
    /**
     * Constructor
     *
     * key is a string consisting of an Endpoint ID and a Profile ID
     */
    DtpcDataPduCollector(std::string key, 
                         const EndpointID& remote_eid, 
                         const u_int32_t profile_id);

    /**
     * Constructor for deserialization
     */
    DtpcDataPduCollector(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcDataPduCollector ();

    /**
     * Add to the data store
     */
    virtual void add_to_datastore();

    /**
     * Delete from the data store
     */
    virtual void del_from_datastore();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /**
     * Perform processing/initialization after a reload from the datastore
     */
    virtual void ds_reload_post_processing();

    /**
     * Process received PDU
     */
    virtual void pdu_received(DtpcProtocolDataUnit* pdu, BundleRef& rcvd_bref);

    /**
     * Aggregation Timer expired processing
     */
    virtual void timer_expired(u_int64_t seq_ctr);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual std::string durable_key() { return key_; }

    /// @{ Accessors
    virtual oasys::Lock&        lock()             { return lock_; }
    virtual std::string         key()              { return key_; }
    virtual u_int32_t           profile_id()       { return profile_id_; }
    virtual const EndpointID&   remote_eid()       { return remote_eid_; }
    virtual u_int64_t           seq_ctr()          { return seq_ctr_; }

    virtual bool in_datastore()                    { return in_datastore_; }
    virtual bool queued_for_datastore()            { return queued_for_datastore_; }
    virtual bool reloaded_from_ds()                { return reloaded_from_ds_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_profile_id(u_int32_t t)       { profile_id_ = t; }
    virtual EndpointID* mutable_remote_eid()       { return &remote_eid_; }

    virtual void set_in_datastore(bool t)          { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t)  { queued_for_datastore_ = t; }
    virtual void set_reloaded_from_ds()            { reloaded_from_ds_ = true; }
    /// @}

protected:
    /**
     * Determine if the Topic ID is valid or can be added on the fly
     */
    virtual bool validate_topic_id(u_int32_t topic_id);

    /**
     * Delivers a Topic Collector to the Topic object for delivery to its registration
     */
    virtual void deliver_topic_collector(DtpcTopicCollector* collector);

    /**
     * Send an ACK PDU
     */
    virtual void send_ack(DtpcProtocolDataUnit* pdu, BundleRef& rcvd_bref);

    /**
     * Deliver the PDU contents to the Topic Collectors
     */
    virtual void deliver_pdu(DtpcProtocolDataUnit* pdu);

    /**
     * Add the PDU to the list of PDUs to be processed
     */
    virtual void queue_pdu(DtpcProtocolDataUnit* pdu);

    /**
     * Delivers the next PDU if applicable
     */
    virtual void check_for_deliverable_pdu();

private:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique string consisting of the EID and Profile ID
    std::string key_;

    /// Profile ID in use
    u_int32_t profile_id_;

    /// Next expected Payload Counter
    u_int64_t seq_ctr_;

    /// EID - Source or Destination depending on context
    EndpointID remote_eid_;

    /// Time when the Data PDU delivery timer should trigger (kept for reload from datastore)
    struct timeval expiration_time_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// Reloaded from the datastore flag
    bool reloaded_from_ds_;

    /// List of PDUs received for the Dest/Profile
    DtpcPduSeqCtrMap pdu_map_;

    /// Timer to trigger delivery of a PDU
    DtpcDeliverPduTimer* timer_;

    /// buffer in which to construct the ACK bundle payload
    DtpcPayloadBuffer buf_;
};

} // namespace dtn

#endif /* _DTPC_DATA_PDU_COLLECTOR_H_ */
