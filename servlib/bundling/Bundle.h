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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#include <map>
#include <sys/time.h>

#include <third_party/oasys/debug/Formatter.h>
#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

#include "BlockInfo.h"
#include "BundleIMCState.h"
#include "BundleMappings.h"
#include "BundlePayload.h"
#include "BundleProtocol.h"
#include "BundleTimestamp.h"
#include "CustodyTimer.h"
#include "ForwardingLog.h"
#include "GbofId.h"
#include "MetadataBlock.h"
#include "naming/EndpointID.h"

namespace dtn {

class BundleListBase;
class ExpirationTimer;


typedef std::shared_ptr<ExpirationTimer> SPtr_ExpirationTimer;

/// Mapping to track which link redirected a bundle to a given alternate link/conversion layer.
/// Typical use is a redirection to a Bundle In Bundle Encapsulation (BIBE) CL to allow a
/// BPv7 bundle to pass through a BPv6 node. The BIBE CL needs to be able to serialize IMC 
/// bundles based on the original Link Name to generate the correct IMC State Block. This map
/// uses the redirection link name as the key and the original link name as the value.
typedef std::map<std::string, std::string> LinkRedirectMap;
typedef std::unique_ptr<LinkRedirectMap>   QPtr_LinkRedirectMap;
typedef LinkRedirectMap::iterator          LinkRedirectIter;


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
    Bundle(int32_t bp_version=BundleProtocol::BP_VERSION_7,
           BundlePayload::location_t location = BundlePayload::DEFAULT);
           //dzdebug BundlePayload::location_t location = BundlePayload::DISK);

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
        COS_INVALID   = -1,         ///< invalid
        COS_BULK      = 0,         ///< lowest priority
        COS_NORMAL    = 1,         ///< regular priority
        COS_EXPEDITED = 2,         ///< important
        COS_RESERVED  = 3          ///< TBD
    } priority_values_t;

    /**
     * Pretty printer function for bundle_priority_t.
     */
    static const char* prioritytoa(u_int8_t priority) {
        switch (priority) {
        case COS_BULK:         return "BULK";
        case COS_NORMAL:     return "NORMAL";
        case COS_EXPEDITED:     return "EXPEDITED";
        default:        return "_UNKNOWN_PRIORITY_";
        }
    }

    /// @{ Accessors
    bundleid_t        bundleid()          const { return bundleid_; }
    oasys::Lock*      lock()              const { return &lock_; }
    const GbofId&     gbofid()            const { return gbofid_; }
    std::string       gbofid_str()        const;

    bool              expired()           const { return expiration_timer_ == nullptr; }
    const SPtr_EID    source()            const { return gbofid_.source(); }
    const SPtr_EID    dest()              const { return sptr_dest_; }
    const SPtr_EID    custodian()         const { return sptr_custodian_; }
    const SPtr_EID    replyto()           const { return sptr_replyto_; }
    const SPtr_EID    prevhop()           const { return sptr_prevhop_; }
    bool              is_fragment()       const { return gbofid_.is_fragment(); }
    bool              is_admin()          const { return is_admin_; }
    bool              do_not_fragment()   const { return do_not_fragment_; }
    bool              custody_requested() const { return custody_requested_; }
    bool              singleton_dest_flag()    const { return singleton_dest_flag_; }
    //dzdebug bool              singleton_dest()    const { return dest_.is_singleton(); }
    bool              singleton_dest()    const { return sptr_dest_->is_singleton(); }
    u_int8_t          priority()          const { return priority_; }
    bool              receive_rcpt()      const { return receive_rcpt_; }
    bool              custody_rcpt()      const { return custody_rcpt_; }
    bool              forward_rcpt()      const { return forward_rcpt_; }
    bool              delivery_rcpt()     const { return delivery_rcpt_; }
    bool              deletion_rcpt()     const { return deletion_rcpt_; }
    bool              app_acked_rcpt()    const { return app_acked_rcpt_; }
    bool      req_time_in_status_rpt()    const { return req_time_in_status_rpt_; }
    size_t          expiration_secs()   const { return expiration_millis_ / 1000; }
    size_t          expiration_millis() const { return expiration_millis_; }
    int64_t           time_to_expiration_secs() const; ///< -1 if expired else seconds to expiration
    int64_t           time_to_expiration_millis() const; ///< -1 if expired else milliseconds to expiration
    size_t            frag_offset()       const { return gbofid_.frag_offset(); }
    size_t            frag_length()       const { return gbofid_.frag_length(); }
    size_t            orig_length()       const { return orig_length_; }
    bool              in_datastore()      const { return in_datastore_; }
    bool      queued_for_datastore()      const { return queued_for_datastore_;    }
    bool              local_custody()     const { return local_custody_; }
    bool              bibe_custody()      const { return bibe_custody_; }
    bool              fragmented_incoming() const { return fragmented_incoming_; }
    const BundlePayload& payload()        const { return payload_; }
    const ForwardingLog* fwdlog()         const { return &fwdlog_; }
    const BundleTimestamp& creation_ts()  const { return gbofid_.creation_ts(); }

    // returns the creation time in millisecs
    // (converts BPv6 seconds to millisecs)
    size_t creation_time_millis() const;

    // returns the creation time in seconds
    // (converts BPv7 milliseconds to seconds)
    size_t creation_time_secs() const;

    const SPtr_BlockInfoVec recv_blocks() const { return recv_blocks_; }
    const MetadataVec& recv_metadata()    const { return recv_metadata_; }
    const LinkMetadataSet& generated_metadata() const { return generated_metadata_; }
    bool              payload_space_reserved() const { return payload_space_reserved_; }
    bool              in_storage_queue()  const { return in_storage_queue_; }
    bool                   deleting()     const { return deleting_; }
    bool          manually_deleting()     const { return manually_deleting_; }
    int32_t           bp_version()        const { return bp_version_; }
    bool              is_bpv6()           const { return (bp_version_ == BundleProtocol::BP_VERSION_6); }
    bool              is_bpv7()           const { return (bp_version_ == BundleProtocol::BP_VERSION_7); }
    bool              is_bpv_unknown()    const { return (bp_version_ == BundleProtocol::BP_VERSION_UNKNOWN); }
    size_t primary_block_crc_type()     const { return primary_block_crc_type_; }
    size_t highest_rcvd_block_number()  const { return highest_rcvd_block_number_; }
    size_t hop_count()                  const { return hop_count_; }
    size_t hop_limit()                  const { return hop_limit_; }
    size_t prev_bundle_age_secs()       const { return prev_bundle_age_millis_ / 1000; }  // convert to seconds
    size_t prev_bundle_age_millis()     const { return prev_bundle_age_millis_; }  // milliseconds
    size_t current_bundle_age_secs()    const;
    size_t current_bundle_age_millis()  const;
    bool     has_bundle_age_block()       const { return has_bundle_age_block_; }
    bool     expired_in_link_queue()      const { return expired_in_link_queue_; }
    bundleid_t frag_created_from_bundleid() const { return frag_created_from_bundleid_; }

