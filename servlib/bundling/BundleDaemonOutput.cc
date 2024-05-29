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
    contactmgr_ = nullptr;
    fragmentmgr_ = nullptr;
    delayed_start_millisecs_ = 0;
    
    memset(&stats_, 0, sizeof(stats_));

    actions_ = new BundleActions();
}

//----------------------------------------------------------------------
BundleDaemonOutput::~BundleDaemonOutput()
{
    delete actions_;
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::shutdown()
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
BundleDaemonOutput::start_delayed(u_int32_t millisecs)
{
    delayed_start_millisecs_ = millisecs;
    start();
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post_event(SPtr_BundleEvent& sptr_event, bool at_back)
{
#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_debug("posting event (%p) with type %s (at %s)",
              sptr_event.get(), sptr_event->type_str(), at_back ? "back" : "head");
#endif
    sptr_event->posted_time_.get_time();
    me_eventq_.push(sptr_event, at_back);
}


//----------------------------------------------------------------------
void
BundleDaemonOutput::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("BundleDaemonOutput : %zu pending_events (max: %zu) -- "
                 "%zu processed_events \n",
    	         me_eventq_.size(),
    	         me_eventq_.max_size(),
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
BundleDaemonOutput::handle_bundle_injected(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // Nothing to do here - just passing through to the router to decide next action
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_send(SPtr_BundleEvent& sptr_event)
{
    BundleSendRequest* event = nullptr;
    event = dynamic_cast<BundleSendRequest*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleSendRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    BundleRef br = event->bundle_;

    if (! br.object()){
        log_err("nullptr bundle object in BundleSendRequest");
        return;
    }

    LinkRef link = contactmgr_->find_link(event->link_.c_str());
    if (link == nullptr){
        if (!BundleDaemon::shutting_down()) {

#ifdef BARD_ENABLED
            if (event->restage_) {
                // unable to send the bundle to the restage link so re-issue a 
                // BundleReceived event after clearing the restage link name
                br->clear_bard_restage_link_name();

                BundleReceivedEvent* event_to_post;
                event_to_post = new BundleReceivedEvent(br.object(), 
                                                        EVENTSRC_RESTAGE, 
                                                        br->payload().length(), 
                                                        BD_MAKE_EID_NULL());
                SPtr_BundleEvent sptr_event_to_post(event_to_post);
                BundleDaemon::post(sptr_event_to_post);
            } else {
                log_err("Cannot send bundle on unknown link %s", event->link_.c_str()); 
            }
#else
            log_err("Cannot send bundle on unknown link %s", event->link_.c_str()); 
            
#endif // BARD_ENABLED
        }
        return;
    }


    ForwardingInfo::action_t fwd_action =
        (ForwardingInfo::action_t)event->action_;

    actions_->queue_bundle(br.object(), link,
        fwd_action, CustodyTimerSpec::defaults_);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_transmitted(SPtr_BundleEvent& sptr_event)
{
    BundleTransmittedEvent* event = nullptr;
    event = dynamic_cast<BundleTransmittedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleTransmittedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != nullptr);

    SPtr_BlockInfoVec sptr_blocks;

    if (!event->blocks_deleted_)
    {
        sptr_blocks = bundle->xmit_blocks()->find_blocks(link);
    
        // Because a CL is running in another thread or process (External CLs),
        // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
        // If an event about a bundle bound for particular link is posted after another,
        // which it might contradict, the BundleDaemon need not reprocess the event.
        // The router (DP) might, however, be interested in the new status of the send.
        if (sptr_blocks == nullptr)
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

        if (bundle->dest()->is_imc_scheme()) {
            // remove the destination nodes for this link
            bundle->imc_link_transmit_failure(link->name_str());
        }
        return;
    } else {
        if (bundle->dest()->is_imc_scheme()) {
            // move the destination nodes for this link to the handled list
            bundle->imc_link_transmit_success(link->name_str());

            if (!BundleDaemon::params_.persistent_fwd_logs_) {
                // if not configured to store the forwarding logs then update
                // the data store here; otherwise, it will be done later
                // when the forwarding log is updated
                daemon_->actions()->store_update(bundle);
            }
        }
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
    if (sptr_blocks != nullptr) {
        if (link->is_reliable()) {
            fragmentmgr_->try_to_reactively_fragment(bundle,
                                                     sptr_blocks.get(),
                                                     event->reliably_sent_);
        } else {
            fragmentmgr_->try_to_reactively_fragment(bundle,
                                                     sptr_blocks.get(),
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


//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_restaged(SPtr_BundleEvent& sptr_event)
{
    BundleRestagedEvent* event = nullptr;
    event = dynamic_cast<BundleRestagedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleRestagedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }


#ifdef BARD_ENABLED
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != nullptr);

    SPtr_BlockInfoVec sptr_blocks;

    if (!event->blocks_deleted_)
    {
        sptr_blocks = bundle->xmit_blocks()->find_blocks(link);
    
        // Because a CL is running in another thread or process (External CLs),
        // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
        // If an event about a bundle bound for particular link is posted after another,
        // which it might contradict, the BundleDaemon need not reprocess the event.
        // The router (DP) might, however, be interested in the new status of the send.
        if (sptr_blocks == nullptr)
        {
            log_info("received a redundant/conflicting bundle_restaged event about "
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
        log_warn("bundle id:%" PRIbid " unexpectedly on link %s queue in restaged event",
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



/***
        // can it be restaged to a pooled instance instead?
        std::string old_restage_link_name = bundle->bard_restage_link_name();

        bundle->clear_bard_restage_link_name();

        bool space_reserved = false;  // an output not an input
        uint64_t prev_amount_reserved = 0;


        // TODO: need to pass in link name that failed and get a new one...

        daemon_->query_accept_bundle_after_failed_restage(bundle);

        if (0 == old_restage_link_name.compare(bundle->bard_restage_link_name())) {
            // just trying to send it back to the same link so 
            // nix that and let it go into internal storage
            bundle->clear_bard_restage_link_name();
        }

***/


        daemon_->query_accept_bundle_after_failed_restage(bundle);


        BundleReceivedEvent* event_to_post;
        event_to_post = new BundleReceivedEvent(bundle,
                                                EVENTSRC_RESTAGE, 
                                                bundle->payload().length(), 
                                                BD_MAKE_EID_NULL());
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
        return;
    } else {
//dzdebug - moved to RestageCL        daemon_->bundle_restaged(bundle, event->disk_usage_);  // have the BD inform the BARD
    }


    stats_.transmitted_bundles_++;
    
    link->stats()->bundles_transmitted_++;
    link->stats()->bytes_transmitted_ += event->disk_usage_;

#ifdef BDOUTPUT_LOG_DEBUG_ENABLED
    log_info("BUNDLE_RESTAGED id:%" PRIbid " (%u disk_usage) -> %s (%s)",
             bundle->bundleid(),
             event->disk_usage_,
             link->name(),
             link->nexthop());
#endif


    /*
     * Grab the latest forwarding log state so we can find the custody
     * timer information (if any).
     */
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog()->get_latest_entry(link, &fwdinfo);

    ASSERTF(ok, "no forwarding log entry for transmission");
    if (fwdinfo.state() != ForwardingInfo::QUEUED) {
        log_err("*%p fwdinfo state %s != expected QUEUED on link %s in BundleRestagedEvent processing",
                bundle, ForwardingInfo::state_to_str(fwdinfo.state()),
                link->name());
    }
    
    /*
     * Update the forwarding log indicating that the bundle is no
     * longer in flight.
     */
    bundle->fwdlog()->update(link, ForwardingInfo::TRANSMITTED);
                            
    // just in case - no harm if there are no blocks
    BundleProtocol::delete_blocks(bundle, link);

#endif //BARD_ENABLED
}




//----------------------------------------------------------------------
void 
BundleDaemonOutput::handle_link_cancel_all_bundles_request(SPtr_BundleEvent& sptr_event)
{
    LinkCancelAllBundlesRequest* event = nullptr;
    event = dynamic_cast<LinkCancelAllBundlesRequest*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkCancelAllBundlesRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);
    
    link->cancel_all_bundles();
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::event_handlers_completed(SPtr_BundleEvent& sptr_event)
{
    /**
     * Once transmission or delivery has been
     * processed by the router, pass the event to the main BundleDaemon
     * to see if it can be deleted.
     */
    if (sptr_event->type_ == BUNDLE_TRANSMITTED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        //dzdebug BundleTransmittedEvent* bte = nullptr;
        //dzdebug bte = dynamic_cast<BundleTransmittedEvent*>(sptr_event.get());
        //dzdebug if (bte == nullptr) {
        //dzdebug     log_err("Error casting event type %s as a BundleTransmittedEvent", 
        //dzdebug             event_to_str(sptr_event->type_));
        //dzdebug     return;
        //dzdebug }

        //dzdebug BundleTransmittedEvent* ev = 
        //dzdebug     new BundleTransmittedEvent_MainProcessor(bte->bundleref_.object(),
        //dzdebug                 bte->contact_, bte->link_, bte->bytes_sent_, bte->reliably_sent_, 
        //dzdebug                 bte->success_, bte->blocks_deleted_);
        //dzdebug ev->daemon_only_ = true;
        //dzdebug BundleDaemon::post(ev);

        // already passed this event to the router so can change it to daemon_only for the BD-Main
        sptr_event->event_processor_ = EVENT_PROCESSOR_MAIN;
        sptr_event->daemon_only_ = true;
        BundleDaemon::post(sptr_event);

    } else if (sptr_event->type_ == BUNDLE_RESTAGED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        //dzdebug BundleRestagedEvent* bre = nullptr;
        //dzdebug bre = dynamic_cast<BundleRestagedEvent*>(sptr_event.get());
        //dzdebug if (bre == nullptr) {
        //dzdebug     log_err("Error casting event type %s as a BundleRestagedEvent", 
        //dzdebug             event_to_str(sptr_event->type_));
        //dzdebug     return;
        //dzdebug }

        //dzdebug BundleRestagedEvent* ev = 
        //dzdebug     new BundleRestagedEvent_MainProcessor(bre->bundleref_.object(),
        //dzdebug                 bre->contact_, bre->link_, bre->disk_usage_,
        //dzdebug                 bre->success_, bre->blocks_deleted_);
        //dzdebug ev->daemon_only_ = true;

        //dzdebug BundleDaemon::post(ev);

        // already passed this event to the router so can change it to daemon_only for the BD-Main
        sptr_event->event_processor_ = EVENT_PROCESSOR_MAIN;
        sptr_event->daemon_only_ = true;
        BundleDaemon::post(sptr_event);

    //dzdebug } else if (sptr_event->type_ == BUNDLE_INJECTED) {
        //dzdebug  -  would have already passed to the router which doesn't process it 
        //dzdebug  -  so why send it to BD-Main which doesn't process it either??
        //dzdebug BundleInjectedEvent* bie = nullptr;
        //dzdebug bie = dynamic_cast<BundleInjectedEvent*>(sptr_event.get());
        //dzdebug if (bie == nullptr) {
        //dzdebug     log_err("Error casting event type %s as a BundleInjectedEvent", 
        //dzdebug             event_to_str(sptr_event->type_));
        //dzdebug     return;
        //dzdebug }

        //dzdebug BundleInjectedEvent* ev = 
        //dzdebug     new BundleInjectedEvent_MainProcessor(bie->bundleref_.object(),
        //dzdebug                                           bie->request_id_);
        //dzdebug BundleDaemon::post(ev);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
   
    if (! sptr_event->daemon_only_) {
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(sptr_event);
        contactmgr_->handle_event(sptr_event);
    }

    // only check to see if bundle can be deleted if event is daemon_only
    event_handlers_completed(sptr_event);

    stats_.events_processed_++;

    if (sptr_event->processed_notifier_) {
        sptr_event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::run()
{
    char threadname[16] = "BD-Output";
    pthread_setname_np(pthread_self(), threadname);
   
    if (delayed_start_millisecs_ > 0) {
        log_always("Delaying start for %u millseconds", delayed_start_millisecs_);
        usleep(delayed_start_millisecs_ * 1000);
        log_always("Starting processing in the BundleDaemonOutput thread");
    }

    SPtr_BundleEvent sptr_event;

    contactmgr_ = daemon_->contactmgr();
    fragmentmgr_ = daemon_->fragmentmgr();
    router_ = daemon_->router();

    while (1) {
        if (should_stop()) {
            break;
        }

        if (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);
            
            // handle the event
            handle_event(sptr_event);

            // clean up the event
            sptr_event.reset();
            
            continue; // no reason to poll
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

} // namespace dtn
