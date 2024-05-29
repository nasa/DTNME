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

#ifndef _BUNDLE_DAEMON_ACS_H_
#define _BUNDLE_DAEMON_ACS_H_


#include <list>

#include <third_party/meutils/thread/MsgQueue.h>

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/tclcmd/IdleTclExit.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

#include "AggregateCustodySignal.h"
#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleProtocol.h"
#include "BundleActions.h"
#include "BundleStatusReport.h"
#include "CustodyList.h"
#include "naming/EndpointID.h"

namespace dtn {

class Bundle;
class BundleAction;
class BundleActions;
class BundleRouter;
class ContactManager;
class FragmentManager;
class RegistrationTable;

/**
 * Class that handles the basic event / action mechanism for 
 * Aggregate Custody Signal events. 
 */
class BundleDaemonACS : public BundleEventHandler,
                        public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonACS();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonACS();

    /**
     * Return the number of events currently waiting for processing.
     * This is overridden in the simulator since it doesn't use a
     * MsgQueue.
     */
    virtual size_t event_queue_size()
    {
    	return me_eventq_.size();
    }

   /**
     * post_event function to queue an event for processing
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

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

    /**
     * Reset all internal stats.
     */
    void reset_stats();

    /**
     * General daemon parameters
     */
    struct Params {
        /// Default constructor
        Params();
        
        /// Whether or not Aggfregate Custody Signals are enabled
        bool acs_enabled_;

        /// max length of the ACS block (0 = infinite)
        u_int acs_size_;

        /// number of seconds to accumulate custody singnals before issuing the ACS
        u_int acs_delay_;
    };

    static Params params_;

    /**
     * Typedef for a shutdown procedure.
     */
    typedef void (*ShutdownProc) (void* args);
    
    /**
     * Accessor for the BundleDaemonACS's shutdown status
     */
    static bool shutting_down()
    {
     	return shutting_down_;
    }

    /**
     * Main event handling function.
     */
    void handle_event(SPtr_BundleEvent& sptr_event);

    /**
     * Implementation of the handle ACS event which the BundleDaemon can call 
     * to keep the ACS implementation modularized. This runs within the
     * BundleDaemon thread.
     */
    void process_acs(SPtr_BundleEvent& sptr_event);

    /**
     * Load Pending ACSs from the datastore and generate the ACSs immediately.
     */
    void load_pendingacs();

    /**
     * Perform ACS related processing for accepting custody of a bundle
     */
    void accept_custody(Bundle* bundle);
    void bibe_accept_custody(Bundle* bundle);

    /**
     * Perform ACS related processing for generating a custody signal
     */
    typedef BundleProtocol::custody_signal_reason_t custody_signal_reason_t;
    bool generate_custody_signal(Bundle* bundle, bool succeeded,
                                 custody_signal_reason_t reason);

    /**
     * Remove a bundle from the custody id list 
     */
    void erase_from_custodyid_list(Bundle* bundle);
    void bibe_erase_from_custodyid_list(Bundle* bundle);

    /**
     * Apply route acs_set command to set ACS values for a route
     */
    void set_route_acs_params(SPtr_EIDPattern& sptr_pat, bool enabled, 
                              u_int acs_delay, u_int acs_size);

    /**
     * Apply route acs_del command to delete ACS values for a route
     *
     * Return value: 0 = success, -1 = not found
     */
    int delete_route_acs_params(SPtr_EIDPattern& sptr_pat);

    /**
     * Format all ACS values for the acs_dump command
     */
    void dump_acs_params(oasys::StringBuffer* buf);

    /***
     * Retrieve the ACS parameters for a specific Endpoint (route)
     *
     * Return value: true = acs param values have changed since the
     *                      previous call based on the passed in revision
     */
    bool get_acs_params_for_endpoint(const SPtr_EID& sptr_ep, u_int* revision,
                                     u_int* acs_delay, u_int* acs_size);

    /***
     * Determine if ACS is enabled for a particular Endpoint
     */
    bool acs_enabled_for_endpoint(const SPtr_EID& sptr_ep);

