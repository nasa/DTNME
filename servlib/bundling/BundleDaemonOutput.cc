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

#include <oasys/io/IO.h>
#include <oasys/util/Time.h>

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

#include "bundling/S10Logger.h"

#ifdef BSP_ENABLED
#  include "security/Ciphersuite.h"
#  include "security/SPD.h"
#  include "security/KeyDB.h"
#endif

#ifdef ACS_ENABLED
#  include "BundleDaemonACS.h"
#endif // ACS_ENABLED



// enable or disable debug level logging in this file
#undef BDOUTPUT_LOG_DEBUG_ENABLED
//#define BDOUTPUT_LOG_DEBUG_ENABLED 1




namespace oasys {
    template <> dtn::BundleDaemonOutput* oasys::Singleton<dtn::BundleDaemonOutput, false>::instance_ = NULL;
}


namespace dtn {

BundleDaemonOutput::Params::Params()
    :  not_currently_used_(false)
{}

BundleDaemonOutput::Params BundleDaemonOutput::params_;

//----------------------------------------------------------------------
void
BundleDaemonOutput::init()
{       
    if (instance_ != NULL) 
    {
        PANIC("BundleDaemonOutput already initialized");
    }

    instance_ = new BundleDaemonOutput();
    instance_->do_init();
}
    
//----------------------------------------------------------------------
BundleDaemonOutput::BundleDaemonOutput()
    : BundleEventHandler("BundleDaemonOutput", "/dtn/bundle/daemon/output"),
      Thread("BundleDaemonOutput", CREATE_JOINABLE)
{
    daemon_ = NULL;
    actions_ = NULL;
    eventq_ = NULL;
    all_bundles_     = NULL;
    pending_bundles_ = NULL;
    custody_bundles_ = NULL;
    contactmgr_ = NULL;
    fragmentmgr_ = NULL;
    reg_table_ = NULL;
    delayed_start_millisecs_ = 0;
    
#ifdef BPQ_ENABLED
    bpq_cache_ = NULL;
#endif /* BPQ_ENABLED */

    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
BundleDaemonOutput::~BundleDaemonOutput()
{
    delete actions_;
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
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
BundleDaemonOutput::post(BundleEvent* event)
{
    instance_->post_event(event);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post_at_head(BundleEvent* event)
{
    instance_->post_event(event, false);
}

//----------------------------------------------------------------------
bool
BundleDaemonOutput::post_and_wait(BundleEvent* event,
                            oasys::Notifier* notifier,
                            int timeout, bool at_back)
{
    /*
     * Make sure that we're either already started up or are about to
     * start. Otherwise the wait call below would block indefinitely.
     */
    ASSERT(! oasys::Thread::start_barrier_enabled());
    ASSERT(event->processed_notifier_ == NULL);
    event->processed_notifier_ = notifier;
    if (at_back) {
        post(event);
    } else {
        post_at_head(event);
    }
    return notifier->wait(NULL, timeout);

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
    buf->appendf("BundleDaemonOutput : %zu pending_events -- "
                 "%" PRIu64 " processed_events \n",
                 event_queue_size(),
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
BundleDaemonOutput::handle_bundle_received(BundleReceivedEvent* event)
{
    (void) event;
    // Nothing to do here - just passing through to the router to decide next action
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

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("trying to find xmit blocks for bundle id:%" PRIbid " on link %s",
              bundle->bundleid(),link->name());
#endif
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
    // If an event about a bundle bound for particular link is posted after another,
    // which it might contradict, the BundleDaemon need not reprocess the event.
    // The router (DP) might, however, be interested in the new status of the send.
    if(blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_transmit event about "
                 "bundle id:%" PRIbid " -> %s (%s)",
                 bundle->bundleid(),
                 link->name(),
                 link->nexthop());
        return;
    }
    
    /*
     * Update statistics and remove the bundle from the link inflight
     * queue. Note that the link's queued length statistics must
     * always be decremented by the full formatted size of the bundle,
     * yet the transmitted length is only the amount reported by the
     * event.
     */
    size_t total_len = BundleProtocol::total_length(blocks);
    

    // remove the bundle from the link's in flight queue
    if (link->del_from_inflight(event->bundleref_, total_len)) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("removed bundle id:%" PRIbid " from link %s inflight queue",
                 bundle->bundleid(),
                 link->name());
#endif
    } else {
        log_warn("bundle id:%" PRIbid " not on link %s inflight queue",
                 bundle->bundleid(),
                 link->name());
    }
    
    // verify that the bundle is not on the link's to-be-sent queue
    if (link->del_from_queue(event->bundleref_, total_len)) {
        log_warn("bundle id:%" PRIbid " unexpectedly on link %s queue in transmitted event",
                 bundle->bundleid(),
                 link->name());
    }
    
    if (!event->success_)
    { 
        // needed for LTPUDP CLA and TableBasedRouter to
        // trigger sending more bundles if the queue is full
        // and no new bundles are incoming when LTP is not successful
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("trying to delete xmit blocks for bundle id:%" PRIbid " on link %s",
               bundle->bundleid(),link->name());
#endif
        BundleProtocol::delete_blocks(bundle, link);

        //XXX/dz Schedule a custody timer if we have custody as a precaution
        if (bundle->local_custody()) {
            ForwardingInfo fwdinfo;
            bundle->fwdlog()->get_latest_entry(link, &fwdinfo);

            bundle->custody_timers()->push_back(
                new CustodyTimer(fwdinfo.timestamp(),
                                 fwdinfo.custody_spec(),
                                 bundle, link));
        }

        bundle->fwdlog()->update(link, ForwardingInfo::TRANSMIT_FAILED);
        return;
    }


    stats_.transmitted_bundles_++;
    
    link->stats()->bundles_transmitted_++;
    link->stats()->bytes_transmitted_ += event->bytes_sent_;

#ifdef BD_LOG_DEBUG_ENABLED
    log_info("BUNDLE_TRANSMITTED id:%" PRIbid " (%u bytes_sent/%u reliable) -> %s (%s)",
             bundle->bundleid(),
             event->bytes_sent_,
             event->reliably_sent_,
             link->name(),
             link->nexthop());
	s10_bundle(S10_TX,bundle,link->nexthop(),0,0,NULL,NULL);
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
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("trying to delete xmit blocks for bundle id:%" PRIbid " on link %s",
                  bundle->bundleid(),link->name());
#endif
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
#ifdef BD_LOG_DEBUG_ENABLED
        oasys::StringBuffer buf;
        bundle->fwdlog()->dump(&buf);
        log_debug("%s",buf.c_str());
#endif
    }
    ASSERTF(ok, "no forwarding log entry for transmission");
    // ASSERT(fwdinfo.state() == ForwardingInfo::QUEUED);
    if (fwdinfo.state() != ForwardingInfo::QUEUED) {
        log_err("*%p fwdinfo state %s != expected QUEUED on link %s",
                bundle, ForwardingInfo::state_to_str(fwdinfo.state()),
                link->name());
    }
    
    /*
     * Update the forwarding log indicating that the bundle is no
     * longer in flight.
     */
#ifdef BD_LOG_DEBUG_ENABLED
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
    if (link->is_reliable()) {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 blocks,
                                                 event->reliably_sent_);
    } else {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 blocks,
                                                 event->bytes_sent_);
    }

    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("trying to delete xmit blocks for bundle id:%" PRIbid " on link %s",
              bundle->bundleid(),link->name());
#endif
    BundleProtocol::delete_blocks(bundle, link);
    blocks = NULL;

    /*
     * Generate the forwarding status report if requested
     */
    if (bundle->forward_rcpt()) {
        BundleDaemon::instance()->generate_status_report(bundle, BundleStatusReport::STATUS_FORWARDED);
    }
    
    /*
     * Schedule a custody timer if we have custody.
     */
    if (bundle->local_custody()) {
        bundle->custody_timers()->push_back(
            new CustodyTimer(fwdinfo.timestamp(),
                             fwdinfo.custody_spec(),
                             bundle, link));
        
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
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_debug("event handlers completed for (%p) %s", event, event->type_str());
#endif
    
    /**
     * Once bundle reception, transmission or delivery has been
     * processed by the router, pass the event to the main BundleDaemon
     * to see if it can be deleted.
     */
    if (event->type_ == BUNDLE_RECEIVED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        BundleReceivedEvent* bre = dynamic_cast<BundleReceivedEvent*>(event);
        BundleReceivedEvent* ev = 
            new BundleReceivedEvent_MainProcessor(bre->bundleref_.object(),
                                                  (event_source_t)bre->source_,
                                                  bre->registration_);
        ev->bytes_received_ = bre->bytes_received_;
        ev->prevhop_ = bre->prevhop_;
        ev->registration_ = bre->registration_;
        ev->link_ = bre->link_.object();
        ev->duplicate_ = bre->duplicate_;
        ev->daemon_only_ = true;

        BundleDaemon::post(ev);

    } else if (event->type_ == BUNDLE_TRANSMITTED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        BundleTransmittedEvent* bte = dynamic_cast<BundleTransmittedEvent*>(event);
        BundleTransmittedEvent* ev = 
            new BundleTransmittedEvent_MainProcessor(bte->bundleref_.object(),
                        bte->contact_, bte->link_, bte->bytes_sent_, bte->reliably_sent_, bte->success_);
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

    daemon_ = BundleDaemon::instance();
    all_bundles_ = daemon_->all_bundles();
    pending_bundles_ = daemon_->pending_bundles();
    custody_bundles_ = daemon_->custody_bundles();

#ifdef BPQ_ENABLED
    bpq_cache_ = daemon_->bpq_cache();
#endif /* BPQ_ENABLED */

    contactmgr_ = daemon_->contactmgr();
    fragmentmgr_ = daemon_->fragmentmgr();
    router_ = daemon_->router();
    reg_table_ = daemon_->reg_table();

    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 1000;

    while (1) {
        if (should_stop()) {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug("BundleDaemonOutput: stopping - event queue size: %zu",
                      eventq_->size());
#endif
            break;
        }

#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, 
                    "BundleDaemonOutput: checking eventq_->size() > 0, its size is %zu", 
                    eventq_->size());
#endif

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            oasys::Time now;
            now.get_time();

            if (now >= event->posted_time_) {
                oasys::Time in_queue;
                in_queue = now - event->posted_time_;
                if (in_queue.sec_ > 2) {
                    log_warn_p(LOOP_LOG, "event %s was in queue for %u.%u seconds",
                               event->type_str(), in_queue.sec_, in_queue.usec_);
                }
            } else {
                log_warn_p(LOOP_LOG, "time moved backwards: "
                           "now %u.%u, event posted_time %u.%u",
                           now.sec_, now.usec_,
                           event->posted_time_.sec_, event->posted_time_.usec_);
            }
            
            
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "BundleDaemonOutput: handling event %s",
                        event->type_str());
#endif
            // handle the event
            handle_event(event);

            int elapsed = now.elapsed_ms();
            if (elapsed > 2000) {
                log_warn_p(LOOP_LOG, "event %s took %u ms to process",
                           event->type_str(), elapsed);
            }

            // record the last event time
            last_event_.get_time();

#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "BundleDaemonOutput: deleting event %s",
                        event->type_str());
#endif
            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, "BundleDaemonOutput: poll_multiple waiting for %d ms", 
                    timeout);
#endif
        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, "poll returned %d", cc);
#endif

        if (cc == oasys::IOTIMEOUT) {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "poll timeout");
#endif
            continue;

        } else if (cc <= 0) {
            log_err_p(LOOP_LOG, "unexpected return %d from poll_multiple!", cc);
            continue;
        }

        // if the event poll fired, we just go back to the top of the
        // loop to drain the queue
        if (event_poll->revents != 0) {
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "poll returned new event to handle");
#endif
        }
    }
}

} // namespace dtn
