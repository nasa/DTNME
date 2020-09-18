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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include <oasys/io/IO.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Time.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleDaemonInput.h"
#include "BundleDaemonOutput.h"
#include "BundleDaemonStorage.h"
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
#include "naming/IPNScheme.h"
#include "reg/AdminRegistration.h"
#include "reg/AdminRegistrationIpn.h"
#include "reg/APIRegistration.h"
#include "reg/PingRegistration.h"
#include "reg/Registration.h"
#include "reg/RegistrationInitialLoadThread.h"
#include "reg/RegistrationTable.h"
#include "reg/IpnEchoRegistration.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "session/Session.h"
#include "storage/BundleStore.h"
#include "storage/RegistrationStore.h"
#include "storage/LinkStore.h"

#ifdef S10_ENABLED
#    include "bundling/S10Logger.h"
#endif // S10_ENABLED

#ifdef BSP_ENABLED
#  include "security/Ciphersuite.h"
#  include "security/KeyDB.h"
#endif

#ifdef ACS_ENABLED
#  include "BundleDaemonACS.h"
#endif // ACS_ENABLED

#ifdef DTPC_ENABLED
#  include "dtpc/DtpcDaemon.h"
#endif // DTPC_ENABLED




// enable or disable debug level logging in this file
#undef BD_LOG_DEBUG_ENABLED
//#define BD_LOG_DEBUG_ENABLED 1





namespace oasys {
    template <> dtn::BundleDaemon* oasys::Singleton<dtn::BundleDaemon, false>::instance_ = NULL;
}

