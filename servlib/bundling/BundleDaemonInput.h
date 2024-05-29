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

#ifndef _BUNDLE_DAEMON_INPUT_H_
#define _BUNDLE_DAEMON_INPUT_H_


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <vector>

#include <third_party/meutils/thread/MsgQueue.h>

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleListStrMap.h"
#include "BundleProtocol.h"
#include "BundleStatusReport.h"

namespace dtn {

class Bundle;
class BundleDaemon;
class BundleRouter;
class FragmentManager;

//dzdebug class BundleDaemonInput : public oasys::Singleton<BundleDaemonInput, false>,
class BundleDaemonInput : 
                     public BundleEventHandler,
                     public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonInput(BundleDaemon* parent);

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonInput();

    /**
     * Cleanly shutdown processing
     */
    virtual void shutdown();

    /**
     * Start thread but delay processing for specified milliseconds
     */
    virtual void start_delayed(u_int32_t millisecs);
    
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
     * Getters for status parameters 
     */
    size_t get_received_bundles() { return stats_.received_bundles_; };
    size_t get_rcvd_from_peer() { return stats_.rcvd_from_peer_; };
    size_t get_rcvd_from_app() { return stats_.rcvd_from_app_; };
    size_t get_rcvd_from_storage() { return stats_.rcvd_from_storage_; };
    size_t get_rcvd_from_restage() { return stats_.rcvd_from_restage_; };
    size_t get_generated_bundles() { return stats_.generated_bundles_; };
    size_t get_rcvd_from_frag() { return stats_.rcvd_from_frag_; };
    size_t get_duplicate_bundles() { return stats_.duplicate_bundles_; };
    size_t get_rejected_bundles() { return stats_.rejected_bundles_; };
    size_t get_injected_bundles() { return stats_.injected_bundles_; };
    size_t get_bpv6_bundles() { return stats_.bpv6_bundles_; };
    size_t get_bpv7_bundles() { return stats_.bpv7_bundles_; };

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
     * Main event handling function.
     */
    void handle_event(SPtr_BundleEvent& sptr_event);

protected:
    friend class BundleDaemon;

   /**
    * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(SPtr_BundleEvent& sptr_event, bool at_back = true);

protected:
    /**
     * Generate delivery events for newly-loaded bundles.
     */
    void generate_delivery_events(Bundle* bundle);

    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_accept(SPtr_BundleEvent& sptr_event) override;
    void handle_bundle_received(SPtr_BundleEvent& sptr_event) override;
    void handle_bundle_inject(SPtr_BundleEvent& sptr_event) override;
    void handle_reassembly_completed(SPtr_BundleEvent& sptr_event) override;
    ///@}

    typedef BundleProtocol::custody_signal_reason_t custody_signal_reason_t;
    typedef BundleProtocol::status_report_flag_t status_report_flag_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;
    
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
    bool try_to_delete(const BundleRef& bundle);

    /**
     * Check if there are any bundles in the pending queue that match
     * the source id, timestamp, and fragmentation offset/length
     * fields.
     */
    Bundle* find_duplicate(Bundle* bundle);

    /**
     * Check the registration table and optionally deliver the bundle
     * to any that match.
     *
     * @return whether or not any matching registrations were found or
     * if the bundle is destined for the local node
     */
    bool check_local_delivery(Bundle* bundle, bool deliver);

    /// The BundleDaemon instance
    BundleDaemon* daemon_ = nullptr;
 
    /// The active bundle router
    BundleRouter* router_ = nullptr;

    /// The fragmentation / reassembly manager
    FragmentManager* fragmentmgr_ = nullptr;

    /// The list of all bundles that we have custody of
    BundleListStrMap* custody_bundles_ = nullptr;
    
    /// The event queue
    meutils::MsgQueue<SPtr_BundleEvent> me_eventq_;




    /// Statistics structure definition
    struct Stats {
        size_t received_bundles_ = 0;
        size_t rcvd_from_peer_ = 0;
        size_t rcvd_from_app_ = 0;
        size_t rcvd_from_storage_ = 0;
        size_t rcvd_from_restage_ = 0;
        size_t generated_bundles_ = 0;
        size_t rcvd_from_frag_ = 0;
        size_t delivered_bundles_ = 0;
        size_t transmitted_bundles_ = 0;
        size_t expired_bundles_ = 0;
        size_t deleted_bundles_ = 0;
        size_t duplicate_bundles_ = 0;
        size_t injected_bundles_ = 0;
        size_t events_processed_ = 0;
        size_t rejected_bundles_ = 0;
        size_t hops_exceeded_ = 0;
        size_t bpv6_bundles_ = 0;
        size_t bpv7_bundles_ = 0;


        void clear() {
            received_bundles_ = 0;
            rcvd_from_peer_ = 0;
            rcvd_from_app_ = 0;
            rcvd_from_storage_ = 0;
            rcvd_from_restage_ = 0;
            generated_bundles_ = 0;
            rcvd_from_frag_ = 0;
            delivered_bundles_ = 0;
            transmitted_bundles_ = 0;
            expired_bundles_ = 0;
            deleted_bundles_ = 0;
            duplicate_bundles_ = 0;
            injected_bundles_ = 0;
            events_processed_ = 0;
            rejected_bundles_ = 0;
            hops_exceeded_ = 0;
            bpv6_bundles_ = 0;
            bpv7_bundles_ = 0;
        }

    };

    /// Stats instance
    Stats stats_;

    /// number of milliseconds to delay before starting processing events
    u_int32_t delayed_start_millisecs_ = 0;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
