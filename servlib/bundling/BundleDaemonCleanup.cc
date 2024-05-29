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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/util/Time.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleDaemonCleanup.h"
#include "routing/BundleRouter.h"


namespace dtn {

BundleDaemonCleanup::Params::Params()
    :  dummy_(true)
{}

BundleDaemonCleanup::Params BundleDaemonCleanup::params_;


//----------------------------------------------------------------------
BundleDaemonCleanup::BundleDaemonCleanup(BundleDaemon* parent)
    : BundleEventHandler("BundleDaemonCleanup", "/dtn/bundle/daemon/cleanup"),
      Thread("BundleDaemonCleanup")
{
    daemon_ = parent;

    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
BundleDaemonCleanup::~BundleDaemonCleanup()
{
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::shutdown()
{
    while (me_eventq_.size() > 0) {
        usleep(100000);
    }

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::post_event(SPtr_BundleEvent& sptr_event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              sptr_event.get(), sptr_event->type_str(), at_back ? "back" : "head");
    sptr_event->posted_time_.get_time();
    me_eventq_.push(sptr_event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::get_bundle_stats(oasys::StringBuffer* buf)
{
    (void) buf;
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("BundleDaemonCleanup: %zu pending_events (max: %zu) -- "
                 "%" PRIu64 " processed_events \n",
    	         me_eventq_.size(),
    	         me_eventq_.max_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::handle_bundle_free(SPtr_BundleEvent& sptr_event)
{
    daemon_->handle_bundle_free(sptr_event);
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
   
    if (! sptr_event->daemon_only_) {
        if (!BundleDaemon::shutting_down()) {
            // dispatch the event to the router(s) and the contact manager
            //  only event currently is the BundleFreeEvent needed by the ExternalRouter

            BundleDaemon::instance()->router()->handle_event(sptr_event);
            //contactmgr_->handle_event(sptr_event);
        }
    }

 
    stats_.events_processed_++;

    if (sptr_event->processed_notifier_) {
        sptr_event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::run()
{
    char threadname[16] = "BD-Cleanup";
    pthread_setname_np(pthread_self(), threadname);
   
    SPtr_BundleEvent sptr_event;

    while (true) {
        if (should_stop()) {
            log_always("BundleDaemonCleanup: stopping - event queue size: %zu",
                      me_eventq_.size());
            break;
        }

        if (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);

            // handle the event
            handle_event(sptr_event);

            // clean up the event
            sptr_event.reset();
            
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

} // namespace dtn

