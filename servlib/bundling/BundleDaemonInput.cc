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
#include "BundleDaemon.h"
#include "BundleDaemonInput.h"
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
#undef BDINPUT_LOG_DEBUG_ENABLED
//#define BDINPUT_LOG_DEBUG_ENABLED 1


//dzdebug namespace oasys {
//dzdebug    template <> dtn::BundleDaemonInput* oasys::Singleton<dtn::BundleDaemonInput, false>::instance_ = NULL;
//dzdebug}

namespace dtn {

//----------------------------------------------------------------------
BundleDaemonInput::BundleDaemonInput(BundleDaemon* parent)
    : BundleEventHandler("BundleDaemonInput", "/dtn/bundle/daemon/input"),
      Thread("BundleDaemonInput")
{
    daemon_ = parent;

    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();

}

//----------------------------------------------------------------------
BundleDaemonInput::~BundleDaemonInput()
{
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemonInput::shutdown()
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
BundleDaemonInput::start_delayed(u_int32_t millisecs)
{
    delayed_start_millisecs_ = millisecs;
    start();
}

//----------------------------------------------------------------------
void
BundleDaemonInput::post_event(BundleEvent* event, bool at_back)
{
#ifdef BDINPUT_LOG_DEBUG_ENABLED
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
#endif
    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonInput::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("BundleDaemonInput  : %zu pending_events (max: %zu) -- "
                 "%zu processed_events \n",
    	         eventq_->size(),
    	         eventq_->max_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonInput::reset_stats()
{
    stats_.clear();
}

//----------------------------------------------------------------------
void
BundleDaemonInput::cancel_custody_timers(Bundle* bundle)
{
    daemon_->cancel_custody_timers(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemonInput::release_custody(Bundle* bundle)
{
    // This will probably never get called because bundles
    // deleted in this thread are just arriving and should not
    // have local custody
    daemon_->release_custody(bundle);
}

//----------------------------------------------------------------------
bool
BundleDaemonInput::check_local_delivery(Bundle* bundle, bool deliver)
{
    return daemon_->check_local_delivery(bundle, deliver);
}

//----------------------------------------------------------------------
void
BundleDaemonInput::handle_bundle_accept(BundleAcceptRequest* request)
{
    Bundle* bundle = request->bundle_.object();

    bool result =
        router_->accept_bundle(bundle, request->reason_);

    *request->result_ = result;
    if ( !*request->result_ ) {
        ++stats_.rejected_bundles_;
    } else {
        BundleStore::instance()->reserve_payload_space(bundle);
    }


    log_info("BUNDLE_ACCEPT_REQUEST: bundle *%p %s (reason %s)",
             bundle,
             *request->result_ ? "accepted" : "not accepted",
             BundleStatusReport::reason_to_str(*request->reason_));
}
    
//----------------------------------------------------------------------
void
BundleDaemonInput::handle_bundle_received(BundleReceivedEvent* event)
{
    const BundleRef& bundleref = event->bundleref_;
    Bundle* bundle = bundleref.object();

    ++stats_.received_bundles_;

    if (bundle->is_bpv7()) {
        ++stats_.bpv7_bundles_;
    } else {
        ++stats_.bpv6_bundles_;
    }

    // update statistics and store an appropriate event descriptor
    const char* source_str = "";
    switch (event->source_) {
    case EVENTSRC_PEER:
        ++stats_.rcvd_from_peer_;
        break;
        
    case EVENTSRC_APP:
        ++stats_.rcvd_from_app_;
        source_str = " (from app)";
        break;
        
    case EVENTSRC_STORE:
        ++stats_.rcvd_from_storage_;
        source_str = " (from data store)";
        break;
        
    case EVENTSRC_ADMIN:
        ++stats_.generated_bundles_;
        source_str = " (generated)";
        break;
        
    case EVENTSRC_FRAGMENTATION:
        ++stats_.rcvd_from_frag_;
        source_str = " (from fragmentation)";
        break;

    case EVENTSRC_ROUTER:
        ++stats_.generated_bundles_;
        source_str = " (from router)";
        break;

    default:
        NOTREACHED;
    }

    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
#ifdef BDINPUT_LOG_DEBUG_ENABLED
        oasys::StaticStringBuffer<2048> buf;
        buf.appendf("BUNDLE_RECEIVED%s: prevhop %s (%" PRIu64 " bytes recvd)\n",
                    source_str, event->prevhop_.c_str(), event->bytes_received_);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_DEBUG, buf.c_str());
#endif
    } else {
        log_info("BUNDLE_RECEIVED%s *%p prevhop %s (%" PRIu64 " bytes recvd)",
                 source_str, bundle, event->prevhop_.c_str(), event->bytes_received_);
    }


    bool ok_to_route = true;


    // XXX/kscott Logging bundle reception to forwarding log moved to below.

    // log a warning if the bundle has a creation time that's in the future
    u_int32_t now = BundleTimestamp::get_current_time();
    if ((bundle->creation_ts().seconds_ > now) &&
        (bundle->creation_ts().seconds_ - now > 30000))
    {
        log_warn("bundle id %" PRIbid " arrived with creation time in the future "
                 "(%" PRIu64 " > %u)",
                 bundle->bundleid(), bundle->creation_ts().seconds_, now);
    }
   
    /*
     * If a previous hop block wasn't included, but we know the remote
     * endpoint id of the link where the bundle arrived, assign the
     * prevhop_ field in the bundle so it's available for routing.
     */
    if (event->source_ == EVENTSRC_PEER)
    {
        if (bundle->prevhop()       == EndpointID::NULL_EID() ||
            bundle->prevhop().str() == "")
        {
            bundle->mutable_prevhop()->assign(event->prevhop_);
        }

        if (bundle->prevhop() != event->prevhop_)
        {
#ifdef BDINPUT_LOG_DEBUG_ENABLED
            log_debug("previous hop mismatch: prevhop header contains '%s' but "
                      "convergence layer indicates prevhop is '%s'",
                      bundle->prevhop().c_str(),
                      event->prevhop_.c_str());
#endif
        }
    }
    
    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     */
    if (BundleDaemon::params_.reactive_frag_enabled_ && (event->source_ == EVENTSRC_PEER)) {
        ASSERT(event->bytes_received_ != 0);
        fragmentmgr_->try_to_convert_to_fragment(bundle);
    }

    /*
     * validate a bundle, including all bundle blocks, received from a peer
     */
    if (event->source_ == EVENTSRC_PEER) { 

        /*
         * Check all BlockProcessors to validate the bundle.
         */
        status_report_reason_t
            reception_reason = BundleProtocol::REASON_NO_ADDTL_INFO,
            deletion_reason = BundleProtocol::REASON_NO_ADDTL_INFO;

        
        bool valid = BundleProtocol::validate(bundle,
                                              &reception_reason,
                                              &deletion_reason);
        
        /*
         * Send the reception receipt if requested within the primary
         * block or some other error occurs that requires a reception
         * status report but may or may not require deleting the whole
         * bundle.
         */
        if (bundle->receive_rcpt() ||
            reception_reason != BundleProtocol::REASON_NO_ADDTL_INFO)
        {
            daemon_->generate_status_report(bundle, 
                         BundleStatusReport::STATUS_RECEIVED, reception_reason);
        }

        /*
         * If the bundle is valid, probe the router to see if it wants
         * to accept the bundle.
         */
        bool accept_bundle = false;
        if (valid) {
            int reason = BundleProtocol::REASON_NO_ADDTL_INFO;
            accept_bundle = router_->accept_bundle(bundle, &reason);
            deletion_reason = static_cast<BundleProtocol::status_report_reason_t>(reason);
        }

        
        /*
         * Delete a bundle if a validation error was encountered or
         * the router doesn't want to accept the bundle, in both cases
         * not giving the reception event to the router.
         */
        if (!accept_bundle) {
            ++stats_.rejected_bundles_;
            daemon_->delete_bundle(bundleref, deletion_reason);
            return;
        }
    }

    /*
     * Check if the bundle is a duplicate, i.e. shares a source id,
     * timestamp, and fragmentation information with some other bundle
     * in the system.
     */
    Bundle* duplicate = find_duplicate(bundle);

    if (duplicate != NULL) {
#ifdef BDINPUT_LOG_DEBUG_ENABLED
        log_debug("got duplicate bundle: %s  orig: %" PRIbid " dupe: %" PRIbid, 
                  bundle->gbofid_str().c_str(), duplicate->bundleid(), bundle->bundleid());
#endif


        ++stats_.duplicate_bundles_;
        
        if (bundle->custody_requested() && duplicate->local_custody())
        {
            daemon_->generate_custody_signal(bundle, false,
                                    BundleProtocol::CUSTODY_REDUNDANT_RECEPTION);
        }

        if (BundleDaemon::params_.suppress_duplicates_) {
            // since we don't want the bundle to be processed by the rest
            // of the system, we mark the event as daemon_only (meaning it
            // won't be forwarded to routers) and return, which should
            // eventually remove all references on the bundle and then it
            // will be deleted

            //XXX/dz TODO - inform ExternalRouter of duplicate being deleted

            --stats_.rcvd_from_peer_;

            status_report_reason_t deletion_reason = BundleProtocol::REASON_NO_ADDTL_INFO;
            daemon_->delete_bundle(bundleref, deletion_reason);
            return;
        }

        // The BP says that the "dispatch pending" retention constraint
        // must be removed from this bundle if there is a duplicate we
        // currently have custody of. This would cause the bundle to have
        // no retention constraints and it now "may" be discarded. Assuming
        // this means it is supposed to be discarded, we have to suppress
        // a duplicate in this situation regardless of the parameter
        // setting. We would then be relying on the custody transfer timer
        // to cause a new forwarding attempt in the case of routing loops
        // instead of the receipt of a duplicate, so in theory we can indeed
        // suppress this bundle. It may not be strictly required to do so,
        // in which case we can remove the following block.
        if (bundle->custody_requested() && duplicate->local_custody()) {
            //XXX/dz TODO - inform ExternalRouter of duplicate being deleted

            --stats_.rcvd_from_peer_;

            status_report_reason_t deletion_reason = BundleProtocol::REASON_NO_ADDTL_INFO;
            daemon_->delete_bundle(bundleref, deletion_reason);
            return;
        }

    }

    /*
     * Add the bundle to the master pending queue and the data store
     * (unless the bundle was just reread from the data store on startup)
     *
     * Note that if add_to_pending returns false, the bundle has
     * already expired so we immediately return instead of trying to
     * deliver and/or forward the bundle. Otherwise there's a chance
     * that expired bundles will persist in the network.
     *
     * add_to_pending writes the bundle to the durable store
     */
    add_to_pending(bundle, (event->source_ != EVENTSRC_STORE));

    event->duplicate_ = duplicate;


    // moved BD processing to here - just expiration timer        
    int64_t expiration_time = bundle->time_to_expiration_millis();
    if (expiration_time <= 0) {
        // bundle has already expired
        ok_to_route = false;
        expiration_time = 1000; // expire the bnudle in 1 second;


        //XXX/dz TODO - how to inform ExternalRouter of receipt of expired bundle
    }

    SPtr_ExpirationTimer exp_sptr = std::make_shared<ExpirationTimer>(bundle);
    bundle->set_expiration_timer(exp_sptr);
    exp_sptr->start(expiration_time, exp_sptr);

    if (ok_to_route) {
        // log the reception in the bundle's forwarding log
        if (event->source_ == EVENTSRC_PEER && event->link_ != NULL)
        {
            bundle->fwdlog()->add_entry(event->link_,
                                        ForwardingInfo::FORWARD_ACTION,
                                        ForwardingInfo::RECEIVED);
        }
        else if (event->source_ == EVENTSRC_APP)
        {
            if (event->registration_ != NULL) {
                bundle->fwdlog()->add_entry(event->registration_,
                                            ForwardingInfo::FORWARD_ACTION,
                                            ForwardingInfo::RECEIVED);
            }
        }
    
        /*
         * If the bundle is a custody bundle and we're configured to take
         * custody, then do so. In case the event was delivered due to a
         * reload from the data store, then if we have local custody, make
         * sure it's added to the custody bundles list.
         */
        if (bundle->local_custody()) {
            // reloading from the data base and it was already in custody
            custody_bundles_->push_back(bundle);

            // if bundle was originally accepted with ACS enabled
            // then add it to the ACS custody id list
            if (bundle->custodyid() > 0) {
                BundleDaemon::instance()->acs_accept_custody(bundle);
            }
        } 
        else if (bundle->custody_requested() && bundle->bibe_custody()) {
            // always accept custody of a BP6 BIBE bundle
            ASSERT(bundle->is_bpv6());
            BundleDaemon::instance()->accept_custody(bundle);
        }

            //XXX/dz - External router should make decision on accepting custody
            //         and send back a request to do so. If not then it is
            //         possible to accept custody here and then have the router
            //         decide to delete the bundle rather than keep it and if
            //         that happens the bundle is lost because a CS was already
            //         sent to the previous custodian where it will be deleted also.
        else if (bundle->custody_requested() 
                   && BundleDaemon::params_.accept_custody_
                   && router_->accept_custody(bundle)
                   && (event->duplicate_ == NULL || !event->duplicate_->local_custody()))
        {
            if (event->source_ != EVENTSRC_STORE) {
                BundleDaemon::instance()->accept_custody(bundle);
        
            }
        }

        /*
         * If this bundle is a duplicate and it has not been suppressed, we
         * can assume the bundle it duplicates has already been delivered or
         * added to the fragment manager if required, so do not do so again.
         * We can bounce out now.
         */
        if (event->duplicate_ != NULL) {
            return;
        }

        /*
         * Check if this is a complete (non-fragment) bundle that
         * obsoletes any fragments that we know about.
         */
        if (! bundle->is_fragment()) {
            fragmentmgr_->delete_obsoleted_fragments(bundle);
        }

        /*
         * Deliver the bundle to any local registrations that it matches,
         * unless it's generated by the router or is a bundle fragment.
         * Delivery of bundle fragments is deferred until after re-assembly.
         */
        bool is_local_delivery = false;
        if ( event->source_ == EVENTSRC_STORE ) {
            BundleDaemon::instance()->generate_delivery_events(bundle);
        } else {
            // do not actually deliver at this point (the false)
            is_local_delivery = check_local_delivery(bundle, false);
                                     //dzdebug (event->source_ != EVENTSRC_ROUTER) &&
                                     //dzdebug (bundle->is_fragment() == false));
            /*
             * Re-assemble bundle fragments that are destined to the local node.
             */
            if (event->source_ != EVENTSRC_FRAGMENTATION && bundle->is_fragment() && is_local_delivery) {
#ifdef BDINPUT_LOG_DEBUG_ENABLED
                log_debug("deferring delivery of bundle *%p "
                          "since bundle is a fragment", bundle);
#endif
                fragmentmgr_->process_for_reassembly(bundle);
            }

        }


        // dzdebug
        // XXX/dz - add configurable paramater to disable enforcing hop limit?
        if (!is_local_delivery) {
            if (bundle->hop_limit() != 0) {
                if (bundle->hop_count() >= bundle->hop_limit()) {
                    // hop count limit reached so bundle cannot be forwarded
                    // delete the bundle...
                    ++stats_.hops_exceeded_;
                    status_report_reason_t deletion_reason = BundleProtocol::REASON_HOP_LIMIT_EXCEEDED;
                    daemon_->delete_bundle(bundleref, deletion_reason);
                    event->daemon_only_ = true;

                    //XXX/dz - how to accont for this in EHSRouter???

                    return;
                }
            }
        }


        event->duplicate_ = duplicate;

        if (event->processed_notifier_) {
            event->processed_notifier_->notify();
            event->processed_notifier_ = NULL;
        }



        /*
         * Inform the router so it can do something further
         * with the bundle in response to the event before we check to 
         * see if it needs to be delivered locally.
         */
        router_->handle_event(event);


        // now deliver the bundle
        if (is_local_delivery && !bundle->is_fragment() && (event->source_ != EVENTSRC_ROUTER)) {
            check_local_delivery(bundle, true);
        }


        /*
         * Generate an event for the Output thread to pass it to the router(s)
         * for additional processing before we check to see if it needs to
         * be delivered locally.
         */
        BundleReceivedEvent* ev = 
            new BundleReceivedEvent_MainProcessor(bundle,
                                                 (event_source_t)event->source_);

        BundleDaemon::post(ev);

    } else {
        /*
         * Inform the router so it can do something further
         * with the bundle in response to the event before we check to 
         * see if it needs to be delivered locally.
         */
        router_->handle_event(event);
    }

}
    
//----------------------------------------------------------------------
void
BundleDaemonInput::handle_bundle_inject(BundleInjectRequest* event)
{
    // link isn't used at the moment, so don't bother searching for
    // it.  TODO:  either remove link ID and forward action from
    // RequestInjectBundle, or make link ID optional and send the
    // bundle on the link if specified.
    /*
      LinkRef link = contactmgr_->find_link(event->link_.c_str());
      if (link == NULL) return;
    */

    EndpointID src(event->src_); 
    EndpointID dest(event->dest_); 
    if ((! src.valid()) || (! dest.valid())) return;
    
    // The bundle's source EID must be either dtn:none or an EID 
    // registered at this node.
    const RegistrationTable* reg_table = 
            BundleDaemon::instance()->reg_table();
    std::string base_reg_str = src.uri().scheme() + "://" + src.uri().host();
    
    if (!reg_table->get(EndpointIDPattern(base_reg_str)) && 
         src != EndpointID::NULL_EID()) {
        log_err("this node is not a member of the injected bundle's source "
                "EID (%s)", src.str().c_str());
        return;
    }
    
    // The new bundle is placed on the pending queue but not
    // in durable storage (no call to BundleActions::inject_bundle)
    Bundle* bundle = new Bundle(BundleDaemon::params_.injected_bundles_in_memory_ ? 
                                BundlePayload::MEMORY : BundlePayload::DISK);
    
    bundle->set_source(src);
    bundle->mutable_dest()->assign(dest);
    
    if (! bundle->mutable_replyto()->assign(event->replyto_))
        bundle->mutable_replyto()->assign(EndpointID::NULL_EID());

    if (! bundle->mutable_custodian()->assign(event->custodian_))
        bundle->mutable_custodian()->assign(EndpointID::NULL_EID()); 

    // bundle COS defaults to COS_BULK
    bundle->set_priority(event->priority_);

    // bundle expiration on remote dtn nodes
    // defaults to 5 minutes
    if(event->expiration_ == 0)
        bundle->set_expiration_secs(300);
    else
        bundle->set_expiration_secs(event->expiration_);
    
    // set the payload (by hard linking, then removing original)
    bundle->mutable_payload()->
        replace_with_file(event->payload_file_.c_str());
#ifdef BDINPUT_LOG_DEBUG_ENABLED
    log_debug("bundle payload size after replace_with_file(): %zd", 
              bundle->payload().length());
#endif
    oasys::IO::unlink(event->payload_file_.c_str(), logpath_);

    /*
     * Deliver the bundle to any local registrations that it matches,
     * unless it's generated by the router or is a bundle fragment.
     * Delivery of bundle fragments is deferred until after re-assembly.
     */
    bool is_local = check_local_delivery(bundle, !bundle->is_fragment());
    
    /*
     * Re-assemble bundle fragments that are destined to the local node.
     */
    if (bundle->is_fragment() && is_local) {
#ifdef BDINPUT_LOG_DEBUG_ENABLED
        log_debug("deferring delivery of injected bundle *%p "
                  "since bundle is a fragment", bundle);
#endif
        fragmentmgr_->process_for_reassembly(bundle);
    }

    // The injected bundle is no longer sent automatically. It is
    // instead added to the pending queue so that it can be resent
    // or sent on multiple links.

    // If add_to_pending returns false, the bundle has already expired
    if (add_to_pending(bundle, 0))
        BundleDaemon::post(new BundleInjectedEvent(bundle, event->request_id_));
    
    ++stats_.injected_bundles_;
    ++stats_.received_bundles_;
}

//----------------------------------------------------------------------
void
BundleDaemonInput::handle_reassembly_completed(ReassemblyCompletedEvent* event)
{
    log_info("REASSEMBLY_COMPLETED bundle id %" PRIbid,
             event->bundle_->bundleid());

    // remove all the fragments from the pending list
    BundleRef ref("BundleDaemonInput::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        daemon_->delete_bundle(ref);
    }

    // post a new event for the newly reassembled bundle
    post_event(new BundleReceivedEvent(event->bundle_.object(),
                                             EVENTSRC_FRAGMENTATION), 
               false);
}



//----------------------------------------------------------------------
void
BundleDaemonInput::event_handlers_completed(BundleEvent* event)
{
    (void) event;
}

//----------------------------------------------------------------------
bool
BundleDaemonInput::add_to_pending(Bundle* bundle, bool add_to_store)
{
    // This is only called by the input thread but keeping updating
    // the pending and dupefinder lists in the BundleDaemon in
    // case we need to control access with a lock

    return daemon_->add_to_pending(bundle, add_to_store);
}

//----------------------------------------------------------------------
bool
BundleDaemonInput::delete_from_pending(const BundleRef& bundle)
{
    return daemon_->delete_from_pending(bundle);
}

//----------------------------------------------------------------------
Bundle*
BundleDaemonInput::find_duplicate(Bundle* bundle)
{
    // This is only called by the input thread but keeping updating
    // the pending and dupefinder lists in the BundleDaemon in
    // case we need to control access with a lock

    return daemon_->find_duplicate(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemonInput::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemonInput::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
   
    // only check to see if bundle can be deleted if event is daemon_only
    if (event->daemon_only_) {
        event_handlers_completed(event);
    }

    ++stats_.events_processed_;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonInput::run()
{
    static const char* LOOP_LOG = "/dtn/bundle/daemon/input/loop";
    
    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }

    char threadname[16] = "BD-Input";
    pthread_setname_np(pthread_self(), threadname);
   

    if (delayed_start_millisecs_ > 0) {
        log_always("Delaying start for %u millseconds", delayed_start_millisecs_);
        usleep(delayed_start_millisecs_ * 1000);
        log_always("Starting processing in the BundleDaemonInput thread");
    }

    BundleEvent* event;

    custody_bundles_ = daemon_->custody_bundles();

    fragmentmgr_ = daemon_->fragmentmgr();
    router_ = daemon_->router();

    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 1000;

    while (1) {
        if (should_stop()) {
            if (0 == eventq_->size()) {
                log_always("BDInput - should stop is true  - exiting main loop");
                break;
            }
            log_always("BDInput - should stop is true but processing eventq of size: %zu", eventq_->size());
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
