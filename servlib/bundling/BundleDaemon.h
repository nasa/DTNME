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

#ifndef _BUNDLE_DAEMON_H_
#define _BUNDLE_DAEMON_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <memory>
#include <vector>

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/tclcmd/IdleTclExit.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/thread/SpinLock.h>

#include "BundleDaemonInput.h"
#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleListIntMap.h"
#include "BundleListStrMap.h"
#include "BundleListStrMultiMap.h"
#include "BundleProtocol.h"
#include "BundleActions.h"
#include "BundleStatusReport.h"

namespace dtn {

class AdminRegistration;
class AdminRegistrationIpn;
class BIBEExtractor;
class Bundle;
class BundleAction;
class BundleActions;
class BundleDaemonACS;
class BundleDaemonInput;
class BundleDaemonOutput;
class BundleDaemonStorage;
class BundleDaemonCleanup;
class BundleRouter;
class ContactManager;
class FragmentManager;
class PingRegistration;
class IpnEchoRegistration;
class RegistrationTable;
class RegistrationInitialLoadThread;


class LTPEngine;
typedef std::shared_ptr<LTPEngine> SPtr_LTPEngine;

// XXX/dz Set the typedefs to the type of list you want to use for each of the 
// bundle lists and enable the #define for those that are maps 
typedef BundleListIntMap       all_bundles_t;
typedef BundleListIntMap       pending_bundles_t;
typedef BundleListStrMap       custody_bundles_t;
typedef BundleListStrMultiMap  dupefinder_bundles_t;

// XXX/dz Looking at the current usage of the custody_bundles, it could be
// combined with the dupefinder list with the addition a method which 
// returns the one bundle which is in local custody for a key and a separate
// counter which tracks the number of bundles in custody.

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class BundleDaemon : public oasys::Singleton<BundleDaemon, false>,
                     public BundleEventHandler,
                     public oasys::Thread
{
public:
    typedef BundleProtocol::status_report_flag_t status_report_flag_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;

    /**
     * Constructor.
     */
    BundleDaemon();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemon();

    /**
     * Release the allocated objects
     */
    void cleanup_allocations();

    /**
     * Set the console parameters to be passed to the BundleDaemon for use by the ExternalRouter
     */
    void set_console_info(in_addr_t console_addr, u_int16_t console_port, bool console_stdio);

    /**
     * Virtual initialization function, overridden in the simulator to
     * install the modified event queue (with no notifier) and the
     * SimBundleActions class.
     */
    virtual void do_init();

    /**
     * Boot time initializer.
     */
    static void init();
    
    virtual void inc_ltp_retran_timers_created();
    virtual void inc_ltp_retran_timers_deleted();


    virtual void inc_ltp_inactivity_timers_created();
    virtual void inc_ltp_inactivity_timers_deleted();

    virtual void inc_ltp_closeout_timers_created();
    virtual void inc_ltp_closeout_timers_deleted();


    virtual void inc_ltpseg_created();
    virtual void inc_ltpseg_deleted();

    virtual void inc_ltp_ds_created();
    virtual void inc_ltp_ds_deleted();

    virtual void inc_ltp_rs_created();
    virtual void inc_ltp_rs_deleted();

    virtual void inc_ltpsession_created();
    virtual void inc_ltpsession_deleted();

    virtual void inc_bibe6_encapsulations();
    virtual void inc_bibe6_extractions();
    virtual void inc_bibe6_extraction_errors();

    virtual void inc_bibe7_encapsulations();
    virtual void inc_bibe7_extractions();
    virtual void inc_bibe7_extraction_errors();


    /**
     * Return the number of events currently waiting for processing.
     * This is overridden in the simulator since it doesn't use a
     * MsgQueue.
     */
    virtual size_t event_queue_size()
    {
    	return eventq_->size();
    }

    /**
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */
    static void post(BundleEvent* event);
 
    /**
     * Queues the event at the head of the queue for processing by the
     * daemon thread.
     */
    static void post_at_head(BundleEvent* event);
    
    /**
     * Post the given event and wait for it to be processed by the
     * daemon thread or for the given timeout to elapse.
     */
    static bool post_and_wait(BundleEvent* event,
                              oasys::Notifier* notifier,
                              int timeout = -1, bool at_back = true);
    
   /**
    * Virtual post_event function, overridden by the Node class in
    * the simulator to use a modified event queue.
    */
    virtual void post_event(BundleEvent* event, bool at_back = true);

    /**
     * Returns the current bundle router.
     */
    BundleRouter* router()
    {
        ASSERT(router_ != NULL);
        return router_;
    }

    /**
     * Return the current actions handler.
     */
    BundleActions* actions() { return actions_; }

    /**
     * Accessor for the contact manager.
     */
    ContactManager* contactmgr() { return contactmgr_; }

    /**
     * Accessor for the fragmentation manager.
     */
    FragmentManager* fragmentmgr() { return fragmentmgr_; }

    /**
     * Accessor for the registration table.
     */
    const RegistrationTable* reg_table() { return reg_table_; }

    /**
     * Accessor for the all bundles list.
     */
    all_bundles_t* all_bundles() { return all_bundles_; }
    all_bundles_t* deleting_bundles() { return deleting_bundles_; }
    
    /**
     * Accessor for the pending bundles list.
     */
    pending_bundles_t* pending_bundles() { return pending_bundles_; }
    
    /**
     * Accessor for the custody bundles list.
     */
    custody_bundles_t* custody_bundles() { return custody_bundles_; }
    
    /**
     * Accessor for the dupefinder bundles list.
     */
    dupefinder_bundles_t* dupefinder_bundles() { return dupefinder_bundles_; }

    /**
     * Format the given StringBuffer with current routing info.
     */
    void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current bundle
     * statistics.
     */
    void get_bundle_stats(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current internal
     * statistics value.
     */
    void get_daemon_stats(oasys::StringBuffer* buf);
    size_t get_received_bundles() { return daemon_input_->get_received_bundles(); }
    void get_ltp_object_stats(oasys::StringBuffer* buf);

    /**
     * Reset all internal stats.
     */
    void reset_stats();

    /**
     * Return the local endpoint identifier.
     */
    const EndpointID& local_eid() { return local_eid_; }

    /**
     * Return the local IPN endpoint identifier.
     */
    const EndpointID& local_eid_ipn() { return local_eid_ipn_; }

    /**
     * Set the local endpoint id.
     */
    void set_local_eid(const char* eid_str);

    /**
     * Set the local IPN endpoint id.
     */
    void set_local_eid_ipn(const char* eid_str);

    /**
     * General daemon parameters
     */
    struct Params {
        /// Default constructor
        Params() {}
        

        /// Whether or not to delete bundles before they're expired if
        /// all routers / registrations have handled it
        bool early_deletion_ = true;

        /// Whether or not to skip routing decisions for and delete duplicate
        /// bundles
        bool suppress_duplicates_ = true;

        /// Whether or not to accept custody when requested
        bool accept_custody_ = true;

        /// Whether or not reactive fragmentation is enabled
        bool reactive_frag_enabled_ = false;
        
        /// Whether or not to retry unacked transmissions on reliable CLs.
        bool retry_reliable_unacked_ = true;

        /// Test hook to permute bundles before delivering to registrations
        bool test_permuted_delivery_ = false;

        /// Whether or not injected bundles are held in memory by default
        bool injected_bundles_in_memory_ = false;

        /// Whether non-opportunistic links should be recreated when the
        /// DTN daemon is restarted.
        bool recreate_links_on_restart_ = true;

        /// Whether or not links should be maintained in the database
        /// (if not then bundle queue-ing will be redone from scratch on reload)
        bool persistent_links_ = false;

        /// Whether or not to write updated forwarding logs to storage
        bool persistent_fwd_logs_ = false;

        /// Whether to clear bundles from opportunistic links when they go unavailable
        bool clear_bundles_when_opp_link_unavailable_ = true;

        /// Set the announce_ipn bool.
        bool announce_ipn_ = true;

        /// Whether to write API Registration bundle lists to the database
        /// XXX/dz defaulting to false but original DTNME reference implementation would be true
        bool serialize_apireg_bundle_lists_ = false;

        /// ipn_echo_service_number_
        u_int64_t ipn_echo_service_number_ = 2047;

        /// ipn_echo_max_return_length_
        u_int64_t ipn_echo_max_return_length_ = 1024;

        /// which BP version the API should generate/send
        bool api_send_bp_version7_ = true;

        /// allow specification of the local LTP Engine ID (otherwise pull from local IPN EID)
        uint64_t ltp_engine_id_ = 0;

        /// specify group and world access to directories and files (owner will always have full access)
        uint16_t file_permissions_ = (0775 & 0xff);

        ///< IP address the TCL command server console is listening on
        in_addr_t console_addr_ = INADDR_ANY;

        ///< Port the TCL command server console is listening on
        u_int16_t console_port_ = 0;

        ///< Whether an interactive TCL console is running
        bool console_stdio_ = true;

        //dzdebug
        bool dzdebug_reg_delivery_cache_enabled_ = true;
    };

    static Params params_;

    /**
     * Typedef for a shutdown procedure.
     */
    typedef void (*ShutdownProc) (void* args);
    
    /**
     * Set an application-specific shutdown handler.
     */
    void set_app_shutdown(ShutdownProc proc, void* data)
    {
        app_shutdown_proc_ = proc;
        app_shutdown_data_ = data;
    }

    /**
     * Set a router-specific shutdown handler.
     */
    void set_rtr_shutdown(ShutdownProc proc, void* data)
    {
        rtr_shutdown_proc_ = proc;
        rtr_shutdown_data_ = data;
    }

    /**
     * Accessor for the BundleDaemon's shutdown status
     */
    static bool shutting_down()
    {
     	return shutting_down_;
    }

    /**
     * Initialize an idle shutdown handler that will cleanly exit the
     * tcl event loop whenever no bundle events have been handled
     * for the specified interval.
     */
    void init_idle_shutdown(int interval);
    
    /**
     * Take custody for the given bundle, sending the appropriate
     * signal to the current custodian.
     */
    void accept_custody(Bundle* bundle);
    void accept_bibe_custody(Bundle* bundle, std::string bibe_custody_dest);


    /**
     * Check the registration table and optionally deliver the bundle
     * to any that match.
     *
     * @return whether or not any matching registrations were found or
     * if the bundle is destined for the local node
     */
    bool check_local_delivery(Bundle* bundle, bool deliver);

    /**
     * This is used for delivering bundle to app by Late Binding
     */
    void check_and_deliver_to_registrations(Bundle* bundle, const EndpointID&);

    /**
     * Main event handling function.
     */
    void handle_event(BundleEvent* event);
    void handle_event(BundleEvent* event, bool closeTransaction);

    /**
     * Load in the previous links data.  This information is used to ensure
     * consistency between links created in this session and links created in
     * previous sessions, especially for opportunistic links. Note that this data
     * is not used to initialize any links but is used to check the consistency
     * of links created by the configuration file or subsequently.  In order to cope
     * with links created in the configuration file this function may be called from
     * the LinkCommand before its normal position at the start of run.  The flag
     * is used to ensure that the body of the code reading the data store is executed
     * exactly once.
     */
    void load_previous_links();
    bool load_previous_links_executed_;


    /**
     * pass through methods to other BundleDaemonXXXs
     */
    void add_bibe_bundle_to_acs(int32_t bp_version, const EndpointID& custody_eid,
                                uint64_t transmission_id, bool success, uint8_t reason);
    void bibe_erase_from_custodyid_list(Bundle* bundle);
    void bibe_accept_custody(Bundle* bundle);
    void switch_bp6_to_bibe_custody(Bundle* bundle, std::string& bibe_custody_dest);
    void bundle_delete_from_storage(Bundle* bundle);
    void bundle_add_update_in_storage(Bundle* bundle);
    void acs_accept_custody(Bundle* bundle);
    void acs_post_at_head(BundleEvent* event);
    void reloaded_pendingacs();
    void set_route_acs_params(EndpointIDPattern& pat, bool enabled, 
                              u_int acs_delay, u_int acs_size);
    int delete_route_acs_params(EndpointIDPattern& pat);
    void dump_acs_params(oasys::StringBuffer* buf);
    void storage_get_stats(oasys::StringBuffer* buf);
    void registration_add_update_in_storage(APIRegistration* reg);

    void bibe_extractor_post(Bundle* bundle, Registration* reg);

    /**
     * Get pointer to the LTP Engine
     */
    SPtr_LTPEngine ltp_engine() { return ltp_engine_; }

    /**
     * Query to see if the incoming bundle can be accepted based on the overall payload quota
     *
     * Some convergence layers previously reserved payload quota space based on the incoming 
     * length of bundle data which is larger than the actual payload length. If so then the
     * amount_previously_reserved_space must be passed in so that the bundle can be officially
     * added to the quota usage and the reserved amount release (usually larger than the 
     * actual payloas length)
     * 
     * @param [in-out] bundle  The incoming bundle object
     * @param [out] payload_space_reserved Whether space was resrerved in the overall payoad quota
     * @param [in-out] amount_previously_reserved_space Amount of previously reserved payload quota
     *
     * @return Whether the bundle can be accepted or not. If not then the caller can check the
     *         payload_space_reserved variable to determine if the bundle was rejected due to the
     *         overall payload quota (or possibly other quotas in a future release).
     */
    bool query_accept_bundle_based_on_quotas(Bundle* bundle, bool& payload_space_reserved, 
                                             uint64_t& amount_previously_reserved_space);

    /**
     * Releases reserved payload space for those bundles that are not yet managed with a 
     * BundleRef and are about to be deleted because they were rejected.
     *
     * @param bundle  The Bundle object about to be deleted
     */
    void release_bundle_without_bref_reserved_space(Bundle* bundle);


    /**
     * Locally generate a status report for the given bundle.
     */
    void generate_status_report(Bundle* bundle,
                                BundleStatusReport::flag_t flag,
                                status_report_reason_t reason =
                                BundleProtocol::REASON_NO_ADDTL_INFO);

protected:
    friend class BundleActions;
    friend class BundleDaemonACS;
    friend class BundleDaemonInput;
    friend class BundleDaemonCleanup;
    friend class RegistrationInitialLoadThread;


    /**
     * Initialize and load in the registrations.
     */
    void load_registrations();
        
    /**
     * Generate delivery events for newly-loaded bundles.
     */
    void generate_delivery_events(Bundle* bundle);

    /**
     * Initialize and load in stored bundles.
     */
    bool load_bundles();
        
    /**
     * Initialize and load in stored Pending ACSs .
     */
    void load_pendingacs();

    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
    void handle_bundle_delivered(BundleDeliveredEvent* event);
    void handle_bundle_acknowledged_by_app(BundleAckEvent* event);
    void handle_bundle_expired(BundleExpiredEvent* event);
    void handle_bundle_free(BundleFreeEvent* event);
    void handle_bundle_cancel(BundleCancelRequest* event);
    void handle_bundle_cancelled(BundleSendCancelledEvent* event);
    void handle_bundle_inject(BundleInjectRequest* event);
    void handle_bundle_delete(BundleDeleteRequest* request);
    void handle_bundle_accept(BundleAcceptRequest* event);
    void handle_bundle_take_custody(BundleTakeCustodyRequest* request);
    void handle_bundle_query(BundleQueryRequest* event);
    void handle_bundle_report(BundleReportEvent* event);
    void handle_bundle_attributes_query(BundleAttributesQueryRequest* request);
    void handle_bundle_attributes_report(BundleAttributesReportEvent* event);
    void handle_registration_added(RegistrationAddedEvent* event);
    void handle_registration_removed(RegistrationRemovedEvent* event);
    void handle_registration_expired(RegistrationExpiredEvent* event);
    void handle_registration_delete(RegistrationDeleteRequest* request);
    void handle_contact_up(ContactUpEvent* event);
    void handle_contact_down(ContactDownEvent* event);
    void handle_contact_query(ContactQueryRequest* event);
    void handle_contact_report(ContactReportEvent* event);
    void handle_link_created(LinkCreatedEvent* event);
    void handle_link_deleted(LinkDeletedEvent* event);
    void handle_link_available(LinkAvailableEvent* event);    
    void handle_link_unavailable(LinkUnavailableEvent* event);
    void handle_link_check_deferred(LinkCheckDeferredEvent* event);
    void handle_link_state_change_request(LinkStateChangeRequest* request);
    void handle_link_create(LinkCreateRequest* event);
    void handle_link_delete(LinkDeleteRequest* request);
    void handle_link_reconfigure(LinkReconfigureRequest* request);
    void handle_link_query(LinkQueryRequest* event);
    void handle_link_report(LinkReportEvent* event);
    void handle_reassembly_completed(ReassemblyCompletedEvent* event);
    void handle_route_add(RouteAddEvent* event);
    void handle_route_recompute(RouteRecomputeEvent* event);
    void handle_route_del(RouteDelEvent* event);
    void handle_route_query(RouteQueryRequest* event);
    void handle_route_report(RouteReportEvent* event);
    void handle_custody_signal(CustodySignalEvent* event);
    void handle_custody_timeout(CustodyTimeoutEvent* event);
    void handle_shutdown_request(ShutdownRequest* event);
    void handle_status_request(StatusRequest* event);
    void handle_cla_set_params(CLASetParamsRequest* request);
    void handle_deliver_bundle_to_reg(DeliverBundleToRegEvent* event);
    void handle_bundle_queued_query(BundleQueuedQueryRequest* request);
    void handle_bundle_queued_report(BundleQueuedReportEvent* event);
    void handle_eid_reachable_query(EIDReachableQueryRequest* request);
    void handle_eid_reachable_report(EIDReachableReportEvent* event);
    void handle_link_attribute_changed(LinkAttributeChangedEvent* event);
    void handle_link_attributes_query(LinkAttributesQueryRequest* request);
    void handle_link_attributes_report(LinkAttributesReportEvent* event);
    void handle_iface_attributes_query(IfaceAttributesQueryRequest* request);
    void handle_iface_attributes_report(IfaceAttributesReportEvent* event);
    void handle_cla_parameters_query(CLAParametersQueryRequest* request);
    void handle_cla_parameters_report(CLAParametersReportEvent* event);
    void handle_aggregate_custody_signal(AggregateCustodySignalEvent* event);
    ///@}

    /// @{
    void event_handlers_completed(BundleEvent* event);
    /// @}

    typedef BundleProtocol::custody_signal_reason_t custody_signal_reason_t;
    /**
     * Generate a custody signal to be sent to the current custodian.
     */
    void generate_custody_signal(Bundle* bundle, bool succeeded,
                                 custody_signal_reason_t reason);
    
    /**
     * Cancel any pending custody timers for the bundle.
     */
    void cancel_custody_timers(Bundle* bundle);

    /**
     * Release custody of the given bundle, sending the appropriate
     * signal to the current custodian.
     */
    void release_custody(Bundle* bundle);

    /**
     * part 1 processing just adds to pending and the BundleStore
     * Add the bundle to the pending list and (optionally) the
     * persistent store, and set up the expiration timer for it.
     *
     * @return true if the bundle is legal to be delivered and/or
     * forwarded, false if it's already expired
     */
    bool add_to_pending(Bundle* bundle, bool add_to_store);
    
    /**
     * Remove the bundle from the pending list and data store, and
     * cancel the expiration timer.
     */
    bool delete_from_pending(const BundleRef& bundle);
    
    /**
     * Check if we should delete this bundle, called just after
     * arrival, once it's been transmitted or delivered at least once,
     * or when we release custody.
     */
    bool try_to_delete(const BundleRef& bundle, const char* event_type);

    /**
     * Delete (rather than silently discard) a bundle, e.g., an expired
     * bundle. Releases custody of the bundle, removes fragmentation state
     * for the bundle if necessary, removes the bundle from the pending
     * list, and sends a bundle deletion status report if necessary.
     */
    bool delete_bundle(const BundleRef& bundle,
                       status_report_reason_t reason =
                           BundleProtocol::REASON_NO_ADDTL_INFO);
    
    /**
     * Check if there are any bundles in the pending queue that match
     * the source id, timestamp, and fragmentation offset/length
     * fields.
     */
    Bundle* find_duplicate(Bundle* bundle);

    /**
     * Deliver the bundle to the given registration
     */
    void deliver_to_registration(Bundle* bundle, Registration* registration,
                                 bool initial_load=false);
    
    /**
     * First stage of shutting down is to close the links that accept new bundles
     */
    void close_standard_transfer_links();

protected:
    /// The active bundle router
    BundleRouter* router_;

    /// The active bundle actions handler
    BundleActions* actions_;

    /// The administrative registration
    AdminRegistration* admin_reg_ = nullptr;

    /// The IPN administrative registration
    AdminRegistrationIpn* admin_reg_ipn_ = nullptr;

    /// The ping registration
    PingRegistration* ping_reg_ = nullptr;

    /// The ipn echo registration
    IpnEchoRegistration* ipn_echo_reg_ = nullptr;

    /// The contact manager
    ContactManager* contactmgr_;

    /// The fragmentation / reassembly manager
    FragmentManager* fragmentmgr_;

    /// The table of active registrations
    RegistrationTable* reg_table_;

    /// The list of all bundles in the system
    all_bundles_t* all_bundles_;
    all_bundles_t* deleting_bundles_;

    /// The list of all bundles that are still being processed
    pending_bundles_t* pending_bundles_;

    /// lock to serialize access while a delete all bundles request is being processed
    oasys::SpinLock pending_lock_;

    /// The list of all bundles that we have custody of
    custody_bundles_t* custody_bundles_;
    
    /// The list of all bundles that are still being processed
    dupefinder_bundles_t* dupefinder_bundles_;

    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// The default endpoint id for reaching this daemon, used for
    /// bundle status reports, routing, etc.
    EndpointID local_eid_;

    /// The IPN endpoint id used to take custody of bundles destined to an IPN node
    EndpointID local_eid_ipn_;
 
    /// Statistics structure definition
    struct Stats {
        size_t delivered_bundles_;
        size_t transmitted_bundles_;
        size_t expired_bundles_;
        size_t deleted_bundles_;
        size_t injected_bundles_;
        size_t events_processed_;
        size_t suppressed_delivery_;
    };

    /// Stats instance
    Stats stats_;

    /// Application-specific shutdown handler
    ShutdownProc app_shutdown_proc_;
 
    /// Application-specific shutdown data
    void* app_shutdown_data_;

    /// Router-specific shutdown handler
    ShutdownProc rtr_shutdown_proc_;

    /// Router-specific shutdown data
    void* rtr_shutdown_data_;

    // indicator that a BundleDaemon shutdown is in progress
    static bool shutting_down_;
    bool final_cleanup_ = false;

    /// Class used for the idle timer
    struct DaemonIdleExit : public oasys::IdleTclExit {
        DaemonIdleExit(int interval) : IdleTclExit(interval, true) {}
        bool is_idle(const struct timeval& now);
    };
    friend struct DaemonIdleExit;

    typedef std::shared_ptr<DaemonIdleExit> SPtr_DaemonIdleExit;
    
    /// Pointer to the idle exit handler (if any)
    SPtr_DaemonIdleExit idle_exit_;

    /// Time value when the last event was handled
    oasys::Time last_event_;

    /// The Bundle Daemon Aggregate Custody Signal thread
    std::unique_ptr<BundleDaemonACS> daemon_acs_;

    /// Shared Pointer to the singleton LTP Engine
    SPtr_LTPEngine ltp_engine_;

    /// The Bundle Daemon Input thread
    std::unique_ptr<BundleDaemonInput> daemon_input_;

    /// The Bundle Daemon Input thread
    std::unique_ptr<BundleDaemonOutput> daemon_output_;

    /// The Bundle Daemon Storage thread
    std::unique_ptr<BundleDaemonStorage> daemon_storage_;

    /// The Bundle Daemon Cleanup thread
    std::unique_ptr<BundleDaemonCleanup> daemon_cleanup_;

    std::unique_ptr<BIBEExtractor> bibe_extractor_;


protected:
     // could use a separate lock for each counter to minimize contention
    oasys::SpinLock ltp_stats_lock_;

    size_t ltp_retran_timers_created_ = 0;
    size_t ltp_retran_timers_deleted_ = 0;

    size_t ltp_inactivity_timers_created_ = 0;
    size_t ltp_inactivity_timers_deleted_ = 0;

    size_t ltp_closeout_timers_created_ = 0;
    size_t ltp_closeout_timers_deleted_ = 0;

    size_t ltpseg_created_ = 0;
    size_t ltpseg_deleted_ = 0;

    size_t ltp_ds_created_ = 0;
    size_t ltp_ds_deleted_ = 0;

    size_t ltp_rs_created_ = 0;
    size_t ltp_rs_deleted_ = 0;

    size_t ltpsession_created_ = 0;
    size_t ltpsession_deleted_ = 0;

    oasys::SpinLock bibe_stats_lock_;

    size_t bibe6_encapsulations_ = 0;
    size_t bibe6_extractions_ = 0;
    size_t bibe6_extraction_errors_ = 0;

    size_t bibe7_encapsulations_ = 0;
    size_t bibe7_extractions_ = 0;
    size_t bibe7_extraction_errors_ = 0;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