#ifdef BARD_ENABLED
    size_t bard_quota_reserved(bool by_src);
    size_t bard_extquota_reserved(bool by_src);
    size_t bard_in_use(bool by_src);
    bool bard_restage_by_src()             const { return bard_restage_by_src_; }
    std::string bard_restage_link_name()   const { return bard_restage_link_name_; }
    bool bard_requested_restage()          const { return ! bard_restage_link_name_.empty(); }

    bool last_restage_attempt_failed()     const { return last_restage_attempt_failed_; }
    void set_last_restage_attempt_failed()       { last_restage_attempt_failed_ = true; }
    bool all_restage_attempts_failed()     const { return all_restage_attempts_failed_; }
    void set_all_restage_attempts_failed()       { all_restage_attempts_failed_ = true; }
#endif // BARD_ENABLED

    bool is_freed()                       const { return freed_; }

#ifdef ECOS_ENABLED
    bool ecos_enabled()                   const { return ecos_enabled_; }
    uint8_t ecos_flags()                  const { return ecos_flags_; }
    bool ecos_critical()                  const { return ecos_flags_ & 0x01; }
    bool ecos_streaming()                 const { return ecos_flags_ & 0x02; }
    bool ecos_has_flowlabel()             const { return ecos_flags_ & 0x04; }
    bool ecos_reliable()                  const { return ecos_flags_ & 0x08; }
    uint8_t ecos_ordinal()                const { return ecos_ordinal_; }
    size_t ecos_flowlabel()             const { return ecos_flowlabel_; }
