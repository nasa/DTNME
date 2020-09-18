/*
 *    Copyright 2004-2006 Intel Corporation
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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include <sys/time.h>

#include <oasys/debug/Formatter.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>

#include "BlockInfo.h"
#include "BundleMappings.h"
#include "BundlePayload.h"
#include "BundleTimestamp.h"
#include "CustodyTimer.h"
#include "ForwardingLog.h"
#include "GbofId.h"
#include "MetadataBlock.h"
#include "SequenceID.h"
#include "naming/EndpointID.h"
#include "security/BundleSecurityConfig.h"
#include "security/Ciphersuite.h"

typedef oasys::ScratchBuffer<u_char*, 64> DataBuffer;

namespace dtn {

class BundleListBase;
class BundleStore;
class ExpirationTimer;
class SQLBundleStore;

/**
 * The internal representation of a bundle.
 *
 * Bundles are reference counted, with references generally
 * correlating one-to-one with each BundleList on which the Bundle
 * resides.
 *
 * However, although the push() methods of the BundleList always add a
 * reference and a backpointer to the bundle, the pop() methods do not
 * decremente the reference count. This means that the caller must
 * explicitly remove it when done with the bundle.
 *
 * Note that delref() will delete the Bundle when the reference count
 * reaches zero, so care must be taken to never use the pointer after
 * that point.
 *
 * The Bundle class maintains a set of back-pointers to each BundleList
 * it is contained on, and list addition/removal methods maintain the
 * invariant that the entiries of this set correlate exactly with the
 * list pointers.
 */
