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

#ifndef __STDC_FORMAT_MACROS
#    define __STDC_FORMAT_MACROS
#    include <inttypes.h>
#    undef __STDC_FORMAT_MACROS
#else
#    include <inttypes.h>
#endif

#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleRef.h"
#include "bundling/SDNV.h"

#include "DtpcDaemon.h"
#include "DtpcPayloadAggregationTimer.h"
#include "DtpcPayloadAggregator.h"
#include "DtpcPayloadAggregatorStore.h"
#include "DtpcProfile.h"
#include "DtpcProfileTable.h"
#include "DtpcTopic.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcPayloadAggregator::DtpcPayloadAggregator(std::string key, 
                                             const EndpointID& dest_eid, 
                                             const u_int32_t profile_id)
    : BundleEventHandler("DtpcPayloadAggregator", "/dtpc/payload/agg"),
      Thread(key.c_str(), CREATE_JOINABLE),
      key_(key),
      profile_id_(profile_id),
      seq_ctr_(1),
      size_(0),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false),
      wait_for_elision_func_(false),
      elision_func_response_notifier_("/dtpc/payload/agg/efunc")
{
    mutable_dest_eid()->assign(dest_eid);

    do_init();

    log_debug("Created Payload Aggregator for Dest: %s Profile: %d",
              dest_eid_.c_str(), profile_id_);
}


//----------------------------------------------------------------------
DtpcPayloadAggregator::DtpcPayloadAggregator(const oasys::Builder&)
    : BundleEventHandler("DtpcPayloadAggregator", "/dtpc/payload/agg"),
      Thread("DtpcPayloadAggregator", CREATE_JOINABLE),
      key_(""),
      profile_id_(0),
      seq_ctr_(1),
      size_(0),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false),
      wait_for_elision_func_(false),
      elision_func_response_notifier_("/dtpc/payload/agg/efunc")
{
    do_init();
}


//----------------------------------------------------------------------
DtpcPayloadAggregator::~DtpcPayloadAggregator () 
{
    delete buf_;

    DtpcTopicAggregatorIterator iter = topic_agg_map_.begin();
    while (iter != topic_agg_map_.end()) {
        delete iter->second;
        topic_agg_map_.erase(iter);

        iter = topic_agg_map_.begin();
    }

    delete eventq_;

    timer_ = nullptr;
}