#endif
    /// @}

    /// @{ Setters and mutable accessors
    // source, timestamp and frag vals are incorporated into the GbofID oject
    void set_source(const SPtr_EID& sptr_src)       { gbofid_.set_source(sptr_src); }
    void set_source(const std::string& src)         { gbofid_.set_source(src); }
    void set_creation_ts(const BundleTimestamp& ts) { gbofid_.set_creation_ts(ts); }
    void set_creation_ts(size_t secs_or_millisecs, size_t seqno) { gbofid_.set_creation_ts(secs_or_millisecs, seqno); }
    void set_fragment(bool is_frag, size_t offset, size_t length) { gbofid_.set_fragment(is_frag, offset, length); }
    void set_is_fragment(bool is_frag) { gbofid_.set_is_fragment(is_frag); }
    void set_frag_offset(size_t offset) { gbofid_.set_frag_offset(offset); }
    void set_frag_length(size_t length) { gbofid_.set_frag_length(length); }

    SPtr_EID&   mutable_dest()         { return sptr_dest_; }
    SPtr_EID&   mutable_replyto()      { return sptr_replyto_; }
    SPtr_EID&   mutable_custodian()    { return sptr_custodian_; }
    SPtr_EID&   mutable_prevhop()      { return sptr_prevhop_; }
    void set_bp_version(int32_t v)    { bp_version_ = v; }
    void set_is_admin(bool t)          { is_admin_ = t; }
    void set_do_not_fragment(bool t)   { do_not_fragment_ = t; }
    void set_custody_requested(bool t) { custody_requested_ = t && is_bpv6(); }
    void set_singleton_dest_flag(bool t)    { singleton_dest_flag_ = t; }
    void set_priority(u_int8_t p)      { priority_ = p; }
    void set_receive_rcpt(bool t)      { receive_rcpt_ = t; }
    void set_custody_rcpt(bool t)      { custody_rcpt_ = t; }
    void set_forward_rcpt(bool t)      { forward_rcpt_ = t; }
    void set_delivery_rcpt(bool t)     { delivery_rcpt_ = t; }
    void set_deletion_rcpt(bool t)     { deletion_rcpt_ = t; }
    void set_app_acked_rcpt(bool t)    { app_acked_rcpt_ = t; }
    void set_req_time_in_status_rpt(bool t)    { req_time_in_status_rpt_ = t; }
    void set_expiration_secs(size_t  e)   { expiration_millis_ = e * 1000; }  // convert seconds to millisecs
    void set_expiration_millis(size_t  e)   { expiration_millis_ = e; }  // in millisecs
    void set_orig_length(size_t  l)  { orig_length_ = l; }
    void set_in_datastore(bool t)      { in_datastore_ = t; }
    void set_queued_for_datastore(bool t)      { queued_for_datastore_ = t; }
    void set_local_custody(bool t)     { local_custody_ = t; }
    void set_bibe_custody(bool t)      { bibe_custody_ = t; }
    void set_fragmented_incoming(bool t) { fragmented_incoming_ = t; }
    void test_set_bundleid(bundleid_t id) { bundleid_ = id; }
    BundlePayload*   mutable_payload() { return &payload_; }
    ForwardingLog*   fwdlog()          { return &fwdlog_; }

    SPtr_ExpirationTimer expiration_timer();

    CustodyTimerVec*  custody_timers()  { return &custody_timers_; }
    SPtr_BlockInfoVec api_blocks()      { return api_blocks_; }
    LinkBlockSet*     xmit_blocks()     { return &xmit_blocks_; }
    SPtr_BlockInfoVec mutable_recv_blocks() { return recv_blocks_; }
    MetadataVec*      mutable_recv_metadata() { return &recv_metadata_; }
    LinkMetadataSet*  mutable_generated_metadata() { return &generated_metadata_; }

    void set_expiration_timer(SPtr_ExpirationTimer e);
    void clear_expiration_timer();

    void set_payload_space_reserved(bool t=true) { payload_space_reserved_ = t; }
    void set_in_storage_queue(bool t)            { in_storage_queue_ = t; }
    void set_deleting(bool t)                    { deleting_ = t; }
    void set_manually_deleting(bool t)           { manually_deleting_ = t; }

    void set_highest_rcvd_block_number(size_t block_num);
    void set_hop_count(size_t t)               { hop_count_ = t; }
    void set_hop_limit(size_t t)               { hop_limit_ = t; }

    void set_prev_bundle_age_millis(size_t t)  { prev_bundle_age_millis_ = t; has_bundle_age_block_ = true; }
    void set_expired_in_link_queue()             { expired_in_link_queue_ = true; }

    void set_frag_created_from_bundleid(bundleid_t t) { frag_created_from_bundleid_ = t; }

    // Aggregate Custody Signal methods
    /**
     * Set/Get the Custody ID that we assign to a bundle when
     * we take custody of it.
     */
    void set_custodyid(bundleid_t t)   { custodyid_ = t; }
    bundleid_t custodyid()             { return custodyid_; }
    void set_custody_dest(std::string& dest)   { custody_dest_ = dest; }
    std::string& custody_dest()                { return custody_dest_; }

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
    void set_cteb_custodyid(size_t  t)  { cteb_custodyid_ = t; }
    size_t  cteb_custodyid()            { return cteb_custodyid_; }