class Bundle : public oasys::Formatter,
               public oasys::Logger,
               public oasys::SerializableObject
{
public:
    /**
     * Default constructor to create an empty bundle, initializing all
     * fields to defaults and allocating a new bundle id.
     *
     * For temporary bundles, the location can be set to MEMORY, and
     * to support the simulator, the location can be overridden to be
     * BundlePayload::NODATA.
     */
    Bundle(BundlePayload::location_t location = BundlePayload::DISK);

    /**
     * Constructor when re-reading the database.
     */
    Bundle(const oasys::Builder&);

    /**
     * Bundle destructor.
     */
    virtual ~Bundle();

    /**
     * Copy the metadata from one bundle to another (used in
     * fragmentation).
     */
    void copy_metadata(Bundle* new_bundle) const;

    /**
     * Virtual from formatter.
     */
    int format(char* buf, size_t sz) const;
    
    /**
     * Virtual from formatter.
     */
    void format_verbose(oasys::StringBuffer* buf);
    
    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    bundleid_t durable_key() { return bundleid_; }

    /**
     * Hook for the bundle store implementation to count the storage
     * impact of this bundle. Currently just returns the payload
     * length but should be extended to include the metadata as well.
     */
    size_t durable_size() { return payload_.length(); }
    
    /**
     * Return the bundle's reference count, corresponding to the
     * number of entries in the mappings set, i.e. the number of
     * BundleLists that have a reference to this bundle, as well as
     * any other scopes that are processing the bundle.
     */
    int refcount() { return refcount_; }

    /**
     * Bump up the reference count. Parameters are used for logging.
     *
     * @return the new reference count
     */
    int add_ref(const char* what1, const char* what2 = "");

    /**
     * Decrement the reference count. Parameters are used for logging.
     *
     * If the reference count becomes zero, the bundle is deleted.
     *
     * @return the new reference count
     */
    int del_ref(const char* what1, const char* what2 = "");

    /**
     * Return the number of mappings for this bundle.
     */
    size_t num_mappings();

    /**
     * Return a pointer to the mappings. Requires that the bundle be
     * locked.
     */
    BundleMappings* mappings();

    /**
     * Return true if the bundle is on the given list. 
     */
    bool is_queued_on(const BundleListBase* l);

    /**
     * Validate the bundle's fields
     */
    bool validate(oasys::StringBuffer* errbuf);

    /**
     * True if any return receipt fields are set
     */
    bool receipt_requested() const
    {
        return (receive_rcpt_ || custody_rcpt_ || forward_rcpt_ ||
                delivery_rcpt_ || deletion_rcpt_);
    }
    
    /**
     * Values for the bundle priority field.
     */
    typedef enum {
        COS_INVALID   = -1, 		///< invalid
        COS_BULK      = 0, 		///< lowest priority
        COS_NORMAL    = 1, 		///< regular priority
        COS_EXPEDITED = 2, 		///< important
        COS_RESERVED  = 3  		///< TBD
    } priority_values_t;

    /**
     * Pretty printer function for bundle_priority_t.
     */
    static const char* prioritytoa(u_int8_t priority) {
        switch (priority) {
        case COS_BULK: 		return "BULK";
        case COS_NORMAL: 	return "NORMAL";
        case COS_EXPEDITED: 	return "EXPEDITED";
        default:		return "_UNKNOWN_PRIORITY_";
        }
    }

    /// @{ Accessors
    bundleid_t        bundleid()          const { return bundleid_; }
    oasys::Lock*      lock()              const { return &lock_; }
    const GbofId&     gbofid()            const { return gbofid_; }
    const std::string gbofid_str()        const { return gbofid_.str(); }

    bool              expired()           const { return expiration_timer_ == NULL; }
    const EndpointID& source()            const { return gbofid_.source(); }
    const EndpointID& dest()              const { return dest_; }
    const EndpointID& custodian()         const { return custodian_; }
    const EndpointID& replyto()           const { return replyto_; }
    const EndpointID& prevhop()           const { return prevhop_; }
    bool              is_fragment()       const { return gbofid_.is_fragment(); }
    bool              is_admin()          const { return is_admin_; }
    bool              do_not_fragment()   const { return do_not_fragment_; }
    bool              custody_requested() const { return custody_requested_; }
    bool              singleton_dest()    const { return singleton_dest_; }
    u_int8_t          priority()          const { return priority_; }
    bool              receive_rcpt()      const { return receive_rcpt_; }
    bool              custody_rcpt()      const { return custody_rcpt_; }
    bool              forward_rcpt()      const { return forward_rcpt_; }
    bool              delivery_rcpt()     const { return delivery_rcpt_; }
    bool              deletion_rcpt()     const { return deletion_rcpt_; }
    bool              app_acked_rcpt()    const { return app_acked_rcpt_; }
    u_int64_t         expiration()        const { return expiration_; }
    int64_t           time_to_expiration(); // -1 if expired else seconds to expiration
    u_int32_t         frag_offset()       const { return gbofid_.frag_offset(); }
    u_int32_t         frag_length()       const { return gbofid_.frag_length(); }
    u_int32_t         orig_length()       const { return orig_length_; }
    bool              in_datastore()      const { return in_datastore_; }
    bool      queued_for_datastore()      const { return queued_for_datastore_;	}
    bool              local_custody()     const { return local_custody_; }
    const std::string& owner()            const { return owner_; }
    bool              fragmented_incoming() const { return fragmented_incoming_; }
    const SequenceID& sequence_id()       const { return sequence_id_; }
    const SequenceID& obsoletes_id()      const { return obsoletes_id_; }
    const EndpointID& session_eid()       const { return session_eid_; }
    u_int8_t          session_flags()     const { return session_flags_; }
    const BundlePayload& payload()        const { return payload_; }
    const ForwardingLog* fwdlog()         const { return &fwdlog_; }
    const BundleTimestamp& creation_ts()  const { return gbofid_.creation_ts(); }
    const BundleTimestamp& extended_id()  const { return extended_id_; }
    const BlockInfoVec& recv_blocks()     const { return recv_blocks_; }
    const MetadataVec& recv_metadata()    const { return recv_metadata_; }
    const LinkMetadataSet& generated_metadata() const { return generated_metadata_; }
    bool              payload_space_reserved() const { return payload_space_reserved_; }
    bool              in_storage_queue()  const { return in_storage_queue_; }
    bool                   deleting()     const { return deleting_; }
    bool          manually_deleting()     const { return manually_deleting_; }

#ifdef BSP_ENABLED
    const BundleSecurityConfig& security_config() const {return security_config_;}
    const u_char *payload_bek() const {return payload_bek_;}
    const u_char * payload_iv() const {return payload_iv_;}
    const u_char * payload_salt() const {return payload_salt_;}
    const u_char * payload_tag() const {return payload_tag_;}
    bool payload_encrypted() const {return payload_encrypted_;}
    bool payload_bek_set() const {return payload_bek_set_;}
    
#endif

    u_int64_t         age()               const { return age_; } ///< [AEB] return age
    oasys::Time       time_aeb()          const { return time_aeb_; } ///< [AEB]
    const BlockInfoVec*    api_blocks_c()   const { return &api_blocks_; }
    const BlockInfoVec&    api_blocks_r()   const { return api_blocks_; }
    bool              is_freed() { return freed_; }

#ifdef ECOS_ENABLED
    bool ecos_enabled()                   const { return ecos_enabled_; }
    uint8_t ecos_flags()                  const { return ecos_flags_; }
    bool ecos_critical()                  const { return ecos_flags_ & 0x01; }
    bool ecos_streaming()                 const { return ecos_flags_ & 0x02; }
    bool ecos_has_flowlabel()             const { return ecos_flags_ & 0x04; }
    bool ecos_reliable()                  const { return ecos_flags_ & 0x08; }
    uint8_t ecos_ordinal()                const { return ecos_ordinal_; }
    uint64_t ecos_flowlabel()             const { return ecos_flowlabel_; }
#endif
    /// @}

    /// @{ Setters and mutable accessors
    EndpointID* mutable_source()       { return gbofid_.mutable_source(); }
    EndpointID* mutable_dest()         { return &dest_; }
    EndpointID* mutable_replyto()      { return &replyto_; }
    EndpointID* mutable_custodian()    { return &custodian_; }
    EndpointID* mutable_prevhop()      { return &prevhop_; }
    void set_is_fragment(bool t)       { gbofid_.set_is_fragment(t); }
    void set_is_admin(bool t)          { is_admin_ = t; }
    void set_do_not_fragment(bool t)   { do_not_fragment_ = t; }
    void set_custody_requested(bool t) { custody_requested_ = t; }
    void set_singleton_dest(bool t)    { singleton_dest_ = t; }
    void set_priority(u_int8_t p)      { priority_ = p; }
    void set_receive_rcpt(bool t)      { receive_rcpt_ = t; }
    void set_custody_rcpt(bool t)      { custody_rcpt_ = t; }
    void set_forward_rcpt(bool t)      { forward_rcpt_ = t; }
    void set_delivery_rcpt(bool t)     { delivery_rcpt_ = t; }
    void set_deletion_rcpt(bool t)     { deletion_rcpt_ = t; }
    void set_app_acked_rcpt(bool t)    { app_acked_rcpt_ = t; }
    void set_expiration(u_int64_t e)   { expiration_ = e; }
    void set_frag_offset(u_int32_t o)  { gbofid_.set_frag_offset(o); }
    void set_frag_length(u_int32_t l)  { gbofid_.set_frag_length(l); }
    void set_orig_length(u_int32_t l)  { orig_length_ = l; }
    void set_in_datastore(bool t)      { in_datastore_ = t; }
    void set_queued_for_datastore(bool t)      { queued_for_datastore_ = t; }
    void set_local_custody(bool t)     { local_custody_ = t; }
    void set_owner(const std::string& s) { owner_ = s; }
    void set_fragmented_incoming(bool t) { fragmented_incoming_ = t; }
    void set_creation_ts(const BundleTimestamp& ts) { gbofid_.set_creation_ts(ts); }
    BundleTimestamp* mutable_creation_ts() { return gbofid_.mutable_creation_ts(); }
    SequenceID* mutable_sequence_id()  { return &sequence_id_; }
    SequenceID* mutable_obsoletes_id() { return &obsoletes_id_; }
    EndpointID* mutable_session_eid()  { return &session_eid_; }
    void set_session_flags(u_int8_t f) { session_flags_ = f; }
    void test_set_bundleid(bundleid_t id) { bundleid_ = id; }
    BundlePayload*   mutable_payload() { return &payload_; }
    ForwardingLog*   fwdlog()          { return &fwdlog_; }
    ExpirationTimer* expiration_timer(){ return expiration_timer_; }
    CustodyTimerVec* custody_timers()  { return &custody_timers_; }
    BlockInfoVec*    api_blocks()      { return &api_blocks_; }
    LinkBlockSet*    xmit_blocks()     { return &xmit_blocks_; }
    BlockInfoVec*    mutable_recv_blocks() { return &recv_blocks_; }
    MetadataVec*     mutable_recv_metadata() { return &recv_metadata_; }
    LinkMetadataSet* mutable_generated_metadata() {
        return &generated_metadata_;
    }
    void set_expiration_timer(ExpirationTimer* e) {
        expiration_timer_ = e;
    }
    void set_payload_space_reserved(bool t=true) { payload_space_reserved_ = t; }
    void set_in_storage_queue(bool t)            { in_storage_queue_ = t; }
    void set_deleting(bool t)                    { deleting_ = t; }
    void set_manually_deleting(bool t)           { manually_deleting_ = t; }

#ifdef BSP_ENABLED
    void set_payload_bek(u_char *bek, u_int32_t bek_len, u_char *iv, u_char *salt) {
        if(payload_bek_!=NULL) {
            free(payload_bek_);
        }
        payload_bek_ = (u_char *)malloc(bek_len);
        payload_bek_len_ = bek_len;
        memcpy(payload_bek_, bek, bek_len);
        memcpy(payload_iv_, iv, 8);
        memcpy(payload_salt_, salt, 4);
        payload_bek_set_ = true;
    }
    void set_payload_encrypted() {
        payload_encrypted_ =true;
    }
    void set_payload_tag(u_char *tag) {
        memcpy(payload_tag_, tag, 16);
    }
    BundleSecurityConfig* mutable_security_config() {
        return &security_config_;
    }

#endif

    void set_age(u_int64_t a)          { age_ = a; } ///< [AEB] set age
    void set_time_aeb(oasys::Time time){ time_aeb_ = time; } ///< [AEB]

#ifdef ACS_ENABLED
    // Aggregate Custody Signal methods
    /**
     * Set/Get the Custody ID that we assign to a bundle when
     * we take custody of it.
     */
    void set_custodyid(bundleid_t t)   { custodyid_ = t; }
    bundleid_t custodyid()             { return custodyid_; }
    /**
     * Set/Get whether or not the bundle was received with a valid CTEB
     */
    void set_cteb_valid(bool t)        { cteb_valid_ = t; }
    bool cteb_valid()                  { return cteb_valid_; }
    /**
     * Set/Get the Custody ID received in a valid CTEB
     * NOTE: using 64 bits because we may process IDs from
     * a server that uses 64 bits even if we are not.
     */
    void set_cteb_custodyid(u_int64_t t)  { cteb_custodyid_ = t; }
    u_int64_t cteb_custodyid()            { return cteb_custodyid_; }
#endif // ACS_ENABLED

#ifdef ECOS_ENABLED
    void set_ecos_enabled(bool t)         { ecos_enabled_ = t; }
    void set_ecos_flags(uint8_t t)        { ecos_flags_ = t; }
    void set_ecos_ordinal(uint8_t t)      { ecos_ordinal_ = t; }
    void set_ecos_flowlabel(uint64_t t)   { ecos_flowlabel_ = t; }
#endif
    /// @}
    
private:
    /*
     * Bundle data fields that correspond to data transferred between
     * nodes according to the bundle protocol.
     */
    GbofId gbofid_;             ///< Unique ID of this bundle maintains the
                                ///      Source eid, Creation timestamp and 
                                ///      Fragment Offset/Length of the bundle
    EndpointID dest_;		///< Destination eid
    EndpointID custodian_;	///< Current custodian eid
    EndpointID replyto_;	///< Reply-To eid
    EndpointID prevhop_;	///< Previous hop eid
    bool is_admin_;		///< Administrative record bundle
    bool do_not_fragment_;	///< Bundle shouldn't be fragmented
    bool custody_requested_;	///< Custody requested
    bool singleton_dest_;	///< Destination endpoint is a singleton
    u_int8_t priority_;		///< Bundle priority
    bool receive_rcpt_;		///< Hop by hop reception receipt
    bool custody_rcpt_;		///< Custody xfer reporting
    bool forward_rcpt_;		///< Hop by hop forwarding reporting
    bool delivery_rcpt_;	///< End-to-end delivery reporting
    bool deletion_rcpt_;	///< Bundle deletion reporting
    bool app_acked_rcpt_;	///< Acknowlege by application reporting
    u_int64_t expiration_;	///< Bundle expiration time
    u_int32_t orig_length_;	///< Length of original bundle
    SequenceID sequence_id_;	///< Sequence id vector
    SequenceID obsoletes_id_;	///< Obsoletes id vector
    EndpointID session_eid_;	///< Session eid
    u_int8_t session_flags_;	///< Session flags
    BundlePayload payload_;	///< Reference to the payload
#ifdef BSP_ENABLED
    u_char *payload_bek_;
    u_int32_t payload_bek_len_;
    u_char payload_salt_[4];
    u_char payload_iv_[8];
    u_char payload_tag_[16];
    bool payload_encrypted_;
    bool payload_bek_set_;
    BundleSecurityConfig security_config_; ///The security config that applies to this particular bundle
#endif
    
    u_int64_t age_;             ///< Age of our bundle [AEB]

    /*
     * Internal fields and structures for managing the bundle that are
     * not transmitted over the network.
     */
    bundleid_t bundleid_;	   ///< Local bundle identifier
    mutable oasys::SpinLock lock_; ///< Lock for bundle data that can be
                                   ///  updated by multiple threads
    bool in_datastore_;		   ///< Is bundle in persistent store
    bool queued_for_datastore_;	   ///< Is bundle queued to be put in persistent store
    bool local_custody_;	   ///< Does local node have custody
    std::string owner_;            ///< Declared entity that "owns" this
                                   ///  bundle, which could be empty
    BundleTimestamp extended_id_;  ///< Identifier for external routers to
                                   ///  refer to duplicate bundles
    ForwardingLog fwdlog_;	   ///< Log of bundle forwarding records
    ExpirationTimer* expiration_timer_;	///< The expiration timer
    CustodyTimerVec custody_timers_; ///< Live custody timers for the bundle
    bool fragmented_incoming_;     ///< Is the bundle an incoming reactive
                                   ///  fragment

    //ExpirationTimer* expiration_timer_aeb_; ///< new timer for debugging [AEB] stuff
    oasys::Time time_aeb_;         ///< keep track of time for [AEB] stuff 

    BlockInfoVec recv_blocks_;	   ///< BP blocks as arrived off the wire
    BlockInfoVec api_blocks_;	   ///< BP blocks given from local API
    LinkBlockSet xmit_blocks_;	   ///< Block vector for each link

    MetadataVec     recv_metadata_;      ///< Metadata as arrived in bundle 
    LinkMetadataSet generated_metadata_; ///< Metadata to be in bundle

    BundleMappings mappings_;      ///< The set of BundleLists that
                               	   ///  contain the Bundle.
    
    int  refcount_;		   ///< Bundle reference count
    bool freed_;		   ///< Flag indicating whether a bundle
                                   ///  free event has been posted
    bool deleting_;		   ///< Flag indicating delete from database is queued
    bool manually_deleting_;	   ///< Flag indicating bundle is being manually delete (external router)

    bool payload_space_reserved_;  ///< Payload space reserved flag
    bool in_storage_queue_;        ///< Flag indicating whether bundle update event is  
                                   ///  queued in the storage thread

#ifdef ACS_ENABLED
    // Aggregate Custody Signal parameters
    bundleid_t custodyid_;          ///< Our Custody ID for the bundle 
    bool cteb_valid_;              ///< Flag indicating the bundle contains 
                                   ///  a valid Custody Transfer Extension
                                   ///  Block (CTEB)
    u_int64_t cteb_custodyid_;     ///< Previous custodian's Custody ID
                                   ///  for the bundle 
#endif // ACS_ENABLED


#ifdef ECOS_ENABLED
    bool ecos_enabled_;
    uint8_t ecos_flags_;
    uint8_t ecos_ordinal_;
    uint64_t ecos_flowlabel_;
#endif

    /**
     * Initialization helper function.
     */
    void init(bundleid_t id);
};


} // namespace dtn

#endif /* _BUNDLE_H_ */
