/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include <sys/time.h>
#include <time.h>
//#ifndef CLOCK_MONOTONIC_RAW  -- clock_nanosleep does not support _RAW yet
//#    define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
//#endif

#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/tclcmd/TclCommand.h>
#include <third_party/oasys/util/Time.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleDaemonACS.h"
#include "BundleDaemonInput.h"
#include "BundleDaemonOutput.h"
#include "BundleDaemonStorage.h"
#include "BundleDaemonCleanup.h"
#include "BundleProtocolVersion7.h"
#include "BundleArchitecturalRestagingDaemon.h"
#include "BundleStatusReport.h"
#include "BundleTimestamp.h"
#include "CustodySignal.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "contacts/Link.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "contacts/InterfaceTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include "ltp/LTPEngine.h"
#include "naming/IPNScheme.h"
#include "reg/AdminRegistration.h"
#include "reg/AdminRegistrationIpn.h"
#include "reg/APIRegistration.h"
#include "reg/IMCGroupPetitionRegistration.h"
#include "reg/IonContactPlanSyncRegistration.h"
#include "reg/PingRegistration.h"
#include "reg/Registration.h"
#include "reg/RegistrationInitialLoadThread.h"
#include "reg/RegistrationTable.h"
#include "reg/IpnEchoRegistration.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "storage/BundleStore.h"
#include "storage/RegistrationStore.h"
#include "storage/LinkStore.h"

#include "naming/EndpointIDPool.h"
#include "naming/SchemeTable.h"
#include "naming/DTNScheme.h"
#include "naming/IPNScheme.h"
#include "naming/WildcardScheme.h"



#ifdef DTPC_ENABLED
#  include "dtpc/DtpcDaemon.h"
#endif // DTPC_ENABLED

// enable or disable debug level logging in this file
#undef BD_LOG_DEBUG_ENABLED
//#define BD_LOG_DEBUG_ENABLED 1





namespace oasys {
    template <> dtn::BundleDaemon* oasys::Singleton<dtn::BundleDaemon, false>::instance_ = nullptr;
}

namespace dtn {

BundleDaemon::Params BundleDaemon::params_;


bool BundleDaemon::shutting_down_ = false;
bool BundleDaemon::final_cleanup_ = false;
bool BundleDaemon::user_initiated_shutdown_ = false;

//----------------------------------------------------------------------
BundleDaemon::BundleDaemon()
    : BundleEventHandler("BundleDaemon", "/dtn/bundle/daemon"),
      Thread("BundleDaemon"),
      load_previous_links_executed_(false)
{
    // default local eids
    qptr_eid_pool_  = std::unique_ptr<EndpointIDPool>(new EndpointIDPool());
    sptr_local_eid_ = make_eid_null();
    sptr_local_eid_ipn_ = make_eid_null();

    actions_ = nullptr;
    
    memset(&stats_, 0, sizeof(stats_));

    all_bundles_        = new all_bundles_t("all_bundles");
    deleting_bundles_   = new all_bundles_t("deleting_bundles");
    pending_bundles_    = new pending_bundles_t("pending_bundles");
    custody_bundles_    = new custody_bundles_t("custody_bundles");
    dupefinder_bundles_ = new dupefinder_bundles_t("dupefinder_bundles");

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();
    reg_table_ = new RegistrationTable();

    daemon_input_   = std::unique_ptr<BundleDaemonInput>(new BundleDaemonInput(this));
    daemon_storage_ = std::unique_ptr<BundleDaemonStorage>(new BundleDaemonStorage(this));
    daemon_acs_     = std::unique_ptr<BundleDaemonACS>(new BundleDaemonACS());
    daemon_output_  = std::unique_ptr<BundleDaemonOutput>(new BundleDaemonOutput(this));
    daemon_cleanup_ = std::unique_ptr<BundleDaemonCleanup>(new BundleDaemonCleanup(this));


#ifdef BARD_ENABLED
    bundle_restaging_daemon_ = std::make_shared<BundleArchitecturalRestagingDaemon>();
#endif  // BARD_ENABLED

    router_ = nullptr;

    app_shutdown_proc_ = nullptr;
    app_shutdown_data_ = nullptr;

    rtr_shutdown_proc_ = 0;
    rtr_shutdown_data_ = 0;
}

//----------------------------------------------------------------------
BundleDaemon::~BundleDaemon()
{
}

//----------------------------------------------------------------------
void
BundleDaemon::cleanup_allocations()
{
    while (!is_stopped()) {
        usleep(100000);
    }

    final_cleanup_ = true;

    bibe_extractor_.reset();
    ltp_engine_.reset();

#ifdef BARD_ENABLED
    bundle_restaging_daemon_.reset();
#endif  // BARD_ENABLED


    daemon_input_.reset();
    daemon_output_.reset();
    daemon_cleanup_.reset();

    daemon_acs_.reset();

#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->set_should_stop();
#endif // DTPC_ENABLED


    daemon_storage_.reset();

    delete contactmgr_;
    delete fragmentmgr_;

    log_always("Calling BundleRouter.shutdown");
    router_->shutdown();
    delete router_;

    delete actions_;

    delete pending_bundles_;
    delete custody_bundles_;
    delete dupefinder_bundles_;
    delete all_bundles_;
    delete deleting_bundles_;

    delete reg_table_;

    BundleProtocolVersion6::delete_block_processors();
    BundleProtocolVersion7::delete_block_processors();
}

//----------------------------------------------------------------------
void 
BundleDaemon::set_console_info(in_addr_t console_addr, u_int16_t console_port, bool console_stdio)
{
    params_.console_addr_ = console_addr;
    params_.console_port_ = console_port;
    params_.console_stdio_ = console_stdio;
}

//----------------------------------------------------------------------
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    BundleProtocol::init_default_processors();
}

//----------------------------------------------------------------------
void
BundleDaemon::set_local_eid(const char* eid_str) 
{
    sptr_local_eid_ = make_eid(eid_str);
}

//----------------------------------------------------------------------
void
BundleDaemon::set_local_eid_ipn(const char* eid_str) 
{
    sptr_local_eid_ipn_ = make_eid(eid_str);
}

//----------------------------------------------------------------------
void
BundleDaemon::init()
{       
    if (instance_ != nullptr) 
    {
        PANIC("BundleDaemon already initialized");
    }

    instance_ = new BundleDaemon();     
    instance_->do_init();
}

//----------------------------------------------------------------------
void
BundleDaemon::post(SPtr_BundleEvent& sptr_event)
{
    instance_->post_event(sptr_event, true);   // at_back = true
}

//----------------------------------------------------------------------
void
BundleDaemon::post_at_head(SPtr_BundleEvent& sptr_event)
{
    instance_->post_event(sptr_event, false);  // at_back = false
}

//----------------------------------------------------------------------
bool
BundleDaemon::post_and_wait(SPtr_BundleEvent& sptr_event,
                            oasys::Notifier* notifier,
                            int timeout, bool at_back)
{
    /*
     * Make sure that we're either already started up or are about to
     * start. Otherwise the wait call below would block indefinitely.
     */
    ASSERT(! oasys::Thread::start_barrier_enabled());

    ASSERT(sptr_event->processed_notifier_ == nullptr);
    sptr_event->processed_notifier_ = notifier;
    instance_->post_event(sptr_event, at_back);
    return notifier->wait(nullptr, timeout);
}