#ifdef ECOS_ENABLED
    void set_ecos_enabled(bool t)         { ecos_enabled_ = t; }
    void set_ecos_flags(uint8_t t)        { ecos_flags_ = t; }
    void set_ecos_ordinal(uint8_t t)      { ecos_ordinal_ = t; }
    void set_ecos_flowlabel(size_t t)   { ecos_flowlabel_ = t; }
#endif

#ifdef BARD_ENABLED
    void set_bard_quota_reserved(bool by_src, size_t t);
    void set_bard_extquota_reserved(bool by_src, size_t t);
    void set_bard_in_use(bool by_src, size_t t);
    void set_bard_restage_by_src(bool t)            { bard_restage_by_src_ = t; }
    void set_bard_restage_link_name(std::string& t) { bard_restage_link_name_ = t; }
    void clear_bard_restage_link_name() { bard_restage_link_name_.clear(); }
    void add_failed_restage_link_name(std::string& link_name);
    bool find_failed_restage_link_name(std::string& link_name);
#endif //BARD_ENABLED
    /// @}
   
    /// get whether or not the router has processed an IMC bundle
    bool router_processed_imc() const { return router_processed_imc_; }

    /// set flag indicaiting that the router has processed an IMC bundle
    /// mark this bundle as having been processed by the IMC router
    /// (or accounted for by a non-IMC router)
    void set_router_processed_imc() { router_processed_imc_ = true; }


    /**** Following IMC methods are pass throughs to the BundleIMCState object ****/
    /// get the number of IMC destination nodes
    size_t num_imc_dest_nodes() const;

    /// add specified node number to both the original and the working list of IMC destinations
    void add_imc_orig_dest_node(size_t dest_node);

    /// add specified node number to the working list of IMC destinations
    void add_imc_dest_node(size_t dest_node);

    /// mark the specified node as handled (delivered or transmitted)
    void imc_dest_node_handled(size_t dest_node);

    /// get the list of [IPN] destination nodes to receive an IMC bundle
    SPtr_IMC_DESTINATIONS_MAP imc_dest_map() const;

    /// get the list of [IPN] destination nodes for a given link
    SPtr_IMC_DESTINATIONS_MAP imc_dest_map_for_link(std::string& linkname) const;

    /// get the list of [IPN] destination nodes for a given link
    std::string imc_link_name_by_index(size_t index_num);

    /// mark all nodes sent to the specified link as handled
    void imc_link_transmit_success(std::string linkname);

    /// remove the entry for this link to clear the slate for the
    /// next transmit attempt
    void imc_link_transmit_failure(std::string linkname);
  

    /// clear all of the IMC destinations by link lists
    void clear_imc_link_lists();

    SPtr_IMC_PROCESSED_REGIONS_MAP imc_processed_regions_map() const;

    /// Whether or not a specified region has been processed
    bool is_imc_region_processed(size_t region);

    /// Add region num to the list of processed regions
    void add_imc_region_processed(size_t region);

    size_t num_imc_processed_regions() const;

    /// add dest_node to the list of nodes rachable via the specified link
    void add_imc_dest_node_via_link(std::string linkname, size_t dest_node);

    /// copy all dest_nodes to the list of nodes rachable via the specified link
    void copy_all_unhandled_nodes_to_via_link(std::string linkname);

    /// get the number of IMC nodes that have been handled [delivered/transmitted]
    size_t num_imc_nodes_handled() const;

    /// get the number of IMC nodes that have not been handled [delivered/transmitted]
    size_t num_imc_nodes_not_handled() const;

    /// add specified node number to the list of alternate IMC router destinations
    void add_imc_alternate_dest_node(size_t dest_node);

    /// Clear the list of alternate IMC router destinations
    void clear_imc_alternate_dest_nodes();

    /// Clear the list of alternate IMC router destinations
    size_t imc_alternate_dest_nodes_count() const;

    /// get the list of [IPN] alternate destination nodes to receive an IMC bundle
    SPtr_IMC_DESTINATIONS_MAP imc_alternate_dest_map() const;

    /// get the list of [IPN] orignal destination nodes
    SPtr_IMC_DESTINATIONS_MAP imc_orig_dest_map() const;

    /// get the list of unrouteable [IPN] destination nodes
    SPtr_IMC_DESTINATIONS_MAP imc_unrouteable_dest_map() const;

    /// Clear the list of unrouteable destination nodes
    void clear_imc_unrouteable_dest_nodes();

    /// add specified node number to the list of unrouteable IMC router destinations
    void add_imc_unrouteable_dest_node(size_t dest_node, bool in_home_region);

    /// get the number of IMC nodes in the home region that are unrouteable
    size_t num_imc_home_region_unrouteable() const;

    /// get the number of IMC nodes in the home region that are unrouteable
    size_t num_imc_outer_regions_unrouteable() const;


    /// get the list of [IPN] nodes that have processed the proxy request
    SPtr_IMC_PROCESSED_BY_NODE_MAP imc_processed_by_nodes_map() const;

    /// copy the the list of [IPN] nodes from another bundle
    void copy_imc_processed_by_node_list(Bundle* other_bundle);

    /// Add node num to the list of nodes that have processed a proxy petition
    void add_imc_processed_by_node(size_t node_num);

    /// Whether or not a specified region has been processed
    bool imc_has_proxy_been_processed_by_node(size_t node_num);
    
    void set_imc_is_proxy_petition(bool t);
    bool imc_is_proxy_petition() const;

    void set_imc_sync_request(bool t);
    bool imc_sync_request() const;

    void set_imc_sync_reply(bool t);
    bool imc_sync_reply() const;

    void set_imc_is_dtnme_node(bool t);
    bool imc_is_dtnme_node() const;

    void set_imc_is_router_node(bool t);
    bool imc_is_router_node() const;

    void set_imc_briefing(bool t);
    bool imc_briefing() const;

    size_t imc_size_of_sdnv_dest_nodes_array() const;
    size_t imc_size_of_sdnv_processed_regions_array() const;

    ssize_t imc_sdnv_encode_dest_nodes_array(u_char* buf_ptr, size_t buf_len) const;
    ssize_t imc_sdnv_encode_processed_regions_array(u_char* buf_ptr, size_t buf_len) const;

    /// add a redirected link name and the original link nmae mapping
    void add_link_redirect_mapping(const std::string& redirect_link, const std::string& orig_link);

    /// get the original link name associated with a redirected link name.
    /// Returnis true of redirected link name found else false.
    bool get_redirect_orig_link(const std::string& redirect_link, std::string& orig_link) const;

    /// remove the redirect entry for the specifed redirected link name
    void clear_redirect_orig_link(const std::string& redirect_link);

