/*
 *    Copyright 2021 United States Government as represented by NASA
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

#ifndef _BUNDLE_RESTAGING_DAEMON_H_
#define _BUNDLE_RESTAGING_DAEMON_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BARD_ENABLED

#include <list>

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

#include "BARDNodeStorageUsage.h"
#include "BARDRestageCLIF.h"
#include "Bundle.h"


namespace dtn {


class BundleArchitecturalRestagingDaemon;

/**
 * Class that maintains and tracks quota and usage information
 * for enforcing quotas on a source or destination basis
 */
class BundleArchitecturalRestagingDaemon : public oasys::Logger,
                              public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleArchitecturalRestagingDaemon();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleArchitecturalRestagingDaemon();

    /**
     * Shutdown initiation
     */
    virtual void shutdown();

    /**
     * Return the number of events currently waiting for processing.
     */
    virtual size_t event_queue_size()
    {
    	return eventq_->size();
    }

   /**
     * Virtual post_event function
     *
     * @param event String command to be processed
     * @param at_back Whether to post this event at the back or in the front of the queue
     */
    virtual void post_event(std::string* event, bool at_back = true);

    /**
     * General daemon parameters
     */
    struct Params {
        /// Default constructor
        Params();
        
        /// Whether or not Aggfregate Custody Signals are enabled
        bool enabled_;
    };

    static Params params_;
    
    /**
     * Query to the Bundle Restaging Daemon whether it is okay to accept the given bundle
     *
     * @param bref Reference to the bundle object in question
     */
    bool query_accept_bundle(Bundle* bundle);

    /**
     * Query to the Bundle Restaging Daemon whether it is okay to reload a bundle
     *
     * @param quota_type
     * @param naming_scheme
     * @param nodename
     * @param node_number
     */
    bool query_accept_reload_bundle(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme,
                                    std::string& nodename, size_t payload_len);

    /**
     * Bundles has been accepted into internal storage so any reserved values need to be 
     * reverses and the inuse values incremented for both the source and destination EIDs
     *
     * @param bundle Pointer to the bundle object
     */
    void bundle_accepted(Bundle* bundle);

    /**
     * Bundles has been been written to exernal storage so need to move the reserved
     * values to the inuse values
     *
     * @param bundle Ponter to the bundle object
     */
    void bundle_restaged(Bundle* bundle, size_t disk_usage);

    /**
     * Reverse out inuse and reserved values after a bundle is deleted
     *
     * @param bref Reference to the bundle object in question
     */
    void bundle_deleted(Bundle* bundle);

    /**
     * Reverse out inuse exernal storage values after a restaged bundle is deleted
     *
     * @param key Key/Directory Name the bundle was restaged under
     * @param disk_usage Amount of disk usage the bundle occupied
     */
    void restaged_bundle_deleted(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, 
                                 std::string& nodename, size_t disk_usage);

    /**
     * Method used by the QuotaCommand to add or update a quota entry
     *
     * @param quota_type Indicates whether the key if for a Destination (0) or Source (1) node
     * @param naming_shceme Indicates whether the key is for an Endpoing ID of type IPN (1), DTN (2) or IMC (3)
     * @param node_number The DTN node name or IPN/IMC node number as text
     * @param internal_bundles Max number of bundles allowed in internal storage (0 = unlimited)
     * @param internal_bytes Max number of payload bytes allowed in internal storage (0 = unlimited)
     * @param refuse_bundle Whether to refuse the bundle if it would exceed an internal quota
     * @param restage_link_name Name of a refuse convergene layer instance to use for offloading bundles to external storage
     * @param auto_reload Whether to automatically reload bundles from external storage when the internal storage drops below 20% of quota
     * @param external_bundles Max number of bundles allowed in external storage (0 = unlimited)
     * @param external_bytes Max number of paylaod bytes allowed in external storage (0 = unlimited)
     */
    bool bardcmd_add_quota(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename,
                          size_t internal_bundles, size_t internal_bytes, bool refuse_bundle, 
                          std::string& restage_link_name, bool auto_reload, 
                          size_t external_bundles, size_t external_bytes,
                          std::string& warning_msg);

    /**
     * Method used by the QuotaCommand to delete a quota entry
     *
     * @param quota_type Indicates whether the key if for a Destination (0) or Source (1) node
     * @param naming_shceme Indicates whether the key is for an Endpoing ID of type IPN (1), DTN (2) or IMC (3)
     * @param node_number The DTN node name or IPN/IMC node number as text
     */
    bool bardcmd_delete_quota(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename);

    /**
     * Method used by the QuotaCommand to set a quota entry to unlimited
     *
     * @param quota_type Indicates whether the key if for a Destination (0) or Source (1) node
     * @param naming_shceme Indicates whether the key is for an Endpoing ID of type IPN (1), DTN (2) or IMC (3)
     * @param node_number The DTN node name or IPN/IMC node number as text
     */
    bool bardcmd_unlimited_quota(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename);

    /**
     * Method used by the QuotaCommand to initiate restaging of over-quota bundles
     *
     * @param quota_type Indicates whether the key if for a Destination (0) or Source (1) node
     * @param naming_shceme Indicates whether the key is for an Endpoing ID of type IPN (1), DTN (2) or IMC (3)
     * @param node_number The DTN node name or IPN/IMC node number as text
     * @return false if quota not configured for restaging else true
     */
    bool bardcmd_force_restage(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename);


    void bardcmd_quotas(oasys::StringBuffer& buf);
    void bardcmd_quotas_exact(oasys::StringBuffer& buf);

    void bardcmd_usage(oasys::StringBuffer& buf);
    void bardcmd_usage_exact(oasys::StringBuffer& buf);
    
    void bardcmd_dump(oasys::StringBuffer& buf);

    bool bardcmd_rescan(std::string& result_str);

    void report_all_restagecl_status(oasys::StringBuffer& buf);

    bool bardcmd_reload(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, std::string& nodename, 
                       size_t new_expiration, std::string& new_dest_eid, std::string& result_str);

    bool bardcmd_reload_all(size_t new_expiration, std::string& result_str);

    bool bardcmd_del_restaged_bundles(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, 
                                                   std::string& nodename, std::string& result_str);

    bool bardcmd_del_all_restaged_bundles(std::string& result_str);



    /**
     * Register a Restage Convergence Layer instance
     */
    void register_restage_cl(SPtr_RestageCLStatus sptr_clstatus);
    void unregister_restage_cl(std::string& link_name);


    void update_restage_usage_stats(bard_quota_type_t quota_type, 
                                    bard_quota_naming_schemes_t naming_scheme,
                                    std::string& nodename, 
                                    size_t num_files, size_t disk_usage);


    /**
     * genearate report for external router
     */
    void generate_usage_report_for_extrtr(BARDNodeStorageUsageMap& bard_usage_map,
                                          RestageCLMap& restage_cl_map);

    /**
     * Called by each Restage CL when it completes its rescan
     * @param spctr_clstats The Restage CL reporting it has completed the rescan
     */
    void rescan_completed();