namespace dtn {

BundleDaemon::Params::Params()
    :  early_deletion_(true),
       suppress_duplicates_(true),
       accept_custody_(true),
       reactive_frag_enabled_(true),
       retry_reliable_unacked_(true),
       test_permuted_delivery_(false),
       injected_bundles_in_memory_(false),
       recreate_links_on_restart_(true),
       persistent_links_(true),
       persistent_fwd_logs_(false),
       clear_bundles_when_opp_link_unavailable_(true),
       announce_ipn_(false),
       serialize_apireg_bundle_lists_(false),
       ipn_echo_service_number_(0),
       ipn_echo_max_return_length_(0)
{}

BundleDaemon::Params BundleDaemon::params_;

bool BundleDaemon::shutting_down_ = false;


//----------------------------------------------------------------------
BundleDaemon::BundleDaemon()
    : BundleEventHandler("BundleDaemon", "/dtn/bundle/daemon"),
      Thread("BundleDaemon", CREATE_JOINABLE),
      load_previous_links_executed_(false)
{
    // default local eids
    local_eid_.assign(EndpointID::NULL_EID());
    local_eid_ipn_.assign(EndpointID::NULL_EID());

    actions_ = NULL;
    eventq_ = NULL;
    
    memset(&stats_, 0, sizeof(stats_));

    all_bundles_        = new all_bundles_t("all_bundles");
    pending_bundles_    = new pending_bundles_t("pending_bundles");
    custody_bundles_    = new custody_bundles_t("custody_bundles");
    dupefinder_bundles_ = new dupefinder_bundles_t("dupefinder_bundles");

#ifdef BPQ_ENABLED
    bpq_cache_          = new BPQCache();
#endif /* BPQ_ENABLED */

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();
    reg_table_ = new RegistrationTable();

    BundleDaemonStorage::init();
    daemon_storage_ = BundleDaemonStorage::instance();

#ifdef ACS_ENABLED
    BundleDaemonACS::init();
    daemon_acs_ = BundleDaemonACS::instance();
#endif // ACS_ENABLED

    BundleDaemonInput::init();
    daemon_input_ = BundleDaemonInput::instance();

    BundleDaemonOutput::init();
    daemon_output_ = BundleDaemonOutput::instance();

#ifdef LTP_MSFC_ENABLED
    LTPEngine::init();
    daemon_ltpudp_engine_ = LTPEngine::instance();
#endif

    router_ = NULL;

    app_shutdown_proc_ = NULL;
    app_shutdown_data_ = NULL;

    rtr_shutdown_proc_ = 0;
    rtr_shutdown_data_ = 0;

#ifdef BDSTATS_ENABLED
    bdstats_init_ = true;
    memset(&bdstats_posted_, 0, sizeof(bdstats_posted_));
    memset(&bdstats_processed_, 0, sizeof(bdstats_processed_));
#endif // BDSTATS_ENABLED

}

//----------------------------------------------------------------------
BundleDaemon::~BundleDaemon()
{
    daemon_input_->set_should_stop();
    daemon_output_->set_should_stop();

#ifdef LTP_MSFC_ENABLED
    daemon_ltpudp_engine_->set_should_stop();
#endif // LTP_MSFC_ENABLED

#ifdef ACS_ENABLED
    daemon_acs_->set_should_stop();
#endif // ACS_ENABLED

#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->set_should_stop();
#endif // DTPC_ENABLED


    daemon_storage_->set_should_stop();

    delete pending_bundles_;
    delete custody_bundles_;
    delete dupefinder_bundles_;
    delete all_bundles_;

#ifdef BPQ_ENABLED
    delete bpq_cache_;
#endif /* BPQ_ENABLED */
    
    delete contactmgr_;
    delete fragmentmgr_;
    delete reg_table_;

    sleep(1);

    delete router_;

    delete actions_;
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
    BundleProtocol::init_default_processors();
#ifdef BSP_ENABLED
    Ciphersuite::init_default_ciphersuites();
    KeyDB::init();
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::set_local_eid(const char* eid_str) 
{
    local_eid_.assign(eid_str);
}

//----------------------------------------------------------------------
void
BundleDaemon::set_local_eid_ipn(const char* eid_str) 
{
    local_eid_ipn_.assign(eid_str);
}

//----------------------------------------------------------------------
void
BundleDaemon::init()
{       
    if (instance_ != NULL) 
    {
        PANIC("BundleDaemon already initialized");
    }

    instance_ = new BundleDaemon();     
    instance_->do_init();
}

//----------------------------------------------------------------------
void
BundleDaemon::post(BundleEvent* event)
{
    switch (event->event_processor_) {
        case EVENT_PROCESSOR_INPUT:
        {
            BundleDaemonInput* daemon_input = BundleDaemonInput::instance();
            daemon_input->post(event);
        }
        break;

        case EVENT_PROCESSOR_OUTPUT:
        {
            BundleDaemonOutput* daemon_output = BundleDaemonOutput::instance();
            daemon_output->post(event);
        }
        break;

        case EVENT_PROCESSOR_STORAGE:
        {
            BundleDaemonStorage* daemon_storage = BundleDaemonStorage::instance();
            daemon_storage->post(event);
        }
        break;

#ifdef DTPC_ENABLED
        case EVENT_PROCESSOR_DTPC:
        {
            DtpcDaemon* dtpc_daemon = DtpcDaemon::instance();
            dtpc_daemon->post(event);
        }
        break;
#endif

        default:
        {
            instance_->post_event(event);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::post_at_head(BundleEvent* event)
{
    switch (event->event_processor_) {
        case EVENT_PROCESSOR_INPUT:
        {
            BundleDaemonInput* daemon_input = BundleDaemonInput::instance();
            daemon_input->post_at_head(event);
        }
        break;

        case EVENT_PROCESSOR_OUTPUT:
        {
            BundleDaemonOutput* daemon_output = BundleDaemonOutput::instance();
            daemon_output->post_at_head(event);
        }
        break;

        case EVENT_PROCESSOR_STORAGE:
        {
            BundleDaemonStorage* daemon_storage = BundleDaemonStorage::instance();
            daemon_storage->post_at_head(event);
        }
        break;

#ifdef DTPC_ENABLED
        case EVENT_PROCESSOR_DTPC:
        {
            DtpcDaemon* dtpc_daemon = DtpcDaemon::instance();
            dtpc_daemon->post_at_head(event);
        }
        break;
#endif

        default:
        {
            instance_->post_event(event, false);
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::post_and_wait(BundleEvent* event,
                            oasys::Notifier* notifier,
                            int timeout, bool at_back)
{
    /*
     * Make sure that we're either already started up or are about to
     * start. Otherwise the wait call below would block indefinitely.
     */
    ASSERT(! oasys::Thread::start_barrier_enabled());

    switch (event->event_processor_) {
        case EVENT_PROCESSOR_INPUT:
        {
            BundleDaemonInput* daemon_input = BundleDaemonInput::instance();
            return daemon_input->post_and_wait(event, notifier, timeout, at_back);
        }

        case EVENT_PROCESSOR_OUTPUT:
        {
            BundleDaemonOutput* daemon_output = BundleDaemonOutput::instance();
            return daemon_output->post_and_wait(event, notifier, timeout, at_back);
        }
        break;

        case EVENT_PROCESSOR_STORAGE:
        {
            BundleDaemonStorage* daemon_storage = BundleDaemonStorage::instance();
            return daemon_storage->post_and_wait(event, notifier, timeout, at_back);
        }
    
#ifdef DTPC_ENABLED
        case EVENT_PROCESSOR_DTPC:
        {
            DtpcDaemon* dtpc_daemon = DtpcDaemon::instance();
            return dtpc_daemon->post_and_wait(event, notifier, timeout, at_back);
        }
        break;
#endif

        default:
        {
            ASSERT(event->processed_notifier_ == NULL);
            event->processed_notifier_ = notifier;
            if (at_back) {
                post(event);
            } else {
                post_at_head(event);
            }
            return notifier->wait(NULL, timeout);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::post_event(BundleEvent* event, bool at_back)
{
#ifdef BDSTATS_ENABLED
    // update the posted event stats
    bdstats_update(&bdstats_posted_, event);
#endif // BDSTATS_ENABLED

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
#endif

    event->posted_time_.get_time();
    eventq_->push(event, at_back);
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
	char bpq_stats[128];

#ifdef BPQ_ENABLED
    snprintf(bpq_stats, 128, "%zu bpq -- ", bpq_cache()->size());
#else
    bpq_stats[0] = 0; // no extra stats if BPQ not enabled
#endif /* BPQ_ENABLED */


    buf->appendf("%zu pending -- "
                 "%zu custody -- "
            	 "%s"
                 "%" PRIbid " received -- "
                 "%" PRIbid " delivered -- "
                 "%" PRIbid " generated -- "
                 "%" PRIbid " transmitted -- "
                 "%" PRIbid " expired -- "
                 "%" PRIbid " duplicate -- "
                 "%" PRIbid " deleted -- "
                 "%" PRIbid " injected -- "
                 "%" PRIbid " rejected -- "
                 "%" PRIbid " suppressed_dlv",
                 pending_bundles()->size(),
                 custody_bundles()->size(),
                 bpq_stats,
                 daemon_input_->get_received_bundles(),
                 stats_.delivered_bundles_,
                 daemon_input_->get_generated_bundles(),
                 stats_.transmitted_bundles_,
                 stats_.expired_bundles_,
                 daemon_input_->get_duplicate_bundles(),
                 stats_.deleted_bundles_,
                 stats_.injected_bundles_,
                 daemon_input_->get_rejected_bundles(),
                 stats_.suppressed_delivery_);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("\nBundleDaemon       : %zu pending_events -- "
                 "%" PRIbid " processed_events -- "
                 "%zu pending_timers \n",
                 event_queue_size(),
                 stats_.events_processed_,
                 oasys::TimerSystem::instance()->num_pending_timers());

    BundleDaemonInput::instance()->get_daemon_stats(buf);
    BundleDaemonOutput::instance()->get_daemon_stats(buf);
    BundleDaemonStorage::instance()->get_daemon_stats(buf);

}


//----------------------------------------------------------------------
void
BundleDaemon::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));

    daemon_input_->reset_stats();
    daemon_output_->reset_stats();
    daemon_storage_->reset_stats();

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
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("generating return receipt status report, "
              "flag = 0x%x, reason = 0x%x", flag, reason);
#endif
    
    Bundle* report = new Bundle();

    bool use_eid_ipn = false;
    if (local_eid_ipn_.scheme() == IPNScheme::instance()) {
        if (orig_bundle->replyto().equals(EndpointID::NULL_EID())){
            use_eid_ipn = (orig_bundle->source().scheme() == IPNScheme::instance());
        } else {
            use_eid_ipn = (orig_bundle->replyto().scheme() == IPNScheme::instance());
        }
    }

    if (use_eid_ipn) {
        BundleStatusReport::create_status_report(report, orig_bundle,
                                                 local_eid_ipn_, flag, reason);
    } else {
        BundleStatusReport::create_status_report(report, orig_bundle,
                                                 local_eid_, flag, reason);
    }

    BundleDaemon::post(new BundleReceivedEvent(report, EVENTSRC_ADMIN) );

#ifdef S10_ENABLED
	s10_bundle(S10_TXADMIN,report,NULL,0,0,orig_bundle,"status report");
#endif
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

    if (bundle->custodian().equals(EndpointID::NULL_EID())) {
        log_err("send_custody_signal(*%p): current custodian is NULL_EID",
                bundle);
        return;
    }
    
#ifdef ACS_ENABLED
    if (bundle->cteb_valid()) {
        if (BundleDaemonACS::instance()->generate_custody_signal(
                                            bundle, succeeded, reason)) {
            // if ACS is providing the signal then nothing further to do
            return;
        }
    }
#endif // ACS_ENABLED

     // send a normal custody signal
    Bundle* signal = new Bundle();
    if (bundle->custodian().scheme() == IPNScheme::instance()) {
        CustodySignal::create_custody_signal(signal, bundle, local_eid_ipn_,
                                             succeeded, reason);
    } else {
        CustodySignal::create_custody_signal(signal, bundle, local_eid_,
                                             succeeded, reason);
    }
 
    BundleDaemon::post(new BundleReceivedEvent(signal, EVENTSRC_ADMIN));

#ifdef S10_ENABLED
	s10_bundle(S10_TXADMIN,signal,NULL,0,0,bundle,"custody signal");
#endif

}

//----------------------------------------------------------------------
void
BundleDaemon::cancel_custody_timers(Bundle* bundle)
{
    bundle->lock()->lock("BundleDaemon::cancel_custody_timers #1");
    CustodyTimerVec::iterator iter =  bundle->custody_timers()->begin();
    bundle->lock()->unlock();

    while ( iter != bundle->custody_timers()->end())
    {
        bool ok = (*iter)->cancel();
        if (!ok) {
            log_crit("unexpected error cancelling custody timer for bundle *%p",
                     bundle);
        }
        
        // release the bundle ref so the bundle can be freed
        // before the timer expires
        (*iter)->bundle_.release();
        
        // the timer will be deleted when it bubbles to the top of the
        // timer queue

        bundle->lock()->lock("BundleDaemon::cancel_custody_timers #2");
        ++iter;
        bundle->lock()->unlock();
 
    }
    
    bundle->lock()->lock("BundleDaemon::cancel_custody_timers #3");
    bundle->custody_timers()->clear();
    bundle->lock()->unlock();
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

    if ( bundle->custodian().equals(local_eid_) ||
         bundle->custodian().equals(local_eid_ipn_) ) {
        log_err("send_custody_signal(*%p): "
                "current custodian is already local_eid",
                bundle);
        return;
    }

    // send a custody acceptance signal to the current custodian (if
    // it is someone, and not the null eid)
    if (! bundle->custodian().equals(EndpointID::NULL_EID())) {
        generate_custody_signal(bundle, true, BundleProtocol::CUSTODY_NO_ADDTL_INFO);
    }

#ifdef S10_ENABLED
    // next line is  for S10
    EndpointID prev_custodian=bundle->custodian();
#endif // S10_ENABLED


    // XXX/dz Need a lock because changing the custodian can happen between the 
    // time the BerkelyDBTable::put() calculates the size for the buffer and
    // when it serializes the bundle into the buffer. If the current custodian
    // is a longer string than the previous one then it aborts with error.
    bundle->lock()->lock("BundleDaemon::accept_custody()");
    
    // now we mark the bundle to indicate that we have custody and add
    // it to the custody bundles list
    if (bundle->dest().scheme() == IPNScheme::instance())  {
        bundle->mutable_custodian()->assign(local_eid_ipn_);
    } else {
        bundle->mutable_custodian()->assign(local_eid_);
    }

    bundle->set_local_custody(true);

#ifdef ACS_ENABLED
    // accept custody for ACS before updating to the datastore so that
    // the CTEB block can be added and then saved in one write
    BundleDaemonACS::instance()->accept_custody(bundle);
#endif // ACS_ENABLED

    bundle->lock()->unlock();

    actions_->store_update(bundle);
    
    custody_bundles_->push_back(bundle);

    // finally, if the bundle requested custody acknowledgements,
    // deliver them now
    if (bundle->custody_rcpt()) {
        generate_status_report(bundle, 
                               BundleStatusReport::STATUS_CUSTODY_ACCEPTED);
    }
#ifdef S10_ENABLED
	s10_bundle(S10_TAKECUST,bundle,prev_custodian.c_str(),0,0,NULL,NULL);
#endif


    // generate custody taken event for the external router
    BundleDaemon::post(new BundleCustodyAcceptedEvent(bundle));
}

//----------------------------------------------------------------------
void
BundleDaemon::release_custody(Bundle* bundle)
{
    log_info("release_custody *%p", bundle);
    
    if (!bundle->local_custody()) {
        log_err("release_custody(*%p): don't have local custody",
                bundle);
        return;
    }

    cancel_custody_timers(bundle);

    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_local_custody(false);
    actions_->store_update(bundle);

    custody_bundles_->erase(bundle);

#ifdef ACS_ENABLED
    // and also remove it from the ACS custody id list
    BundleDaemonACS::instance()->erase_from_custodyid_list(bundle);
#endif // ACS_ENABLED
}

//----------------------------------------------------------------------
void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      Registration* registration,
                                      bool initial_load)
{
    if ( !initial_load && NULL != registration->initial_load_thread() ) {
        // Registration is currently being loaded with pending bundles and
        // this new bundle will get picked up in the proper order
        return;
    }

    ASSERT(!bundle->is_fragment());

    ForwardingInfo::state_t state = bundle->fwdlog()->get_latest_entry(registration);
    switch(state) {
    case ForwardingInfo::PENDING_DELIVERY:
        // Expected case
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("delivering bundle to reg previously marked as PENDING_DELIVERY");
#endif
        break;
    case ForwardingInfo::DELIVERED:
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since already delivered",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
#endif
        return;
    default:
        log_warn("deliver_to_registration called with bundle not marked " \
                 "as PENDING_DELIVERY.  Delivering anyway...");
        // XXX/dz this was marking the bundle ForwardingInfo::DELIVERED but that  
        // is now done while processing the BundleDelivered events
        bundle->fwdlog()->add_entry(registration,
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::PENDING_DELIVERY);
        break;
    }

#if 0
    if (state != ForwardingInfo::NONE)
    {
        ASSERT(state == ForwardingInfo::DELIVERED);
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since already delivered",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
        return;
    }
#endif
    
    // if this is a session registration and doesn't have either the
    // SUBSCRIBE or CUSTODY bits (i.e. it's publish-only), don't
    // deliver the bundle
    if (registration->session_flags() == Session::PUBLISH)
    {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since it's a publish-only session registration",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
#endif
        return;
    }

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("delivering bundle *%p to registration %d (%s)",
              bundle, registration->regid(),
              registration->endpoint().c_str());
#endif

    if (registration->deliver_if_not_duplicate(bundle)) {
        // XXX/demmer this action could be taken from a registration
        // flag, i.e. does it want to take a copy or the actual
        // delivery of the bundle

        APIRegistration* api_reg = dynamic_cast<APIRegistration*>(registration);
        if (api_reg == NULL) {
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug("not updating registration %s",
                      registration->endpoint().c_str());
#endif
        } else {
            // XXX/dz  BundleDelivered event processing now handles this
            //log_debug("updating registration %s",
            //          api_reg->endpoint().c_str());
            //bundle->fwdlog()->update(registration,
            //                         ForwardingInfo::DELIVERED);
            //oasys::ScopeLock l(api_reg->lock(), "new bundle for reg");
            //api_reg->update();
        }
    } else {
        ++stats_.suppressed_delivery_;
        log_notice("suppressing duplicate delivery of bundle *%p "
                   "to registration %d (%s) - and deleting bundle",
                   bundle, registration->regid(),
                   registration->endpoint().c_str());

        // Inform router it was "delivered"
        BundleDeliveredEvent* event = new BundleDeliveredEvent(bundle, registration);
        router_->handle_event(event);
        delete event;

        BundleRef bref("deliver_dupe_suppressed");
        bref = bundle;
        //dz debug  try_to_delete(bref);
        delete_bundle(bref);
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::check_local_delivery(Bundle* bundle, bool deliver)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("checking for matching registrations for bundle *%p and deliver(%d)",
              bundle, deliver);
#endif

    RegistrationList matches;
    RegistrationList::iterator iter;

    reg_table_->get_matching(bundle->dest(), &matches);

    if (deliver) {
        ASSERT(!bundle->is_fragment());
        for (iter = matches.begin(); iter != matches.end(); ++iter) {
            Registration* registration = *iter;
            // deliver_to_registration(bundle, registration);

            /*
             * Mark the bundle as needing delivery to the registration.
             * Marking is durable and should be transactionalized together
             * with storing the bundle payload and metadata to disk if
             * the storage mechanism supports it (i.e. if transactions are
             * supported, we should be in one).
             */
            bundle->fwdlog()->add_entry(registration,
                                        ForwardingInfo::FORWARD_ACTION,
                                        ForwardingInfo::PENDING_DELIVERY);
            BundleDaemon::post(new DeliverBundleToRegEvent(bundle, registration->regid()));
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug("Marking bundle as PENDING_DELIVERY to reg %d", registration->regid());
#endif
        }
    }

    // Durably store our decisions about the registrations to which the bundle
    // should be delivered.  Actual delivery happens when the
    // DeliverBundleToRegEvent we just posted is processed.
    if (matches.size()>0) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("XXX Need to update bundle if not came from store.");
#endif
        actions_->store_update(bundle);
    }

    return (matches.size() > 0) || bundle->dest().subsume(local_eid_);
}

//----------------------------------------------------------------------
void
BundleDaemon::check_and_deliver_to_registrations(Bundle* bundle, const EndpointID& reg_eid)
{
    int num;

    RegistrationList matches;
    RegistrationList::iterator iter;

    num = reg_table_->get_matching(reg_eid, &matches);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("checking for matching entries in table for %s - matches: %d", reg_eid.c_str(), num);
#else
    (void) num;
#endif

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        Registration* registration = *iter;
        deliver_to_registration(bundle, registration);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delete(BundleDeleteRequest* request)
{
    if (request->bundle_.object()) {
        log_info("BUNDLE_DELETE: bundle *%p (reason %s)",
                 request->bundle_.object(),
                 BundleStatusReport::reason_to_str(request->reason_));
        delete_bundle(request->bundle_, request->reason_);
    }
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_take_custody(BundleTakeCustodyRequest* request)
{
    if (request->bundleref_.object()) {
            accept_custody(request->bundleref_.object());
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_acknowledged_by_app(BundleAckEvent* ack)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("ack from regid(%d): (%s: (%" PRIu64 ", %" PRIu64 "))",
              ack->regid_,
              ack->sourceEID_.c_str(),
              ack->creation_ts_.seconds_, ack->creation_ts_.seqno_);
#endif

    // Make sure we're happy with the registration provided.
    Registration* reg = reg_table_->get(ack->regid_);
    if ( reg==NULL ) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("BAD: can't get reg from regid(%d)", ack->regid_);
#endif
        return;
    }
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("Acking registration is not an APIRegistration");
#endif
        return;
    }

    api_reg->bundle_ack(ack->sourceEID_, ack->creation_ts_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_accept(BundleAcceptRequest* request)
{
    (void) request;
    ASSERTF(false, "handle_bundle_accept() was moved to BundleDaemonInput");
}
    
//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_received(BundleReceivedEvent* event)
{
    (void) event;
    // all processing moved to Input thread - fall through to see if bundle can be deleted
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_deliver_bundle_to_reg(DeliverBundleToRegEvent* event)
{

    const RegistrationTable* reg_table = 
            BundleDaemon::instance()->reg_table();

    const BundleRef& bundleref = event->bundleref_;
    u_int32_t regid = event->regid_;
  
    Registration* registration = NULL;

    registration = reg_table->get(regid);

    if (registration==NULL) {
        log_warn("Can't find registration %d any more", regid);
        return;
    }

    Bundle* bundle = bundleref.object();
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Delivering bundle id:%" PRIbid " to registration %d %s",
              bundle->bundleid(),
              registration->regid(),
              registration->endpoint().c_str());
#endif

    deliver_to_registration(bundle, registration);
 }

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    // processed in BDOutput -fall through and try to delete bundle

    if (event->success_) {
        ++stats_.transmitted_bundles_;
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    // update statistics
    stats_.delivered_bundles_++;
    
    /*
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(event->registration_);
    if (api_reg != NULL) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("updating registration %s",
                  api_reg->endpoint().c_str());
#endif
        bundle->fwdlog()->update(api_reg,
                                 ForwardingInfo::DELIVERED);

        if (params_.serialize_apireg_bundle_lists_) {
            //XXX/dz I don't think this update is necesssary even if true
            oasys::ScopeLock l(api_reg->lock(), "handle_bundle_delivered");
            api_reg->update();
        }
    }


#ifdef BD_LOG_DEBUG_ENABLED
    log_info("BUNDLE_DELIVERED id:%" PRIbid " (%zu bytes) -> regid %d (%s)",
             bundle->bundleid(), bundle->payload().length(),
             event->registration_->regid(),
             event->registration_->endpoint().c_str());
#endif

#ifdef S10_ENABLED
	s10_bundle(S10_DELIVERED,bundle,event->registration_->endpoint().c_str(),0,0,NULL,NULL);
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

        } else if (bundle->custodian().equals(EndpointID::NULL_EID())) {
            log_info("custodial bundle *%p delivered before custody accepted",
                     bundle);

        } else {
            generate_custody_signal(bundle, true,
                                    BundleProtocol::CUSTODY_NO_ADDTL_INFO);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_expired(BundleExpiredEvent* event)
{
    // update statistics
    stats_.expired_bundles_++;
    
    const BundleRef& bundle = event->bundleref_;

    log_info("BUNDLE_EXPIRED *%p", bundle.object());

    // note that there may or may not still be a pending expiration
    // timer, since this event may be coming from the console, so we
    // just fall through to delete_bundle which will cancel the timer

    delete_bundle(bundle, BundleProtocol::REASON_LIFETIME_EXPIRED);
    
    // fall through to notify the routers
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_cancel(BundleCancelRequest* event)
{
    BundleRef br = event->bundle_;

    if(!br.object()) {
        log_err("NULL bundle object in BundleCancelRequest");
        return;
    }

    // If the request has a link name, we are just canceling the send on
    // that link.
    if (!event->link_.empty()) {
        LinkRef link = contactmgr_->find_link(event->link_.c_str());
        if (link == NULL) {
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
BundleDaemon::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
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
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed 
    // messages. If an event about a bundle bound for particular link is 
    // posted after  another, which it might contradict, the BundleDaemon 
    // need not reprocess the event. The router (DP) might, however, be 
    // interested in the new status of the send.
    if (blocks == NULL)
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
    if (link->queue()->contains(bundle))
    {
        log_warn("cancelled bundle id:%" PRIbid " still on link %s queue",
                 bundle->bundleid(), link->name());
    }

    /*
     * The bundle should no longer be on the link queue or on the
     * inflight queue if it was cancelled.
     */
    if (link->inflight()->contains(bundle))
    {
        log_warn("cancelled bundle id:%" PRIbid " still on link %s inflight list",
                 bundle->bundleid(), link->name());
    }

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
    blocks = NULL;

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
BundleDaemon::handle_bundle_inject(BundleInjectRequest* event)
{
    (void) event;
    ASSERTF(false, "handle_bundle_inject() was moved to BundleDaemonInput");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_query(BundleQueryRequest*)
{
    BundleDaemon::post_at_head(new BundleReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_report(BundleReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_query(BundleAttributesQueryRequest* request)
{
    BundleRef &br = request->bundle_;
    if (! br.object()) return; // XXX or should it post an empty report?

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug(
        "BundleDaemon::handle_bundle_attributes_query: query %s, bundle *%p",
        request->query_id_.c_str(), br.object());
#endif

    // we need to keep a reference to the bundle because otherwise it may
    // be deleted before the event is handled
    BundleDaemon::post(
        new BundleAttributesReportEvent(request->query_id_,
                                        br,
                                        request->attribute_names_,
                                        request->metadata_blocks_));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_report(BundleAttributesReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_bundle_attributes_report: query %s",
              event->query_id_.c_str());
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_added(RegistrationAddedEvent* event)
{
    Registration* registration = event->registration_;
    log_info("REGISTRATION_ADDED %d %s",
             registration->regid(), registration->endpoint().c_str());

    if (!reg_table_->add(registration,
                         (event->source_ == EVENTSRC_APP) ? true : false))
    {
        log_err("error adding registration %d to table",
                registration->regid());
    }

    // kick off a helper thread to deliver the bundles to the 
    // registration so that it can be done in the background
    registration->stop_initial_load_thread(); // stop an old one if there is one
    RegistrationInitialLoadThread* loader = new RegistrationInitialLoadThread(registration);
    loader->start();
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_removed(RegistrationRemovedEvent* event)
{

    Registration* registration = event->registration_;
    log_info("REGISTRATION_REMOVED %d %s",
             registration->regid(), registration->endpoint().c_str());

    if (!reg_table_->del(registration)) {
        log_err("error removing registration %d from table",
                registration->regid());
        return;
    }

    post(new RegistrationDeleteRequest(registration));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_expired(RegistrationExpiredEvent* event)
{
    Registration* registration = event->registration_;

    if (reg_table_->get(registration->regid()) == NULL) {
        // this shouldn't ever happen
        log_err("REGISTRATION_EXPIRED -- dead regid %d", registration->regid());
        return;
    }
    
    registration->set_expired(true);
    
    if (registration->active()) {
        // if the registration is currently active (i.e. has a
        // binding), we wait for the binding to clear, which will then
        // clean up the registration
        log_info("REGISTRATION_EXPIRED %d -- deferred until binding clears",
                 registration->regid());
    } else {
        // otherwise remove the registration from the table
        log_info("REGISTRATION_EXPIRED %d", registration->regid());
        reg_table_->del(registration);
        post_at_head(new RegistrationDeleteRequest(registration));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_delete(RegistrationDeleteRequest* request)
{
    log_info("REGISTRATION_DELETE %d", request->registration_->regid());

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(request->registration_);
    if (NULL != api_reg  &&  api_reg->add_to_datastore()) {
        // the Stroage thread will delete the registration after its processing
        post(new StoreRegistrationDeleteEvent(request->registration_->regid(), api_reg));
    } else {
        delete request->registration_;
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_created(LinkCreatedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);

    if (link->isdeleted()) {
        log_warn("BundleDaemon::handle_link_created: "
                 "link %s deleted prior to full creation", link->name());
        event->daemon_only_ = true;
        return;
    }

    // Add (or update) this Link to the persistent store
    post(new StoreLinkUpdateEvent(link.object()));

    log_info("LINK_CREATED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_deleted(LinkDeletedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);

    // If link has been used in some forwarding log entry during this run of the
    // daemon or the link is a reincarnation of a link that was extant when a
    // previous run was terminated, then the persistent storage should be left
    // intact (and the link spec will remain in ContactManager::previous_links_
    // so that consistency will be checked).  Otherwise delete the entry in Links.

    if (!(link->used_in_fwdlog() || link->reincarnated()))
    {
        post(new StoreLinkDeleteEvent(link->name_str()));
    }

    log_info("LINK_DELETED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_available(LinkAvailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
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
BundleDaemon::handle_link_unavailable(LinkUnavailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isavailable());
    
    log_info("LINK UNAVAILABLE *%p", link.object());
   
    if (params_.clear_bundles_when_opp_link_unavailable_) {
        if (link->is_opportunistic()) {
            post(new LinkCancelAllBundlesRequest(link));
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_check_deferred(LinkCheckDeferredEvent* event)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("LinkCheckDeferred event for *%p", event->linkref_.object());
#else
    (void) event;
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_state_change_request(LinkStateChangeRequest* request)
{
    LinkRef link = request->link_;
    if (link == NULL) {
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
        post_at_head(new LinkUnavailableEvent(link,
                     ContactEvent::reason_t(reason)));
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

        post_at_head(new LinkAvailableEvent(link,
                     ContactEvent::reason_t(reason)));
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
            ASSERT(link->contact() != NULL);
            post_at_head(new ContactDownEvent(link->contact(),
                         ContactEvent::reason_t(reason)));
        }

        // close the link
        actions_->close_link(link);
        
        // now, based on the reason code, update the link availability
        // and set state accordingly
        if (reason == ContactEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            post_at_head(new LinkUnavailableEvent(link,
                         ContactEvent::reason_t(reason)));
        }
    
        break;

    default:
        PANIC("unhandled state %s", Link::state_to_str(new_state));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_create(LinkCreateRequest* request)
{
    //lock the contact manager so no one creates a link before we do
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), "BundleDaemon::handle_link_create");
    //check for an existing link with that name
    LinkRef linkCheck = cm->find_link(request->name_.c_str());
    if(linkCheck != NULL)
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

    if (link == NULL) {
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
BundleDaemon::handle_link_delete(LinkDeleteRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);

    log_info("LINK_DELETE *%p", link.object());
    if (!link->isdeleted()) {
        contactmgr_->del_link(link);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_reconfigure(LinkReconfigureRequest *request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);

    link->reconfigure_link(request->parameters_);
    log_info("LINK_RECONFIGURE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_query(LinkQueryRequest*)
{
    BundleDaemon::post_at_head(new LinkReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_report(LinkReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_query(BundleQueuedQueryRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);
    ASSERT(link->clayer() != NULL);

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_bundle_queued_query: "
              "query %s, checking if bundle *%p is queued on link *%p",
              request->query_id_.c_str(),
              request->bundle_.object(), link.object());
#endif
    
    bool is_queued = request->bundle_->is_queued_on(link->queue());
    BundleDaemon::post(
        new BundleQueuedReportEvent(request->query_id_, is_queued));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_report(BundleQueuedReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_bundle_queued_report: query %s, %s",
              event->query_id_.c_str(),
              (event->is_queued_? "true" : "false"));
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_eid_reachable_query(EIDReachableQueryRequest* request)
{
    Interface *iface = request->iface_;
    ASSERT(iface != NULL);
    ASSERT(iface->clayer() != NULL);

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
BundleDaemon::handle_eid_reachable_report(EIDReachableReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_eid_reachable_report: query %s, %s",
              event->query_id_.c_str(),
              (event->is_reachable_? "true" : "false"));
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attribute_changed(LinkAttributeChangedEvent *event)
{
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
BundleDaemon::handle_link_attributes_query(LinkAttributesQueryRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);
    ASSERT(link->clayer() != NULL);

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
BundleDaemon::handle_link_attributes_report(LinkAttributesReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_link_attributes_report: query %s",
              event->query_id_.c_str());
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_iface_attributes_query(
                  IfaceAttributesQueryRequest* request)
{
    Interface *iface = request->iface_;
    ASSERT(iface != NULL);
    ASSERT(iface->clayer() != NULL);

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
BundleDaemon::handle_iface_attributes_report(IfaceAttributesReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon::handle_iface_attributes_report: query %s",
              event->query_id_.c_str());
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_parameters_query(CLAParametersQueryRequest* request)
{
    ASSERT(request->cla_ != NULL);

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
BundleDaemon::handle_cla_parameters_report(CLAParametersReportEvent* event)
{
    (void)event;
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Bundledaemon::handle_cla_parameters_report: query %s",
              event->query_id_.c_str());
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_up(ContactUpEvent* event)
{
    const ContactRef contact = event->contact_;
    LinkRef link = contact->link();
    ASSERT(link != NULL);

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

#ifdef S10_ENABLED
    s10_contact(S10_CONTUP,contact.object(),NULL);
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_down(ContactDownEvent* event)
{
    const ContactRef contact = event->contact_;
    int reason = event->reason_;
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    
    log_info("CONTACT_DOWN *%p (%s) (contact %p)",
             link.object(), ContactEvent::reason_to_str(reason),
             contact.object());

    // update the link stats
    link->stats_.uptime_ += (contact->start_time().elapsed_ms() / 1000);

#ifdef S10_ENABLED
    s10_contact(S10_CONTDOWN,contact.object(),NULL);
#endif

}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_query(ContactQueryRequest*)
{
    BundleDaemon::post_at_head(new ContactReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_report(ContactReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_reassembly_completed(ReassemblyCompletedEvent* event)
{
    log_info("REASSEMBLY_COMPLETED bundle id %" PRIbid "",
             event->bundle_->bundleid());

    // remove all the fragments from the pending list
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        delete_bundle(ref);
    }

    // post a new event for the newly reassembled bundle
    post_at_head(new BundleReceivedEvent(event->bundle_.object(),
                                         EVENTSRC_FRAGMENTATION));
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_route_add(RouteAddEvent* event)
{
    log_info("ROUTE_ADD *%p", event->entry_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_recompute(RouteRecomputeEvent* event)
{
    (void) event;
    log_info("ROUTE_RECOMPUTE processed");
    router()->recompute_routes();
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_del(RouteDelEvent* event)
{
    log_info("ROUTE_DEL %s", event->dest_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_query(RouteQueryRequest*)
{
    BundleDaemon::post_at_head(new RouteReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_report(RouteReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_signal(CustodySignalEvent* event)
{
    log_info("CUSTODY_SIGNAL: %s %" PRIu64 ".%" PRIu64 " %s (%s)",
             event->data_.orig_source_eid_.c_str(),
             event->data_.orig_creation_tv_.seconds_,
             event->data_.orig_creation_tv_.seqno_,
             event->data_.succeeded_ ? "succeeded" : "failed",
             CustodySignal::reason_to_str(event->data_.reason_));

    GbofId gbof_id;
    gbof_id.mutable_source()->assign(event->data_.orig_source_eid_ );
    gbof_id.mutable_creation_ts()->seconds_ = event->data_.orig_creation_tv_.seconds_;
    gbof_id.mutable_creation_ts()->seqno_ = event->data_.orig_creation_tv_.seqno_;
    gbof_id.set_is_fragment(event->data_.admin_flags_ & 
                            BundleProtocol::ADMIN_IS_FRAGMENT);
    gbof_id.set_frag_length(gbof_id.is_fragment() ? 
                            event->data_.orig_frag_length_ : 0);
    gbof_id.set_frag_offset(gbof_id.is_fragment() ? 
                            event->data_.orig_frag_offset_ : 0);

    BundleRef orig_bundle("handle_custody_signal");
    orig_bundle = custody_bundles_->find(gbof_id.str());
    
    if (orig_bundle == NULL) {
        log_warn("received custody signal for bundle %s %" PRIu64 ".%" PRIu64 " "
                 "but don't have custody",
                 event->data_.orig_source_eid_.c_str(),
                 event->data_.orig_creation_tv_.seconds_,
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
                   event->data_.orig_source_eid_.c_str(),
                   event->data_.orig_creation_tv_.seconds_,
                   event->data_.orig_creation_tv_.seqno_);
        
        release = true;
    }

#ifdef S10_ENABLED
	s10_bundle(S10_RELCUST,orig_bundle.object(),event->data_.orig_source_eid_.c_str(),0,0,NULL,NULL);
#endif
    
    if (release) {
        release_custody(orig_bundle.object());
        try_to_delete(orig_bundle);
    }
}

//----------------------------------------------------------------------
#ifdef ACS_ENABLED
void
BundleDaemon::handle_aggregate_custody_signal(AggregateCustodySignalEvent* event)
{
    // XXX/dz this currently needs to run from within the BD thread instead
    // of the BDACS thread as timing can cause a deadlock while trying
    // to update the BundleStore
    // XXX/dz the above may not be a problem now that the BDStorage thread
    // has been implemented...
    BundleDaemonACS::instance()->process_acs(event);
}
#endif // ACS_ENABLED

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    LinkRef link   = event->link_;
    ASSERT(link != NULL);

    // Check to see if the bundle is still pending
    BundleRef bref("handle_custody_timerout");
    bref = pending_bundles_->find(event->bundle_id_);
    
    if (bref != NULL) {
        Bundle* bundle = bref.object();

        log_info("CUSTODY_TIMEOUT *%p, *%p", bundle, link.object());
    
        // remove and delete the expired timer from the bundle
        oasys::ScopeLock l(bundle->lock(), "BundleDaemon::handle_custody_timeout");

        bool found = false;
        CustodyTimer* timer = NULL;
        CustodyTimerVec::iterator iter;
        for (iter = bundle->custody_timers()->begin();
             iter != bundle->custody_timers()->end();
             ++iter)
        {
            timer = *iter;
            if (timer->link_ == link)
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

        ASSERT(!timer->cancelled());

        // modify the TRANSMITTED entry in the forwarding log to indicate
        // that we got a custody timeout. then when the routers go through
        // to figure out whether the bundle needs to be re-sent, the
        // TRANSMITTED entry is no longer in there
        bool ok = bref->fwdlog()->update(link, ForwardingInfo::CUSTODY_TIMEOUT);
        if (!ok) {
            log_err("custody timeout can't find ForwardingLog entry for link *%p",
                    link.object());
        }
        
        delete timer;

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
BundleDaemon::handle_shutdown_request(ShutdownRequest* request)
{
    shutting_down_ = true;

    (void)request;

    log_notice("Received shutdown request");

    oasys::TimerThread::instance()->pause_processing();

    InterfaceTable::instance()->shutdown();

    contactmgr_->set_shutting_down();
    contactmgr_->lock()->lock("BundleDaemon::handle_shutdown #1");

    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter = links->begin();

    // close any open links
    while (iter != links->end())
    {
        contactmgr_->lock()->unlock();
        LinkRef link = *iter;
        if (link->isopen()) {
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug("Shutdown: closing link *%p\n", link.object());
#endif
            link->close();
        }

        contactmgr_->lock()->lock("BundleDaemon::handle_shutdown #2");
        ++iter;
    }

    contactmgr_->lock()->unlock();

    // Shutdown all actively registered convergence layers.
    ConvergenceLayer::shutdown_clayers();

    // call the rtr shutdown procedure
    if (rtr_shutdown_proc_) {
        (*rtr_shutdown_proc_)(rtr_shutdown_data_);
    }

    // call the app shutdown procedure
    if (app_shutdown_proc_) {
        (*app_shutdown_proc_)(app_shutdown_data_);
    }

    // signal all objects to shutdown
    daemon_output_->set_should_stop();

    // give convergence layers time to finish in-progress bundle reception
    sleep(2);


#ifdef ACS_ENABLED
    daemon_acs_->set_should_stop();
#endif // ACS_ENABLED
    
#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->set_should_stop();
#endif // DTPC_ENABLED

    // wait for input queue to finish??
    daemon_input_->set_should_stop();



    log_always("shutting down - close out storage");
    daemon_storage_->set_should_stop();
    daemon_storage_->commit_all_updates();
    log_always("finished storage close out");


    // signal to the main loop to bail
    set_should_stop();

    // fall through -- the DTNServer will close and flush all the data
    // stores
    BundleProtocol::delete_block_processors();
#ifdef BSP_ENABLED
    Ciphersuite::shutdown();
#endif

}
//----------------------------------------------------------------------

void
BundleDaemon::handle_cla_set_params(CLASetParamsRequest* request)
{
    ASSERT(request->cla_ != NULL);
    request->cla_->set_cla_parameters(request->parameters_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_status_request(StatusRequest* request)
{
    (void)request;
    log_info("Received status request");
}

//----------------------------------------------------------------------
void
BundleDaemon::event_handlers_completed(BundleEvent* event)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("event handlers completed for (%p) %s", event, event->type_str());
#endif
    
    /**
     * Once bundle reception, transmission or delivery has been
     * processed by the router, check to see if it's still needed,
     * otherwise we delete it.
     */
    if (event->type_ == BUNDLE_RECEIVED) {
        try_to_delete(((BundleReceivedEvent*)event)->bundleref_);
    } else if (event->type_ == BUNDLE_TRANSMITTED) {
        try_to_delete(((BundleTransmittedEvent*)event)->bundleref_);
    } else if (event->type_ == BUNDLE_DELIVERED) {
        try_to_delete(((BundleDeliveredEvent*)event)->bundleref_);
    }

    /**
     * Once the bundle expired event has been processed, the bundle
     * shouldn't exist on any more lists.
     */
    if (event->type_ == BUNDLE_EXPIRED) {
        Bundle* bundle = ((BundleExpiredEvent*)event)->bundleref_.object();
        if (bundle==NULL) {
            log_warn("can't get bundle from event->bundleref_.object()");
            return;
        }
        size_t num_mappings = bundle->num_mappings();
        if (num_mappings != 1) {
            log_warn("expired bundle *%p still has %zu mappings (i.e. not just in ALL_BUNDLES)",
                     bundle, num_mappings);
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    bool ok_to_route = true;

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("adding bundle *%p to pending list (%d)",
              bundle, add_to_store);
#endif
 
    pending_bundles_->push_back(bundle);
    dupefinder_bundles_->push_back(bundle);
    
    if (add_to_store) {
        actions_->store_add(bundle);
    }

    return ok_to_route;
}



//----------------------------------------------------------------------
bool
BundleDaemon::resume_add_to_pending(Bundle* bundle, bool add_to_store)
{
    (void)add_to_store;

    if (shutting_down_) {
        return false;
    }

    bool ok_to_route = true;

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("resume_add_to_pending: bundle *%p", bundle);
#endif
 
   struct timeval now;

   gettimeofday(&now, 0);

   // schedule the bundle expiration timer
   struct timeval expiration_time;


   u_int64_t temp = BundleTimestamp::TIMEVAL_CONVERSION +
           bundle->creation_ts().seconds_ +
           bundle->expiration();

   // The expiration time has to be expressible in a
   // time_t (signed long on Linux)
   if (temp>INT_MAX) {
           log_crit("Expiration time too large.");
           ASSERT(false);
   }
   expiration_time.tv_sec = temp;

   long int when = expiration_time.tv_sec - now.tv_sec;

   expiration_time.tv_usec = now.tv_usec;
 
    //+[AEB] handling
    bool age_block_exists = false;

    if ((bundle->recv_blocks()).find_block(BundleProtocol::AGE_BLOCK) != false) {
        age_block_exists = true;
    }

    if (bundle->creation_ts().seconds_ == 0 || age_block_exists == true) {
        if(bundle->creation_ts().seconds_ == 0) {
            log_info_p("/dtn/bundle/expiration", "[AEB]: creation ts is 0");
        } 
        if(age_block_exists) {
            log_info_p("/dtn/bundle/expiration", "[AEB]: AgeBlock exists.");
        }

        expiration_time.tv_sec = 
            BundleTimestamp::TIMEVAL_CONVERSION +
            BundleTimestamp::get_current_time() +
            bundle->expiration() -
            bundle->age() - 
            (bundle->time_aeb().elapsed_ms() / 1000); // this might not be necessary

        expiration_time.tv_usec = now.tv_usec;

        when = expiration_time.tv_sec - now.tv_sec;

        log_info_p("/dtn/bundle/expiration", "[AEB]: expiring in %lu seconds", when);
    }
    //+[AEB] end handling
  
   if (when > 0) {
#ifdef BD_LOG_DEBUG_ENABLED
       log_debug_p("/dtn/bundle/expiration",
                   "scheduling expiration for bundle id %" PRIbid " at %u.%u "
                   "(in %lu seconds)",
                   bundle->bundleid(),
                   (u_int)expiration_time.tv_sec, (u_int)expiration_time.tv_usec,
                   when);
#endif
   } else {
       log_warn_p("/dtn/bundle/expiration",
                  "scheduling IMMEDIATE expiration for bundle id %" PRIbid ": "
                  "[expiration %" PRIu64 ", creation time %" PRIu64 ".%" PRIu64 ", offset %u, now %u.%u]",
                  bundle->bundleid(), bundle->expiration(),
                  bundle->creation_ts().seconds_,
                  bundle->creation_ts().seqno_,
                  BundleTimestamp::TIMEVAL_CONVERSION,
                  (u_int)now.tv_sec, (u_int)now.tv_usec);
       expiration_time = now;
       ok_to_route = false;
   }

   bundle->set_expiration_timer(new ExpirationTimer(bundle));
   bundle->expiration_timer()->schedule_at(&expiration_time);

    return ok_to_route;
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_from_pending(const BundleRef& bundle)
{
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("removing bundle *%p from pending list - time to expiration: %"  PRIi64, 
             bundle.object(), bundle->time_to_expiration());
#endif

    // first try to cancel the expiration timer if it's still
    // around
   
    // XXX/dz get lock before tinkering with the expiration timer
    //        in case it is firing at the same time
    //        (happens while reloading bundles that have expired) 
    bundle->lock()->lock(__FUNCTION__);

    if (!shutting_down_) {
        if (bundle->expiration_timer()) {
            // must release bundle lock before cancelling so we only
            // do that if we know it will not be fired while we are 
            // cancelling -  TODO: use the new oasys::SafeTimer?
            ExpirationTimer* exptmr = NULL;

            if (bundle->time_to_expiration() > 10) {
                exptmr = bundle->expiration_timer();

                bundle->set_expiration_timer(NULL);

                // release the lock before cancelling
                bundle->lock()->unlock();

                bool cancelled = exptmr->cancel();
                if (!cancelled) {
                        log_crit("unexpected error cancelling expiration timer "
                                 "for bundle *%p", bundle.object());
                }
            }

            if (exptmr->bundleref_ != NULL) {
                exptmr->bundleref_.release();
            }
        }
    }
    if (bundle->lock()->is_locked_by_me()) {
        bundle->lock()->unlock();
    }

    // XXX/demmer the whole BundleDaemon core should be changed to use
    // BundleRefs instead of Bundle*, as should the BundleList API, as
    // should the whole system, really...
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("pending_bundles size %zu", pending_bundles_->size());
#endif
    
    oasys::Time now;
    now.get_time();
    
    bool erased = pending_bundles_->erase(bundle.object());
    dupefinder_bundles_->erase(bundle.object());

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon: pending_bundles erasure took %u ms",
              now.elapsed_ms());
#endif

    if (!erased) {
        log_err("unexpected error removing bundle from pending list");
    }

    return erased;
}

//----------------------------------------------------------------------
bool
BundleDaemon::try_to_delete(const BundleRef& bundle)
{
    /*
     * Check to see if we should remove the bundle from the system.
     * 
     * If we're not configured for early deletion, this never does
     * anything. Otherwise it relies on the router saying that the
     * bundle can be deleted.
     */

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("pending_bundles size %zd", pending_bundles_->size());
#endif
    if (! bundle->is_queued_on(pending_bundles_))
    {
        if (bundle->expired()) {
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug("try_to_delete(*%p): bundle already expired",
                      bundle.object());
#endif
            return false;
        }
        
        log_err("try_to_delete(*%p): bundle not in pending list!",
                bundle.object());
        return false;
    }

    if (!params_.early_deletion_) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("try_to_delete(*%p): not deleting because "
                  "early deletion disabled",
                  bundle.object());
#endif
        return false;
    }

    if (! router_->can_delete_bundle(bundle)) {
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("try_to_delete(*%p): not deleting because "
                  "router wants to keep bundle",
                  bundle.object());
#endif
        return false;
    }
    
#ifdef BPQ_ENABLED
    if (bpq_cache()->bundle_in_bpq_cache(bundle.object())) {
#ifdef BD_LOG_DEBUG_ENABLED
    	log_debug("try_to_delete(*%p): not deleting because"
    			  "bundle is in BPQ cache",
    			  bundle.object());
#endif
    	return false;
    }
#endif

    return delete_bundle(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_bundle(const BundleRef& bundleref,
                            status_report_reason_t reason)
{
    Bundle* bundle = bundleref.object();

//dz debug    ++stats_.deleted_bundles_;
    
    // send a bundle deletion status report if we have custody or the
    // bundle's deletion status report request flag is set and a reason
    // for deletion is provided
    bool send_status = (bundle->local_custody() ||
                       (bundle->deletion_rcpt() &&
                        reason != BundleProtocol::REASON_NO_ADDTL_INFO));
        
    // check if we have custody, if so, remove it
    if (bundle->local_custody()) {
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

#ifdef BPQ_ENABLED
    // Forcibly delete bundle from BPQ cache
    // The cache list will be there even if it is not being used
    bpq_cache()->check_and_remove(bundle);

#endif

    // delete the bundle from the pending list
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("pending_bundles size %zd", pending_bundles_->size());
#endif
    bool erased = true;
    if (bundle->is_queued_on(pending_bundles_)) {
        erased = delete_from_pending(bundleref);
    }

    if (erased && send_status) {
        generate_status_report(bundle, BundleStatusReport::STATUS_DELETED, reason);
    }

    // cancel the bundle on all links where it is queued or in flight
    oasys::Time now;
    now.get_time();
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

    // cancel the bundle on all API registrations
    RegistrationList matches;
    RegistrationList::iterator reglist_iter;

    reg_table_->get_matching(bundle->dest(), &matches);

    for (reglist_iter = matches.begin(); reglist_iter != matches.end(); ++reglist_iter) {
        Registration* registration = *reglist_iter;
        registration->delete_bundle(bundle);
    }

    // XXX/demmer there may be other lists where the bundle is still
    // referenced so the router needs to be told what to do...
    
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("BundleDaemon: canceling deleted bundle on all links took %u ms",
               now.elapsed_ms());
#endif

    return erased;
}

//----------------------------------------------------------------------
Bundle*
BundleDaemon::find_duplicate(Bundle* bundle)
{
    Bundle *found = NULL;
    BundleRef bref("find_duplicate temporary");

    // if custody requested check for a duplicate already in custody
    if (bundle->custody_requested() ) {
        bref = custody_bundles_->find(bundle->gbofid_str());
        found = bref.object();
    }

    // if no duplicate in custody check the pending bundles for a dup 
    if ( NULL == found ) {
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
BundleDaemon::handle_bundle_free(BundleFreeEvent* event)
{
    Bundle* bundle = event->bundle_;
    event->bundle_ = NULL;

    //dz debug - could be 1 or 2 if in storage_queue   ASSERT(bundle->refcount() == 1);
    ASSERT(all_bundles_->contains(bundle));

    all_bundles_->erase(bundle);
    
    bundle->lock()->lock("BundleDaemon::handle_bundle_free");

//dz debug     if (bundle->queued_for_datastore()) {
//dz debug         log_crit("should not happen in BundleDaemon??? removing freed bundle (id %d) from data store", bundle->bundleid());
//dz debug         actions_->store_del(bundle);
//dz debug     }
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("deleting freed bundle");
#endif

    delete bundle;

    //dz debug
    ++stats_.deleted_bundles_;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
    
    if (! event->daemon_only_) {
        if (!shutting_down_) {
            // dispatch the event to the router(s) and the contact manager
            router_->handle_event(event);
            contactmgr_->handle_event(event);
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
BundleDaemon::load_registrations()
{
    admin_reg_ipn_ = NULL;

    admin_reg_ = new AdminRegistration();
    {
        RegistrationAddedEvent e(admin_reg_, EVENTSRC_ADMIN);
        handle_event(&e);
    }

    EndpointID ping_eid(local_eid());
    bool ok = ping_eid.append_service_tag("ping");
    if (!ok) {
        log_crit("local eid (%s) scheme must be able to append service tags",
                 local_eid().c_str());
        exit(1);
    }
    
    ping_reg_ = new PingRegistration(ping_eid);
    {
        RegistrationAddedEvent e(ping_reg_, EVENTSRC_ADMIN);
        handle_event(&e);
    }

    if (local_eid_ipn_.scheme() == IPNScheme::instance()) {
        // create an AdminReg for the local IPN EID if defined
        admin_reg_ipn_ = new AdminRegistrationIpn();
        RegistrationAddedEvent e(admin_reg_ipn_, EVENTSRC_ADMIN);
        handle_event(&e);
    } else {
        // otherwise set the local IPN EID to match the local DTN EID
        // so that will be used when working with IPN destined bundles
        set_local_eid_ipn(local_eid_.c_str());
    }

    if (params_.ipn_echo_service_number_ > 0) {
        if (local_eid_ipn_.scheme() == IPNScheme::instance()) {
            u_int64_t node;
            u_int64_t service;
            IPNScheme::parse(local_eid_ipn_,&node,&service);
            EndpointID ipn_echo_eid = EndpointID::NULL_EID();
            IPNScheme::format(&ipn_echo_eid, node, params_.ipn_echo_service_number_);
            ipn_echo_reg_ = new IpnEchoRegistration(ipn_echo_eid,
                                                    params_.ipn_echo_max_return_length_);  
            {
                RegistrationAddedEvent e(ipn_echo_reg_, EVENTSRC_ADMIN);
                handle_event(&e);
            }
        }
    }

    Registration* reg;
    RegistrationStore* reg_store = RegistrationStore::instance();
    RegistrationStore::iterator* iter = reg_store->new_iterator();

    while (iter->next() == 0) {
        reg = reg_store->get(iter->cur_val());
        if (reg == NULL) {
            log_err("error loading registration %d from data store",
                    iter->cur_val());
            continue;
        }
        
        reg->set_in_datastore(true);
        daemon_storage_->reloaded_registration();

        RegistrationAddedEvent e(reg, EVENTSRC_STORE);
        handle_event(&e);
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

#ifdef BD_LOG_DEBUG_ENABLED
            log_debug("load_previous_link - Persistent Lnks disabled - delete key: %s", key.c_str());
#endif
            link_store->del(key);
            ++cnt;

            iter = link_store->new_iterator();
        }
        log_info("load_previous_links - Persistent Links is disabled - deleted %d links from database", cnt);
        return;
    }

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Entering load_previous_links");
#endif

    /*!
     * By default this routine is called during the start of the run of the BundleDaemon
     * event loop, but this may need to be preempted if the DTNME configuration file
     * 'link add' commands
     */
    if (load_previous_links_executed_)
    {
#ifdef BD_LOG_DEBUG_ENABLED
    	log_debug("load_previous_links called again - skipping data store reads");
#endif
    	return;
    }

    load_previous_links_executed_ = true;

    while (iter->next() == 0) {
        link = link_store->get(iter->cur_val());
        if (link == NULL) {
            log_err("error loading link %s from data store",
                    iter->cur_val().c_str());
            continue;
         }

        link->set_in_datastore(true);
        daemon_storage_->reloaded_link();

#ifdef BD_LOG_DEBUG_ENABLED
        log_debug("Read link %s from data store", iter->cur_val().c_str());
#endif
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
#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Exiting load_previous_links");
#endif
}
//----------------------------------------------------------------------
void
BundleDaemon::load_bundles()
{
    Bundle* bundle;
    BundleStore* bundle_store = BundleStore::instance();
    BundleStore::iterator* iter = bundle_store->new_iterator();

    log_notice("loading bundles from data store");


    u_int64_t total_size = 0;

    std::vector<Bundle*> doa_bundles;
    
    for (iter->begin(); iter->more(); iter->next()) {
        bundle = bundle_store->get(iter->cur_val());
        
        if (bundle == NULL) {
            log_err("error loading bundle %" PRIbid " from data store",
                    iter->cur_val());
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
            continue;
        }

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
        post(new BundleReceivedEvent(bundle, EVENTSRC_STORE));

        // in the constructor, we disabled notifiers on the event
        // queue, so in case loading triggers other events, we just
        // let them queue up and handle them later when we're done
        // loading all the bundles
    }
    
    delete iter;

    // now that the durable iterator is gone, purge the doa bundles
    for (unsigned int i = 0; i < doa_bundles.size(); ++i) {
        actions_->store_del(doa_bundles[i]);
        delete doa_bundles[i];
    }

#ifdef BD_LOG_DEBUG_ENABLED
    log_debug("Done with load_bundles");
#endif
}

//----------------------------------------------------------------------
#ifdef ACS_ENABLED
void
BundleDaemon::load_pendingacs()
{
    BundleDaemonACS::instance()->load_pendingacs();
}
#endif // ACS_ENABLED

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

        if ( info->regid()!=0 ) {
            if ( info->state()==ForwardingInfo::PENDING_DELIVERY ) {
                BundleDaemon::post(new DeliverBundleToRegEvent(bundle, info->regid()));
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
    idle_exit_ = new DaemonIdleExit(interval);
}

//----------------------------------------------------------------------
void
BundleDaemon::run()
{
    char threadname[16] = "BD-Main";
    pthread_setname_np(pthread_self(), threadname);
   
    // pause to give the TCL console time to start - timing issue caused buffer overflow detected error
    sleep(1);


    static const char* LOOP_LOG = "/dtn/bundle/daemon/loop";
    
    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }

    // start up the timer thread to manage the timer system
    oasys::TimerThread::init();

    // If you are using the version of oasys that has the TimerReaperThread then
    // this will start it up. If not then cancelled timers will be recovered 
    // when they expire.
#ifdef HAVE_TIMER_REAPER_THREAD
    oasys::TimerReaperThread::init();
#endif

    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
    router_->initialize();

    load_registrations();
    load_previous_links();
    load_bundles();

    daemon_storage_->start();

#ifdef ACS_ENABLED
    load_pendingacs();
    daemon_acs_->start();
#endif // ACS_ENABLED

    BundleEvent* event;

    // delaying start of these two threads to provide time to process
    // configuration file events (link and route definitions) before
    // we start receiving bundles through the convergence layers
    daemon_output_->start_delayed(2000);
    daemon_input_->start_delayed(2000);  // 2000 == 2 seconds

    last_event_.get_time();
    
#ifdef BDSTATS_ENABLED
    // initialize the BD Stats before starting the processing loop
    bdstats_update();
    new BDStatusThread();
#endif // BDSTATS_ENABLED


#ifdef DTPC_ENABLED
    DtpcDaemon::instance()->start();
#endif // DTPC_ENABLED

#ifdef LTP_MSFC_ENABLED
    LTPEngine::instance()->start();
#endif // LTP_MSFC_ENABLED


    // Start the inerfaces running
    InterfaceTable::instance()->activate_interfaces();


    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    while (1) {
        if (should_stop()) {
            if (0 == eventq_->size()) {
                log_debug("BundleDaemon: stopping");
                break;
            }
        }

         // XXX/dz BundleDaemon used to process the expired timers here
         // and get a timeout value until the next timer is due. Now
         // using the TimerThread for that and just hard coding a 
         // timeout value so we can check for should_stop regularly
         // in case there are no events coming through.
        int timeout = 10;

#ifdef BD_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, 
                    "BundleDaemon: checking eventq_->size() > 0, its size is %zu", 
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
            
            
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "BundleDaemon: handling event %s",
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

#ifdef BDSTATS_ENABLED
            // update the pprocessed event stats
            bdstats_update(&bdstats_processed_, event);
#endif // BDSTATS_ENABLED
            
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "BundleDaemon: deleting event %s",
                        event->type_str());
#endif
            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

#ifdef BD_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, "BundleDaemon: poll_multiple waiting for %d ms", 
                    timeout);
#endif
        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);
#ifdef BD_LOG_DEBUG_ENABLED
        log_debug_p(LOOP_LOG, "poll returned %d", cc);
#endif

        if (cc == oasys::IOTIMEOUT) {
#ifdef BD_LOG_DEBUG_ENABLED
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
#ifdef BD_LOG_DEBUG_ENABLED
            log_debug_p(LOOP_LOG, "poll returned new event to handle");
#endif
        }
    }

#ifdef HAVE_TIMER_REAPER_THREAD
    oasys::TimerReaperThread::instance()->set_should_stop();
#endif
#ifdef LTP_MSFC_ENABLED
    LTPEngine::instance()->set_should_stop();
    sleep(1);
#endif
    // moved to DTNServer.cc:   oasys::TimerThread::instance()->shutdown();
}


#ifdef BDSTATS_ENABLED
void BundleDaemon::bdstats_update(BDStats* stats, BundleEvent* event)
{
    oasys::ScopeLock l(&bdstats_lock_, "bdstats_update");

    if (bdstats_init_) {
        bdstats_init_ = false;
        bdstats_start_time_.get_time();
        bdstats_time_.get_time();

        // output the header line - note that the "secs" title needs to be deleted
        // in an Excel spreadsheet to generate a chart with seconds as the X-axis
        log_crit_p("bdstats", "secs,BunRcvPost,BunRcvProc,BunXmtPost,BunXmtProc,"
                              "BunFreePost,BunFreeProc,LnkCreatedProc,ContactUpProc,"
                              "OtherPost,OtherProc,"
                              "DeliverBunPost,DeliverBunProc,"
                              "BunDeliveredPost,BunDeliveredProc,"
                              "BDmnEventQSize,StorageEventQSize,InputEventQSize");
    }

    // BDStatusThread calls this with NULL parameters to
    // signal that it is time to output the stats
    if (NULL == stats || NULL == event) {
        log_crit_p("bdstats", "%d,%"  PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 
                              ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%zu,%zu,%zu",
                    bdstats_start_time_.elapsed_ms()/1000,
                    bdstats_posted_.bundle_received_,
                    bdstats_processed_.bundle_received_,
                    bdstats_posted_.bundle_transmitted_,
                    bdstats_processed_.bundle_transmitted_,
                    bdstats_posted_.bundle_free_,
                    bdstats_processed_.bundle_free_,
                    bdstats_processed_.link_created_,
                    bdstats_processed_.contact_up_,
                    bdstats_posted_.other_, 
                    bdstats_processed_.other_,
                    bdstats_posted_.deliver_bundle_,
                    bdstats_processed_.deliver_bundle_,
                    bdstats_posted_.bundle_delivered_,
                    bdstats_processed_.bundle_delivered_,


                    eventq_->size(),
                    daemon_storage_->event_queue_size(),
                    daemon_input_->event_queue_size() );

        memset(&bdstats_posted_, 0, sizeof(bdstats_posted_));
        memset(&bdstats_processed_, 0, sizeof(bdstats_processed_));
        bdstats_time_.get_time();

    } else {

        switch (event->type_) {
            case BUNDLE_ACCEPT_REQUEST: ++stats->bundle_accept_; break;
            case BUNDLE_RECEIVED: ++stats->bundle_received_; break;
            case BUNDLE_TRANSMITTED: ++stats->bundle_transmitted_; break;
            case BUNDLE_FREE: ++stats->bundle_free_; break;
            case LINK_CREATED: stats->link_created_ += 10000; break;
            case CONTACT_UP: stats->contact_up_ += 10000; break;
            case DELIVER_BUNDLE_TO_REG: ++stats->deliver_bundle_; break;
            case BUNDLE_DELIVERED: ++stats->bundle_delivered_; break;
            default: ++stats->other_; break;
            // XXX/dz use this version to see what the "other" events actually are
            // so you can decide if you want to add them for tracking:
            //default: ++stats->other_; log_crit("Other Event: %s", event->type_str()); break;
        };
    }

}

//----------------------------------------------------------------------
BundleDaemon::BDStatusThread::BDStatusThread()
    : Thread("BundleDaemon::BDStatusThread", DELETE_ON_EXIT)
{
    start();
}

//----------------------------------------------------------------------
void
BundleDaemon::BDStatusThread::run()
{
    BundleDaemon* daemon = BundleDaemon::instance();

    struct timespec ts_status;
    clock_gettime(CLOCK_MONOTONIC, &ts_status);  // _RAW not supported by clock_nanosleep yet
    while (! should_stop()) {
        ts_status.tv_sec += 1;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts_status, NULL);  // _RAW not supported yet

        daemon->bdstats_update(NULL, NULL);
    }
}

#endif // BDSTATS_ENABLED

} // namespace dtn