protected:
    /// verify the BundleIMCState exists after attempting to create it if necessary
    bool validate_bundle_imc_state();
    bool validate_bundle_imc_state() const;

    /// generate output of the orignal [as received] IMC Destinations list
    void format_verbose_imc_orig_dest_map(oasys::StringBuffer* buf);

    /// generate output of the working IMC Destinations list
    void format_verbose_imc_dest_map(oasys::StringBuffer* buf);

    /// generate output of the IMC Destinations sublists by Link
    void format_verbose_imc_dest_nodes_per_link(oasys::StringBuffer* buf);

    /// generate output of the IMC Processed Regions
    void format_verbose_imc_state(oasys::StringBuffer* buf);

private:
    /**
     * Initialization helper function.
     */
    void init(bundleid_t id);                   ///< Internal initialization method

private:
    /*
     * Bundle data fields that correspond to data transferred between
     * nodes according to the bundle protocol.
     */
    GbofId gbofid_;                 ///< Unique ID of this bundle maintains the
                                    ///      Source eid, Creation timestamp and 
                                    ///      Fragment Offset/Length of the bundle
    SPtr_EID sptr_dest_;            ///< Destination eid
    SPtr_EID sptr_custodian_;       ///< Current custodian eid
    SPtr_EID sptr_replyto_;         ///< Reply-To eid
    SPtr_EID sptr_prevhop_;         ///< Previous hop eid
    bool is_admin_;                 ///< Administrative record bundle
    bool do_not_fragment_;          ///< Bundle shouldn't be fragmented
    bool custody_requested_;        ///< Bundle Custody requested
    bool singleton_dest_flag_;      ///< Destination endpoint is a singleton as received off the "wire"
    u_int8_t priority_;             ///< Bundle priority
    bool receive_rcpt_;             ///< Hop by hop reception receipt
    bool custody_rcpt_;             ///< Custody xfer reporting
    bool forward_rcpt_;             ///< Hop by hop forwarding reporting
    bool delivery_rcpt_;            ///< End-to-end delivery reporting
    bool deletion_rcpt_;            ///< Bundle deletion reporting
    bool app_acked_rcpt_;           ///< Acknowlege by application reporting
    bool req_time_in_status_rpt_;   ///< request time be included in the status reports
    size_t  expiration_millis_;   ///< Bundle expiration time in millisecs  (BPv7 uses milliseconds; BPv6 uses seconds)
    oasys::Time received_time_;     ///< Time bundle was createed/received for Age Block millisecs calculations

    size_t prev_bundle_age_millis_; ///< Accumulated Bundle Age at time of receipt from BPv7 Bundle Age Block (millisecs)
    bool has_bundle_age_block_;     ///< Indication the bundle has a Bundle Age Block
    size_t orig_length_;            ///< Length of original bundle
    BundlePayload payload_;         ///< Reference to the payload
    
    /*
     * Internal fields and structures for managing the bundle that are
     * not transmitted over the network.
     */
    bundleid_t bundleid_;                   ///< Local bundle identifier
    mutable oasys::SpinLock lock_;          ///< Lock for bundle data that can be
                                            ///  updated by multiple threads
    int32_t bp_version_;                    ///< Bundle Protocol Version
    size_t  primary_block_crc_type_;      ///< Primary Block CRC Type
    bool in_datastore_;                     ///< Is bundle in persistent store
    bool queued_for_datastore_;             ///< Is bundle queued to be put in persistent store
    bool local_custody_;                    ///< Does local node have custody
    bool bibe_custody_;                     ///< Does local node have custody
    ForwardingLog fwdlog_;                  ///< Log of bundle forwarding records
    SPtr_ExpirationTimer expiration_timer_; ///< The expiration timer
    CustodyTimerVec custody_timers_;        ///< Live custody timers for the bundle
    bool fragmented_incoming_;              ///< Is the bundle an incoming reactive
                                            ///  fragment

    SPtr_BlockInfoVec recv_blocks_;         ///< BP blocks as arrived off the wire
    SPtr_BlockInfoVec api_blocks_;          ///< BP blocks given from local API
    LinkBlockSet xmit_blocks_;              ///< Block vector for each link

    MetadataVec     recv_metadata_;         ///< Metadata as arrived in bundle 
    LinkMetadataSet generated_metadata_;    ///< Metadata to be in bundle

    BundleMappings mappings_;               ///< The set of BundleLists that
                                            ///  contain the Bundle.
    
    int  refcount_;                         ///< Bundle reference count
    bool freed_;                            ///< Flag indicating whether a bundle
                                            ///  free event has been posted
    bool deleting_;                         ///< Flag indicating delete from database is queued
    bool manually_deleting_;                ///< Flag indicating bundle is being manually delete (external router)

    bool payload_space_reserved_;           ///< Payload space reserved flag
    bool in_storage_queue_;                 ///< Flag indicating whether bundle update event is  
                                            ///  queued in the storage thread


    size_t highest_rcvd_block_number_;     ///< Highest block number received (BP7 only)
    size_t hop_count_;                     ///< Current number of hops this bundle has travered (BP7 only)
    size_t hop_limit_;                     ///< Max number of hops allowed before deleting bundle (BP7 only)

    // Aggregate Custody Signal parameters
    bundleid_t custodyid_;                   ///< Our Custody ID for the bundle 
    std::string custody_dest_;               ///< BP7 destination EID or "bpv6" for BP6 compatibility
    bool cteb_valid_;                        ///< Flag indicating the bundle contains 
                                             ///  a valid Custody Transfer Extension
                                             ///  Block (CTEB)
    size_t cteb_custodyid_;                ///< Previous custodian's Custody ID
                                             ///  for the bundle

    bool expired_in_link_queue_ = false;     /// Whether the bundle expired before it could be sent

    bundleid_t frag_created_from_bundleid_;  ///< original bundle ID this fragment was created from