protected:
    /**
     * Main thread function that dispatches events.
     */
    void run();

    /**
     * Internal Usage Tracking methods
     */
    bool query_accept_bundle_by_destination(Bundle* bundle);
    bool query_accept_bundle_by_source(Bundle* bundle);
    bool query_accept_bundle(std::string& key, bool restage_by_src, Bundle* bundle);

    void bundle_accepted_by_destination(Bundle* bundle);
    void bundle_accepted_by_source(Bundle* bundle);
    void bundle_accepted(std::string& key, bool key_is_by_src, Bundle* bundle);

    void bundle_restaged_by_destination(Bundle* bundle, size_t disk_usage);
    void bundle_restaged_by_source(Bundle* bundle, size_t disk_usage);
    void bundle_restaged(std::string& key, bool key_is_by_src, Bundle* bundle, size_t disk_usage);

    void bundle_deleted_by_destination(Bundle* bundle);
    void bundle_deleted_by_source(Bundle* bundle);
    void bundle_deleted(std::string& key, bool key_is_by_src, Bundle* bundle);

    bool find_restage_link_in_good_state(std::string& link_name);
    const char* get_restage_link_state_str(std::string link_name);

    /**
     * methods to update the persistent datastore
     */
    void load_saved_quotas();
    void update_datastore(BARDNodeStorageUsage* usage_ptr);
    void delete_from_datastore(BARDNodeStorageUsage* usage_ptr);

    // format quota usage values for reports
    std::string format_internal_quota_usage(BARDNodeStorageUsage* usage_ptr);
    std::string format_external_quota_usage(BARDNodeStorageUsage* usage_ptr);

    size_t max_inuse_quota_percent(BARDNodeStorageUsage* usage_ptr);
    size_t max_committed_quota_percent(BARDNodeStorageUsage* usage_ptr);

    /**
     * Utility function to generate a BARDNodeStorageUsage key string given an EndpointID
     * (not part of the BARDNodeStorageUsage to minimize the external router client side dependencies)
     *
     * @param quota_type Indicates whether the key if for a Destination or Source Endpoint ID
     * @param eid The EndpointID associrated with the node; The scheme and name/node number are extracted from the eid
     */
    std::string make_bardnodestorageusage_key(bard_quota_type_t quota_type, const EndpointID& eid);

    /**
     * Utility function to translate an endpoint into the BARDNodeStorageUsage scheme and nodename components
     * (not part of the BARDNodeStorageUsage to minimize the external router client side dependencies)
     *
     * @param eid The EndpointID to parse
     * @param scheme The scheme type of the EndpointID
     * @param nodename The nodename of the EndpointID
     */
    void endpoint_to_key_components(const EndpointID& eid, bard_quota_naming_schemes_t& scheme, 
                                    std::string& nodename);

    void extrtr_usage_rpt_load_usage_map(BARDNodeStorageUsageMap& bard_usage_map);
    void extrtr_usage_rpt_load_restage_cl_map(RestageCLMap& restage_cl_map);

