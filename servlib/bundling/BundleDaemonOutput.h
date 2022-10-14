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

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleProtocol.h"
#include "BundleActions.h"

namespace dtn {

class Bundle;
class BundleDaemon;
class BundleActions;
class BundleRouter;
class ContactManager;
class FragmentManager;

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class BundleDaemonOutput : public BundleEventHandler,
                           public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonOutput(BundleDaemon* parent);

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonOutput();

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
        return eventq_->size();
    }

   /**
    * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

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
    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_injected(BundleInjectedEvent* event);
    void handle_bundle_send(BundleSendRequest* event);
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
    void handle_bundle_restaged(BundleRestagedEvent* event);
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

    /// The contact manager
    ContactManager* contactmgr_;

    /// The fragmentation / reassembly manager
    FragmentManager* fragmentmgr_;

    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// Statistics structure definition
    struct Stats {
        size_t transmitted_bundles_;
        size_t events_processed_;
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
