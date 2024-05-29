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


#include <map>

#include "AcsExpirationTimer.h"
#include "BundleProtocol.h"
#include "CborUtil.h"
#include "naming/EndpointID.h"

namespace dtn {

class AcsEntry;
class Bundle;
class PendingAcs;

// map of ACS Entries
typedef std::map<uint64_t, AcsEntry*> AcsEntryMap;
typedef std::pair<uint64_t, AcsEntry*> AcsEntryPair;
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
        uint8_t        admin_type_  = 0;
        uint8_t        admin_flags_ = 0;
        bool           succeeded_   = false;
        bool           redundant_   = false;
        uint8_t        reason_      = 0;
        AcsEntryMap*   acs_entry_map_ = nullptr;
    };

    /**
     * Constructor-like function to create a new aggregate custody signal bundle.
     */
    static void create_aggregate_custody_signal(Bundle*           bundle,
                                                PendingAcs*       pacs, 
                                                const SPtr_EID&   sptr_source_eid);

    /**
     * Parsing function for aggregate custody signal bundles.
     */
    static bool parse_aggregate_custody_signal(data_t* data,
                                               const u_char* bp, uint32_t len);

    /**
     * Pretty printer for custody signal reasons.
     */
    static const char* reason_to_str(uint8_t reason);

    /**
     * Constructor-like function to create a new BIBE custody signal bundle.
     */
    static void create_bibe_custody_signal(Bundle* bundle,
                                           PendingAcs* pacs,
                                           const SPtr_EID& sptr_source_eid);

    /**
     * Parsing function for a BIBE custody signal bundles.
     */
    static bool parse_bibe_custody_signal(CborValue& cvAdminElements, CborUtil& cborutil, data_t* data);
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
    uint64_t left_edge_;

    /// Difference between this left edge and the right edge of previous fill 
    uint64_t diff_to_prev_right_edge_;

    /// Length of fill
    uint64_t length_of_fill_;

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
    uint32_t            pacs_id()               { return pacs_id_; }
    int32_t             bp_version()            { return bp_version_; }
    SPtr_EID            custody_eid()           { return sptr_custody_eid_; }
    bool                succeeded()             { return succeeded_; }
    uint32_t            reason()                { return reason_; }
    uint32_t            acs_payload_length()    { return acs_payload_length_; }
    uint32_t            num_custody_ids()       { return num_custody_ids_; }
    AcsEntryMap*        acs_entry_map()         { return acs_entry_map_; }
    SPtr_AcsExpirationTimer acs_expiration_timer()  { return acs_expiration_timer_; }
    bool                in_datastore()          { return in_datastore_; }
    bool                queued_for_datastore()  { return queued_for_datastore_;	}
    uint32_t            params_revision()       { return params_revision_; }
    uint32_t            acs_delay()             { return acs_delay_; }
    uint32_t            acs_size()              { return acs_size_; }
    /// @}

    /// @{ Setters and mutable accessors
    void set_acs_entry_map(AcsEntryMap* m )     { acs_entry_map_ = m; }

    void set_acs_expiration_timer(SPtr_AcsExpirationTimer t)  {
        acs_expiration_timer_ = t; 
    }
    void clear_acs_expiration_timer()          { acs_expiration_timer_ = nullptr; }

    void set_in_datastore(bool t)              { in_datastore_ = t; }
    void set_bp_version(int32_t bpv)           { bp_version_ = bpv; }
    void set_queued_for_datastore(bool t)      { queued_for_datastore_ = t; }
    uint32_t* mutable_params_revision()        { return &params_revision_; }
    uint32_t* mutable_acs_delay()              { return &acs_delay_; }
    uint32_t* mutable_acs_size()               { return &acs_size_; }
    /// @}
private:
    friend class AggregateCustodySignal;
    friend class BundleDaemonACS;

    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique key consisting of destination + reason code
    std::string key_;

    /// unique ID for a pending ACS so expiration timer can verify
    /// that it is sending the correct ACS when triggered
    uint32_t pacs_id_;

    /// Custodian EID to receive this ACS when it is generated
    SPtr_EID sptr_custody_eid_;

    /// signal type (SUCCESS or FAILURE)
    bool succeeded_;

    /// reason code
    uint32_t reason_;

    /// ACS length length of payload
    uint32_t acs_payload_length_;

    /// Number of custody ids to be acked with this ACS
    uint32_t num_custody_ids_;

    /// map of the Custody IDs for this ACS
    AcsEntryMap* acs_entry_map_; 

    /// Pointer to an ACS Expiration Timer
    SPtr_AcsExpirationTimer acs_expiration_timer_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// ACS parameter revision tracker so it can be determined if
    /// the values have changed
    uint32_t params_revision_;

    /// Seconds to accumulate Custody IDs before sending an ACS
    uint32_t acs_delay_;

    /// Max size for the ACS payload 
    uint32_t acs_size_;

    /// Bundle Protocol version
    int32_t bp_version_;
};

} // namespace dtn


#endif /* _AGGREGATE_CUSTODY_SIGNAL_H_ */
