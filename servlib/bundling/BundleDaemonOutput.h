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

#ifndef _BUNDLE_DAEMON_OUTPUT_H_
#define _BUNDLE_DAEMON_OUTPUT_H_


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <vector>

#include <oasys/compat/inttypes.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>

#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleProtocol.h"
#include "BundleActions.h"
#include "BundleStatusReport.h"

#ifdef BPQ_ENABLED
#    include "BPQBlock.h"
#    include "BPQCache.h"
#endif /* BPQ_ENABLED */

namespace dtn {

class Bundle;
class BundleAction;
class BundleActions;
class BundleRouter;
class ContactManager;
class FragmentManager;
class RegistrationTable;

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class BundleDaemonOutput : public oasys::Singleton<BundleDaemonOutput, false>,
                     public BundleEventHandler,
                     public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonOutput();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonOutput();

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
     * Getters for status parameters 
     */
    bundleid_t get_received_bundles() { return stats_.received_bundles_; };
    bundleid_t get_generated_bundles() { return stats_.generated_bundles_; };
    bundleid_t get_duplicate_bundles() { return stats_.duplicate_bundles_; };
    bundleid_t get_rejected_bundles() { return stats_.rejected_bundles_; };

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
        
        /// no params currently defined for this class
        bool not_currently_used_;
    };

    static Params params_;

    /**
     * Main event handling function.
     */
    void handle_event(BundleEvent* event);
    void handle_event(BundleEvent* event, bool closeTransaction);

protected:
    friend class BundleActions;
    friend class BundleDaemon;

    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    void handle_bundle_injected(BundleInjectedEvent* event);
    void handle_bundle_send(BundleSendRequest* event);
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
    void handle_link_cancel_all_bundles_request(LinkCancelAllBundlesRequest* event);
    ///@}

    /// @{
    void event_handlers_completed(BundleEvent* event);
    /// @}

    /// The BundleDaemon instance
    BundleDaemon* daemon_;
 
    /// The active bundle router
    BundleRouter* router_;

    /// The active bundle actions handler
    BundleActions* actions_;

    /// The administrative registration
    AdminRegistration* admin_reg_;

    /// The ping registration
    PingRegistration* ping_reg_;

    /// The contact manager
    ContactManager* contactmgr_;

    /// The fragmentation / reassembly manager
    FragmentManager* fragmentmgr_;

    /// The table of active registrations
    const RegistrationTable* reg_table_;

    /// The list of all bundles in the system
    all_bundles_t* all_bundles_;

    /// The list of all bundles that are still being processed
    pending_bundles_t* pending_bundles_;

    /// The list of all bundles that we have custody of
    custody_bundles_t* custody_bundles_;
    
    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// Statistics structure definition
    struct Stats {
        bundleid_t received_bundles_;
        bundleid_t delivered_bundles_;
        bundleid_t generated_bundles_;
        bundleid_t transmitted_bundles_;
        bundleid_t expired_bundles_;
        bundleid_t deleted_bundles_;
        bundleid_t duplicate_bundles_;
        bundleid_t injected_bundles_;
        bundleid_t events_processed_;
        bundleid_t rejected_bundles_;
    };

    /// Stats instance
    Stats stats_;

    /// Time value when the last event was handled
    oasys::Time last_event_;

    /// number of milliseconds to delay before starting processing events
    u_int32_t delayed_start_millisecs_;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
