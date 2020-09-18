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

#ifdef DTPC_ENABLED

#include <sys/time.h>
#include <time.h>

#include <oasys/io/IO.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Time.h>

#include "bundling/BundleEvent.h"

#include "DtpcDaemon.h"
#include "DtpcPayloadAggregator.h"
#include "DtpcProfileTable.h"
#include "DtpcReceiver.h"
#include "DtpcRetransmitTimer.h"
#include "DtpcTopicTable.h"

#include "DtpcDataPduCollectorStore.h"
#include "DtpcPayloadAggregatorStore.h"
#include "DtpcProfileStore.h"
#include "DtpcTopicStore.h"

template <>
dtn::DtpcDaemon* oasys::Singleton<dtn::DtpcDaemon, false>::instance_ = NULL;

namespace dtn {

DtpcDaemon::Params::Params()
    : require_predefined_topics_(false),
      restrict_send_to_registered_client_(false),
      ipn_receive_service_number(129),
      ipn_transmit_service_number(128)
{}

DtpcDaemon::Params DtpcDaemon::params_;

bool DtpcDaemon::shutting_down_ = false;


//----------------------------------------------------------------------
DtpcDaemon::DtpcDaemon()
    : BundleEventHandler("DtpcDaemon", "/dtpc/daemon"),
      Thread("DtpcDaemon", CREATE_JOINABLE)
{
    // default local eids
    local_eid_.assign(EndpointID::NULL_EID());
    local_eid_ipn_.assign(EndpointID::NULL_EID());

    eventq_ = NULL;
    dtpc_receiver = NULL;
    
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
DtpcDaemon::~DtpcDaemon()
{
}

//----------------------------------------------------------------------
void 
DtpcDaemon::clean_up()
{
    // disconnect registrations so pending ADIs get written to datastore
    DtpcTopicTable::instance()->close_registrations();

    dtpc_receiver->set_should_stop();
    dtpc_receiver = NULL;

    DtpcPayloadAggregator* payload_agg;
    DtpcPayloadAggregatorIterator iter_pa = payload_agg_map_.begin();
    while (iter_pa != payload_agg_map_.end()) {
        payload_agg = iter_pa->second;
        payload_agg->set_should_stop();
    }

    // give all of the threads time to finish processing
    // before deleting them
    sleep(1);

    DtpcTopicRegistrationIterator iter_tr;
    while (!topic_reg_map_.empty()) {
        iter_tr = topic_reg_map_.begin();
        delete iter_tr->second;
        topic_reg_map_.erase(iter_tr);
    }

    while (!payload_agg_map_.empty()) {
        iter_pa = payload_agg_map_.begin();
        delete iter_pa->second;
        payload_agg_map_.erase(iter_pa);
    }

    DtpcProtocolDataUnitIterator iter_pdu;
    while (!unacked_pdu_list_.empty()) {
        iter_pdu = unacked_pdu_list_.begin();
        delete iter_pdu->second;
        unacked_pdu_list_.erase(iter_pdu);
    }

    DtpcDataPduCollectorIterator iter_dpc;
    while (!pdu_collector_map_.empty()) {
        iter_dpc = pdu_collector_map_.begin();
        delete iter_dpc->second;
        pdu_collector_map_.erase(iter_dpc);
    }

    DtpcTopicTable::shutdown();
    DtpcProfileTable::shutdown();

    delete eventq_;
    eventq_ = NULL;

    //if (!getenv("OASYS_CLEANUP_SINGLETONS")) {
    //    DtpcDaemon* this_ptr = instance_;
    //    instance_ = NULL;
    //    delete this_ptr;
    //}
}

//----------------------------------------------------------------------
void
DtpcDaemon::do_init()
{
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
}

//----------------------------------------------------------------------
void
DtpcDaemon::set_local_eid(const char* eid_str) 
{
    local_eid_.assign(eid_str);
}

//----------------------------------------------------------------------
void
DtpcDaemon::set_local_eid_ipn(const char* eid_str) 
{
    local_eid_ipn_.assign(eid_str);
}

//----------------------------------------------------------------------
void
DtpcDaemon::init()
{       
    if (instance_ != NULL) 
    {
        PANIC("DtpcDaemon already initialized");
    }

    instance_ = new DtpcDaemon();     
    instance_->do_init();
}

//----------------------------------------------------------------------
void
DtpcDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

//----------------------------------------------------------------------
void
DtpcDaemon::post_at_head(BundleEvent* event)
{
    instance_->post_event(event, false);
}

//----------------------------------------------------------------------
bool
DtpcDaemon::post_and_wait(BundleEvent* event,
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
DtpcDaemon::post_event(BundleEvent* event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");

    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
DtpcDaemon::get_stats(oasys::StringBuffer* buf)
{
    (void) buf;
}

//----------------------------------------------------------------------
void
DtpcDaemon::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("%zu pending_events -- "
                 "%u processed_events -- "
                 "%zu pending_timers",
                 event_queue_size(),
                 stats_.events_processed_,
                 oasys::TimerSystem::instance()->num_pending_timers());
}


//----------------------------------------------------------------------
void
DtpcDaemon::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_dtpc_topic_registration(DtpcTopicRegistrationEvent* event)
{
    log_info("DTPC Topic Registration event received for ID: %"PRIu32" has elision func: %s",
             event->topic_id_, event->has_elision_func_?"true":"false");

    // verify that the topic exists - if not then add or abort per configuration
    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    DtpcTopic* topic = toptab->get(event->topic_id_);

    if (NULL == topic) {
        if (params_.require_predefined_topics_) {
            // on the fly topics not allowed
            log_err("DTPC Topic Registration event: registration rejected - no predefined Topic ID: %"PRIu32,
                    event->topic_id_);
            *event->result_ = -1;
            return;
        } else {
            // add the on the fly topic
	    oasys::StringBuffer errmsg;
            if (! toptab->add(&errmsg, event->topic_id_, false, "<on the fly API topic registration>")) {
                log_err("DTPC Topic Registration event: error adding on the fly Topic ID: %"PRIu32,
                        event->topic_id_);
                *event->result_ = -2; // internal error
                return;
            }
            topic = toptab->get(event->topic_id_);
        }
    }

    DtpcTopicRegistrationIterator iter = topic_reg_map_.find(event->topic_id_);
    if (iter != topic_reg_map_.end()) {
        log_debug("DTPC Topic Registration event: registration rejected - busy for Topic ID: %"PRIu32,
                 event->topic_id_);
        *event->result_ = -3; // busy
        return;
    }

    DtpcRegistration* reg = new DtpcRegistration(event->topic_id_, event->has_elision_func_);
    DtpcTopicRegistrationInsertResult result;
    result = topic_reg_map_.insert(DtpcTopicRegistrationPair(event->topic_id_, reg));

    if ( result.second == false ) {
        log_err("DTPC Topic Registration event: error storing registration for Topic ID: %"PRIu32,
                 event->topic_id_);

        *event->result_ = -2; // internal error
    } else {
        topic->set_registration(reg);
        *event->registration_ = reg;
        *event->result_ = 0; // success
    }
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_dtpc_topic_unregistration(DtpcTopicUnregistrationEvent* event)
{
    log_info("DTPC Topic Unregistration event received for ID: %"PRIu32,
             event->topic_id_);

    DtpcTopicRegistrationIterator iter = topic_reg_map_.find(event->topic_id_);
    if (iter != topic_reg_map_.end()) {
        DtpcTopicTable* toptab = DtpcTopicTable::instance();
        DtpcTopic* topic = toptab->get(event->topic_id_);
        if (NULL != topic) topic->set_registration(NULL);

        delete iter->second;
        topic_reg_map_.erase(iter);
        *event->result_ = 0;
        return;
    } else {
        *event->result_ = -1; // not found
    }
}

//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_send_data_item(DtpcSendDataItemEvent* event)
{
    log_info("DTPC Send Data Item event received for Topic ID: %"PRIu32,
             event->topic_id_);

    // verify that the topic exists - if not then add or abort per configuration
    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    if (! toptab->find(event->topic_id_) ) {
        if (params_.require_predefined_topics_) {
            // on the fly topics not allowed
            log_err("DTPC Send Data Item event: request rejected - no predefined Topic ID: %"PRIu32,
                    event->topic_id_);
            *event->result_ = -1;
            return;
        } else {
            // add the on the fly topic
	    oasys::StringBuffer errmsg;
            if (! toptab->add(&errmsg, event->topic_id_, false, "<on the fly SEND topic registration>")) {
                log_err("DTPC Send Data Item: error adding on the fly Topic ID: %"PRIu32,
                        event->topic_id_);
                *event->result_ = -2; // internal error
                return;
            }
        }
    }

   // verify the transmission profile ID exists
    DtpcProfileTable* proftab = DtpcProfileTable::instance();
    if (! proftab->find(event->profile_id_) ) {
        // on the fly topics not allowed
        log_err("DTPC Send Data Item event: request rejected - no predefined Profile ID: %"PRIu32,
                event->profile_id_);
        *event->result_ = -1;
        return;
    }


    // Create/add to Payload Aggregator - key is dest_eid plus profile ID
    char work[256];
    snprintf(work, sizeof(work)-1, "%s~%"PRIu32, event->dest_eid_.c_str(), event->profile_id_);
    work[255] = 0;
    std::string key(work);
    

    DtpcPayloadAggregator* payload_agg = NULL;
    DtpcPayloadAggregatorIterator iter = payload_agg_map_.find(key);
    if (iter != payload_agg_map_.end()) {
        payload_agg = iter->second;
    } else {
        payload_agg = new DtpcPayloadAggregator(key, event->dest_eid_, event->profile_id_);
        payload_agg_map_.insert(DtpcPayloadAggregatorPair(key, payload_agg));
        payload_agg->start();
    }

    ASSERT(NULL != payload_agg);

    log_debug("DTPC Send: Success - Dest: %s Profile: %"PRIu32" Topic: %"PRIu32" Length: %zu",
              event->dest_eid_.c_str(), event->profile_id_, event->topic_id_, event->data_item_->size());


    DtpcSendDataItemEvent* send_event;
    send_event = new DtpcSendDataItemEvent(event->topic_id_, event->data_item_, 
                                           event->dest_eid_, event->profile_id_, event->result_);
    //dzdebug send_event->processed_notifier_ = event->processed_notifier_;
    payload_agg->post(send_event);
}

//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_payload_aggregation_timer_expired(DtpcPayloadAggregationTimerExpiredEvent* event)
{
    //log_debug("DTPC Aggregation Expired event received for key: %s SeqCtr: %"PRIu64,
    //         event->key_.c_str(), event->seq_ctr_);

    DtpcPayloadAggregator* payload_agg = NULL;
    DtpcPayloadAggregatorIterator iter = payload_agg_map_.find(event->key_);
    if (iter == payload_agg_map_.end()) {
        log_err("DTPC Aggregation Expired event - Payload Aggregator not found: %s",
                 event->key_.c_str());
        return;
    }
        
    payload_agg = iter->second;
    // post at head so it will be processed next because it 
    // might be paused waiting for an elision function response
    payload_agg->post_at_head(new DtpcPayloadAggregationTimerExpiredEvent(event->key_, event->seq_ctr_));
}

//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_transmitted_event(DtpcPduTransmittedEvent* event)
{
    DtpcProtocolDataUnit* pdu = event->pdu_;

    DtpcProfile* profile = DtpcProfileTable::instance()->get(pdu->profile_id());
    if (NULL == profile) {
        log_err("handle_dtpc_transmitted_event did not find Profile ID: "
                "%"PRIu32" - aborting payload", pdu->profile_id());
        ASSERT(NULL != profile);
    }
    struct timeval retran_time = profile->calc_retransmit_time();

    DtpcProtocolDataUnitIterator itr = unacked_pdu_list_.find(pdu->key());

    if (itr == unacked_pdu_list_.end()) {
        unacked_pdu_list_.insert(DtpcProtocolDataUnitPair(pdu->key(), pdu));
        log_debug("handle_dtpc_transmitted_event - Added PDU (%s) to unacked_pdu_list", pdu->key().c_str());
    } else {
        log_debug("handle_dtpc_transmitted_event - PDU (%s) already in unacked_pdu_list", pdu->key().c_str());
    }

    // start a retransmit timer
    pdu->set_retransmit_timer(new DtpcRetransmitTimer(pdu->key()), retran_time.tv_sec);
    pdu->retransmit_timer()->schedule_at(&retran_time);
}

//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_delete_request(DtpcPduDeleteRequest* event)
{
    DtpcProtocolDataUnit* pdu = event->pdu_;

    // first cancel the retransmit timer
    if (pdu->retransmit_timer()) {
        log_debug("handle_dtpc_delete_request - cancelling retransmit timer for PDU %s",
                  pdu->key().c_str());
        
        bool cancelled = pdu->retransmit_timer()->cancel();
        if (!cancelled) {
            log_warn("handle_dtpc_delete_request - unexpected error cancelling retransmit timer "
                     "for PDU %s", pdu->key().c_str());
        }

        pdu->set_retransmit_timer(NULL, 0);
    }

    DtpcProtocolDataUnitIterator itr = unacked_pdu_list_.find(pdu->key());

    if (itr == unacked_pdu_list_.end()) {
        log_debug("handle_dtpc_delete_request - PDU (%s) not found in unacked_pdu_list", 
                  pdu->key().c_str());
    } else {
        log_debug("handle_dtpc_delete_request - delete PDU (%s) from unacked_pdu_list", 
                  pdu->key().c_str());
        unacked_pdu_list_.erase(itr);
    }
}

//----------------------------------------------------------------------
void DtpcDaemon::handle_dtpc_retransmit_timer_expired(DtpcRetransmitTimerExpiredEvent* event)
{
    std::string key = event->key_;
    DtpcProtocolDataUnit* pdu = NULL;

    DtpcProtocolDataUnitIterator itr = unacked_pdu_list_.find(key);

    if (itr == unacked_pdu_list_.end()) {
        log_warn("handle_dtpc_retransmit_timer_expired - PDU (%s) not found in unacked_pdu_list", 
                  key.c_str());
    } else {
        log_debug("handle_dtpc_transmitted_event - initiate retransmit of PDU (%s)", 
                  key.c_str());

        pdu = itr->second;

        // clear the reference to the timer
        pdu->set_retransmit_timer(NULL, 0);

        // Create/add to Payload Aggregator - key is remote (destination) eid plus profile ID
        char work[256];
        snprintf(work, sizeof(work)-1, "%s~%"PRIu32, pdu->remote_eid().c_str(), pdu->profile_id());
        work[255] = 0;
        std::string key(work);
    

        DtpcPayloadAggregator* payload_agg = NULL;
        DtpcPayloadAggregatorIterator iter = payload_agg_map_.find(key);
        if (iter != payload_agg_map_.end()) {
            payload_agg = iter->second;
        } else {
            payload_agg = new DtpcPayloadAggregator(key, pdu->remote_eid(), pdu->profile_id());
            payload_agg_map_.insert(DtpcPayloadAggregatorPair(key, payload_agg));
            payload_agg->start();
        }

        ASSERT(NULL != payload_agg);

        payload_agg->retransmit_pdu(pdu);
    }
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_dtpc_ack_received_event(DtpcAckReceivedEvent* event)
{
    std::string key = event->pdu_->key();
    DtpcProtocolDataUnit* pdu = NULL;

    DtpcProtocolDataUnitIterator itr = unacked_pdu_list_.find(key);

    if (itr == unacked_pdu_list_.end()) {
        // not found so look for the alternate ION key -
        // we send to ipn:x.129 and expect ACK back from that address
        // but ION sends it with a source ipn:x.128 so we compensate
        // with the ion_alt_key_
        log_debug("handle_dtpc_ack_received_event - PDU (%s) not found in unacked_pdu_list",
                  key.c_str());
        if (event->pdu_->has_ion_alt_key()) {
            key = event->pdu_->ion_alt_key();
            log_debug("handle_dtpc_ack_received_event - search for ion_alt_key: %s", 
                      key.c_str());
            itr = unacked_pdu_list_.find(key);
        }
    }

    if (itr == unacked_pdu_list_.end()) {
        log_debug("handle_dtpc_ack_received_event - PDU (%s) not found in unacked_pdu_list", 
                  event->pdu_->key().c_str());
    } else {
        log_debug("handle_dtpc_ack_received_event - ACK received for PDU (%s)", 
                  key.c_str());

        pdu = itr->second;

        // first cancel the retransmit timer
        if (pdu->retransmit_timer()) {
            log_debug("handle_dtpc_ack_received_event - cancelling retransmit timer for PDU %s",
                      pdu->key().c_str());
            
            bool cancelled = pdu->retransmit_timer()->cancel();
            if (!cancelled) {
                log_err("unexpected error cancelling retransmit timer "
                        "for PDU %s", pdu->key().c_str());
            }

            pdu->set_retransmit_timer(NULL, 0);
        }
        unacked_pdu_list_.erase(itr);

        delete pdu;
    }

    // release allocated memory
    delete event->pdu_;
    event->pdu_ = NULL;
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_dtpc_data_received_event(DtpcDataReceivedEvent* event)
{
    //XXX/dz  CCSDS 934.2-R3 E3.10.1.1 says the PDU's profile ID should exist in the DTPC entity
    //        before instantiating a DtpcDataPduCollector but that may have been an oversight
    //        as we were moving away from using a predefined profile on the receiver side

    // Create/add to Data PDU Collector - key is remote (source) EID plus profile ID
    DtpcProtocolDataUnit* pdu = event->pdu_;
    DtpcDataPduCollector* collector = NULL;
    DtpcDataPduCollectorIterator iter = pdu_collector_map_.find(pdu->collector_key());
    if (iter != pdu_collector_map_.end()) {
        collector = iter->second;
    } else {
        collector = new DtpcDataPduCollector(pdu->collector_key(), 
                                             pdu->remote_eid(), 
                                             pdu->profile_id());
        pdu_collector_map_.insert(DtpcDataPduCollectorPair(pdu->collector_key(), collector));
    }

    ASSERT(NULL != collector);

    log_debug("DTPC Recv : Success - Source: %s Profile: %"PRIu32" Length: %zu",
              pdu->remote_eid().c_str(), pdu->profile_id(), pdu->size());

    collector->pdu_received(pdu, event->rcvd_bref_);
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_dtpc_deliver_pdu_event(DtpcDeliverPduTimerExpiredEvent* event)
{
    // Create/add to Data Collector - key is dest_eid plus profile ID
    DtpcDataPduCollector* collector = NULL;
    DtpcDataPduCollectorIterator iter = pdu_collector_map_.find(event->key_);
    if (iter == pdu_collector_map_.end()) {
        log_debug("handle_dtpc_deliver_pdu_event - PDU Collector not found: %s",
                  event->key_.c_str());
    } else {
        collector = iter->second;
        ASSERT(NULL != collector);

        log_debug("handle_dtpc_deliver_pdu_event - trigger PDU Collector to deliver PDU: %s",
                  event->key_.c_str());

        collector->timer_expired(event->seq_ctr_);
    }
}


//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_topic_expiration_check(DtpcTopicExpirationCheckEvent* event)
{
    log_debug("DTPC Topic Expiration Check event received for Topic ID: %"PRIu32,
              event->topic_id_);

    // verify that the topic exists - if not then add or abort per configuration
    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    DtpcTopic* topic = toptab->get(event->topic_id_);

    if (NULL != topic) {
        topic->remove_expired_items();
    }
}


//----------------------------------------------------------------------
void 
DtpcDaemon::handle_dtpc_elision_func_response(DtpcElisionFuncResponse* event)
{
    log_debug("DTPC elision function response received for Topic ID: %"PRIu32,
              event->topic_id_);

    // verify that the topic exists - if not then add or abort per configuration
    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    if (! toptab->find(event->topic_id_) ) {
        log_err("DTPC elision function response: Topic ID not found: %"PRIu32,
                event->topic_id_);
        return;
    }

   // verify the transmission profile ID exists
    DtpcProfileTable* proftab = DtpcProfileTable::instance();
    if (! proftab->find(event->profile_id_) ) {
        // on the fly topics not allowed
        log_err("DTPC elision function response: Profile ID not found: %"PRIu32,
                event->profile_id_);
        return;
    }

    // Create/add to Payload Aggregator - key is dest_eid plus profile ID
    char work[256];
    snprintf(work, sizeof(work)-1, "%s~%"PRIu32, event->dest_eid_.c_str(), event->profile_id_);
    work[255] = 0;
    std::string key(work);
    

    DtpcPayloadAggregator* payload_agg = NULL;
    DtpcPayloadAggregatorIterator iter = payload_agg_map_.find(key);
    if (iter != payload_agg_map_.end()) {
        payload_agg = iter->second;
    } else {
        // on the fly topics not allowed
        log_err("DTPC elision function response: Payload Aggregator not found: %s", key.c_str());
    }

    ASSERT(NULL != payload_agg);

    log_debug("DTPC elision function response: Dest: %s Profile: %"PRIu32" Topic: %"PRIu32" ADIs: %zu",
              event->dest_eid_.c_str(), event->profile_id_, event->topic_id_, 
              event->data_item_list_->size());

    payload_agg->elision_func_response(event->topic_id_, event->modified_, 
                                       event->data_item_list_);
}

//----------------------------------------------------------------------
void
DtpcDaemon::event_handlers_completed(BundleEvent* event)
{
    (void) event;
    //log_debug("event handlers completed for (%p) %s", event, event->type_str());
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
DtpcDaemon::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
    
    event_handlers_completed(event);

    stats_.events_processed_++;

    if (event->daemon_only_ && event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
DtpcDaemon::load_profiles()
{
    log_debug("Entering load_profiles");

    DtpcProfile* profile;
    DtpcProfileStore* profile_store = DtpcProfileStore::instance();
    DtpcProfileStore::iterator* iter = profile_store->new_iterator();

    while (iter->next() == 0) {
        profile = profile_store->get(iter->cur_val());
        if (profile == NULL) {
            log_err("error loading profile %"PRIu32" from data store",
                    iter->cur_val());
            continue;
         }

        profile->set_reloaded_from_ds();
        log_debug("Read profile %"PRIu32" from data store", iter->cur_val());
        DtpcProfileTable::instance()->add_reloaded_profile(profile);
    }

    delete iter;

    DtpcProfileTable::instance()->storage_initialized();

    log_debug("Exiting load_profiles");
}

//----------------------------------------------------------------------
void
DtpcDaemon::load_topics()
{
    log_debug("Entering load_topics");

    DtpcTopic* topic;
    DtpcTopicStore* topic_store = DtpcTopicStore::instance();
    DtpcTopicStore::iterator* iter = topic_store->new_iterator();

    while (iter->next() == 0) {
        topic = topic_store->get(iter->cur_val());
        if (topic == NULL) {
            log_err("error loading topic %"PRIu32" from data store",
                    iter->cur_val());
            continue;
         }

        topic->set_reloaded_from_ds();
        log_debug("Read topic %"PRIu32" from data store", iter->cur_val());
        DtpcTopicTable::instance()->add_reloaded_topic(topic);
    }

    delete iter;

    DtpcTopicTable::instance()->storage_initialized();

    log_debug("Exiting load_topics");
}

//----------------------------------------------------------------------
void
DtpcDaemon::load_payload_aggregators()
{
    log_debug("Entering load_payload_aggregators");

    DtpcPayloadAggregator* payload_agg;
    DtpcPayloadAggregatorStore* payload_agg_store = DtpcPayloadAggregatorStore::instance();
    DtpcPayloadAggregatorStore::iterator* iter = payload_agg_store->new_iterator();

    while (iter->next() == 0) {
        payload_agg = payload_agg_store->get(iter->cur_val());
        if (payload_agg == NULL) {
            log_err("error loading payload aggregator [%s] from data store",
                    iter->cur_val().c_str());
            continue;
        }

        log_debug("Read payload_agg [%s] from data store", iter->cur_val().c_str());
        payload_agg_map_.insert(DtpcPayloadAggregatorPair(payload_agg->durable_key(), payload_agg));
        payload_agg->set_reloaded_from_ds();
        payload_agg->start();
    }

    delete iter;

    log_debug("Exiting load_payload_aggregators");
}

//----------------------------------------------------------------------
void
DtpcDaemon::load_data_pdu_collectors()
{
    log_debug("Entering load_data_pdu_collectors");

    DtpcDataPduCollector* collector;
    DtpcDataPduCollectorStore* collector_store = DtpcDataPduCollectorStore::instance();
    DtpcDataPduCollectorStore::iterator* iter = collector_store->new_iterator();

    while (iter->next() == 0) {
        collector = collector_store->get(iter->cur_val());
        if (collector == NULL) {
            log_err("error loading data pdu collector [%s] from data store",
                    iter->cur_val().c_str());
            continue;
        }

        log_debug("Read collector [%s] from data store", iter->cur_val().c_str());
        pdu_collector_map_.insert(DtpcDataPduCollectorPair(collector->durable_key(), collector));
        collector->ds_reload_post_processing();
    }

    delete iter;

    log_debug("Exiting load_data_pdu_collectors");
}

//----------------------------------------------------------------------
void
DtpcDaemon::run()
{
    char threadname[16] = "DtpcDaemon";
    pthread_setname_np(pthread_self(), threadname);
   
    static const char* LOOP_LOG = "/dtpc/daemon/loop";
    
#ifdef DTPC_STANDALONE
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
#endif // DTPC_STANDALONE

    load_profiles();
    load_topics();
    load_payload_aggregators();
    load_data_pdu_collectors();

    dtpc_receiver = new DtpcReceiver();
    dtpc_receiver->do_init();
    dtpc_receiver->start();


    BundleEvent* event;
    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    while (1) {
        if (should_stop()) {
            log_debug("DtpcDaemon: stopping");
            break;
        }

        int timeout = 10;

        //log_debug_p(LOOP_LOG, 
        //            "DtpcDaemon: checking eventq_->size() > 0, its size is %zu", 
        //            eventq_->size());

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
            
            
            log_debug_p(LOOP_LOG, "DtpcDaemon: handling event %s",
                        event->type_str());
            // handle the event
            handle_event(event);

            int elapsed = now.elapsed_ms();
            if (elapsed > 2000) {
                log_warn_p(LOOP_LOG, "event %s took %u ms to process",
                           event->type_str(), elapsed);
            }

            // record the last event time
            last_event_.get_time();

            log_debug_p(LOOP_LOG, "DtpcDaemon: deleting event %s",
                        event->type_str());
            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        //log_debug_p(LOOP_LOG, "DtpcDaemon: poll_multiple waiting for %d ms", 
        //            timeout);
        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);
        //log_debug_p(LOOP_LOG, "poll returned %d", cc);

        if (cc == oasys::IOTIMEOUT) {
            //log_debug_p(LOOP_LOG, "poll timeout");
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

    clean_up();
}

} // namespace dtn

#endif // DTPC_ENABLED
