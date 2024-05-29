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

#include <third_party/meutils/thread/MsgQueue.h>

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
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
        return me_eventq_.size();
    }

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
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_injected(SPtr_BundleEvent& sptr_event) override;
    void handle_bundle_send(SPtr_BundleEvent& sptr_event) override;
    void handle_bundle_transmitted(SPtr_BundleEvent& sptr_event) override;
    void handle_bundle_restaged(SPtr_BundleEvent& sptr_event) override;
    void handle_link_cancel_all_bundles_request(SPtr_BundleEvent& sptr_event) override;
    ///@}

    /// @{
    void event_handlers_completed(SPtr_BundleEvent& sptr_event);
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
    meutils::MsgQueue<SPtr_BundleEvent> me_eventq_;

    /// Statistics structure definition
    struct Stats {
        size_t transmitted_bundles_;
        size_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    /// number of milliseconds to delay before starting processing events
    u_int32_t delayed_start_millisecs_;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
