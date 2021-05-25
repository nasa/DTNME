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

#ifndef _BUNDLE_DAEMON_CLEANUP_H_
#define _BUNDLE_DAEMON_CLEANUP_H_


#include <list>

#include <third_party/oasys/compat/inttypes.h>
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/tclcmd/IdleTclExit.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/Time.h>

//#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleProtocol.h"
#include "naming/EndpointID.h"

namespace dtn {

class Bundle;
class BundleDaemon;
class BundleRouter;

/**
 * Class that handles the basic event / action mechanism for 
 * Aggregate Custody Signal events. 
 */
class BundleDaemonCleanup : public BundleEventHandler,
                            public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonCleanup(BundleDaemon* parent);

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonCleanup();

    /**
     * Cleanly shutdown processing
     */
    virtual void shutdown();

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
        bool dummy_;
    };

    static Params params_;

    /**
     * Typedef for a shutdown procedure.
     */
    typedef void (*ShutdownProc) (void* args);
    
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
    void handle_bundle_free(BundleFreeEvent* event);
    ///@}

    /// @{
    void event_handlers_completed(BundleEvent* event);
    /// @}

    /// The BundleDaemon instance
    BundleDaemon* daemon_;
 
    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// Statistics structure definition
    struct Stats {
        u_int64_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    /// Time value when the last event was handled
    oasys::Time last_event_;
};

} // namespace dtn


#endif /* _BUNDLE_DAEMON_CLEANUP_H_ */
