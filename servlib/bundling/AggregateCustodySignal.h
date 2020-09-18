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

#ifndef _AGGREGATE_CUSTODY_SIGNAL_H_
#define _AGGREGATE_CUSTODY_SIGNAL_H_

#ifdef ACS_ENABLED

#include <map>

#include "AcsExpirationTimer.h"
#include "BundleProtocol.h"
#include "naming/EndpointID.h"

namespace dtn {

class AcsEntry;
class Bundle;
class PendingAcs;

// map of ACS Entries
typedef std::map<u_int64_t, AcsEntry*> AcsEntryMap;
typedef std::pair<u_int64_t, AcsEntry*> AcsEntryPair;
typedef AcsEntryMap::iterator AcsEntryIterator;

// map of Pending ACS's
typedef std::map<std::string, PendingAcs*> PendingAcsMap;
typedef std::pair<std::string, PendingAcs*> PendingAcsPair;
typedef PendingAcsMap::iterator PendingAcsIterator;
typedef std::pair<PendingAcsIterator, bool> PendingAcsInsertResult;


/**
 * Utility class to format and parse aggregate custody signal bundles.
 */
class AggregateCustodySignal {
public:
    /**
     * The reason codes are defined in the bundle protocol class.
     */
    typedef BundleProtocol::custody_signal_reason_t reason_t;

    /**
     * Struct to hold the payload data of the aggregate custody signal.
     */
    struct data_t {
        u_int8_t        admin_type_;
        u_int8_t        admin_flags_;
        bool            succeeded_;
        u_int8_t        reason_;
        AcsEntryMap*    acs_entry_map_;
    };

    /**
     * Constructor-like function to create a new aggregate custody signal bundle.
     */
    static void create_aggregate_custody_signal(Bundle*           bundle,
                                                PendingAcs*       pacs, 
                                                const EndpointID& source_eid);

    /**
     * Parsing function for aggregate custody signal bundles.
     */
    static bool parse_aggregate_custody_signal(data_t* data,
                                               const u_char* bp, u_int len);

    /**
     * Pretty printer for custody signal reasons.
     */
    static const char* reason_to_str(u_int8_t reason);
};

/**
 * ACS Entry structure consisting of a starting Custody ID and 
 * length of consecutive IDs to acknowledge plus a perameter which 
 * tracks the length of the SDNV encoded entry
 */
class AcsEntry: public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    AcsEntry();

    /**
     * Constructor for deserialization
     */
    AcsEntry(const oasys::Builder&);

    /**
     * Destructor.
     */
    ~AcsEntry() {};

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);

    /// Left Edge Custody ID
    u_int64_t left_edge_;

    /// Difference between this left edge and the right edge of previous fill 
    u_int64_t diff_to_prev_right_edge_;

    /// Length of fill
    u_int64_t length_of_fill_;

    /// SDNV length of entry (diff + fill len)
    int sdnv_length_;
};




/**
 * PendingACS structure which details the type of signal, 
 * the list of ACS Entries and  the overall encoded length 
 * of the Aggregate Custody Signal.
 */
class PendingAcs: public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    PendingAcs(std::string key);

    /**
     * Constructor for deserialization
     */
    PendingAcs(const oasys::Builder&);

    /**
     * Destructor.
     */
    ~PendingAcs ();

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    std::string durable_key() { return key_; }

    /// @{ Accessors
    oasys::Lock&        lock()                  { return lock_; }
    std::string&        key()                   { return key_; }
    u_int32_t&          pacs_id()               { return pacs_id_; }
    EndpointID&         custody_eid()           { return custody_eid_; }
    bool&               succeeded()             { return succeeded_; }
    BundleProtocol::custody_signal_reason_t& reason() { return reason_; }
    u_int32_t&          acs_payload_length()    { return acs_payload_length_; }
    u_int32_t&          num_custody_ids()       { return num_custody_ids_; }
    AcsEntryMap*        acs_entry_map()         { return acs_entry_map_; }
    AcsExpirationTimer* acs_expiration_timer()  { return acs_expiration_timer_; }
    bool              in_datastore()      const { return in_datastore_; }
    bool      queued_for_datastore()      const { return queued_for_datastore_;	}
    u_int               params_revision()       { return params_revision_; }
    u_int               acs_delay()             { return acs_delay_; }
    u_int               acs_size()              { return acs_size_; }
    /// @}

    /// @{ Setters and mutable accessors
    void set_acs_entry_map(AcsEntryMap* m )     { acs_entry_map_ = m; }
    void set_acs_expiration_timer(AcsExpirationTimer* t)  { 
        acs_expiration_timer_ = t; 
    }
    void set_in_datastore(bool t)              { in_datastore_ = t; }
    void set_queued_for_datastore(bool t)      { queued_for_datastore_ = t; }
    u_int* mutable_params_revision()           { return &params_revision_; }
    u_int* mutable_acs_delay()                 { return &acs_delay_; }
    u_int* mutable_acs_size()                  { return &acs_size_; }
    /// @}
private:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique key consisting of destination + reason code
    std::string key_;

    /// unique ID for a pending ACS so expiration timer can verify
    /// that it is sending the correct ACS when triggered
    u_int32_t pacs_id_;

    /// Custodian EID to receive this ACS when it is generated
    EndpointID custody_eid_;

    /// signal type (SUCCESS or FAILURE)
    bool succeeded_;

    /// reason code
    BundleProtocol::custody_signal_reason_t reason_;

    /// ACS length length of payload
    u_int32_t acs_payload_length_;

    /// Number of custody ids to be acked with this ACS
    u_int32_t num_custody_ids_;

    /// map of the Custody IDs for this ACS
    AcsEntryMap* acs_entry_map_; 

    /// Pointer to an ACS Expiration Timer
    AcsExpirationTimer* acs_expiration_timer_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// ACS parameter revision tracker so it can be determined if
    /// the values have changed
    u_int params_revision_;

    /// Seconds to accumulate Custody IDs before sending an ACS
    u_int acs_delay_;

    /// Max size for the ACS payload 
    u_int acs_size_;
};

} // namespace dtn

#endif // ACS_ENABLED

#endif /* _AGGREGATE_CUSTODY_SIGNAL_H_ */