#ifdef ECOS_ENABLED
    bool ecos_enabled_;                      ///< Whether the Extended Class of Service (BP6 only) is enabled for this bundle
    uint8_t ecos_flags_;                     ///< Extended Class of Service (BP6 only) flags
    uint8_t ecos_ordinal_;                   ///< Extended Class of Service (BP6 only) ordinal value
    size_t ecos_flowlabel_;                ///< Extended Class of Service (BP6 only) flow label
#endif

#ifdef BARD_ENABLED
    size_t bard_quota_reserved_by_src_ = 0;      ///< num bytes reserved for internal storage by source
    size_t bard_quota_reserved_by_dst_ = 0;      ///< num bytes reserved for internal storage by destination
    size_t bard_extquota_reserved_by_src_ = 0;   ///< num bytes reserved for external storage by source
    size_t bard_extquota_reserved_by_dst_ = 0;   ///< num bytes reserved for external storage by destination
    size_t bard_in_use_by_src_ = 0;              ///< num bytes tracked as in_use in internal storage by source
    size_t bard_in_use_by_dst_ = 0;              ///< num bytes tracked as in_use in internal storage by destination
    bool bard_restage_by_src_ = false;           ///< whether restaging begin done by src or dst
    std::string bard_restage_link_name_;         ///< restage CL link name to use
    bool last_restage_attempt_failed_ = false;   ///< whether the last restage attempt failed
    bool all_restage_attempts_failed_ = false;   ///< whether all attempts to restage the bundle have failed

    typedef std::map<std::string,bool> RESTAGE_LINK_NAME_MAP;
    typedef std::unique_ptr<RESTAGE_LINK_NAME_MAP> QPtr_RestageLinkNameMap;
    QPtr_RestageLinkNameMap qptr_failed_restage_link_names_;
#endif //BARD_ENABLED

    /// Flag indicating that the router has processed the IMC bundle to prevent early deletion after a local delivery
    bool router_processed_imc_ = false;

    QPtr_BundleIMCState qptr_imc_state_;    ///< Pointer to BundleIMCState object if instantiated

    /// Mapping to track which link redirected a bundle to a given alternate link/conversion layer.
    /// Typical use is a redirection to a Bundle In Bundle Encapsulation (BIBE) CL to allow a
    /// BPv7 bundle to pass through a BPv6 node. The BIBE CL needs to be able to serialize IMC 
    /// bundles based on the original Link Name to generate the correct IMC State Block. This map
    QPtr_LinkRedirectMap qptr_link_redirect_map_;
};


} // namespace dtn

#endif /* _BUNDLE_H_ */