//----------------------------------------------------------------------
void
BundleDaemon::post_event(SPtr_BundleEvent& sptr_event, bool at_back)
{
    if (final_cleanup_) {
        // clearing BundleLists trigger events that need to be intercepted while shutting down
        return;
    }

    switch (sptr_event->event_processor_) {
        case EVENT_PROCESSOR_INPUT:
        {
            daemon_input_->post_event(sptr_event, at_back);
        }
        break;

        case EVENT_PROCESSOR_OUTPUT:
        {
            daemon_output_->post_event(sptr_event, at_back);
        }
        break;

        case EVENT_PROCESSOR_STORAGE:
        {
            daemon_storage_->post_event(sptr_event, at_back);
        }
        break;

        case EVENT_PROCESSOR_CLEANUP:
        {
            daemon_cleanup_->post_event(sptr_event, at_back);
        }
        break;

#ifdef DTPC_ENABLED
        case EVENT_PROCESSOR_DTPC:
        {
            DtpcDaemon* dtpc_daemon = DtpcDaemon::instance();
            dtpc_daemon->post_event(sptr_event, at_back);
        }
        break;
#endif

        default:
        {
            sptr_event->posted_time_.get_time();
            me_eventq_.push(sptr_event, at_back);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::get_routing_state(oasys::StringBuffer* buf)
{
    router_->get_routing_state(buf);
    contactmgr_->dump(buf);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_bundle_stats(oasys::StringBuffer* buf)
{
    buf->appendf("Bundle Statistics: \n");
    buf->appendf("received: %zu"
                 "  (from_peer: %zu"
                 "  from_apps: %zu"
                 "  generated:  %zu"
                 "  from_frag:  %zu"
                 "  injected:  %zu"
                 "  from_storage:  %zu"
                 "  from_restage:  %zu"
                 "  duplicates:  %zu"
                 ")\n",
                 daemon_input_->get_received_bundles(),
                 daemon_input_->get_rcvd_from_peer(),
                 daemon_input_->get_rcvd_from_app(),
                 daemon_input_->get_generated_bundles(),
                 daemon_input_->get_rcvd_from_frag(),
                 stats_.injected_bundles_,
                 daemon_input_->get_rcvd_from_storage(),
                 daemon_input_->get_rcvd_from_restage(),
                 daemon_input_->get_duplicate_bundles());

    buf->appendf("pending: %zu  custody: %zu"
                 "  delivered: %zu"
                 "  transmitted: %zu"
                 "  restaged: %zu"
                 "  expired: %zu"
                 "  rejected: %zu"
                 "  suppressed_dlv: %zu"
                 "  deleted: %zu"
                 "\n",
                 pending_bundles()->size(),
                 custody_bundles()->size(),
                 stats_.delivered_bundles_,
                 stats_.transmitted_bundles_,
                 stats_.restaged_bundles_,
                 stats_.expired_bundles_,
                 daemon_input_->get_rejected_bundles() + stats_.rejected_bundles_,
                 stats_.suppressed_delivery_,
                 stats_.deleted_bundles_);

    buf->appendf("Totals - BPv7 bundles: %zu"
                 "  BPv6 bundles: %zu"
                 "\n\n",
                 daemon_input_->get_bpv7_bundles(),
                 daemon_input_->get_bpv6_bundles());


    buf->appendf("BIBE extraction - max queued: %zu\n", bibe_extractor_->get_max_queued());
    buf->appendf("BIBE in BP6 - encapsulations: %zu" " extractions: %zu" " extraction errors: %zu" "\n",
                 bibe6_encapsulations_, bibe6_extractions_, bibe6_extraction_errors_);
                 
    buf->appendf("BIBE in BP7 - encapsulations: %zu" " extractions: %zu" " extraction errors: %zu" "\n\n",
                 bibe7_encapsulations_, bibe7_extractions_, bibe7_extraction_errors_);



    buf->appendf("BundleList sizes: \nall: %zu (max: %zu)  pending: %zu  custody: %zu  dupefinder: %zu  deleting: %zu (max: %zu)\n\n",
                 all_bundles()->size(),
                 all_bundles()->max_size(),
                 pending_bundles()->size(),
                 custody_bundles()->size(),
                 dupefinder_bundles()->size(),
                 deleting_bundles()->size(),
                 deleting_bundles()->max_size());
}

//----------------------------------------------------------------------
void
BundleDaemon::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("\nEID Pool size      : EIDs: %zu  Patterns: %zu \n\n", 
                 qptr_eid_pool_->size_eids(),
                 qptr_eid_pool_->size_patterns());

    buf->appendf("\nBundleDaemon       : %zu pending_events (max: %zu) -- "
                 "%" PRIbid " processed_events -- "
                 "%zu pending_timers (%zu cancelled)\n",
    	         me_eventq_.size(),
    	         me_eventq_.max_size(),
                 stats_.events_processed_,
                 oasys::SharedTimerSystem::instance()->num_pending_timers(),
                 oasys::SharedTimerSystem::instance()->num_cancelled_timers());

    daemon_input_->get_daemon_stats(buf);
    daemon_output_->get_daemon_stats(buf);
    daemon_storage_->get_daemon_stats(buf);
    daemon_cleanup_->get_daemon_stats(buf);

}


//----------------------------------------------------------------------
void
BundleDaemon::get_ltp_object_stats(oasys::StringBuffer* buf)
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);

    buf->appendf("LTP Object Tracking statistics for all Ltp links:\n");
    buf->appendf("    LtpSessions created: %zu deleted: %zu  in use: %zu\n",
                 ltpsession_created_, ltpsession_deleted_, (ltpsession_created_ - ltpsession_deleted_));
    buf->appendf("    LtpDataSegs created: %zu deleted: %zu  in use: %zu\n",
                 ltp_ds_created_, ltp_ds_deleted_, (ltp_ds_created_ - ltp_ds_deleted_));
    buf->appendf("    LtpRptSegs created: %zu deleted: %zu  in use: %zu\n",
                 ltp_rs_created_, ltp_rs_deleted_, (ltp_rs_created_ - ltp_rs_deleted_));
    buf->appendf("    LtpSegments created: %zu deleted: %zu  in use: %zu\n",
                 ltpseg_created_, ltpseg_deleted_, (ltpseg_created_ - ltpseg_deleted_));

    buf->appendf("    LtpTimers - Retransmit - created: %zu deleted: %zu  in use: %zu\n",
                 ltp_retran_timers_created_, ltp_retran_timers_deleted_, (ltp_retran_timers_created_ - ltp_retran_timers_deleted_));

    buf->appendf("    LtpTimers - Inactivity - created: %zu deleted: %zu  in use: %zu\n", 
                 ltp_inactivity_timers_created_, ltp_inactivity_timers_deleted_, (ltp_inactivity_timers_created_ - ltp_inactivity_timers_deleted_));
    buf->appendf("    LtpTimers - Closeout   - created: %zu deleted: %zu  in use: %zu\n\n", 
                 ltp_closeout_timers_created_, ltp_closeout_timers_deleted_, (ltp_closeout_timers_created_ - ltp_closeout_timers_deleted_));
}


//----------------------------------------------------------------------
void
BundleDaemon::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));

    daemon_input_->reset_stats();
    daemon_output_->reset_stats();
    daemon_storage_->reset_stats();
    daemon_cleanup_->reset_stats();

    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::reset_stats");
    
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    for (iter = links->begin(); iter != links->end(); ++iter) {
        (*iter)->reset_stats();
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_status_report(Bundle* orig_bundle,
                                     BundleStatusReport::flag_t flag,
                                     status_report_reason_t reason)
{
    if (!BundleProtocol::params_.status_rpts_enabled_) {
        return;
    }


#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("generating return receipt status report, "
              "flag = 0x%x, reason = 0x%x", flag, reason);
#endif
    
    Bundle* report = new Bundle(orig_bundle->bp_version());

    bool use_eid_ipn = false;
    if (sptr_local_eid_ipn_->is_ipn_scheme()) {
        if (orig_bundle->replyto() == null_eid()) { 
            use_eid_ipn = orig_bundle->source()->is_ipn_scheme();
        } else {
            use_eid_ipn = orig_bundle->replyto()->is_ipn_scheme();
        }
    }

    if (use_eid_ipn) {
        BundleStatusReport::create_status_report(report, orig_bundle,
                                                 sptr_local_eid_ipn_, flag, reason);
    } else {
        BundleStatusReport::create_status_report(report, orig_bundle,
                                                 sptr_local_eid_, flag, reason);
    }

    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event =  new BundleReceivedEvent(report, EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event(event);
    BundleDaemon::post(sptr_event);
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_custody_signal(Bundle* bundle, bool succeeded,
                                      custody_signal_reason_t reason)
{
    if (bundle->local_custody()) {
        log_err("send_custody_signal(*%p): already have local custody",
                bundle);
        return;
    }

    if (bundle->custodian() == null_eid()) {
        log_err("send_custody_signal(*%p): current custodian is NULL_EID",
                bundle);
        return;
    }
    
    if (bundle->cteb_valid()) {
        if (daemon_acs_->generate_custody_signal( bundle, succeeded, reason)) {
            // if ACS is providing the signal then nothing further to do
            return;
        }
    }

     // send a normal custody signal
    Bundle* signal = new Bundle(bundle->bp_version());

    if (bundle->custodian()->is_ipn_scheme()) {
        CustodySignal::create_custody_signal(signal, bundle, sptr_local_eid_ipn_,
                                             succeeded, reason);
    } else {
        CustodySignal::create_custody_signal(signal, bundle, sptr_local_eid_,
                                             succeeded, reason);
    }
 
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event = new BundleReceivedEvent(signal, EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event(event);
    BundleDaemon::post(sptr_event);
}

//----------------------------------------------------------------------
void
BundleDaemon::cancel_custody_timers(Bundle* bundle)
{
    SPtr_CustodyTimer ct_sptr;

    CustodyTimerVec::iterator iter;
    CustodyTimerVec::iterator del_iter;

    bundle->lock()->lock("BundleDaemon::cancel_custody_timers #1");
    iter =  bundle->custody_timers()->begin();
    del_iter = bundle->custody_timers()->end();;
    bundle->lock()->unlock();

    while ( iter != bundle->custody_timers()->end())
    {
        ct_sptr = *iter;

        if (ct_sptr != nullptr) {
            // releases bundle and link as well cancelling the timer
            ct_sptr->cancel();
        }

        del_iter = iter;

        bundle->lock()->lock("BundleDaemon::cancel_custody_timers #2");
        iter =  bundle->custody_timers()->begin();

        if (del_iter == iter) {
            // no changes in the vector so we can now delete it
            bundle->custody_timers()->erase(del_iter);

            iter =  bundle->custody_timers()->begin();
        }

        bundle->lock()->unlock();
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::accept_custody(Bundle* bundle)
{
    log_info("accept_custody *%p", bundle);
    
    if (bundle->local_custody()) {
        log_err("accept_custody(*%p): already have local custody",
                bundle);
        return;
    }

    if ((bundle->custodian() == sptr_local_eid_) ||
        (bundle->custodian() == sptr_local_eid_ipn_)) {
        log_err("send_custody_signal(*%p): "
                "current custodian is already local_eid",
                bundle);
        return;
    }

    // send a custody acceptance signal to the current custodian (if
    // it is someone, and not the null eid)
    if (bundle->custodian() != null_eid()) {
        generate_custody_signal(bundle, true, BundleProtocol::CUSTODY_NO_ADDTL_INFO);
    }

    // XXX/dz Need a lock because changing the custodian can happen between the 
    // time the BerkelyDBTable::put() calculates the size for the buffer and
    // when it serializes the bundle into the buffer. If the current custodian
    // is a longer string than the previous one then it aborts with error.
    bundle->lock()->lock("BundleDaemon::accept_custody()");
    
    // now we mark the bundle to indicate that we have custody and add
    // it to the custody bundles list
    if (bundle->dest()->scheme() == IPNScheme::instance())  {
        bundle->mutable_custodian() = sptr_local_eid_ipn_;
    } else {
        bundle->mutable_custodian() = sptr_local_eid_;
    }

    bundle->set_local_custody(true);
    bundle->set_bibe_custody(true);  // local custody is also good for BIBE custody transfer

    // accept custody for ACS before updating to the datastore so that
    // the CTEB block can be added and then saved in one write
    daemon_acs_->accept_custody(bundle);

    bundle->lock()->unlock();

    actions_->store_update(bundle);
    
    custody_bundles_->push_back(bundle);

    // finally, if the bundle requested custody acknowledgements,
    // deliver them now
    if (bundle->custody_rcpt()) {
        generate_status_report(bundle, 
                               BundleStatusReport::STATUS_CUSTODY_ACCEPTED);
    }


    // generate custody taken event for the external router
    BundleCustodyAcceptedEvent* event = new BundleCustodyAcceptedEvent(bundle);
    SPtr_BundleEvent sptr_event(event);
    BundleDaemon::post(sptr_event);
}

//----------------------------------------------------------------------
void
BundleDaemon::accept_bibe_custody(Bundle* bundle, std::string bibe_custody_dest)
{
    if (!bundle->bibe_custody()) {
        oasys::ScopeLock scoplok(bundle->lock(), __func__);

        bundle->set_bibe_custody(true);  // local custody is also good for BIBE custody transfer

        if (!bundle->local_custody()) {
            bundle->set_custody_dest(bibe_custody_dest);
            daemon_acs_->bibe_accept_custody(bundle);

            scoplok.unlock();

            custody_bundles_->push_back(bundle);

            // generate custody taken event for the external router
            BundleCustodyAcceptedEvent* event = new BundleCustodyAcceptedEvent(bundle);
            SPtr_BundleEvent sptr_event(event);
            BundleDaemon::post(sptr_event);
        } else {
            scoplok.unlock();
        }

        actions_->store_update(bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::release_custody(Bundle* bundle)
{
    //log_info("release_custody *%p", bundle);
    
    if (!bundle->local_custody() && !bundle->bibe_custody()) {
        log_err("release_custody(*%p): don't have local custody",
                bundle);
        return;
    }

    cancel_custody_timers(bundle);

    if (bundle->local_custody()) {
        bundle->mutable_custodian() = make_eid_null();
        bundle->set_local_custody(false);
    }
    bundle->set_bibe_custody(false);

    actions_->store_update(bundle);

    custody_bundles_->erase(bundle);

    // and also remove it from the ACS custody id list
    daemon_acs_->erase_from_custodyid_list(bundle);

    // bottom line is that custody was released 
    // so we can just fudge the reason code for the router
    router_->handle_custody_released(bundle->bundleid(), true, BundleProtocol::REASON_NO_ADDTL_INFO);
}

//----------------------------------------------------------------------
void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      SPtr_Registration& sptr_reg,
                                      bool initial_load)
{
    if (shutting_down_) {
        return;
    }


    if ( !initial_load && nullptr != sptr_reg->initial_load_thread() ) {
        // Registration is currently being loaded with pending bundles and
        // this new bundle will get picked up in the proper order
        return;
    }

    ASSERT(!bundle->is_fragment());

    ForwardingInfo::state_t state = bundle->fwdlog()->get_latest_entry(sptr_reg.get());
    switch(state) {
    case ForwardingInfo::PENDING_DELIVERY:
        // Expected case
        break;
    case ForwardingInfo::DELIVERED:
        return;
    default:
        log_warn("deliver_to_registration called with bundle not marked " \
                 "as PENDING_DELIVERY.  Delivering anyway...");
        bundle->fwdlog()->add_entry(sptr_reg.get(),
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::PENDING_DELIVERY);
        break;
    }


#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("delivering bundle *%p to registration %d (%s)",
              bundle, sptr_reg->regid(),
              sptr_reg->endpoint()->c_str());
#endif

    int deliver_status = sptr_reg->deliver_if_not_duplicate(bundle, sptr_reg);
    if (deliver_status != REG_DELIVER_BUNDLE_QUEUED) {
        BundleEvent* event = new BundleDeliveredEvent(bundle, sptr_reg);
        SPtr_BundleEvent sptr_event(event);
        BundleDaemon::post(sptr_event);

        if (deliver_status == REG_DELIVER_BUNDLE_DUPLICATE) {
            ++stats_.suppressed_delivery_;

            log_info("suppressing duplicate delivery of bundle *%p - %s - "
                      "to registration %d (%s) - and deleting bundle",
                      bundle, bundle->gbofid_str().c_str(), sptr_reg->regid(),
                      sptr_reg->endpoint()->c_str());

            BundleRef bref("deliver_dupe_suppressed");
            bref = bundle;
            delete_bundle(bref);
        } else if (deliver_status == REG_DELIVER_BUNDLE_DROPPED) {
            ///XXX/TODO distinguish between duplicate suppressed and dropped???
            ++stats_.suppressed_delivery_;

            log_info("suppressing duplicate delivery of bundle *%p - %s - "
                      "to registration %d (%s) - and deleting bundle",
                      bundle, bundle->gbofid_str().c_str(), sptr_reg->regid(),
                      sptr_reg->endpoint()->c_str());
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::check_local_delivery(Bundle* bundle, bool deliver)
{
    if (shutting_down_)
    {
      return false;
    }

    RegistrationList matches;
    RegistrationList::iterator iter;

    SPtr_Registration sptr_reg;

    reg_table_->get_matching(bundle->dest(), &matches);

    if (deliver && router_->auto_deliver_bundles()) {
        ASSERT(!bundle->is_fragment());
        for (iter = matches.begin(); iter != matches.end(); ++iter) {
            sptr_reg = *iter;
            // deliver_to_registration(bundle, sptr_reg);

            /*
             * Mark the bundle as needing delivery to the registration.
             * Marking is durable and should be transactionalized together
             * with storing the bundle payload and metadata to disk if
             * the storage mechanism supports it (i.e. if transactions are
             * supported, we should be in one).
             */
            bundle->fwdlog()->add_entry(sptr_reg.get(),
                                        ForwardingInfo::FORWARD_ACTION,
                                        ForwardingInfo::PENDING_DELIVERY);


            // let's try just delivering now    BundleDaemon::post(new DeliverBundleToRegEvent(bundle, sptr_reg->regid()));
            int deliver_status = sptr_reg->deliver_if_not_duplicate(bundle, sptr_reg);

            if (deliver_status != REG_DELIVER_BUNDLE_QUEUED) {
                BundleEvent* event = new BundleDeliveredEvent(bundle, sptr_reg);
                SPtr_BundleEvent sptr_event(event);
                BundleDaemon::post(sptr_event);

                if (deliver_status == REG_DELIVER_BUNDLE_DUPLICATE) {
                    ++stats_.suppressed_delivery_;

                    log_info("suppressing duplicate delivery of bundle *%p - %s - "
                              "to registration %d (%s) - and deleting bundle",
                              bundle, bundle->gbofid_str().c_str(), sptr_reg->regid(),
                              sptr_reg->endpoint()->c_str());

                    BundleRef bref("deliver_dupe_suppressed");
                    bref = bundle;
                    delete_bundle(bref);
                } else if (deliver_status == REG_DELIVER_BUNDLE_DROPPED) {
                    ///XXX/TODO distinguish between duplicate suppressed and dropped???
                    ++stats_.suppressed_delivery_;

                    log_info("suppressing duplicate delivery of bundle *%p - %s - "
                              "to registration %d (%s) - and deleting bundle",
                              bundle, bundle->gbofid_str().c_str(), sptr_reg->regid(),
                              sptr_reg->endpoint()->c_str());
                }
            }
        }
    }

    return ((matches.size() > 0) || 
            bundle->dest()->subsume(sptr_local_eid_) || 
            bundle->dest()->subsume_ipn(sptr_local_eid_ipn_));
}

//----------------------------------------------------------------------
void
BundleDaemon::check_and_deliver_to_registrations(Bundle* bundle, const SPtr_EID& sptr_eid)
{
    int num;

    RegistrationList matches;
    RegistrationList::iterator iter;

    num = reg_table_->get_matching(sptr_eid, &matches);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("checking for matching entries in table for %s - matches: %d", 
              sptr_eid->c_str(), num);
#else
    (void) num;
#endif

    SPtr_Registration sptr_reg;

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        sptr_reg = *iter;
        deliver_to_registration(bundle, sptr_reg);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delete(SPtr_BundleEvent& sptr_event)
{
    BundleDeleteRequest* request = nullptr;
    request = dynamic_cast<BundleDeleteRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a BundleDeleteRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (request->bundle_.object()) {
        log_info("BUNDLE_DELETE_REQUEST: bundle *%p (reason %s)",
                 request->bundle_.object(),
                 BundleStatusReport::reason_to_str(request->reason_));

        ++stats_.rejected_bundles_;

        delete_bundle(request->bundle_, request->reason_);
    } else if (request->delete_all_) {

#if BARD_ENABLED
        std::string dummy_result_str;
        bundle_restaging_daemon_->bardcmd_del_all_restaged_bundles(dummy_result_str);
#endif

        oasys::ScopeLock scoplok(&pending_lock_, __func__);

        pending_bundles_->lock()->lock(__func__);

        log_info("BUNDLE_DELETE_REQUEST: deleting all %zu pending bundles", 
                 pending_bundles_->size());

        BundleRef bref(__func__);
        pending_bundles_t::iterator iter;

        iter = pending_bundles_->begin();
        while (iter != pending_bundles_->end()) {
            bref = iter->second;  // for <map> lists

            // increment iter before calling delete_bundle which deletes 
            // it from the pending_bundles_ list
            ++iter;

            ++stats_.rejected_bundles_;

            delete_bundle(bref, request->reason_);
        }

        pending_bundles_->lock()->unlock();
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_take_custody(SPtr_BundleEvent& sptr_event)
{
    BundleTakeCustodyRequest* request = nullptr;
    request = dynamic_cast<BundleTakeCustodyRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a BundleTakeCustodyRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (request->bundleref_.object()) {
            accept_custody(request->bundleref_.object());
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_acknowledged_by_app(SPtr_BundleEvent& sptr_event)
{
    BundleAckEvent* ack = nullptr;
    ack = dynamic_cast<BundleAckEvent*>(sptr_event.get());
    if (ack == nullptr) {
        log_err("Error casting event type %s as a BundleAckEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("ack from regid(%d): (%s: (%" PRIu64 ", %" PRIu64 "))",
              ack->regid_,
              ack->sourceEID_.c_str(),
              ack->creation_ts_.secs_or_millisecs_, ack->creation_ts_.seqno_);
#endif

    // Make sure we're happy with the registration provided.
    SPtr_Registration sptr_reg = reg_table_->get(ack->regid_);
    if (!sptr_reg) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("BAD: can't get reg from regid(%d)", ack->regid_);
#endif
        return;
    }
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(sptr_reg.get());
    if (api_reg == nullptr) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("Acking registration is not an APIRegistration");
#endif
        return;
    }

    api_reg->bundle_ack(ack->sptr_source_, ack->creation_ts_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_accept(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    ASSERTF(false, "handle_bundle_accept() was moved to BundleDaemonInput");
}
    
//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_received(SPtr_BundleEvent& sptr_event)
{
    BundleReceivedEvent* event = nullptr;
    event = dynamic_cast<BundleReceivedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleReceivedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    const BundleRef& bundleref = event->bundleref_;
    Bundle* bundle = bundleref.object();

    // See if bundle can be delivered locally


    /*
     * Deliver the bundle to any local registrations that it matches,
     * unless it's generated by the router or is a bundle fragment.
     * Delivery of bundle fragments is deferred until after re-assembly.
     */
    bool is_local_delivery = false;
    if ( event->source_ == EVENTSRC_STORE ) {
        generate_delivery_events(bundle);
    } else {
        // initiate delivery if not a fragment 
        // else just check for local delivery to see if needs to be reassembled
        is_local_delivery = check_local_delivery(bundle, ((event->source_ != EVENTSRC_ROUTER) &&
                                                          (bundle->is_fragment() == false)));

        event->local_delivery_ = is_local_delivery;
        event->singleton_dest_ = bundle->dest()->is_singleton();

        // inform the router that the bundle was received
        router_->post(sptr_event);

        /*
         * Re-assemble bundle fragments that are destined to the local node.
         */
        if (is_local_delivery && bundle->is_fragment() && event->source_ != EVENTSRC_FRAGMENTATION) {
            fragmentmgr_->process_for_reassembly(bundle);
        }

        // XXX/dz - add configurable parameter to disable enforcing hop limit?
        if (!is_local_delivery) {
            if (bundle->hop_limit() != 0) {
                if (bundle->hop_count() >= bundle->hop_limit()) {
                    // hop count limit reached so bundle cannot be forwarded
                    // delete the bundle...
                    ++stats_.hops_exceeded_;
                    status_report_reason_t deletion_reason = BundleProtocol::REASON_HOP_LIMIT_EXCEEDED;
                    delete_bundle(bundleref, deletion_reason);
                    event->daemon_only_ = true;

                    //XXX/dz - how to account for this in EHSRouter???
                }
            }
        }
    }

    try_to_delete(event->bundleref_, "BundleReceived");

}

//----------------------------------------------------------------------
void
BundleDaemon::handle_deliver_bundle_to_reg(SPtr_BundleEvent& sptr_event)
{
    DeliverBundleToRegEvent* event = nullptr;
    event = dynamic_cast<DeliverBundleToRegEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a DeliverBundleToRegEvent", 
                event_to_str(sptr_event->type_));
        return;
    }


    const RegistrationTable* reg_table = 
            BundleDaemon::instance()->reg_table();

    const BundleRef& bundleref = event->bundleref_;
    u_int32_t regid = event->regid_;
  
    SPtr_Registration sptr_reg = reg_table->get(regid);

    if (!sptr_reg) {
        log_warn("Can't find registration %d any more", regid);
        return;
    }

    Bundle* bundle = bundleref.object();
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Delivering bundle id:%" PRIbid " to registration %d %s",
              bundle->bundleid(),
              sptr_reg->regid(),
              sptr_reg->endpoint()->c_str());
#endif

    deliver_to_registration(bundle, sptr_reg);
 }

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_transmitted(SPtr_BundleEvent& sptr_event)
{
    BundleTransmittedEvent* event = nullptr;
    event = dynamic_cast<BundleTransmittedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleTransmittedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // processed in BDOutput -fall through and try to delete bundle

    if (event->success_) {
        ++stats_.transmitted_bundles_;
    }

    try_to_delete(event->bundleref_, "BundleTransmitted");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_try_delete_request(SPtr_BundleEvent& sptr_event)
{
    BundleTryDeleteRequest* event = nullptr;
    event = dynamic_cast<BundleTryDeleteRequest*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleTryDeleteRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    try_to_delete(event->bundleref_, "BundleTryDeleteRequest");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delivered(SPtr_BundleEvent& sptr_event)
{
    BundleDeliveredEvent* event = nullptr;
    event = dynamic_cast<BundleDeliveredEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleDeliveredEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // update statistics
    stats_.delivered_bundles_++;
    
    /*
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(event->sptr_reg_.get());
    if (api_reg != nullptr) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("BundleDelivered processed for registration %s and bundle: *%p",
                  api_reg->endpoint()->c_str(), bundle);
#endif
        bundle->fwdlog()->update(api_reg,
                                 ForwardingInfo::DELIVERED);

        if (params_.serialize_apireg_bundle_lists_) {
            //XXX/dz I don't think this update is necesssary even if true
            oasys::ScopeLock l(api_reg->lock(), "handle_bundle_delivered");
            api_reg->update();
        }
    } else {
        IMCGroupPetitionRegistration* imc_reg = dynamic_cast<IMCGroupPetitionRegistration*>(event->sptr_reg_.get());
        if (imc_reg != nullptr) {
            bundle->fwdlog()->update(imc_reg,
                                     ForwardingInfo::DELIVERED);
        } else {
            IonContactPlanSyncRegistration* ion_sync_reg = dynamic_cast<IonContactPlanSyncRegistration*>(event->sptr_reg_.get());
            if (ion_sync_reg != nullptr) {
                bundle->fwdlog()->update(ion_sync_reg,
                                         ForwardingInfo::DELIVERED);
            }
        }
    }


#ifdef BD_LOG_DEBUG_ENABLED
    log_info("BUNDLE_DELIVERED id:%" PRIbid " (%zu bytes) -> regid %d (%s)",
             bundle->bundleid(), bundle->payload().length(),
             event->sptr_reg_->regid(),
             event->sptr_reg_->endpoint()->c_str());
#endif

    /*
     * Generate the delivery status report if requested.
     */
    if (bundle->delivery_rcpt())
    {
        generate_status_report(bundle, BundleStatusReport::STATUS_DELIVERED);
    }

    /*
     * If this is a custodial bundle and it was delivered, we either
     * release custody (if we have it), or send a custody signal to
     * the current custodian indicating that the bundle was
     * successfully delivered, unless there is no current custodian
     * (the eid is still dtn:none).
     */
    if (bundle->custody_requested())
    {
        if (bundle->local_custody()) {
            release_custody(bundle);

        } else if (bundle->custodian() == null_eid()) {
            log_info("custodial bundle *%p delivered before custody accepted",
                     bundle);

        } else {
            generate_custody_signal(bundle, true,
                                    BundleProtocol::CUSTODY_NO_ADDTL_INFO);
        }
    }

    try_to_delete(event->bundleref_, "BundleDelivered");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_restaged(SPtr_BundleEvent& sptr_event)
{
    BundleRestagedEvent* event = nullptr;
    event = dynamic_cast<BundleRestagedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleRestagedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // real work done in BD-Output; this is just to see if the bundle can be deleted
    try_to_delete(event->bundleref_, "BundleRestaged");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_expired(SPtr_BundleEvent& sptr_event)
{
    BundleExpiredEvent* event = nullptr;
    event = dynamic_cast<BundleExpiredEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleExpiredEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // update statistics
    stats_.expired_bundles_++;
    
    const BundleRef& bref = event->bundleref_;

    log_info("BUNDLE_EXPIRED *%p", bref.object());

    // note that there may or may not still be a pending expiration
    // timer, since this event may be coming from the console, so we
    // just fall through to delete_bundle which will cancel the timer

    if ( ( !params_.early_deletion_ ) && ( params_.keep_expired_bundles_ ) )
    {
        return;
    }

    delete_bundle(bref, BundleProtocol::REASON_LIFETIME_EXPIRED);
    
    // fall through to notify the routers
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_cancel(SPtr_BundleEvent& sptr_event)
{
    BundleCancelRequest* event = nullptr;
    event = dynamic_cast<BundleCancelRequest*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleCancelRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    BundleRef br = event->bundle_;

    if(!br.object()) {
        log_err("nullptr bundle object in BundleCancelRequest");
        return;
    }

    // If the request has a link name, we are just canceling the send on
    // that link.
    if (!event->link_.empty()) {
        LinkRef link = contactmgr_->find_link(event->link_.c_str());
        if (link == nullptr) {
            log_err("BUNDLE_CANCEL no link with name %s", event->link_.c_str());
            return;
        }

        log_info("BUNDLE_CANCEL bundle %" PRIbid " on link %s", br->bundleid(),
                event->link_.c_str());
        
        actions_->cancel_bundle(br.object(), link);
    }
    
    // If the request does not have a link name, the bundle itself has been
    // canceled (probably by an application).
    else {
        delete_bundle(br, BundleProtocol::REASON_TRANSMISSION_CANCELLED);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_cancelled(SPtr_BundleEvent& sptr_event)
{
    BundleSendCancelledEvent* event = nullptr;
    event = dynamic_cast<BundleSendCancelledEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleSendCancelledEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    Bundle* bundle = event->bundleref_.object();
    LinkRef link = event->link_;
    
    log_info("BUNDLE_CANCELLED id:%" PRIbid " -> %s (%s)",
            bundle->bundleid(),
            link->name(),
            link->nexthop());
    
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("trying to find xmit blocks for bundle id:%" PRIbid " on link %s",
              bundle->bundleid(), link->name());
#endif
    SPtr_BlockInfoVec blocks = bundle->xmit_blocks()->find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed 
    // messages. If an event about a bundle bound for particular link is 
    // posted after  another, which it might contradict, the BundleDaemon 
    // need not reprocess the event. The router (DP) might, however, be 
    // interested in the new status of the send.
    if (blocks == nullptr)
    {
        log_info("received a redundant/conflicting bundle_cancelled event "
                 "about bundle id:%" PRIbid " -> %s (%s)",
                 bundle->bundleid(),
                 link->name(),
                 link->nexthop());
        return;
    }

    /*
     * The bundle should no longer be on the link queue or on the
     * inflight queue if it was cancelled.
     */
    //dzdebug if (link->queue()->contains(bundle))
    //dzdebug {
    //dzdebug     log_warn("cancelled bundle id:%" PRIbid " still on link %s queue",
    //dzdebug              bundle->bundleid(), link->name());
    //dzdebug }

    /*
     * The bundle should no longer be on the link queue or on the
     * inflight queue if it was cancelled.
     */
    //dzdebug if (link->inflight()->contains(bundle))
    //dzdebug {
    //dzdebug     log_warn("cancelled bundle id:%" PRIbid " still on link %s inflight list",
    //dzdebug              bundle->bundleid(), link->name());
    //dzdebug }

    /*
     * Update statistics. Note that the link's queued length must
     * always be decremented by the full formatted size of the bundle.
     */
    link->stats()->bundles_cancelled_++;
    
    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("trying to delete xmit blocks for bundle id:%" PRIbid " on link %s",
              bundle->bundleid(), link->name());
#endif
    BundleProtocol::delete_blocks(bundle, link);
    blocks = nullptr;

    /*
     * Update the forwarding log.
     */
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("trying to update the forwarding log for "
              "bundle id:%" PRIbid " on link %s to state CANCELLED",
              bundle->bundleid(), link->name());
#endif
    bundle->fwdlog()->update(link, ForwardingInfo::CANCELLED);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_inject(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    ASSERTF(false, "handle_bundle_inject() was moved to BundleDaemonInput");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;

    // just trigger a Bundle Report Event
    //XXX/dz could use this event to prod the External Router instead of a Bundle Report Event
    BundleReportEvent* event_to_post = new BundleReportEvent();
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post_at_head(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // nothing to do - passes throough to the External Router to generate the report
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_query(SPtr_BundleEvent& sptr_event)
{
    BundleAttributesQueryRequest* request = nullptr;
    request = dynamic_cast<BundleAttributesQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a BundleAttributesQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    BundleRef &br = request->bundle_;
    if (! br.object()) return; // XXX or should it post an empty report?

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug(
        "BundleDaemon::handle_bundle_attributes_query: query %s, bundle *%p",
        request->query_id_.c_str(), br.object());
#endif

    // we need to keep a reference to the bundle because otherwise it may
    // be deleted before the event is handled
    BundleAttributesReportEvent* event_to_post;
    event_to_post = new BundleAttributesReportEvent(request->query_id_,
                                                    br,
                                                    request->attribute_names_,
                                                    request->metadata_blocks_);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // just passes through to the External Router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_added(SPtr_BundleEvent& sptr_event)
{
    RegistrationAddedEvent* event = nullptr;
    event = dynamic_cast<RegistrationAddedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationAddedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    SPtr_Registration sptr_reg = event->sptr_reg_;
    log_info("REGISTRATION_ADDED %d %s",
             sptr_reg->regid(), sptr_reg->endpoint()->c_str());

    if (!reg_table_->add(sptr_reg, (event->source_ == EVENTSRC_APP)))
    {
        log_err("error adding registration %d to table",
                sptr_reg->regid());
    }

    // kick off a helper thread to deliver the bundles to the 
    // registration so that it can be done in the background
    sptr_reg->stop_initial_load_thread(); // stop an old one if there is one
    RegistrationInitialLoadThread* loader = new RegistrationInitialLoadThread(sptr_reg);
    loader->start();
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_removed(SPtr_BundleEvent& sptr_event)
{
    RegistrationRemovedEvent* event = nullptr;
    event = dynamic_cast<RegistrationRemovedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationRemovedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }


    SPtr_Registration sptr_reg = event->sptr_reg_;
    log_info("REGISTRATION_REMOVED %d %s",
             sptr_reg->regid(), sptr_reg->endpoint()->c_str());

    if (!reg_table_->del(sptr_reg)) {
        log_err("error removing registration %d from table",
                sptr_reg->regid());
        return;
    }

    RegistrationDeleteRequest* event_to_post = new RegistrationDeleteRequest(sptr_reg);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_expired(SPtr_BundleEvent& sptr_event)
{
    RegistrationExpiredEvent* event = nullptr;
    event = dynamic_cast<RegistrationExpiredEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationExpiredEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    SPtr_Registration sptr_reg = event->sptr_reg_;

    if (reg_table_->get(sptr_reg->regid()) == nullptr) {
        // this shouldn't ever happen
        log_err("REGISTRATION_EXPIRED -- dead regid %d", sptr_reg->regid());
        return;
    }
    
    sptr_reg->set_expired(true);
    
    if (sptr_reg->active()) {
        // if the registration is currently active (i.e. has a
        // binding), we wait for the binding to clear, which will then
        // clean up the registration
        log_info("REGISTRATION_EXPIRED %d -- deferred until binding clears",
                 sptr_reg->regid());
    } else {
        // otherwise remove the registration from the table
        log_info("REGISTRATION_EXPIRED %d", sptr_reg->regid());
        reg_table_->del(sptr_reg);
        RegistrationDeleteRequest* event_to_post = new RegistrationDeleteRequest(sptr_reg);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        post_at_head(sptr_event_to_post);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_delete(SPtr_BundleEvent& sptr_event)
{
    RegistrationDeleteRequest* request = nullptr;
    request = dynamic_cast<RegistrationDeleteRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a RegistrationDeleteRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    log_info("REGISTRATION_DELETE %d", request->sptr_reg_->regid());

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(request->sptr_reg_.get());
    if (nullptr != api_reg  &&  api_reg->add_to_datastore()) {
        // the Stroage thread will delete the registration after its processing
        StoreRegistrationDeleteEvent* event_to_post;
        event_to_post = new StoreRegistrationDeleteEvent(request->sptr_reg_->regid(), 
                                                         request->sptr_reg_);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        post_at_head(sptr_event_to_post);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_created(SPtr_BundleEvent& sptr_event)
{
    LinkCreatedEvent* event = nullptr;
    event = dynamic_cast<LinkCreatedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkCreatedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);

    if (link->isdeleted()) {
        log_warn("BundleDaemon::handle_link_created: "
                 "link %s deleted prior to full creation", link->name());
        event->daemon_only_ = true;
        return;
    }

    // Add (or update) this Link to the persistent store
    StoreLinkUpdateEvent* event_to_post;
    event_to_post = new StoreLinkUpdateEvent(link.object());
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    post(sptr_event_to_post);

    log_info("LINK_CREATED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_deleted(SPtr_BundleEvent& sptr_event)
{
    LinkDeletedEvent* event = nullptr;
    event = dynamic_cast<LinkDeletedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkDeletedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);

    // If link has been used in some forwarding log entry during this run of the
    // daemon or the link is a reincarnation of a link that was extant when a
    // previous run was terminated, then the persistent storage should be left
    // intact (and the link spec will remain in ContactManager::previous_links_
    // so that consistency will be checked).  Otherwise delete the entry in Links.

    if (!(link->used_in_fwdlog() || link->reincarnated()))
    {
        StoreLinkDeleteEvent* event_to_post;
        event_to_post = new StoreLinkDeleteEvent(link->name_str());
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        post_at_head(sptr_event_to_post);
    }

    log_info("LINK_DELETED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_available(SPtr_BundleEvent& sptr_event)
{
    LinkAvailableEvent* event = nullptr;
    event = dynamic_cast<LinkAvailableEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkAvailableEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);
    ASSERT(link->isavailable());

    if (link->isdeleted()) {
        log_warn("BundleDaemon::handle_link_available: "
                 "link %s already deleted", link->name());
        event->daemon_only_ = true;
        return;
    }

    log_info("LINK_AVAILABLE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_unavailable(SPtr_BundleEvent& sptr_event)
{
    LinkUnavailableEvent* event = nullptr;
    event = dynamic_cast<LinkUnavailableEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkUnavailableEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);
    ASSERT(!link->isavailable());
    
    log_info("LINK UNAVAILABLE *%p", link.object());
   
    if (params_.clear_bundles_when_opp_link_unavailable_) {
        if (link->is_opportunistic()) {
            LinkCancelAllBundlesRequest* event_to_post;
            event_to_post = new LinkCancelAllBundlesRequest(link);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            post_at_head(sptr_event_to_post);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_state_change_request(SPtr_BundleEvent& sptr_event)
{
    if (shutting_down_) {
        return;
    }

    LinkStateChangeRequest* request = nullptr;
    request = dynamic_cast<LinkStateChangeRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a LinkStateChangeRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = request->link_;
    if (link == nullptr) {
        log_warn("LINK_STATE_CHANGE_REQUEST received invalid link");
        return;
    }

    Link::state_t new_state = Link::state_t(request->state_);
    Link::state_t old_state = Link::state_t(request->old_state_);
    int reason = request->reason_;

    if (link->isdeleted() && new_state != Link::CLOSED) {
        log_warn("BundleDaemon::handle_link_state_change_request: "
                 "link %s already deleted; cannot change link state to %s",
                 link->name(), Link::state_to_str(new_state));
        return;
    }
    
    if (link->contact() != request->contact_) {
        log_warn("stale LINK_STATE_CHANGE_REQUEST [%s -> %s] (%s) for "
                 "link *%p: contact %p != current contact %p", 
                 Link::state_to_str(old_state), Link::state_to_str(new_state),
                 ContactEvent::reason_to_str(reason), link.object(),
                 request->contact_.object(), link->contact().object());
        return;
    }

    log_info("LINK_STATE_CHANGE_REQUEST [%s -> %s] (%s) for link *%p",
             Link::state_to_str(old_state), Link::state_to_str(new_state),
             ContactEvent::reason_to_str(reason), link.object());

    //avoid a race condition caused by opening a partially closed link
    oasys::ScopeLock l;
    if (new_state == Link::OPEN)
    {
        l.set_lock(contactmgr_->lock(), "BundleDaemon::handle_link_state_change_request");
    }
    
    switch(new_state) {
    case Link::UNAVAILABLE:
        if (link->state() != Link::AVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state UNAVAILABLE in state %s",
                    link.object(), Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);

        {
            LinkUnavailableEvent* event_to_post;
            event_to_post = new LinkUnavailableEvent(link, ContactEvent::reason_t(reason));
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            post_at_head(sptr_event_to_post);
        }
        break;

    case Link::AVAILABLE:
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);
            
        } else {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link.object(), Link::state_to_str(link->state()));
            return;
        }

        {
            LinkAvailableEvent* event_to_post;
            event_to_post = new LinkAvailableEvent(link, ContactEvent::reason_t(reason));
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            post_at_head(sptr_event_to_post);
        }
        break;
        
    case Link::OPENING:
    case Link::OPEN:
        // force the link to be available, since someone really wants it open
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);
        }
        actions_->open_link(link);
        break;

    case Link::CLOSED:
        // The only case where we should get this event when the link
        // is not actually open is if it's in the process of being
        // opened but the CL can't actually open it.
        if (! link->isopen() && ! link->isopening()) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "setting state CLOSED (%s) in unexpected state %s",
                    link.object(), ContactEvent::reason_to_str(reason),
                    Link::state_to_str(link->state()));
            break;
        }

        // If the link is open (not OPENING), we need a ContactDownEvent
        if (link->isopen()) {
            ASSERT(link->contact() != nullptr);
            ContactDownEvent* event_to_post;
            event_to_post = new ContactDownEvent(link->contact(), ContactEvent::reason_t(reason));
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            post_at_head(sptr_event_to_post);
        }

        // close the link
        actions_->close_link(link);
        
        // now, based on the reason code, update the link availability
        // and set state accordingly
        if (reason == ContactEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            LinkUnavailableEvent* event_to_post;
            event_to_post = new LinkUnavailableEvent(link, ContactEvent::reason_t(reason));
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            post_at_head(sptr_event_to_post);
        }
    
        break;

    default:
        PANIC("unhandled state %s", Link::state_to_str(new_state));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_create(SPtr_BundleEvent& sptr_event)
{
    LinkCreateRequest* request = nullptr;
    request = dynamic_cast<LinkCreateRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a LinkCreateRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    //lock the contact manager so no one creates a link before we do
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), "BundleDaemon::handle_link_create");
    //check for an existing link with that name
    LinkRef linkCheck = cm->find_link(request->name_.c_str());
    if(linkCheck != nullptr)
    {
    	log_err( "Link already exists with name %s, aborting create", request->name_.c_str());
        request->daemon_only_ = true;
    	return;
    }
  
    std::string nexthop("");

    int argc = request->parameters_.size();
    char* argv[argc];
    AttributeVector::iterator iter;
    int i = 0;
    for (iter = request->parameters_.begin();
         iter != request->parameters_.end();
         iter++)
    {
        if (iter->name() == "nexthop") {
            nexthop = iter->string_val();
        }
        else {
            std::string arg = iter->name() + iter->string_val();
            argv[i] = new char[arg.length()+1];
            memcpy(argv[i], arg.c_str(), arg.length()+1);
            i++;
        }
    }
    argc = i+1;

    const char *invalidp;
    LinkRef link = Link::create_link(request->name_, request->link_type_,
                                     request->cla_, nexthop.c_str(), argc,
                                     (const char**)argv, &invalidp);
    for (i = 0; i < argc; i++) {
        delete argv[i];
    }

    if (link == nullptr) {
        log_err("LINK_CREATE %s failed", request->name_.c_str());
        return;
    }

    if (!contactmgr_->add_new_link(link)) {
        log_err("LINK_CREATE %s failed, already exists",
                request->name_.c_str());
        link->delete_link();
        return;
    }
    log_info("LINK_CREATE %s: *%p", request->name_.c_str(), link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_delete(SPtr_BundleEvent& sptr_event)
{
    LinkDeleteRequest* request = nullptr;
    request = dynamic_cast<LinkDeleteRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a LinkDeleteRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = request->link_;
    ASSERT(link != nullptr);

    log_info("LINK_DELETE *%p", link.object());
    if (!link->isdeleted()) {
        contactmgr_->del_link(link);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_reconfigure(SPtr_BundleEvent& sptr_event)
{
    LinkReconfigureRequest* request = nullptr;
    request = dynamic_cast<LinkReconfigureRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a LinkReconfigureRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = request->link_;
    ASSERT(link != nullptr);

    link->reconfigure_link(request->parameters_);
    log_info("LINK_RECONFIGURE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;

    // nothing to process - just trigger a LinkReportEvent
    LinkReportEvent* event_to_post = new LinkReportEvent();
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post_at_head(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through to the ExternalRouter which actually generates the report
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_query(SPtr_BundleEvent& sptr_event)
{
    BundleQueuedQueryRequest* request = nullptr;
    request = dynamic_cast<BundleQueuedQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a BundleQueuedQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = request->link_;
    ASSERT(link != nullptr);
    ASSERT(link->clayer() != nullptr);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_bundle_queued_query: "
              "query %s, checking if bundle *%p is queued on link *%p",
              request->query_id_.c_str(),
              request->bundle_.object(), link.object());
#endif
    
    bool is_queued = request->bundle_->is_queued_on(link->queue());
    BundleQueuedReportEvent* event_to_post = new BundleQueuedReportEvent(request->query_id_, is_queued);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // not curretly used
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_eid_reachable_query(SPtr_BundleEvent& sptr_event)
{
    EIDReachableQueryRequest* request = nullptr;
    request = dynamic_cast<EIDReachableQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a EIDReachableQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    Interface *iface = request->iface_;
    ASSERT(iface != nullptr);
    ASSERT(iface->clayer() != nullptr);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_eid_reachable_query: query %s, "
              "checking if endpoint %s is reachable via interface *%p",
              request->query_id_.c_str(), request->endpoint_.c_str(), iface);
#endif

    iface->clayer()->is_eid_reachable(request->query_id_,
                                      iface,
                                      request->endpoint_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_eid_reachable_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through to the external router to proces
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attribute_changed(SPtr_BundleEvent& sptr_event)
{
    LinkAttributeChangedEvent* event = nullptr;
    event = dynamic_cast<LinkAttributeChangedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkAttributeChangedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;

    if (link->isdeleted()) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("BundleDaemon::handle_link_attribute_changed: "
                  "link %s deleted", link->name());
#endif
        event->daemon_only_ = true;
        return;
    }

    // Update any state as necessary
    AttributeVector::iterator iter;
    for (iter = event->attributes_.begin();
         iter != event->attributes_.end();
         iter++)
    {
        if (iter->name() == "nexthop") {
            link->set_nexthop(iter->string_val());
        }
        else if (iter->name() == "how_reliable") {
            link->stats()->reliability_ = iter->u_int_val();
        }
        else if (iter->name() == "how_available") {
            link->stats()->availability_ = iter->u_int_val();
        }
    }
    log_info("LINK_ATTRIB_CHANGED *%p", link.object());
}
  
//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attributes_query(SPtr_BundleEvent& sptr_event)
{
    LinkAttributesQueryRequest* request = nullptr;
    request = dynamic_cast<LinkAttributesQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a LinkAttributesQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = request->link_;
    ASSERT(link != nullptr);
    ASSERT(link->clayer() != nullptr);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_link_attributes_query: query %s, link *%p",
              request->query_id_.c_str(), link.object());
#endif

    link->clayer()->query_link_attributes(request->query_id_,
                                          link,
                                          request->attribute_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through for the external router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_iface_attributes_query(SPtr_BundleEvent& sptr_event)
{
    IfaceAttributesQueryRequest* request = nullptr;
    request = dynamic_cast<IfaceAttributesQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a IfaceAttributesQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    Interface *iface = request->iface_;
    ASSERT(iface != nullptr);
    ASSERT(iface->clayer() != nullptr);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_iface_attributes_query: "
              "query %s, interface *%p", request->query_id_.c_str(), iface);
#endif

    iface->clayer()->query_iface_attributes(request->query_id_,
                                            iface,
                                            request->attribute_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_iface_attributes_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes thorugh to the external router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_parameters_query(SPtr_BundleEvent& sptr_event)
{
    CLAParametersQueryRequest* request = nullptr;
    request = dynamic_cast<CLAParametersQueryRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a CLAParametersQueryRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    ASSERT(request->cla_ != nullptr);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_cla_parameters_query: "
              "query %s, convergence layer %s",
              request->query_id_.c_str(), request->cla_->name());
#endif

    request->cla_->query_cla_parameters(request->query_id_,
                                        request->parameter_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_parameters_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes thorugh to the external router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_up(SPtr_BundleEvent& sptr_event)
{
    ContactUpEvent* event = nullptr;
    event = dynamic_cast<ContactUpEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ContactUpEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    const ContactRef contact = event->contact_;
    LinkRef link = contact->link();
    ASSERT(link != nullptr);

    if (link->isdeleted()) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("BundleDaemon::handle_contact_up: "
                  "cannot bring contact up on deleted link %s", link->name());
#endif
        event->daemon_only_ = true;
        return;
    }

    //ignore stale notifications that an old contact is up
    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::handle_contact_up");
    if (link->contact() != contact)
    {
        log_info("CONTACT_UP *%p (contact %p) being ignored (old contact)",
                 link.object(), contact.object());
        return;
    }
    
    log_info("CONTACT_UP *%p (contact %p)", link.object(), contact.object());
    link->set_state(Link::OPEN);
    link->stats_.contacts_++;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_down(SPtr_BundleEvent& sptr_event)
{
    ContactDownEvent* event = nullptr;
    event = dynamic_cast<ContactDownEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ContactDownEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    const ContactRef contact = event->contact_;
    int reason = event->reason_;
    LinkRef link = contact->link();
    ASSERT(link != nullptr);

    log_info("CONTACT_DOWN *%p (%s) (contact %p)",
             link.object(), ContactEvent::reason_to_str(reason),
             contact.object());

    // update the link stats
    link->stats_.uptime_ += (contact->start_time().elapsed_ms() / 1000);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;

    // trigger a Contact Report for the external router -- could just use this event
    ContactReportEvent* event_to_post = new ContactReportEvent();
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post_at_head(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through to the external router to generate the report
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_reassembly_completed(SPtr_BundleEvent& sptr_event)
{
    ReassemblyCompletedEvent* event = nullptr;
    event = dynamic_cast<ReassemblyCompletedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ReassemblyCompletedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    log_info("REASSEMBLY_COMPLETED bundle id %" PRIbid "",
             event->bundle_->bundleid());

    // remove all the fragments from the pending list
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != nullptr) {
        delete_bundle(ref);
    }

    // post a new event for the newly reassembled bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(event->bundle_.object(), EVENTSRC_FRAGMENTATION, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    post_at_head(sptr_event_to_post);
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_route_add(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes thorugh to the router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_recompute(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    log_info("ROUTE_RECOMPUTE processed");
    router()->recompute_routes();
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_del(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through to the router to process
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_query(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // trigger a Route Report for the external router -- could just use this event
    RouteReportEvent* event_to_post = new RouteReportEvent();
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post_at_head(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_report(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    // passes through to the external router to generate the report
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_signal(SPtr_BundleEvent& sptr_event)
{
    CustodySignalEvent* event = nullptr;
    event = dynamic_cast<CustodySignalEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a CustodySignalEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    log_info("CUSTODY_SIGNAL: %s %" PRIu64 ".%" PRIu64 " %s (%s)",
             event->data_.sptr_orig_source_eid_->c_str(),
             event->data_.orig_creation_tv_.secs_or_millisecs_,
             event->data_.orig_creation_tv_.seqno_,
             event->data_.succeeded_ ? "succeeded" : "failed",
             CustodySignal::reason_to_str(event->data_.reason_));

    GbofId gbof_id;
    gbof_id.set_source( event->data_.sptr_orig_source_eid_ );
    gbof_id.set_creation_ts( event->data_.orig_creation_tv_.secs_or_millisecs_, event->data_.orig_creation_tv_.seqno_);
    gbof_id.set_is_fragment(event->data_.admin_flags_ & 
                            BundleProtocolVersion6::ADMIN_IS_FRAGMENT);
    gbof_id.set_frag_length(gbof_id.is_fragment() ? 
                            event->data_.orig_frag_length_ : 0);
    gbof_id.set_frag_offset(gbof_id.is_fragment() ? 
                            event->data_.orig_frag_offset_ : 0);

    BundleRef orig_bundle("handle_custody_signal");
    orig_bundle = custody_bundles_->find(gbof_id.str());
    
    if (orig_bundle == nullptr) {
        log_warn("received custody signal for bundle %s %" PRIu64 ".%" PRIu64 " "
                 "but don't have custody",
                 event->data_.sptr_orig_source_eid_->c_str(),
                 event->data_.orig_creation_tv_.secs_or_millisecs_,
                 event->data_.orig_creation_tv_.seqno_);
        return;
    }

    // Update the event Bundle ID for passing on to the External Router (and possibly others)
    event->bundle_id_ = orig_bundle->bundleid();

    // release custody if either the signal succeded or if it
    // (paradoxically) failed due to duplicate transmission
    bool release = event->data_.succeeded_;
    if ((event->data_.succeeded_ == false) &&
        (event->data_.reason_ == BundleProtocol::CUSTODY_REDUNDANT_RECEPTION))
    {
        log_notice("releasing custody for bundle %s %" PRIu64 ".%" PRIu64 " "
                   "due to redundant reception",
                   event->data_.sptr_orig_source_eid_->c_str(),
                   event->data_.orig_creation_tv_.secs_or_millisecs_,
                   event->data_.orig_creation_tv_.seqno_);
        
        release = true;
    }

    if (release) {
        release_custody(orig_bundle.object());
        try_to_delete(orig_bundle, "CustodyReleased");
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_aggregate_custody_signal(SPtr_BundleEvent& sptr_event)
{
    // XXX/dz this currently needs to run from within the BD thread instead
    // of the BDACS thread as timing can cause a deadlock while trying
    // to update the BundleStore
    // XXX/dz the above may not be a problem now that the BDStorage thread
    // has been implemented...
    daemon_acs_->process_acs(sptr_event);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_timeout(SPtr_BundleEvent& sptr_event)
{
    CustodyTimeoutEvent* event = nullptr;
    event = dynamic_cast<CustodyTimeoutEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a CustodyTimeoutEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link   = event->link_;
    ASSERT(link != nullptr);

    // Check to see if the bundle is still pending
    BundleRef bref("handle_custody_timerout");
    bref = pending_bundles_->find(event->bundle_id_);
    
    if (bref != nullptr) {
        Bundle* bundle = bref.object();

        // remove and delete the expired timer from the bundle
        bool found = false;
        SPtr_CustodyTimer timer;
        CustodyTimerVec::iterator iter;

        oasys::ScopeLock l(bundle->lock(), "BundleDaemon::handle_custody_timeout");

        for (iter = bundle->custody_timers()->begin();
             iter != bundle->custody_timers()->end();
             ++iter)
        {
            timer = *iter;

            if (timer->link_name().compare(link->name()) == 0)
            {
                if (timer->pending()) {
                    log_err("multiple pending custody timers for link %s",
                            link->nexthop());
                    continue;
                }
            
                found = true;
                bundle->custody_timers()->erase(iter);
                break;
            }
    
        }

        if (!found) {
            log_err("custody timeout for *%p *%p: timer not found in bundle list",
                    bundle, link.object());
            return;
        }

        // modify the TRANSMITTED entry in the forwarding log to indicate
        // that we got a custody timeout. then when the routers go through
        // to figure out whether the bundle needs to be re-sent, the
        // TRANSMITTED entry is no longer in there
        bool ok = bref->fwdlog()->update(link, ForwardingInfo::CUSTODY_TIMEOUT);
        if (!ok) {
            log_err("custody timeout can't find ForwardingLog entry for link *%p",
                    link.object());
        }
    } else {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("custody timeout for Bundle ID %" PRIbid " which is no longer in pending list (link: *%p)",
                  event->bundle_id_, link.object());
#endif
    }

    // now fall through to let the router handle the event, typically
    // triggering a retransmission to the link in the event
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_shutdown_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;

    shutting_down_ = true;

    log_notice("Received shutdown request");

    oasys::SharedTimerReaperThread::instance()->shutdown();
    oasys::SharedTimerThread::instance()->pause_processing();

    // begin orderly shutdown of the LTP Engine
    if (ltp_engine_ != nullptr) {
        ltp_engine_->start_shutdown();
    }

    // stop interfaces to prevent new connections while shutting down
    InterfaceTable::instance()->shutdown();

    // close links that actually transfer bundles between nodes
    //(leave restage links open for now)
    close_standard_transfer_links();

    // shutdown the LTP Engine waiting for it to complete in-progress bundle extraction
    if (ltp_engine_ != nullptr) {
        ltp_engine_->shutdown();
        ltp_engine_ = nullptr;
    }

    // all incoming bundles should be queued by now
    // wait for the BDInput to possibly route pending bundles to the restager
    while (daemon_input_->event_queue_size() > 0) {
        usleep(100000);
    }

    // call the router shutdown procedure
    if (rtr_shutdown_proc_) {
        (*rtr_shutdown_proc_)(rtr_shutdown_data_);
    }


    // call the app shutdown procedure
    if (app_shutdown_proc_) {
        (*app_shutdown_proc_)(app_shutdown_data_);
    }

    daemon_acs_->set_should_stop();
    while (!daemon_acs_->is_stopped()) {
        usleep(100000);
    }
    
#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->set_should_stop();
#endif // DTPC_ENABLED

    // and then close the restage CLs
    close_restage_links();

    // Shutdown all actively registered convergence layers.
    ConvergenceLayer::shutdown_clayers();

    // stop the Bundle In Bundle Extractor
    bibe_extractor_->set_should_stop();

    // signal all objects to shutdown
    daemon_output_->shutdown();

    // wait for input queue to finish??
    daemon_input_->shutdown();

    log_always("shutting down - closing out storage with %zu pending events...", 
               daemon_storage_->event_queue_size());
    daemon_storage_->set_should_stop();
    daemon_storage_->commit_all_updates();
    log_always("finished storage close out");

    // signal to the main loop to bail
    set_should_stop();

    daemon_cleanup_->shutdown();

#ifdef BARD_ENABLED
    bundle_restaging_daemon_->shutdown();
#endif  // BARD_ENABLED

    // fall through -- the DTNServer will close and flush all the data
    // stores

    SchemeTable::reset();

    delete DTNScheme::instance();
    delete IPNScheme::instance();
    delete WildcardScheme::instance();

    CLVector::reset();

    // log the final statistics
    oasys::StringBuffer buf;
    buf.append("\n\nBundleDaemon shutting down - final statistics:\n");
    BundleDaemon::instance()->get_bundle_stats(&buf);
    log_multiline(oasys::LOG_ALWAYS, buf.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon:: close_standard_transfer_links()
{
    // close links that actually transfer bundles between nodes
    //(leave the Restage links open for now to allow processing of bundles
    // in the limbo statei: to be written to external storage but not there yet)

    // prevent contacts from being [re]opened and then close all open contacts(links)
    // - for LTP, this will also remove each node in LTPEngine as the links close
    do {
        contactmgr_->set_shutting_down();

        contactmgr_->lock()->lock(__func__);

        const LinkSet* links = contactmgr_->links();
        LinkSet::const_iterator iter = links->begin();

        // close open links
        while (iter != links->end())
        {
            contactmgr_->lock()->unlock();
            LinkRef link = *iter;

            if (strcmp(link->cl_name(), "restage") != 0) {
                if (link->isopen() || link->isopening()) {
                    link->close();
                    link->set_state(Link::UNAVAILABLE);
                }
                link->delete_link();
            }

            contactmgr_->lock()->lock("close_standard_transfer_links #2");
            ++iter;
        }

        contactmgr_->lock()->unlock();
    } while (false);   // end of scopelock

    do {
        contactmgr_->set_shutting_down();

        contactmgr_->lock()->lock(__func__);

        const LinkSet* links = contactmgr_->previous_links();
        LinkSet::const_iterator iter = links->begin();

        // close open links
        while (iter != links->end())
        {
            contactmgr_->lock()->unlock();
            LinkRef link = *iter;

            if (strcmp(link->cl_name(), "restage") != 0) {
                if (link->isopen() || link->isopening()) {
                    link->close();
                    link->set_state(Link::UNAVAILABLE);
                }
                link->delete_link();
            }

            contactmgr_->lock()->lock("close_standard_transfer_links #3");
            ++iter;
        }

        contactmgr_->lock()->unlock();
    } while (false);   // end of scopelock
}


//----------------------------------------------------------------------
void
BundleDaemon:: close_restage_links()
{
    do {
        contactmgr_->set_shutting_down();

        contactmgr_->lock()->lock(__func__);

        const LinkSet* links = contactmgr_->links();
        LinkSet::const_iterator iter = links->begin();

        // close open links
        while (iter != links->end())
        {
            contactmgr_->lock()->unlock();
            LinkRef link = *iter;

            if (strcmp(link->cl_name(), "restage") == 0) {
                if (link->isopen()) {
                    link->close();
                }
            }

            contactmgr_->lock()->lock("close_restage_links #2");
            ++iter;
        }
    } while (false);   // end of scopelock
}



//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_set_params(SPtr_BundleEvent& sptr_event)
{
    CLASetParamsRequest* request = nullptr;
    request = dynamic_cast<CLASetParamsRequest*>(sptr_event.get());
    if (request == nullptr) {
        log_err("Error casting event type %s as a CLASetParamsRequest", 
                event_to_str(sptr_event->type_));
        return;
    }

    ASSERT(request->cla_ != nullptr);
    request->cla_->set_cla_parameters(request->parameters_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_status_request(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    log_info("Received status request");
}

//----------------------------------------------------------------------
void
BundleDaemon::event_handlers_completed(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    if (shutting_down_) {
        return;
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    bool ok_to_route = true;

    oasys::ScopeLock scoplok(&pending_lock_, __func__);

    pending_bundles_->push_back(bundle);
    dupefinder_bundles_->push_back(bundle);

#if BARD_ENABLED
    bundle_restaging_daemon_->bundle_accepted(bundle);
#endif // BARD_ENABLED

    if (add_to_store) {
        actions_->store_add(bundle);
    }

    return ok_to_route;
}


//----------------------------------------------------------------------
bool
BundleDaemon::query_accept_bundle_based_on_quotas(Bundle* bundle, bool& payload_space_reserved, 
                                                  uint64_t& amount_previously_reserved_space)
{
    // Some convergence layers previously reserved payload quota space based on the incoming 
    // length of bundle data which is larger than the actual payload length so we need to handle
    // that case as well as a first attempt to reserve payload quota.

    // -- allow for repeated calls to query the BARD after payload space has been reserved
    //   and applied to the bundle
    payload_space_reserved = bundle->payload_space_reserved();

    uint64_t payload_len = bundle->payload().length();

    BundleStore* bs = BundleStore::instance();

    if (payload_space_reserved) {
        // if bundle already has applied the reserved payload quota space then
        // the previously reserved space must be zero
        ASSERT(amount_previously_reserved_space == 0);

    } else if (amount_previously_reserved_space > 0) {
        ASSERT(amount_previously_reserved_space >= payload_len);

        // payload quota space was previously reserved
        // - apply it to the bundle
        bs->reserve_payload_space(bundle);
        
        // - and release the temp reserved space
        bs->release_payload_space(amount_previously_reserved_space);

        amount_previously_reserved_space = 0;
        payload_space_reserved = true;
    }


    // try to reserve storage space for the payload 
    if (!payload_space_reserved) {


        uint64_t tmp_payload_storage_bytes_reserved = 0;
        uint32_t ctr = 0;
        while (!should_stop() && (++ctr < 50) && (tmp_payload_storage_bytes_reserved != payload_len))
        {
            if (bs->try_reserve_payload_space(payload_len)) {
                tmp_payload_storage_bytes_reserved = payload_len;
            } else {
                usleep(100000);
            }
        }
        
        if (!should_stop()) {
            if (tmp_payload_storage_bytes_reserved == payload_len) {
                // bundle already approved to use payload space
                bs->reserve_payload_space(bundle);
    
                // release the temp reserved space since the bundle now has the actually space reserved
                bs->release_payload_space(tmp_payload_storage_bytes_reserved);

                payload_space_reserved = true;
            }
        }
    }

    bool result = payload_space_reserved;

#ifdef BARD_ENABLED
    // if okay to accept bundle into the overall payload quota then
    // query the BARD for internal or external storage acceptance
    if (result) {
        result = bundle_restaging_daemon_->query_accept_bundle(bundle);

        //log_always("dzdebug - %s - called BARD->query_accept_bundle(*%p) - result=%s",
        //           __func__, bundle, result?"true":"false");
    }
#endif // BARD_ENABLED

    return result;
}

//----------------------------------------------------------------------
bool
BundleDaemon::query_accept_bundle_based_on_quotas_internal_storage_only(Bundle* bundle, 
                                                                        bool& payload_space_reserved, 
                                                                        uint64_t& amount_previously_reserved_space)
{
    // Some convergence layers previously reserved payload quota space based on the incoming 
    // length of bundle data which is larger than the actual payload length so we need to handle
    // that case as well as a first attempt to reserve payload quota.

    // -- allow for repeated calls to query the BARD after payload space has been reserved
    //   and applied to the bundle
    payload_space_reserved = bundle->payload_space_reserved();

    uint64_t payload_len = bundle->payload().length();

    BundleStore* bs = BundleStore::instance();

    if (payload_space_reserved) {
        // if bundle already has applied the reserved payload quota space then
        // the previously reserved space must be zero
        ASSERT(amount_previously_reserved_space == 0);

    } else if (amount_previously_reserved_space > 0) {
        ASSERT(amount_previously_reserved_space >= payload_len);

        // payload quota space was previously reserved
        // - apply it to the bundle
        bs->reserve_payload_space(bundle);
        
        // - and release the temp reserved space
        bs->release_payload_space(amount_previously_reserved_space);

        amount_previously_reserved_space = 0;
        payload_space_reserved = true;
    }


    // try to reserve storage space for the payload 
    if (!payload_space_reserved) {


        uint64_t tmp_payload_storage_bytes_reserved = 0;
        uint32_t ctr = 0;
        while (!should_stop() && (++ctr < 50) && (tmp_payload_storage_bytes_reserved != payload_len))
        {
            if (bs->try_reserve_payload_space(payload_len)) {
                tmp_payload_storage_bytes_reserved = payload_len;
            } else {
                usleep(100000);
            }
        }
        
        if (!should_stop()) {
            if (tmp_payload_storage_bytes_reserved == payload_len) {
                // bundle already approved to use payload space
                bs->reserve_payload_space(bundle);
    
                // release the temp reserved space since the bundle now has the actually space reserved
                bs->release_payload_space(tmp_payload_storage_bytes_reserved);

                payload_space_reserved = true;
            }
        }
    }

    bool result = payload_space_reserved;

#ifdef BARD_ENABLED
    // if okay to accept bundle into the overall payload quota then
    // query the BARD for internal storage acceptance
    if (result) {
        result = bundle_restaging_daemon_->query_accept_bundle_internal_storage_only(bundle);

        //log_always("dzdebug - %s - called BARD->query_accept_bundle_internal_storage_only(*%p) - result=%s",
        //           __func__, bundle, result?"true":"false");
    }
#endif // BARD_ENABLED

    return result;
}

//----------------------------------------------------------------------
bool
BundleDaemon::query_accept_bundle_after_failed_restage(Bundle* bundle)
{

    // -- allow for repeated calls to query the BARD after payload space has been reserved
    //   and applied to the bundle
    uint64_t payload_space_reserved = bundle->payload_space_reserved();

    uint64_t payload_len = bundle->payload().length();

    BundleStore* bs = BundleStore::instance();

    // try to reserve storage space for the payload 
    if (!payload_space_reserved) {

        uint64_t tmp_payload_storage_bytes_reserved = 0;
        uint32_t ctr = 0;
        while (!should_stop() && (++ctr < 50) && (tmp_payload_storage_bytes_reserved != payload_len))
        {
            if (bs->try_reserve_payload_space(payload_len)) {
                tmp_payload_storage_bytes_reserved = payload_len;
            } else {
                usleep(100000);
            }
        }
        
        if (!should_stop()) {
            if (tmp_payload_storage_bytes_reserved == payload_len) {
                // bundle already approved to use payload space
                bs->reserve_payload_space(bundle);
    
                // release the temp reserved space since the bundle now has the actually space reserved
                bs->release_payload_space(tmp_payload_storage_bytes_reserved);

                payload_space_reserved = true;
            }
        }
    }

    bool result = payload_space_reserved;

#ifdef BARD_ENABLED
    // if okay to accept bundle into the overall payload quota then
    // query the BARD for internal or external storage acceptance
    if (result) {
        result = bundle_restaging_daemon_->query_accept_bundle_after_failed_restage(bundle);

        //log_always("dzdebug - %s - called BARD->query_accept_bundle(*%p) - result=%s",
        //           __func__, bundle, result?"true":"false");
    }
#endif // BARD_ENABLED

    return result;
}


//----------------------------------------------------------------------
void
BundleDaemon::release_bundle_without_bref_reserved_space(Bundle* bundle)
{
    if (bundle->payload_space_reserved()) {
        BundleStore* bs = BundleStore::instance();

        bs->release_payload_space(bundle->payload().length());
    }

#ifdef BARD_ENABLED
    bundle_restaging_daemon_->bundle_deleted(bundle);
#endif // BARD_ENABLED
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_from_pending(const BundleRef& bundle)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("removing bundle *%p from pending list - time to expiration: %"  PRIi64, 
             bundle.object(), bundle->time_to_expiration());
#endif

    // try to cancel the expiration timer if it's still
    // around
   
    SPtr_ExpirationTimer exptmr = bundle->expiration_timer();


    if (exptmr != nullptr) {
        bundle->clear_expiration_timer();

        // cancel also releases its bundle ref
        bool cancelled = exptmr->cancel();

        if (!cancelled && !shutting_down_) {
            log_debug("unexpected error cancelling expiration timer "
                      "for bundle *%p", bundle.object());
        }
    }

    bool erased = pending_bundles_->erase(bundle.object());
    dupefinder_bundles_->erase(bundle.object());

    if (!erased) {
        log_err("unexpected error removing bundle from pending list");
    }

    return erased;
}

//----------------------------------------------------------------------
bool
BundleDaemon::try_to_delete(const BundleRef& bundle, const char* event_type)
{
    (void) event_type;

    /*
     * Check to see if we should remove the bundle from the system.
     * 
     * If we're not configured for early deletion, this never does
     * anything. Otherwise it relies on the router saying that the
     * bundle can be deleted.
     */

    if (! bundle->is_queued_on(pending_bundles_))
    {
        // this can happen bundle is already expired or was delivered locally
        return false;
    }

    if (!params_.early_deletion_) {
        return false;
    }

    if (! router_->can_delete_bundle(bundle)) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("try_to_delete(%s - *%p): not deleting because "
                  "router wants to keep bundle",
                  event_type, bundle.object());
#endif
        return false;
    }
    
    return delete_bundle(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_bundle(const BundleRef& bundleref,
                            status_report_reason_t reason)
{
    Bundle* bundle = bundleref.object();

    // send a bundle deletion status report if we have custody or the
    // bundle's deletion status report request flag is set and a reason
    // for deletion is provided
    bool send_status = (bundle->local_custody() ||
                       (bundle->deletion_rcpt() &&
                        reason != BundleProtocol::REASON_NO_ADDTL_INFO));
        
    // check if we have custody, if so, remove it
    if (bundle->local_custody() || bundle->bibe_custody()) {
        release_custody(bundle);
    }

    // XXX/demmer if custody was requested but we didn't take it yet
    // (due to a validation error, space constraints, etc), then we
    // should send a custody failed signal here

    // check if bundle is a fragment, if so, remove any fragmentation state
    if (bundle->is_fragment()) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("Calling fragmentmgr_->delete_fragment");
#endif
        fragmentmgr_->delete_fragment(bundle);
    }

    // notify the router that it's time to delete the bundle
    router_->delete_bundle(bundleref);


    // move the bundle to the deleted list
    if (!bundle->is_queued_on(deleting_bundles_)) {
        deleting_bundles_->push_back(bundle);
    }

    // delete the bundle from the pending list
    bool erased = true;
    if (bundle->is_queued_on(pending_bundles_)) {
        erased = delete_from_pending(bundleref);
    }

    if (erased && send_status) {
        generate_status_report(bundle, BundleStatusReport::STATUS_DELETED, reason);
    }

    // cancel the bundle on all links where it is queued or in flight
#ifdef BD_LOG_DEBUG_ENABLED
    oasys::Time now;
    now.get_time();
#endif


    if (bundle->num_mappings() > 2) {
        oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::delete_bundle");
        const LinkSet* links = contactmgr_->links();
        LinkSet::const_iterator iter;
        for (iter = links->begin(); iter != links->end(); ++iter) {
            const LinkRef& link = *iter;
            
            if (link->queue()->contains(bundle) ||
                link->inflight()->contains(bundle))
            {
                actions_->cancel_bundle(bundle, link);
            }
        }
    }

    if (bundle->num_mappings() > 2) {
        // cancel the bundle on all API registrations
        RegistrationList matches;
        RegistrationList::iterator reglist_iter;

        reg_table_->get_matching(bundle->dest(), &matches);

        SPtr_Registration sptr_reg;
        for (reglist_iter = matches.begin(); reglist_iter != matches.end(); ++reglist_iter) {
            sptr_reg  = *reglist_iter;
            sptr_reg ->delete_bundle(bundle);
        }
    }

    // XXX/demmer there may be other lists where the bundle is still
    // referenced so the router needs to be told what to do...
    
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon: canceling deleted bundle on all links took %u ms",
               now.elapsed_ms());
#endif


    // remove the bundle from the all_bundles_ list
    all_bundles_->erase(bundle);

    return erased;
}

//----------------------------------------------------------------------
Bundle*
BundleDaemon::find_duplicate(Bundle* bundle)
{
    Bundle *found = nullptr;
    BundleRef bref("find_duplicate temporary");

    // if custody requested check for a duplicate already in custody
    if (bundle->custody_requested() ) {
        bref = custody_bundles_->find(bundle->gbofid_str());
        found = bref.object();
    }

    // if no duplicate in custody check the pending bundles for a dup 
    if ( nullptr == found ) {
        bref = dupefinder_bundles_->find(bundle->gbofid_str());
        found = bref.object();
    }

    // XXX/dz Note that the original code checked for equality
    // in the payload length as well. The TableBasedRouter only
    // checks the GbofId info to decide if it is a duplicate and
    // issues a DeleteBundleRequest if so regardless of the 
    // suppress_duplicates setting. Seems to me that either
    // the GbofId is sufficient to decide if it is a dup or you
    // need to check the entire contents (hash?) to decide and
    // it should be consistent throughout the code.

    return found;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_free(SPtr_BundleEvent& sptr_event)
{
    BundleFreeEvent* event = nullptr;
    event = dynamic_cast<BundleFreeEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleFreeEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    Bundle* bundle = event->bundle_;
    event->bundle_ = nullptr;

    ASSERT(bundle->refcount() == 1);

    if (!deleting_bundles_->contains(bundle)) {
        if (all_bundles_->contains(bundle)) {
            all_bundles_->erase(bundle);
        }
    } else {
        deleting_bundles_->erase(bundle);
    }
    
    bundle->lock()->lock("BundleDaemon::handle_bundle_free");

#if BARD_ENABLED
    bundle_restaging_daemon_->bundle_deleted(bundle);
#endif // BARD_ENABLED

    delete bundle;

    ++stats_.deleted_bundles_;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
    
    if (! sptr_event->daemon_only_) {
        if (!shutting_down_) {
            // dispatch the event to the router(s) and the contact manager
            router_->post(sptr_event);
            contactmgr_->handle_event(sptr_event);
        }
    }

    event_handlers_completed(sptr_event);

    stats_.events_processed_++;

    if (sptr_event->processed_notifier_) {
        sptr_event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::load_registrations()
{
    sptr_admin_reg_ = std::make_shared<AdminRegistration>();
    {
        BundleEvent* event = new RegistrationAddedEvent(sptr_admin_reg_, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    }

    if (sptr_local_eid_->is_dtn_scheme()) {
        std::string eid_str = sptr_local_eid_->uri().authority() + "/ping";
        SPtr_EID sptr_ping_eid = make_eid_dtn(eid_str);
        sptr_ping_reg_ = std::make_shared<PingRegistration>(sptr_ping_eid);

        BundleEvent* event = new RegistrationAddedEvent(sptr_ping_reg_, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    }

    if (sptr_local_eid_ipn_->is_ipn_scheme()) {
        // create an AdminReg for the local IPN EID if defined
        sptr_admin_reg_ipn_ = std::make_shared<AdminRegistrationIpn>();
        BundleEvent* event = new RegistrationAddedEvent(sptr_admin_reg_ipn_, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    } else {
        // otherwise set the local IPN EID to match the local DTN EID
        // so that will be used when working with IPN destined bundles
        sptr_local_eid_ipn_ = sptr_local_eid_;
    }

    if (params_.ipn_echo_service_number_ > 0) {
        if (sptr_local_eid_ipn_->is_ipn_scheme()) {
            SPtr_EID sptr_ipn_echo_eid = make_eid_ipn(sptr_local_eid_ipn_->node_num(), 
                                                      params_.ipn_echo_service_number_);
            sptr_ipn_echo_reg_ = std::make_shared<IpnEchoRegistration>(sptr_ipn_echo_eid);

            {
                BundleEvent* event = new RegistrationAddedEvent(sptr_ipn_echo_reg_, EVENTSRC_ADMIN);
                SPtr_BundleEvent sptr_event(event);
                handle_event(sptr_event);
            }
        }
    }

    /// The IMC Group Petition registration
    if (true) {
        SPtr_EID sptr_imc_group_petition_eid = make_eid("imc:0.0");
        sptr_imc_group_petition_reg_ = std::make_shared<IMCGroupPetitionRegistration>(sptr_imc_group_petition_eid);
        BundleEvent* event = new RegistrationAddedEvent(sptr_imc_group_petition_reg_, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    }

    /// The ION Contact Plan Sync registration
    if (true) {
        SPtr_EID sptr_ion_contact_pl_sync_eid = make_eid("imc:0.1");
        sptr_ion_contact_plan_sync_reg_ = std::make_shared<IonContactPlanSyncRegistration>(sptr_ion_contact_pl_sync_eid);
        BundleEvent* event = new RegistrationAddedEvent(sptr_ion_contact_plan_sync_reg_, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    }



    Registration* reg;
    RegistrationStore* reg_store = RegistrationStore::instance();
    RegistrationStore::iterator* iter = reg_store->new_iterator();

    while (iter->next() == 0) {
        reg = reg_store->get(iter->cur_val());
        SPtr_Registration sptr_reg(reg);

        if (reg == nullptr) {
            log_err("error loading registration %d from data store",
                    iter->cur_val());
            continue;
        }
        
        reg->set_in_datastore(true);
        daemon_storage_->reloaded_registration();

        //RegistrationAddedEvent e(reg, EVENTSRC_STORE);
        //handle_event(&e);
        BundleEvent* event = new RegistrationAddedEvent(sptr_reg, EVENTSRC_STORE);
        SPtr_BundleEvent sptr_event(event);
        handle_event(sptr_event);
    }

    delete iter;
}

//----------------------------------------------------------------------
void
BundleDaemon::load_previous_links()
{
    Link* link;
    LinkStore* link_store = LinkStore::instance();
    LinkStore::iterator* iter = link_store->new_iterator();
    LinkRef link_ref;

    if (! params_.persistent_links_)
    {
        int cnt = 0;
        std::string key;
        while (iter->next() == 0 && cnt < 100000) {   // prevent endless loop just in case
            key = iter->cur_val();
            delete iter;            

            link_store->del(key);
            ++cnt;

            iter = link_store->new_iterator();
        }
        log_info("load_previous_links - Persistent Links is disabled - deleted %d links from database", cnt);

        delete iter;
        return;
    }


    /*!
     * By default this routine is called during the start of the run of the BundleDaemon
     * event loop, but this may need to be preempted if the DTNME configuration file
     * 'link add' commands
     */
    if (load_previous_links_executed_)
    {
    	return;
    }

    load_previous_links_executed_ = true;

    while (iter->next() == 0) {
        link = link_store->get(iter->cur_val());
        if (link == nullptr) {
            log_err("error loading link %s from data store",
                    iter->cur_val().c_str());
            continue;
         }

        link->set_in_datastore(true);
        daemon_storage_->reloaded_link();

        link_ref = LinkRef(link, "Read from datastore");
        contactmgr_->add_previous_link(link_ref);
    }

    // Check links created in config file have names consistent with previous usage
    contactmgr_->config_links_consistent();

    // If configured, reincarnate other non-OPPORTUNISTIC links recorded from previous
    // runs of the daemon.
    if (params_.recreate_links_on_restart_)
    {
    	contactmgr_->reincarnate_links();
    }

    delete iter;
}

//----------------------------------------------------------------------
bool
BundleDaemon::load_bundles()
{
    Bundle* bundle;
    BundleStore* bundle_store = BundleStore::instance();
    BundleStore::iterator* iter = bundle_store->new_iterator();

    log_notice("loading bundles from data store");


    u_int64_t total_size = 0;

    std::vector<Bundle*> doa_bundles;

    size_t num_bundles_loaded = 0;

    int iter_status = iter->begin();

    while ((iter_status == oasys::DS_OK) && iter->more()) {
        bundle = bundle_store->get(iter->cur_val());
        
        if (bundle == nullptr) {
            log_err("error loading bundle %" PRIbid " from data store",
                    iter->cur_val());

            iter_status = iter->next();
            continue;
        }

        total_size += bundle->durable_size();

        daemon_storage_->reloaded_bundle();

        bundle_store->reserve_payload_space(bundle);

        // if the bundle payload file is missing, we need to kill the
        // bundle, but we can't do so while holding the durable
        // iterator or it may deadlock, so cleanup is deferred 
        if (bundle->payload().location() != BundlePayload::DISK) {
            log_err("error loading payload for *%p from data store",
                    bundle);
            doa_bundles.push_back(bundle);

            iter_status = iter->next();
            continue;
        }

        ++num_bundles_loaded;

        // reset the flags indicating it is in the datastore
        bundle->set_in_datastore(true);
        bundle->set_queued_for_datastore(true);


        BundleProtocol::reload_post_process(bundle);

        //WAS
        //BundleReceivedEvent e(bundle, EVENTSRC_STORE);
        //handle_event(&e);

        // We're going to post this to the event queue, since 
        // the act of determining the registration(s) to which the
        // bundle should be delivered will cause the bundle to be
        // updated in the store (as the PENDING deliveries are
        // marked).
        // Note that since delivery is via the DELIVER_TO_REG
        // event, delivery will happen one event queue loop after
        // the receivedEvent is processed.
        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
        BundleReceivedEvent* event_to_post;
        event_to_post = new BundleReceivedEvent(bundle, EVENTSRC_STORE, sptr_dummy_prevhop);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        post(sptr_event_to_post);

        // in the constructor, we disabled notifiers on the event
        // queue, so in case loading triggers other events, we just
        // let them queue up and handle them later when we're done
        // loading all the bundles


        iter_status = iter->next();
    }
    
    delete iter;

    if (iter_status != oasys::DS_NOTFOUND) {
        log_crit("%s - Error while reading BundleStore database - aborting", __func__);
        return false;
    }

    size_t num_doa = doa_bundles.size();

    // now that the durable iterator is gone, purge the doa bundles
    for (unsigned int i = 0; i < doa_bundles.size(); ++i) {
        actions_->store_del(doa_bundles[i]);
        delete doa_bundles[i];
    }

    log_always("Loaded %zu bundles from storage; %zu bundles had errors reading the payload file and were deleted",
               num_bundles_loaded, num_doa);

    return true;
}

//----------------------------------------------------------------------
void
BundleDaemon::load_pendingacs()
{
    daemon_acs_->load_pendingacs();
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_delivery_events(Bundle* bundle)
{
    oasys::ScopeLock(bundle->fwdlog()->lock(), "generating delivery events");

    ForwardingLog::Log flog = bundle->fwdlog()->log();
    ForwardingLog::Log::const_iterator iter;
    for (iter = flog.begin(); iter != flog.end(); ++iter)
    {
        const ForwardingInfo* info = &(*iter);

        if ( info->regid() != 0 ) {
            if ( info->state() == ForwardingInfo::PENDING_DELIVERY ) {
                DeliverBundleToRegEvent* event;
                event = new DeliverBundleToRegEvent(bundle, info->regid());
                SPtr_BundleEvent sptr_event(event);
                BundleDaemon::post(sptr_event);
            }
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::DaemonIdleExit::is_idle(const struct timeval& tv)
{
    oasys::Time now(tv.tv_sec, tv.tv_usec);
    u_int elapsed = (now - BundleDaemon::instance()->last_event_).in_milliseconds();

    BundleDaemon* d = BundleDaemon::instance();
    d->logf(oasys::LOG_DEBUG,
            "checking if is_idle -- last event was %u msecs ago",
            elapsed);

    // fudge
    if (elapsed + 500 > interval_ * 1000) {
        d->logf(oasys::LOG_NOTICE,
                "more than %u seconds since last event, "
                "shutting down daemon due to idle timer",
                interval_);
        
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::init_idle_shutdown(int interval)
{
    idle_exit_ = std::make_shared<DaemonIdleExit>(interval);
    oasys::SPtr_IdleTclExit sptr = idle_exit_;
    idle_exit_->start(sptr);
}

//----------------------------------------------------------------------
void
BundleDaemon::run()
{
    char threadname[16] = "BD-Main";
    pthread_setname_np(pthread_self(), threadname);
   
    // pause to give the TCL console time to start - timing issue caused buffer overflow detected error
    sleep(1);

    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }

    // start up the timer thread to manage the timer system
    oasys::SharedTimerThread::init();

    // If you are using the version of oasys that has the TimerReaperThread then
    // this will start it up. If not then cancelled timers will be recovered 
    // when they expire.
    oasys::SharedTimerReaperThread::init();

    bibe_extractor_ = std::unique_ptr<BIBEExtractor>(new BIBEExtractor());

    if (params_.ltp_engine_id_ == 0) {
        if (sptr_local_eid_ipn_->is_ipn_scheme()) {
            params_.ltp_engine_id_ = sptr_local_eid_ipn_->node_num();
        }
    }

    ltp_engine_ = std::make_shared<LTPEngine>(params_.ltp_engine_id_);

#ifdef BARD_ENABLED
    // BARD must be started before reloading bundles
    bundle_restaging_daemon_->start();
#endif  // BARD_ENABLED

    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
    router_->initialize();
    router_->start();

    load_registrations();
    load_previous_links();
    if (!load_bundles()) {
        // abort - load_bundles already output a crit message
        exit(1);
    }


    daemon_storage_->start();

    load_pendingacs();
    daemon_acs_->start();

    SPtr_BundleEvent sptr_event;

    // delaying start of these two threads to provide time to process
    // configuration file events (link and route definitions) before
    // we start receiving bundles through the convergence layers
    daemon_output_->start_delayed(2000);
    daemon_input_->start_delayed(2000);  // 2000 == 2 seconds
    daemon_cleanup_->start();

    last_event_.get_time();
    
#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->start();
#endif // DTPC_ENABLED

    // Start the inerfaces running
    InterfaceTable::instance()->activate_interfaces();


    //log_always("dzdebug - sizes - Bundle:%zu  ForwardingLog:%zu  CustTimerVec:%zu BTimestamp:%zu GbofId:%zu OTime:%zu EID:%zu", 
    //           sizeof(Bundle), sizeof(ForwardingLog), sizeof(CustodyTimerVec),
    //           sizeof(BundleTimestamp), sizeof(GbofId), sizeof(oasys::Time),
    //           sizeof(EndpointID));

    //log_always("dzdebug - sizes - Bundle:%zu  LinkBlockSet:%zu MetadataSet:%zu LinkMetadataSet:%zu BundleMappings:%zu Sptr:%zu",
    //           sizeof(Bundle), 
    //           sizeof(LinkBlockSet), sizeof(MetadataVec), sizeof(LinkMetadataSet),
    //           sizeof(BundleMappings), sizeof(SPtr_EID));


    while (1) {
        if (should_stop()) {
            if (0 == me_eventq_.size()) {
                log_debug("BundleDaemon: stopping");
                break;
            }
        }

        if (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);
            
            // handle the event
            handle_event(sptr_event);

            // record the last event time
            last_event_.get_time();

            // clean up the event
            sptr_event.reset();
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }

    // moved to DTNServer.cc:   oasys::TimerThread::instance()->shutdown();
}

//----------------------------------------------------------------------
void
BundleDaemon::add_bibe_bundle_to_acs(int32_t bp_version, const SPtr_EID& sptr_custody_eid,
                                     uint64_t transmission_id, bool success, uint8_t reason)
{
    if (!shutting_down_) {
        daemon_acs_->add_bibe_bundle_to_acs(bp_version, sptr_custody_eid, transmission_id, success, reason);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::bibe_erase_from_custodyid_list(Bundle* bundle)
{
    if (!shutting_down_) {
        daemon_acs_->bibe_erase_from_custodyid_list(bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::bibe_accept_custody(Bundle* bundle)
{
    if (!shutting_down_) {
        daemon_acs_->bibe_accept_custody(bundle);
        actions_->store_update(bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::switch_bp6_to_bibe_custody(Bundle* bundle, std::string& bibe_custody_dest)
{
    if (!shutting_down_) {
        bibe_erase_from_custodyid_list(bundle);

        oasys::ScopeLock scoplok(bundle->lock(), __func__);

        bundle->set_custody_dest(bibe_custody_dest);
        bibe_accept_custody(bundle);

        scoplok.unlock();

        actions_->store_update(bundle);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::bibe_extractor_post(Bundle* bundle, SPtr_Registration& sptr_reg)
{
    if (!shutting_down_) {
        bibe_extractor_->post(bundle, sptr_reg);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::bundle_delete_from_storage(Bundle* bundle)
{
    if (!shutting_down_) {
        daemon_storage_->bundle_delete(bundle);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::bundle_add_update_in_storage(Bundle* bundle)
{
    if (!shutting_down_) {
        daemon_storage_->bundle_add_update(bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::acs_accept_custody(Bundle* bundle)
{
    if (!shutting_down_) {
        daemon_acs_->accept_custody(bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::acs_post_at_head(BundleEvent* event)
{
    if (!shutting_down_) {
        daemon_acs_->post_event(event, false);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::reloaded_pendingacs()
{
    if (!shutting_down_) {
        daemon_storage_->reloaded_pendingacs();
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::set_route_acs_params(SPtr_EIDPattern& sptr_pat, bool enabled, 
                                   u_int acs_delay, u_int acs_size)
{
    if (!shutting_down_) {
        daemon_acs_->set_route_acs_params(sptr_pat, enabled, acs_delay, acs_size);
    }
}

//----------------------------------------------------------------------
int
BundleDaemon::delete_route_acs_params(SPtr_EIDPattern& sptr_pat)
{
    if (!shutting_down_) {
        return daemon_acs_->delete_route_acs_params(sptr_pat);
    }
    return 0;
}

//----------------------------------------------------------------------
void
BundleDaemon::dump_acs_params(oasys::StringBuffer* buf)
{
    if (!shutting_down_) {
        daemon_acs_->dump_acs_params(buf);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::storage_get_stats(oasys::StringBuffer* buf)
{
    if (!shutting_down_) {
        daemon_storage_->get_stats(buf);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::registration_add_update_in_storage(SPtr_Registration& sptr_reg)
{
    if (!shutting_down_) {
        daemon_storage_->registration_add_update(sptr_reg);
    }
}


void BundleDaemon::inc_ltp_retran_timers_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_retran_timers_created_; 
}
void BundleDaemon::inc_ltp_retran_timers_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_retran_timers_deleted_; 
}

void BundleDaemon::inc_ltp_inactivity_timers_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_inactivity_timers_created_; 
}
void BundleDaemon::inc_ltp_inactivity_timers_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_inactivity_timers_deleted_; 
}

void BundleDaemon::inc_ltp_closeout_timers_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_closeout_timers_created_; 
}
void BundleDaemon::inc_ltp_closeout_timers_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_closeout_timers_deleted_; 
}

void BundleDaemon::inc_ltpseg_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltpseg_created_; 
}
void BundleDaemon::inc_ltpseg_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltpseg_deleted_; 
}


void BundleDaemon::inc_ltp_ds_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_ds_created_; 
}
void BundleDaemon::inc_ltp_ds_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_ds_deleted_; 
}


void BundleDaemon::inc_ltp_rs_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_rs_created_; 
}
void BundleDaemon::inc_ltp_rs_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltp_rs_deleted_; 
}


void BundleDaemon::inc_ltpsession_created()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltpsession_created_; 
}
void BundleDaemon::inc_ltpsession_deleted()
{
    oasys::ScopeLock scoplok(&ltp_stats_lock_, __func__);
    ++ltpsession_deleted_; 
}

void BundleDaemon::inc_bibe6_encapsulations()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe6_encapsulations_;
}
void BundleDaemon::inc_bibe6_extraction_errors()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe6_extraction_errors_;
}
void BundleDaemon::inc_bibe6_extractions()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe6_extractions_;
}

void BundleDaemon::inc_bibe7_encapsulations()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe7_encapsulations_;
}
void BundleDaemon::inc_bibe7_extraction_errors()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe7_extraction_errors_;
}
void BundleDaemon::inc_bibe7_extractions()
{
    oasys::ScopeLock scoplok(&bibe_stats_lock_, __func__);
    ++bibe7_extractions_;
}


#if BARD_ENABLED
//----------------------------------------------------------------------
void
BundleDaemon::bard_bundle_restaged(Bundle* bundle, size_t disk_usage)
{
    if (!shutting_down_) {
        ++stats_.restaged_bundles_;
        bundle_restaging_daemon_->bundle_restaged(bundle, disk_usage);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::bard_bundle_deleted(Bundle* bundle)
{
    if (!shutting_down_) {
        bundle_restaging_daemon_->bundle_deleted(bundle);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::bard_generate_usage_report_for_extrtr(BARDNodeStorageUsageMap& bard_usage_map,
                                                    RestageCLMap& restage_cl_map)
{
    bundle_restaging_daemon_->generate_usage_report_for_extrtr(bard_usage_map, restage_cl_map);
}

#endif // BARD_ENABLED


//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid(const std::string& eid_str)
{
    return qptr_eid_pool_->make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid(const char* eid_cstr)
{
    return qptr_eid_pool_->make_eid(eid_cstr);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_dtn(const std::string& host, const std::string& service)
{
    return qptr_eid_pool_->make_eid_dtn(host, service);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_dtn(const std::string& ssp)
{
    return qptr_eid_pool_->make_eid_dtn(ssp);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_ipn(size_t node_num, size_t service_num)
{
    return qptr_eid_pool_->make_eid_ipn(node_num, service_num);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_imc(size_t group_num, size_t service_num)
{
    return qptr_eid_pool_->make_eid_imc(group_num, service_num);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_scheme(const char* scheme, const char* ssp)
{
    return qptr_eid_pool_->make_eid_scheme(scheme, ssp);
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_null()
{
    return qptr_eid_pool_->make_eid_null();
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::null_eid()
{
    return qptr_eid_pool_->null_eid();
}

//----------------------------------------------------------------------
SPtr_EID
BundleDaemon::make_eid_imc00()
{
    return qptr_eid_pool_->make_eid_imc00();
}

//----------------------------------------------------------------------
void
BundleDaemon::dump_eid_pool(oasys::StringBuffer* buf)
{
    qptr_eid_pool_->dump(buf);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::make_pattern(const std::string& pattern_str)
{
    return qptr_eid_pool_->make_pattern(pattern_str);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::make_pattern(const char* pattern_cstr)
{
    return qptr_eid_pool_->make_pattern(pattern_cstr);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::make_pattern_null()
{
    return qptr_eid_pool_->make_pattern_null();
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::make_pattern_wild()
{
    return qptr_eid_pool_->make_pattern_wild();
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::append_pattern_wild(const SPtr_EID& sptr_eid)
{
    return qptr_eid_pool_->append_pattern_wild(sptr_eid);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
BundleDaemon::wild_pattern()
{
    return qptr_eid_pool_->wild_pattern();
}



} // namespace dtn
