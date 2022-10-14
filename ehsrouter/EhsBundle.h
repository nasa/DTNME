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

#ifndef _EHS_BUNDLE_H_
#define _EHS_BUNDLE_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#include <map>
#include <string.h>

#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "EhsBundleRef.h"
#include "routing/ExternalRouterIF.h"



namespace dtn {


// Bundle Priority (Class of Service)
#define COS_BULK       0
#define COS_NORMAL     1
#define COS_EXPEDITED  2
#define COS_RESERVED   3

#define MAX_ECOS_PRIORITY  255


class EhsDtnNode;


// NOTE - std::map definitions for EHS Bundles are in EhsBundleRef.h


class EhsBundle
{
public:
    /**
     * Constructor for the different message types
     */
    EhsBundle(EhsDtnNode* parent, ExternalRouterIF::extrtr_bundle_ptr_t& bundleptr);

    /**
     * Destructor.
     */
    virtual ~EhsBundle() {};

    /**
     * String key to sort bundles by priority (for Link transmission)
     */
    virtual std::string priority_key();

    /**
     * Message processing methods
     */
    virtual void process_bundle_report(ExternalRouterIF::extrtr_bundle_ptr_t& bundleptr);

    virtual void process_bundle_custody_accepted_event(uint64_t custody_id);

    /**
     * Set/Get flag indicating an exit is in progress 
     * - no need to issue a free bundle request
     */
    virtual void set_exiting();
    virtual bool exiting();

    /**
     * Set/Get flag indicating bundle has been deleted at the DTNME node
     */
    virtual void set_deleted();
    virtual bool deleted();

    /**
     * Return the bundle's reference count, corresponding to the
     * number of entries in the mappings set, i.e. the number of
     * BundleLists that have a reference to this bundle, as well as
     * any other scopes that are processing the bundle.
     */
    virtual int refcount();

    /**
     * Bump up the reference count. Parameters are used for logging.
     *
     * @return the new reference count
     */
    virtual int add_ref(const char* what1, const char* what2 = "");

    /**
     * Decrement the reference count. Parameters are used for logging.
     *
     * If the reference count becomes zero, the bundle is deleted.
     *
     * @return the new reference count
     */
    virtual int del_ref(const char* what1, const char* what2 = "");

    /**
     * Clear all custody indicators
     */
    virtual void release_custody();

    /**
     * Check to see if the bundle has expired
     */
    virtual bool expired();

    /**
     * Get the current time (seconds) with the DTN offset applied
     */
    virtual u_int32_t get_current_time();


    /**
     * Report/Status methods
     */
    virtual void bundle_list(oasys::StringBuffer* buf);
    virtual void format_verbose(oasys::StringBuffer* buf);

    /**
     * Getters
     */
    virtual uint64_t bundleid()            { return bundleid_; }
    virtual uint64_t bp_version()          { return bp_version_; }
    virtual uint64_t custodyid()           { return custodyid_; }
    virtual std::string source()           { return source_; }
    virtual std::string dest()             { return dest_; }
//    virtual std::string replyto()          { return replyto_; }
//    virtual std::string csutodian()        { return custodian_; }
    virtual std::string gbofid_str()       { return gbofid_str_; }
    virtual std::string prev_hop()         { return prev_hop_; }
    virtual bool is_ipn_dest()             { return is_ipn_dest_; }
    virtual bool is_ipn_source()           { return is_ipn_source_; }
    virtual uint64_t ipn_dest_node()       { return ipn_dest_node_; }
    virtual uint64_t ipn_source_node()     { return ipn_source_node_; }
    virtual bool local_custody()           { return local_custody_; }
    virtual int length()                   { return length_; }
    virtual uint64_t expiration()          { return expiration_; }
    virtual std::string custody_str()      { return local_custody_ ? "true" : "false"; }
    virtual bool custody_requested()       { return custody_requested_; }
    virtual std::string received_from_link_id()     { return received_from_link_id_; }

    virtual bool is_cos_bulk()             { return priority_ == COS_BULK; }
    virtual bool is_cos_normal()           { return priority_ == COS_NORMAL; }
    virtual bool is_cos_expedited()        { return priority_ == COS_EXPEDITED; }
    virtual bool is_cos_reserved()         { return priority_ == COS_RESERVED; }

    virtual bool is_ecos_critical()        { return (0 != (ecos_flags_ & 0x01)); }
    virtual bool is_ecos_streaming()       { return (0 != (ecos_flags_ & 0x02)); }
    virtual bool is_ecos_has_flow_label()  { return (0 != (ecos_flags_ & 0x04)); }
    virtual bool is_ecos_reliable()        { return (0 != (ecos_flags_ & 0x08)); }

    virtual bool is_fwd_link_destination() { return is_fwd_link_destination_; }
    virtual bool not_in_resync_report() { return not_in_resync_report_; }


    /**
     * Setters
     */
    virtual void set_is_fwd_link_destination(bool t) { is_fwd_link_destination_ = t; }

    /// Set the not_in_report flag to faciliate syncing the bundle list with the DTNME server
    virtual void prepare_for_resync()         { not_in_resync_report_ = true; }


protected:
    /**
     * Initialization routine
     */
    virtual void init();

    /**
     * Log message method for internal use - passes log info to parent
     */
    virtual void log_msg(oasys::log_level_t level, const char*format, ...);

protected:
    /// Lock for sequential access
    oasys::SpinLock lock_;

    /// Parent EhsDtnNode
    EhsDtnNode* parent_;

    /// Bundle ID
    uint64_t bundleid_;

    /// Bundle Protocol Version
    uint64_t bp_version_;

    /// Custody ID
    uint64_t custodyid_;

    std::string source_;

    std::string dest_;

//    std::string custodian_;

//    std::string replyto_;

    std::string gbofid_str_;

    std::string prev_hop_;

    size_t length_;

    std::string location_;

    std::string payload_file_;

//    bool is_fragment_;

//    bool is_admin_;

    bool do_not_fragment_;

    /// Bundle Priority (Class of Service)
    int priority_;

    /// Extended Class of Service values
    uint8_t ecos_flags_;
    uint8_t ecos_ordinal_;
    uint64_t ecos_flowlabel_;

    bool custody_requested_;

    bool local_custody_;

    bool singleton_dest_;

    bool custody_rcpt_;

    bool receive_rcpt_;

    bool forward_rcpt_;

    bool delivery_rcpt_;

    bool deletion_rcpt_;

    bool app_acked_rcpt_;

    uint64_t creation_ts_seconds_;

    uint64_t creation_ts_seqno_;

    uint64_t expiration_;

    int orig_length_;

    int frag_offset_;

    int frag_length_;

    uint64_t local_id_;

    std::string owner_;

    std::string received_from_link_id_;

    // calculated values
    bool     is_ipn_source_;
    bool     is_ipn_dest_;
    uint64_t ipn_source_node_;
    uint64_t ipn_dest_node_;
    bool is_fwd_link_destination_;

    /// Priority key string value
    std::string priority_key_;

    /// Indication that the priority key has been set
    bool priority_key_set_;

    /// Tracks number of references to the bndle
    int refcount_;

    /// Indication program is exiting so no need to
    /// issue free bundle request
    bool exiting_;

    /// Indication if already deleted at DTNME node
    bool deleted_;

    /// Flag used to resync EhsExtRouter with the DTNME server. Bundles are initially flagged 
    /// as not in the bundle report and then changed while processing the received report. After 
    /// report has been processed all bundles flagged as not in the report can be deleted.
    bool not_in_resync_report_ = false;
};

} // namespace dtn

#endif /* _EHS_BUNDLE_H_ */