    /**
     * Add BP7 BIBE Custody Transfer entry
     */
    void add_bibe_bundle_to_acs(int32_t bp_version, const SPtr_EID& sptr_custody_eid, 
                                uint64_t transmission_id, bool success, uint8_t reason);

protected:
    friend class BundleActions;

    typedef enum {
        AE_OP_NOOP = 0,
        AE_OP_INSERT_FIRST_ENTRY,
        AE_OP_INSERT_ENTRY,
        AE_OP_INSERT_ENTRY_AT_END,
        AE_OP_EXTEND_ENTRY,
        AE_OP_PREPEND_ENTRY,
    } ae_operation_t;

    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_acs_expired(SPtr_BundleEvent& sptr_event) override;
    void handle_add_bundle_to_acs(SPtr_BundleEvent& sptr_event) override;
    void handle_aggregate_custody_signal(SPtr_BundleEvent& sptr_event) override;
    ///@}

    /**
     * Determine which entries need to change to add a new custody ID to
     * the Pending ACS and what the change will do to the size of the 
     * its payload.
     */
    int determine_acs_changes(PendingAcs* pacs, ae_operation_t &ae_op,
                              uint64_t custody_id, AcsEntryIterator &ae_itr,
                              AcsEntry* &entry, AcsEntry* &next_entry,
                              int &new_entry_size, int &new_next_size,
                              uint64_t &new_entry_diff, 
                              uint64_t &new_next_diff);

    /**
     * Apply the changes previously determined to the Pending ACS. 
     */
    void apply_acs_changes(PendingAcs* pacs, ae_operation_t &ae_op,
                           uint64_t custody_id,
                           AcsEntryIterator &ae_itr, int delta,
                           AcsEntry* &entry, AcsEntry* &next_entry,
                           int &new_entry_size, int &new_next_size,
                           uint64_t &new_entry_diff, 
                           uint64_t &new_next_diff);

    /**
     * Update the datastore with the latest copy of the Pending ACS
     */
    void update_pending_acs_store(PendingAcs* pacs);

    /**
     * Generate an Aggregate Custody Signal bundle 
     */
    void generate_acs(PendingAcs* pacs, bool update_datastore=true);
    void bibe_generate_acs(PendingAcs* pacs, bool update_datastore=true);

    /**
     * Create a new ACS Entry and insert it into the map
     */
    void add_acs_entry(AcsEntryMap* aemap, uint64_t left_edge, uint64_t fill_len,
                               uint64_t diff_to_prev_right_edge, int sdnv_len);

    /// The event queue
    meutils::MsgQueue<SPtr_BundleEvent> me_eventq_;

    /// Statistics structure definition
    struct Stats {
        u_int32_t acs_accepted_;
        u_int32_t acs_released_;
        u_int32_t acs_redundant_;
        u_int32_t total_released_;
        u_int32_t acs_not_found_;
        u_int32_t acs_generated_;
        u_int32_t acs_reloaded_;
        u_int32_t acs_invalid_;
        u_int64_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    // indicator that a BundleDaemonACS shutdown is in progress
    static bool shutting_down_;

    /// The list of all bundles that we have custody of
    CustodyList custody_list_;
    
    /// Map of pending ACS's
    PendingAcsMap* pending_acs_map_;

    /// Counter to determine if the ACS parameters have changed
    u_int acs_params_revision_;

    /// Lock to control access to the acs route params list
    oasys::SpinLock acs_route_params_list_lock_;

    /// List of route specific overrides of the ACS parameters
    typedef struct ACSRouteParams_t {
        SPtr_EIDPattern sptr_endpoint_;
        bool acs_enabled_;
        u_int acs_delay_;
        u_int acs_size_;
        u_int match_len_;
    } ACSRouteParams;
    typedef std::list<ACSRouteParams*> ACSRouteParamsList;
    ACSRouteParamsList acs_route_params_list_;

    /// Debug parameter used to track when an operation type changes.
    ae_operation_t dbg_last_ae_op_;
};

} // namespace dtn


#endif /* _BUNDLE_DAEMON_H_ */