//----------------------------------------------------------------------
void
DtpcPayloadAggregator::do_init()
{
    buf_ = new DtpcPayloadBuffer();

    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();

    memset(&stats_, 0, sizeof(stats_));
    memset(&expiration_time_, 0, sizeof(expiration_time_));
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::add_to_datastore()
{
    oasys::ScopeLock l(&lock_, "add_to_datastore");

    if (!queued_for_datastore_) {
        DtpcPayloadAggregatorStore::instance()->add(this);
        in_datastore_ = true;
        queued_for_datastore_ = true;

        log_debug("Added PayloadAggregator to the datastore: %s", 
                  durable_key().c_str());
    }
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::del_from_datastore()
{
    oasys::ScopeLock l(&lock_, "del_from_datastore");

    if (queued_for_datastore_) {
        DtpcPayloadAggregatorStore::instance()->del(durable_key());
    }
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::serialize(oasys::SerializeAction* a)
{
    DtpcTopicAggregator* topic_agg = NULL;

    a->process("key", &key_);
    a->process("profile_id", &profile_id_);
    a->process("dest_eid", &dest_eid_);
    a->process("seq_ctr", &seq_ctr_);
    a->process("size", &size_);
    a->process("expiration_time_sec", &expiration_time_.tv_sec);
    a->process("expiration_time_nsec", &expiration_time_.tv_usec);

    size_t num_topic_aggs = topic_agg_map_.size();
    a->process("num_topic_aggs", &num_topic_aggs);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        for ( size_t ix=0; ix<num_topic_aggs; ++ix ) {
            topic_agg = new DtpcTopicAggregator(oasys::Builder::builder());
            topic_agg->serialize(a);
            topic_agg_map_.insert(DtpcTopicAggregatorPair(topic_agg->topic_id(), topic_agg));
        }
    } else {
        // MARSHAL or INFO
        DtpcTopicAggregatorIterator iter = topic_agg_map_.begin();
        while (iter != topic_agg_map_.end()) {
            topic_agg = iter->second;
            topic_agg->serialize(a);
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void 
DtpcPayloadAggregator::ds_reload_post_processing()
{
    oasys::ScopeLock l(&lock_, "ds_reload_post_processing");

    set_in_datastore(true);
    set_queued_for_datastore(true);
    set_reloaded_from_ds();

    if (0 != expiration_time_.tv_sec) {
        struct timeval curr_time;
        gettimeofday(&curr_time, 0);
        log_debug("Starting expiration timer for reloaded PayAgg - Secs: %ld   (Current: %ld)",
                  expiration_time_.tv_sec, curr_time.tv_sec);

        // start a timer ticking for this payload if reloaded from datastore
        timer_ = std::make_shared<DtpcPayloadAggregationTimer>(key_, seq_ctr_);
        timer_->set_sptr(timer_);
        oasys::SPtr_Timer otimer = timer_;
        timer_->schedule_at( &expiration_time_, otimer);
    }
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::post(BundleEvent* event)
{
    post_event(event);
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::post_at_head(BundleEvent* event)
{
    post_event(event, false);
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::post_event(BundleEvent* event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");

    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
int
DtpcPayloadAggregator::send_data_item(u_int32_t topic_id, DtpcApplicationDataItem* data_item)
{
    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("send_data_item(%" PRIu32 ", %" PRIu32 "): Profile definition was deleted?",
                profile_id_, topic_id);
        return -3;  //XXX/dz need formal definitions for these error codes
    }

    log_debug("send_data_item for Dest: %s Profile: %d -- Topic: %d Len: %" PRIu64,
              dest_eid_.c_str(), profile_id_, topic_id, data_item->size());

    oasys::ScopeLock l (&lock_, "send_data_item");

    DtpcTopicAggregator* topic_agg = NULL;
    DtpcTopicAggregatorIterator iter = topic_agg_map_.find(topic_id);
    if (iter != topic_agg_map_.end()) {
        topic_agg = iter->second;
    } else {
        topic_agg = new DtpcTopicAggregator(topic_id, this);
        topic_agg_map_.insert(DtpcTopicAggregatorPair(topic_id, topic_agg));
    }

    ASSERT(NULL != topic_agg);
    ASSERT(!wait_for_elision_func_);
    bool optimize = (0 != profile->aggregation_size_limit());
    size_ += topic_agg->send_data_item(data_item, optimize, &wait_for_elision_func_);

    if (!wait_for_elision_func_ && (size_ > profile->aggregation_size_limit())) {
        log_debug("send_data_item for Dest: %s Profile: %d -- size limit reached (%" PRIu64 ") - sending payload",
                  dest_eid_.c_str(), profile_id_, size_);

        // cancel any pending timer
        if (timer_ != nullptr) {
            timer_->cancel();
            timer_ = nullptr;
            memset(&expiration_time_, 0, sizeof(expiration_time_));
        }

        send_payload();
    } else if ((timer_ == nullptr) && (profile->aggregation_time_limit() > 0)) {
        gettimeofday(&expiration_time_, 0);
        expiration_time_.tv_sec += profile->aggregation_time_limit();

        // start a timer ticking for this payload
        timer_ = std::make_shared<DtpcPayloadAggregationTimer>(key_, seq_ctr_);
        timer_->set_sptr(timer_);
        oasys::SPtr_Timer otimer = timer_;
        timer_->schedule_at( &expiration_time_, otimer);
    }

    DtpcPayloadAggregatorStore::instance()->update(this);

    log_debug("send_data_item - Updated PayloadAggregator in the datastore: %s", 
              durable_key().c_str());

    return 0;
}

//----------------------------------------------------------------------
void 
DtpcPayloadAggregator::elision_func_response(u_int32_t topic_id, bool modified,
                                             DtpcApplicationDataItemList* data_item_list)
{
    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("elision_func_response(%" PRIu32 ", %" PRIu32 "): Profile definition was deleted?",
                profile_id_, topic_id);
        return;  //XXX/dz PANIC??
    }

    oasys::ScopeLock l (&lock_, "elision_func_response");

    DtpcTopicAggregatorIterator iter = topic_agg_map_.find(topic_id);
    if (iter == topic_agg_map_.end()) {
        log_err("elision_func_response - Topic (%" PRIu32 ") Aggregator not found",
                topic_id);
    } else {
        DtpcTopicAggregator* topic_agg = NULL;
        topic_agg = iter->second;
        ASSERT(NULL != topic_agg);

        size_ += topic_agg->elision_func_response(modified, data_item_list);

        if (size_ > profile->aggregation_size_limit()) {
            log_debug("elision_func_response for Dest: %s Profile: %d -- size limit reached "
                      "after elision func (%" PRIu64 ") - sending payload",
                      dest_eid_.c_str(), profile_id_, size_);

            // cancel any pending timer
            if (timer_ != nullptr) {
                timer_->cancel();
                timer_ = nullptr;
                memset(&expiration_time_, 0, sizeof(expiration_time_));
            }

            send_payload();
        } else if ((timer_ == nullptr) && (profile->aggregation_time_limit() > 0)) {
            // we really shouldn't reach this condition
            gettimeofday(&expiration_time_, 0);
            expiration_time_.tv_sec += profile->aggregation_time_limit();

            // start a timer ticking for this payload
            timer_ = std::make_shared<DtpcPayloadAggregationTimer>(key_, seq_ctr_);
            timer_->set_sptr(timer_);
            oasys::SPtr_Timer otimer = timer_;
            timer_->schedule_at( &expiration_time_, otimer);
        }
    }

    // release the wait for elision func response
    wait_for_elision_func_ = false;
    elision_func_response_notifier_.notify();

    DtpcPayloadAggregatorStore::instance()->update(this);

    log_debug("elision_func_response - Updated PayloadAggregator in the datastore: %s", 
              durable_key().c_str());
}

//----------------------------------------------------------------------
void 
DtpcPayloadAggregator::timer_expired(u_int64_t seq_ctr)
{
    bool update_ds = false;
    oasys::ScopeLock l(&lock_, "timer_expired");

    if (seq_ctr != seq_ctr_) {
        log_debug("timer_expired for Dest: %s Profile: %d -- SeqCtr: %" PRIu64 " - ignore: current SeqCtr: %" PRIu64,
              dest_eid_.c_str(), profile_id_, seq_ctr, seq_ctr_);  
    } else {
        log_debug("timer_expired for Dest: %s Profile: %d -- SeqCtr: %" PRIu64 " - sending payload",
                  dest_eid_.c_str(), profile_id_, seq_ctr);  

        if (timer_ != nullptr) {
            // timer deletes itself on expiration
            timer_ = nullptr;
            memset(&expiration_time_, 0, sizeof(expiration_time_));
        }

        send_payload();
        update_ds = true;
    }

    if (update_ds) {
        DtpcPayloadAggregatorStore::instance()->update(this);

        log_debug("timer_expired - Updated PayloadAggregator in the datastore: %s", 
                  durable_key().c_str());
    }
}

//----------------------------------------------------------------------
bool
DtpcPayloadAggregator::send_payload()
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());

    bool result = false;

    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("send_payload did not find Profile ID: %" PRIu32 " - aborting payload", profile_id_);
        ASSERT(NULL != profile);
    }

    // load the header info
    buf_->clear();
    buf_->reserve(size_ + topic_agg_map_.size()*8 + 22);
    
    int64_t actual_size = 0;

    // First field/byte has bit flags
    //     Version Number  2 bits  
    //     Reserved        5 bits
    //     PDU Type        1 bit  (0=data, 1=ack)
    // reference Table A4-1
    char* ptr = buf_->tail_buf(1);
    *ptr = 0;
    *ptr |= (DtpcProtocolDataUnit::DTPC_VERSION_NUMBER << DtpcProtocolDataUnit::DTPC_VERSION_BIT_SHIFT);
    *ptr |= DtpcProtocolDataUnit::DTPC_PDU_TYPE_DATA;
    ++actual_size;
    buf_->incr_len(1);

    // Next field is the Profile ID (SDNV)
    int len = SDNV::encoding_len(profile_id_);
    ptr = buf_->tail_buf(len);
    len = SDNV::encode(profile_id_, ptr, len);
    ASSERT(len > 0);
    actual_size += len;
    buf_->incr_len(len);

    // Next field is the Payload Sequence Number (SDNV)
    // - do not increment seq ctr until we know we have data to send
    u_int64_t psn = (profile->retransmission_limit() > 0) ? seq_ctr_ : 0;
    len = SDNV::encoding_len(psn);
    ptr = buf_->tail_buf(len);
    len = SDNV::encode(psn, ptr, len);
    ASSERT(len > 0);
    actual_size += len;
    buf_->incr_len(len);

    // save off the header size so we can tell if data was added to the payload
    int64_t header_size = actual_size;    

    // loop through the topics adding their data items to the payload
    DtpcTopicAggregator* topic_agg = NULL;
    DtpcTopicAggregatorIterator iter = topic_agg_map_.begin();
    while (iter != topic_agg_map_.end()) {
        topic_agg = iter->second;
        actual_size += topic_agg->load_payload(buf_);
        ++iter;
    }

    if (actual_size > header_size) {
        // actually need to send the payload
        log_debug("Send payload - Dest: %s Profile: %" PRIu32 " size: %" PRIu64, 
                  dest_eid_.c_str(), profile_id_, actual_size);

        // transmit the payload
        if (transmit_pdu()) {
            // now we can bump the seq ctr
            ++seq_ctr_;
            result = true;
        }
    } else {
        log_debug("send_payload for Dest: %s Profile: %d -- Payload size is zero - skip",
                  dest_eid_.c_str(), profile_id_);
    }

    if (profile->aggregation_time_limit() > 0) {
        // start a new timer
        gettimeofday(&expiration_time_, 0);
        expiration_time_.tv_sec += profile->aggregation_time_limit();

        // start a timer ticking for this payload
        timer_ = std::make_shared<DtpcPayloadAggregationTimer>(key_, seq_ctr_);
        timer_->set_sptr(timer_);
        oasys::SPtr_Timer otimer = timer_;
        timer_->schedule_at( &expiration_time_, otimer);
    }

    return result;
}


//----------------------------------------------------------------------
//XXX/dz - need different versions of this method for built-in 
//         vs standalone imlpementations
Bundle*
DtpcPayloadAggregator::init_bundle()
{
    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("init_bundle did not find Profile ID: %" PRIu32 " - aborting payload", profile_id_);
        ASSERT(NULL != profile);
    }

    Bundle* bundle = nullptr;
    
    if (BundleDaemon::params_.api_send_bp_version7_) {
        bundle = new Bundle();
    } else {
        bundle = new Bundle(BundleProtocol::BP_VERSION_6);
    }


    // load the EIDs
    bundle->set_source(DtpcDaemon::instance()->local_eid());
    bundle->mutable_dest()->assign(dest_eid());
    bundle->mutable_replyto()->assign(profile->replyto());
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_singleton_dest(true);

    // set the priority code
    switch (profile->priority()) {
        case 0: bundle->set_priority(Bundle::COS_BULK); break;
        case 1: bundle->set_priority(Bundle::COS_NORMAL); break;
        case 2: bundle->set_priority(Bundle::COS_EXPEDITED); break;
        case 3: bundle->set_priority(Bundle::COS_RESERVED); break;
    default:
        log_err("invalid priority level %d - setting to BULK", (int)profile->priority());
        bundle->set_priority(Bundle::COS_BULK);
    };   

    //XXX/dz need to set the ECOS priority
    //bundle->set_ecos_ordinal(profile->ecos_ordinal());


    //XXX/dz add fragment flag to Profile??
    //bundle->set_do_not_fragment(profile->do_not_fragment());

    // set the bundle flags
    bundle->set_custody_requested(profile->custody_transfer());
    bundle->set_receive_rcpt(profile->rpt_reception());
    bundle->set_delivery_rcpt(profile->rpt_delivery());
    bundle->set_forward_rcpt(profile->rpt_forward());
    bundle->set_custody_rcpt(profile->rpt_acceptance());
    bundle->set_deletion_rcpt(profile->rpt_deletion());

    // assume full expiration time which may be overridden on retransmit
    bundle->set_expiration_secs(profile->expiration());

    return bundle;
}

//----------------------------------------------------------------------
//XXX/dz - need different versions of this method for built-in 
//         vs standalone imlpementations
bool
DtpcPayloadAggregator::transmit_pdu()
{
    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("transmit_pdu did not find Profile ID:"
                " %" PRIu32 " - aborting payload", profile_id_);
        ASSERT(NULL != profile);
    }


    BundleRef bref("DtpcPayloadAggregator::transmit_pdu");
    bref = init_bundle();

    // fill the bundle payload with buf_
    bref->mutable_payload()->set_length(buf_->len());
    bref->mutable_payload()->set_data(buf_->buf(), buf_->len());

    log_debug("transmit_pdu - posting bundle to send (%s~%" PRIi32 "~%" PRIu64 ") - length: %ld", 
              dest_eid().c_str(), profile_id_, seq_ctr_, buf_->len());

    BundleDaemon::post(new BundleReceivedEvent(bref.object(), EVENTSRC_APP));

    // reset the size for the next payload aggregation
    size_ = 0;

    // create and queue a PDU if Transmission Service is indicated
    if (profile->retransmission_limit() > 0) {
        DtpcProtocolDataUnit* pdu = new DtpcProtocolDataUnit(dest_eid(), profile_id_, seq_ctr_);
        pdu->set_creation_ts(bref->creation_time_secs());
        pdu->set_buf(buf_);

        // create a new buffer for the next PDU
        buf_ = new DtpcPayloadBuffer();

        // post the PDU transmitted event which will add the PDU to the list 
        // waiting to be ACK'd and start a retransmit timer
        DtpcDaemon::post(new DtpcPduTransmittedEvent(pdu));
    }

    return true;
}

//----------------------------------------------------------------------
//XXX/dz - need different versions of this method for built-in 
//         vs standalone imlpementations
bool
DtpcPayloadAggregator::retransmit_pdu(DtpcProtocolDataUnit* pdu)
{
    DtpcProfile* profile = DtpcProfileTable::instance()->get(profile_id_);
    if (NULL == profile) {
        log_err("retransmit_payload did not find Profile ID: "
                "%" PRIu32 " - aborting payload", profile_id_);
        ASSERT(NULL != profile);
    }

    // note that this is unsigned arithmetic
    u_int64_t expiration_remaining = pdu->creation_ts() + profile->expiration() -
                                         BundleTimestamp::get_current_time_secs();

    if (pdu->retransmit_count() >= profile->retransmission_limit()) {
        // post a delete PDU event
        DtpcDaemon::post(new DtpcPduDeleteRequest(pdu));

        return false;
    } else if (expiration_remaining >= profile->expiration()) {
        // check to make sure there is time left before PDU expires
        log_debug("retransmit_pdu - expiration time exceeded with retransmits left: %s %d/%d",
                  pdu->key().c_str(), pdu->retransmit_count(), profile->retransmission_limit());

        // post a delete PDU event
        DtpcDaemon::post(new DtpcPduDeleteRequest(pdu));
 
        return false;
    }


    BundleRef bref("DtpcPayloadAggregator::retransmit_pdu");
    bref = init_bundle();

    // fill the bundle payload with the PDU data for retransmission
    bref->mutable_payload()->set_length(pdu->size());
    bref->mutable_payload()->set_data(pdu->buf()->buf(), pdu->size());

    // override the expiration on retransmits
    bref->set_expiration_secs(expiration_remaining);

    log_debug("retransmit_payload for Dest: %s Profile: %d -- posting bundle to send",
              dest_eid_.c_str(), profile_id_);

    BundleDaemon::post(new BundleReceivedEvent(bref.object(), EVENTSRC_APP));

    // post event to delete or monitor the PDU
    if (pdu->retransmit_count() >= profile->retransmission_limit()) {
        DtpcDaemon::post(new DtpcPduDeleteRequest(pdu));
    } else {
        DtpcDaemon::post(new DtpcPduTransmittedEvent(pdu));
    }

    return true;
}

//----------------------------------------------------------------------
void 
DtpcPayloadAggregator::handle_dtpc_send_data_item(DtpcSendDataItemEvent* event)
{
    log_debug("DTPC Send Data Item event received for Topic ID: %" PRIu32,
              event->topic_id_);

    ASSERT(NULL != event->result_);
    *event->result_ = send_data_item(event->topic_id_, event->data_item_);
}

//----------------------------------------------------------------------
void 
DtpcPayloadAggregator::handle_dtpc_payload_aggregation_timer_expired(DtpcPayloadAggregationTimerExpiredEvent* event)
{
    timer_expired(event->seq_ctr_);
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::event_handlers_completed(BundleEvent* event)
{
    log_debug("event handlers completed for (%p) %s", event, event->type_str());
    
    /**
     * Once bundle reception, transmission or delivery has been
     * processed by the router, check to see if it's still needed,
     * otherwise we delete it.
     */
    if (event->type_ == DTPC_SEND_DATA_ITEM) {
        DtpcSendDataItemEvent* ev = (DtpcSendDataItemEvent*)event;
        if (*ev->result_ < 0) {
            log_debug("DTPC Send Data Item event - deleting DataItem after failure");
            delete ev->data_item_;
            ev->data_item_ = NULL;
        }
    }
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
    
    event_handlers_completed(event);

    stats_.events_processed_++;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
DtpcPayloadAggregator::run()
{
    char threadname[16] = "DtpcPayldAggtr";
    pthread_setname_np(pthread_self(), threadname);
   
    static const char* LOOP_LOG = "/dtpc/payload/agg/loop";
    
    if (!reloaded_from_ds_) {
        add_to_datastore();
    }

    BundleEvent* event;
    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    while (1) {
        if (should_stop()) {
            log_debug("DtpcPayloadAggregator - stopping");
            break;
        }

        // pause processing events if waiting for an elision function response
        if (wait_for_elision_func_) {
            elision_func_response_notifier_.wait();
        }

        int timeout = 10;

        //log_debug_p(LOOP_LOG, 
        //            "DtpcPayloadAggregator: checking eventq_->size() > 0, its size is %zu", 
        //            eventq_->size());

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            // handle the event
            handle_event(event);

            // record the last event time
            last_event_.get_time();

            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        //log_debug_p(LOOP_LOG, "DtpcPayloadAggregator: poll_multiple waiting for %d ms", 
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
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
