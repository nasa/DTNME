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

    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
    
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
BundleDaemonCleanup::~BundleDaemonCleanup()
{
    delete eventq_;
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::shutdown()
{
    while (eventq_->size() > 0) {
        usleep(100000);
    }

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::post_event(BundleEvent* event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
    event->posted_time_.get_time();
    eventq_->push(event, at_back);
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
    	         eventq_->size(),
    	         eventq_->max_size(),
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
BundleDaemonCleanup::handle_bundle_free(BundleFreeEvent* event)
{
    daemon_->handle_bundle_free(event);
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::event_handlers_completed(BundleEvent* event)
{
    (void) event;
}


//----------------------------------------------------------------------
void
BundleDaemonCleanup::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void) closeTransaction;

    dispatch_event(event);
   
    if (! event->daemon_only_) {
        if (!BundleDaemon::shutting_down()) {
            // dispatch the event to the router(s) and the contact manager
            //  only event currently is the BundleFreeEvent needed by the ExternalRouter
            BundleDaemon::instance()->router()->handle_event(event);
            //contactmgr_->handle_event(event);
        }
    }

 
    event_handlers_completed(event);

    stats_.events_processed_++;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonCleanup::run()
{
    char threadname[16] = "BD-Cleanup";
    pthread_setname_np(pthread_self(), threadname);
   
    static const char* LOOP_LOG = "/dtn/bundle/daemon/cleanup/loop";

    BundleEvent* event;

    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 100;

    while (1) {
        if (should_stop()) {
            log_always("BundleDaemonCleanup: stopping - event queue size: %zu",
                      eventq_->size());
            break;
        }

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            oasys::Time now;
            now.get_time();

            
            if (now >= event->posted_time_) {
                oasys::Time in_queue;
                in_queue = now - event->posted_time_;
                if (in_queue.sec_ > 2) {
                    log_warn_p(LOOP_LOG, "event %s was in queue for %" PRIu64 ".%" PRIu64 " seconds",
                               event->type_str(), in_queue.sec_, in_queue.usec_);
                }
            } else {
                log_warn_p(LOOP_LOG, "time moved backwards: "
                           "now %" PRIu64 ".%" PRIu64 ", event posted_time %" PRIu64 ".%" PRIu64 "",
                           now.sec_, now.usec_,
                           event->posted_time_.sec_, event->posted_time_.usec_);
            }
            
            // handle the event
            handle_event(event);

            int elapsed = now.elapsed_ms();
            if (elapsed > 2000) {
                log_warn_p(LOOP_LOG, "event %s took %u ms to process",
                           event->type_str(), elapsed);
            }

            // record the last event time
            last_event_.get_time();

            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);

        if (cc == oasys::IOTIMEOUT) {
            log_debug_p(LOOP_LOG, "poll timeout");
            continue;

        } else if (cc <= 0) {
            log_err_p(LOOP_LOG, "unexpected return %d from poll_multiple!", cc);
            continue;
        }

        // if the event poll fired, we just go back to the top of the
        // loop to drain the queue
        if (event_poll->revents != 0) {
            log_debug_p(LOOP_LOG, "poll returned new event to handle");
        }
    }
}

} // namespace dtn

