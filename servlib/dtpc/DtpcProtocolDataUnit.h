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

#ifndef _DTPC_PROTOCOL_DATA_UNIT_H_
#define _DTPC_PROTOCOL_DATA_UNIT_H_

#include <map>

#include "oasys/debug/Log.h"
#include <oasys/serialize/Serialize.h>
#include "oasys/util/ScratchBuffer.h"
#include "oasys/thread/SpinLock.h"

#include "naming/EndpointID.h"

namespace dtn {

class DtpcPayloadAggregationTimer;
class DtpcApplicationDataItem;
class DtpcProtocolDataUnit;
class DtpcProfile;
class DtpcRetransmitTimer;


// Definitions for using maps of DTPC ProtocolDataUnits
typedef std::map<std::string, DtpcProtocolDataUnit*>  DtpcProtocolDataUnitMap;
typedef std::pair<std::string, DtpcProtocolDataUnit*> DtpcProtocolDataUnitPair;
typedef DtpcProtocolDataUnitMap::iterator             DtpcProtocolDataUnitIterator;
typedef std::pair<DtpcProtocolDataUnitIterator, bool> DtpcProtocolDataUnitInsertResult;

typedef std::map<u_int64_t, DtpcProtocolDataUnit*>    DtpcPduSeqCtrMap;
typedef std::pair<u_int64_t, DtpcProtocolDataUnit*>   DtpcPduSeqCtrPair;
typedef DtpcPduSeqCtrMap::iterator                    DtpcPduSeqCtrIterator;
typedef std::pair<DtpcPduSeqCtrIterator, bool>        DtpcPduSeqCtrInsertResult;

typedef oasys::ScratchBuffer<u_int8_t*, 1024>         DtpcPayloadBuffer;

/**
 * DTPC ProtocolDataUnit object is instantiated for each PDU
 * transmitted utilizing the Transport Service. The key is a combo of
 * the Destination/Profile/SeqCtr.
 */
class DtpcProtocolDataUnit: public oasys::Logger,
                            public oasys::SerializableObject
{
public:
    /**
     * DTPC constants 
     */
    typedef enum {
        DTPC_VERSION_NUMBER    = 0,
        DTPC_VERSION_BIT_SHIFT = 6,
        DTPC_VERSION_MASK      = 0xc0,

        DTPC_PDU_TYPE_DATA  = 0,
        DTPC_PDU_TYPE_ACK   = 1,
        DTPC_PDU_TYPE_MASK  = 0x01,
        
    } dtpc_constants_t;


    /**
     * Constructor
     *
     * key is a string consisting of an Endpoint ID and a Profile ID
     */
    DtpcProtocolDataUnit(const EndpointID& remote_eid, 
                         u_int32_t profile_id,
                         u_int64_t seq_ctr);

    /**
     * Constructor for deserialization
     */
    DtpcProtocolDataUnit(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcProtocolDataUnit ();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);


    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual std::string durable_key() { return key(); }

    /// @{ Accessors
    virtual oasys::Lock&             lock()              { return lock_; }
    virtual std::string              key();
    virtual std::string              ion_alt_key();      // see cc file for exlpanation
    virtual bool                     has_ion_alt_key();
    virtual std::string              collector_key();
    virtual u_int32_t                profile_id()        { return profile_id_; }
    virtual const EndpointID&        remote_eid()        { return remote_eid_; }
    virtual const EndpointID&        local_eid()         { return local_eid_; }
    virtual u_int64_t                seq_ctr()           { return seq_ctr_; }
    virtual int                      retransmit_count()  { return retransmit_count_; }
    virtual DtpcRetransmitTimer*     retransmit_timer()  { return retransmit_timer_; }
    virtual time_t                   retransmit_ts()     { return retransmit_ts_; }
    virtual int64_t                  size()              { return buf_->len(); }
    virtual const DtpcPayloadBuffer* buf()               { return buf_; }
    virtual time_t                   creation_ts()       { return creation_ts_; }
    virtual time_t                   expiration_ts()     { return expiration_ts_; }
    virtual bool                     app_ack()           { return app_ack_; }
    virtual size_t                   topic_block_index() { return topic_block_index_; }

    virtual const bool&              in_datastore()      { return in_datastore_; }
    virtual const bool&      queued_for_datastore()      { return queued_for_datastore_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_profile_id(u_int32_t t);
    virtual void set_local_eid(const EndpointID& eid);
    virtual void set_remote_eid(const EndpointID& eid);
    virtual void set_seq_ctr(u_int64_t t);
    virtual int inc_retransmit_count()                        { return ++retransmit_count_; }
    virtual void set_retransmit_timer(DtpcRetransmitTimer* t, time_t ts) { retransmit_timer_ = t; retransmit_ts_ = ts; }
    virtual void set_buf(DtpcPayloadBuffer* t)                { buf_ = t; }
    virtual void set_creation_ts(time_t t)                    { creation_ts_ = t; }
    virtual void set_expiration_ts(time_t t)                  { expiration_ts_ = t; }
    virtual void set_app_ack(bool t)                          { app_ack_ = t; }
    virtual void set_topic_block_index(size_t t)              { topic_block_index_ = t; }

    virtual void set_in_datastore(bool t)          { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t)  { queued_for_datastore_ = t; }
    /// @}
private:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique string consisting of the EID and Profile ID and Seq Ctr
    std::string key_;

    /// unique string consisting of the EID and Profile ID
    std::string collector_key_;

    /// Flag indicating that the key string is up to date
    bool key_is_set_;

    /// An alternate key for ION compatibility:
    /// we send to ipn:x.129 and expect ack back from that address
    /// but ack come back with a source ipn:x.128 so we compensate
    /// with the ion_alt_key_
    std::string ion_alt_key_;
    bool has_ion_alt_key_;

    /// Profile ID in use
    u_int32_t profile_id_;

    /// Payload Counter
    u_int64_t seq_ctr_;

    /// EID - Remote EID - Source or Destination depending on context
    EndpointID remote_eid_;

    /// EID - Local EID (dest EID of a recevied Data PDU)
    EndpointID local_eid_;

    /// Number of times this PDU has been retransmitted
    int retransmit_count_;

    /// Expiration time (seconds since 1/1/1970 - actual time)
    time_t retransmit_ts_;

    /// Retransmit Timer signals when payload should be re-sent
    DtpcRetransmitTimer* retransmit_timer_;

    /// buffer to hold the payload
    //oasys::ScratchBuffer<u_int8_t*, 4096>* buf_;
    DtpcPayloadBuffer* buf_;

    /// Creation timestamp (seconds since 1/1/2000)
    time_t creation_ts_;

    /// Expiration time (seconds since 1/1/1970 - actual time, not a lifetime)
    time_t expiration_ts_;

    /// Application ACK requested flag (used when receiving a DPDU)
    bool app_ack_;

    /// Index within buf_ where the first Topic Block starts
    size_t topic_block_index_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /**
     * internal method to set the key strings
     */
    virtual void set_key();
};

} // namespace dtn

#endif /* _DTPC_PROTOCOL_DATA_UNIT_H_ */
