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
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemonOutput.h"
#include "SDNV.h"

#include "contacts/ContactManager.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "naming/IPNScheme.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "storage/BundleStore.h"


// enable or disable debug level logging in this file
#undef BDOUTPUT_LOG_DEBUG_ENABLED
//#define BDOUTPUT_LOG_DEBUG_ENABLED 1




namespace dtn {

BundleDaemonOutput::Params::Params()
    :  not_currently_used_(false)
{}

BundleDaemonOutput::Params BundleDaemonOutput::params_;

//----------------------------------------------------------------------
BundleDaemonOutput::BundleDaemonOutput(BundleDaemon* parent)
    : BundleEventHandler("BundleDaemonOutput", "/dtn/bundle/daemon/output"),
      Thread("BundleDaemonOutput")
{
    daemon_ = parent;
    contactmgr_ = NULL;
    fragmentmgr_ = NULL;
    delayed_start_millisecs_ = 0;
    
    memset(&stats_, 0, sizeof(stats_));

    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
}

//----------------------------------------------------------------------
BundleDaemonOutput::~BundleDaemonOutput()
{
    delete actions_;
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::start_delayed(u_int32_t millisecs)
{
    delayed_start_millisecs_ = millisecs;
    start();
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post_event(BundleEvent* event, bool at_back)
{
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
#endif
    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("BundleDaemonOutput : %zu pending_events (max: %zu) -- "
                 "%" PRIu64 " processed_events \n",
    	         eventq_->size(),
    	         eventq_->max_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonOutput::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_injected(BundleInjectedEvent* event)
{
    (void) event;
    // Nothing to do here - just passing through to the router to decide next action
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_send(BundleSendRequest* event)
{
    LinkRef link = contactmgr_->find_link(event->link_.c_str());
    if (link == NULL){
        if (!BundleDaemon::shutting_down()) {
            log_err("Cannot send bundle on unknown link %s", event->link_.c_str()); 
        }
        return;
    }

    BundleRef br = event->bundle_;
    if (! br.object()){
        log_err("NULL bundle object in BundleSendRequest");
        return;
    }

    ForwardingInfo::action_t fwd_action =
        (ForwardingInfo::action_t)event->action_;

    actions_->queue_bundle(br.object(), link,
        fwd_action, CustodyTimerSpec::defaults_);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != NULL);

    SPtr_BlockInfoVec blocks;

    if (!event->blocks_deleted_)
    {
        blocks = bundle->xmit_blocks()->find_blocks(link);
    
        // Because a CL is running in another thread or process (External CLs),
        // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
        // If an event about a bundle bound for particular link is posted after another,
        // which it might contradict, the BundleDaemon need not reprocess the event.
        // The router (DP) might, however, be interested in the new status of the send.
        if (blocks == nullptr)
        {
            log_info("received a redundant/conflicting bundle_transmit event about "
                     "bundle id:%" PRIbid " -> %s (%s)",
                     bundle->bundleid(),
                     link->name(),
                     link->nexthop());
            return;
        }
    }
    
    /*
     * Update statistics and remove the bundle from the link inflight
     * queue. Note that the link's queued length statistics must
     * always be decremented by the full size of the bundle payload,
     * yet the transmitted length is only the amount reported by the
     * event.
     */
    // remove the bundle from the link's in flight queue
    if (link->del_from_inflight(event->bundleref_)) {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
        log_debug("removed bundle id:%" PRIbid " from link %s inflight queue",
                 bundle->bundleid(),
                 link->name());
#endif
    } else {
        if (!bundle->expired()) {
            log_warn("bundle id:%" PRIbid " not on link %s inflight queue",
                     bundle->bundleid(),
                     link->name());
        }
    }
    
    // verify that the bundle is not on the link's to-be-sent queue
    if (link->del_from_queue(event->bundleref_)) {
        log_warn("bundle id:%" PRIbid " unexpectedly on link %s queue in transmitted event",
                 bundle->bundleid(),
                 link->name());
    }
    
    if (!event->success_)
    { 
        // needed for LTPUDP CLA and TableBasedRouter to
        // trigger sending more bundles if the queue is full
        // and no new bundles are incoming when LTP is not successful
        if (!event->blocks_deleted_) {
            BundleProtocol::delete_blocks(bundle, link);
        }

        //XXX/dz Schedule a custody timer if we have custody as a precaution
        if (bundle->local_custody() || bundle->bibe_custody()) {
            ForwardingInfo fwdinfo;
            bundle->fwdlog()->get_latest_entry(link, &fwdinfo);

            SPtr_CustodyTimer ct_sptr = std::make_shared<CustodyTimer>(bundle, link);

            bundle->custody_timers()->push_back(ct_sptr);

            ct_sptr->start(fwdinfo.timestamp(), fwdinfo.custody_spec(), ct_sptr);
        }

        bundle->fwdlog()->update(link, ForwardingInfo::TRANSMIT_FAILED);
        return;
    }


    stats_.transmitted_bundles_++;
    
    link->stats()->bundles_transmitted_++;
    link->stats()->bytes_transmitted_ += event->bytes_sent_;

#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_info("BUNDLE_TRANSMITTED id:%" PRIbid " (%u bytes_sent/%u reliable) -> %s (%s)",
             bundle->bundleid(),
             event->bytes_sent_,
             event->reliably_sent_,
             link->name(),
             link->nexthop());
#endif


    /*
     * If we're configured to wait for reliable transmission, then
     * check the special case where we transmitted some or all a
     * bundle but nothing was acked. In this case, we create a
     * transmission failed event in the forwarding log and don't do
     * any of the rest of the processing below.
     *
     * Note also the special care taken to handle a zero-length
     * bundle. XXX/demmer this should all go away when the lengths
     * include both the header length and the payload length (in which
     * case it's never zero).
     *
     * XXX/demmer a better thing to do (maybe) would be to record the
     * lengths in the forwarding log as part of the transmitted entry.
     */
    if (BundleDaemon::params_.retry_reliable_unacked_ &&
        link->is_reliable() &&
        (event->bytes_sent_ != event->reliably_sent_) &&
        (event->reliably_sent_ == 0))
    {
        bundle->fwdlog()->update(link, ForwardingInfo::TRANSMIT_FAILED);
        BundleProtocol::delete_blocks(bundle, link);

        log_warn("XXX/demmer fixme transmitted special case");
        
        return;
    }

    /*
     * Grab the latest forwarding log state so we can find the custody
     * timer information (if any).
     */
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog()->get_latest_entry(link, &fwdinfo);
    if(!ok)
    {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
        oasys::StringBuffer buf;
        bundle->fwdlog()->dump(&buf);
        log_debug("%s",buf.c_str());
#endif
    }
    ASSERTF(ok, "no forwarding log entry for transmission");
    // ASSERT(fwdinfo.state() == ForwardingInfo::QUEUED);
    if (fwdinfo.state() != ForwardingInfo::QUEUED) {
        log_err("*%p fwdinfo state %s != expected QUEUED on link %s in BundleTransmittedEvent processing",
                bundle, ForwardingInfo::state_to_str(fwdinfo.state()),
                link->name());
    }
    
    /*
     * Update the forwarding log indicating that the bundle is no
     * longer in flight.
     */
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_debug("updating forwarding log entry on *%p for *%p to TRANSMITTED",
              bundle, link.object());
#endif
    bundle->fwdlog()->update(link, ForwardingInfo::TRANSMITTED);
                            
    /*
     * Check for reactive fragmentation. If the bundle was only
     * partially sent, then a new bundle received event for the tail
     * part of the bundle will be processed immediately after this
     * event.
     */
    if (blocks != nullptr) {
        if (link->is_reliable()) {
            fragmentmgr_->try_to_reactively_fragment(bundle,
                                                     blocks,
                                                     event->reliably_sent_);
        } else {
            fragmentmgr_->try_to_reactively_fragment(bundle,
                                                     blocks,
                                                     event->bytes_sent_);
        }
    }

    // just in case - no harm if there are no blocks
    BundleProtocol::delete_blocks(bundle, link);

    /*
     * Generate the forwarding status report if requested
     */
    if (bundle->forward_rcpt()) {
        BundleDaemon::instance()->generate_status_report(bundle, BundleStatusReport::STATUS_FORWARDED);
    }
    
    /*
     * Schedule a custody timer if we have custody.
     */
    if (bundle->local_custody() || bundle->bibe_custody()) {
        SPtr_CustodyTimer ct_sptr = std::make_shared<CustodyTimer>(bundle, link);

        bundle->custody_timers()->push_back(ct_sptr);

        ct_sptr->start(fwdinfo.timestamp(), fwdinfo.custody_spec(), ct_sptr);
        
        // XXX/TODO: generate failed custodial signal for "forwarded
        // over unidirectional link" if the bundle has the retention
        // constraint "custody accepted" and all of the nodes in the
        // minimum reception group of the endpoint selected for
        // forwarding are known to be unable to send bundles back to
        // this node
    }
}

void 
BundleDaemonOutput::handle_link_cancel_all_bundles_request(LinkCancelAllBundlesRequest* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    link->cancel_all_bundles();
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::event_handlers_completed(BundleEvent* event)
{
    /**
     * Once transmission or delivery has been
     * processed by the router, pass the event to the main BundleDaemon
     * to see if it can be deleted.
     */
    if (event->type_ == BUNDLE_TRANSMITTED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        BundleTransmittedEvent* bte = dynamic_cast<BundleTransmittedEvent*>(event);
        BundleTransmittedEvent* ev = 
            new BundleTransmittedEvent_MainProcessor(bte->bundleref_.object(),
                        bte->contact_, bte->link_, bte->bytes_sent_, bte->reliably_sent_, 
                        bte->success_, bte->blocks_deleted_);
        ev->daemon_only_ = true;

        BundleDaemon::post(ev);

    } else if (event->type_ == BUNDLE_INJECTED) {
        BundleInjectedEvent* bie = dynamic_cast<BundleInjectedEvent*>(event);
        BundleInjectedEvent* ev = 
            new BundleInjectedEvent_MainProcessor(bie->bundleref_.object(),
                                                  bie->request_id_);
        BundleDaemon::post(ev);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
   
    if (! event->daemon_only_) {
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(event);
        contactmgr_->handle_event(event);
    }

    // only check to see if bundle can be deleted if event is daemon_only
    event_handlers_completed(event);

    stats_.events_processed_++;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::run()
{
    static const char* LOOP_LOG = "/dtn/bundle/daemon/output/loop";
    
    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }

    char threadname[16] = "BD-Output";
    pthread_setname_np(pthread_self(), threadname);
   
    if (delayed_start_millisecs_ > 0) {
        log_always("Delaying start for %u millseconds", delayed_start_millisecs_);
        usleep(delayed_start_millisecs_ * 1000);
        log_always("Starting processing in the BundleDaemonOutput thread");
    }

    BundleEvent* event;

    contactmgr_ = daemon_->contactmgr();
    fragmentmgr_ = daemon_->fragmentmgr();
    router_ = daemon_->router();

    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 100;

    while (1) {
        if (should_stop()) {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug("BundleDaemonOutput: stopping - event queue size: %zu",
                      eventq_->size());
#endif
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
            continue;

        } else if (cc <= 0) {
            log_err_p(LOOP_LOG, "unexpected return %d from poll_multiple!", cc);
            continue;
        }

        // if the event poll fired, we just go back to the top of the
        // loop to drain the queue
    }
}

} // namespace dtn