protected:
    /// for serializing access
    oasys::SpinLock lock_;

    /// Whether the BARD has been started; 
    /// allows configuration file quotas to be overridden by those in the data store
    bool bard_started_ = false;

    /// List of only defined quota records
    BARDNodeStorageUsageMap quota_map_;

    /// List of all usage records (includes those with defined quota recs)
    BARDNodeStorageUsageMap usage_map_;


    RestageCLMap restagecl_map_;  ///< map of all of the Restage CL instances that register


    /// The event queue
    oasys::MsgQueue<std::string*>* eventq_;

    std::string last_not_found_error_msg_link_name_;   ///< Tracks name of link output in last "not found" error message
    time_t      last_not_found_error_msg__time_ = 0;   ///< Tracks time of last "not found" error message to prevent flodding the log file

    size_t total_deleted_restaged_ = 0;     ///< Count of the number of restaged bundles that have been deleted (reloaded or expired)
    size_t total_restaged_ = 0;             ///< Count of the number of bundles that have been restaged


    bool rescanning_ = false;               ///< Whether a user initiated rescan is in progress
    size_t expected_rescan_responses_ = 0;  ///< Number of Restage CLs expected to call rescan_completed()
    size_t rescan_responses_ = 0;           ///< Number of Restage CLs that have called rescan_completed()
    time_t rescan_time_initiated_ = 0;      ///< Time when a rescan was initiated
};

} // namespace dtn


#endif  // BARD_ENABLED

#endif /* _BUNDLE_DAEMON_H_ */
