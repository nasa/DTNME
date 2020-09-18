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
#endif
#include <inttypes.h>

#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "naming/IPNScheme.h"


#include "DtpcDaemon.h"
#include "DtpcDeliverPduTimer.h"
#include "DtpcPayloadAggregationTimer.h"
#include "DtpcDataPduCollector.h"
#include "DtpcTopicCollector.h"
#include "DtpcTopicTable.h"
#include "DtpcProfile.h"
#include "DtpcProfileTable.h"
#include "DtpcTopic.h"
#include "DtpcDataPduCollectorStore.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcDataPduCollector::DtpcDataPduCollector(std::string key, 
                                           const EndpointID& remote_eid, 
                                           const u_int32_t profile_id)
    : Logger("DtpcDataPduCollector", "/dtpc/dpducollector"),
      key_(key),
      profile_id_(profile_id),
      seq_ctr_(1),
      in_datastore_(false),
      queued_for_datastore_(false)
{
    mutable_remote_eid()->assign(remote_eid);

    log_debug("Created Data Pdu Collector for Dest: %s Profile: %d",
              remote_eid_.c_str(), profile_id_);

    memset(&expiration_time_, 0, sizeof(expiration_time_));

    add_to_datastore();
}


//----------------------------------------------------------------------
DtpcDataPduCollector::DtpcDataPduCollector(const oasys::Builder&)
    : Logger("DtpcDataPduCollector", "/dtpc/payload/collector"),
      key_(""),
      profile_id_(0),
      seq_ctr_(1),
      in_datastore_(false),
      queued_for_datastore_(false)
{
    memset(&expiration_time_, 0, sizeof(expiration_time_));
}


//----------------------------------------------------------------------
DtpcDataPduCollector::~DtpcDataPduCollector () 
{
    DtpcProtocolDataUnit* pdu;
    DtpcPduSeqCtrIterator iter;
    while (! pdu_map_.empty()) {
        iter = pdu_map_.begin();
        pdu = iter->second;

        pdu_map_.erase(iter);
        delete pdu;
    }
}


//----------------------------------------------------------------------
void
DtpcDataPduCollector::add_to_datastore()
{
    oasys::ScopeLock l(&lock_, "add_to_datastore");

    queued_for_datastore_ = true;
    DtpcDataPduCollectorStore::instance()->add(this);
    in_datastore_ = true;
}


//----------------------------------------------------------------------
void
DtpcDataPduCollector::del_from_datastore()
{
    oasys::ScopeLock l(&lock_, "del_from_datastore");

    if (queued_for_datastore_) {
        DtpcDataPduCollectorStore::instance()->del(durable_key());
    }
}


//----------------------------------------------------------------------
void
DtpcDataPduCollector::serialize(oasys::SerializeAction* a)
{
    DtpcProtocolDataUnit* pdu = NULL;

    a->process("key", &key_);
    a->process("profile_id", &profile_id_);
    a->process("remote_eid", &remote_eid_);
    a->process("seq_ctr", &seq_ctr_);
    a->process("expiration_time_sec", &expiration_time_.tv_sec);
    a->process("expiration_time_nsec", &expiration_time_.tv_usec);

    u_int32_t num_pdus = pdu_map_.size();
    a->process("num_pdus", &num_pdus);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        for ( size_t ix=0; ix<num_pdus; ++ix ) {
            pdu = new DtpcProtocolDataUnit(oasys::Builder::builder());
            pdu->serialize(a);
            pdu_map_.insert(DtpcPduSeqCtrPair(pdu->seq_ctr(), pdu));
        }
    } else {
        // MARSHAL or INFO
        DtpcPduSeqCtrIterator iter = pdu_map_.begin();
        while (iter != pdu_map_.end()) {
            pdu = iter->second;
            pdu->serialize(a);
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::ds_reload_post_processing()
{
    oasys::ScopeLock l(&lock_, "ds_reload_post_processing");

    in_datastore_ = true;
    queued_for_datastore_ = true;
    reloaded_from_ds_ = true;

    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);

    // Discard all expired DPDUs
    DtpcProtocolDataUnit* pdu;
    DtpcPduSeqCtrIterator iter;
    while (! pdu_map_.empty()) {
        iter = pdu_map_.begin();
        pdu = iter->second;

        if (curr_time.tv_sec > pdu->expiration_ts()) {
            log_debug("ds_reload_post_processing - delete expired PDU: %"PRIu64,
                     pdu->seq_ctr());

            seq_ctr_ = pdu->seq_ctr() + 1;
            pdu_map_.erase(iter);
            delete pdu;
        } else {
            // wait for up to 1 second before expiration for prior PDUs 
            // to arrive then give up and deliver what we have
            expiration_time_.tv_sec = pdu->expiration_ts() - 1;
            expiration_time_.tv_usec = 0;
    
            timer_ = new DtpcDeliverPduTimer(pdu->collector_key(), pdu->seq_ctr());
            timer_->schedule_at(&expiration_time_);

            log_debug("ds_reload_post_processing - started expiration timer for PDU: %"PRIu64" secs: %ld",
                     pdu->seq_ctr(), expiration_time_.tv_sec);

            break;
        }
    }
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::pdu_received(DtpcProtocolDataUnit* pdu, BundleRef& rcvd_bref)
{
    oasys::ScopeLock l(&lock_, "DtpcDataPduCollector::pdu_received");

    if (pdu->app_ack()) {
        send_ack(pdu, rcvd_bref);
    }

    if (0 == pdu->seq_ctr()) {
        log_info("pdu_received - no transport service - deliver DPDU - SeqCtr: %"PRIu64,
                  pdu->seq_ctr());
        // No Transport Service so deliver PDU immmediately
        deliver_pdu(pdu);
    } else if (pdu->seq_ctr() < seq_ctr_) {
        // duplicate suppression - discard PDU
        log_warn("pdu_received - deleting duplicate or late PDU - SeqCtr: %"PRIu64" Expected: %"PRIu64,
                  pdu->seq_ctr(), seq_ctr_);
        delete pdu;
    } else if (pdu->seq_ctr() == seq_ctr_) {
        log_info("pdu_received - deliver DPDU - SeqCtr: %"PRIu64,
                  pdu->seq_ctr());
        // this is the PDU we are waiting for so deliver immediately
        deliver_pdu(pdu);
    } else {
        // not the PDU we are waiting for so add it to the PDU list and
        // update the delivery timer if appropriate
        log_warn("pdu_received - queue DPDU - SeqCtr: %"PRIu64" Expected: %"PRIu64,
                  pdu->seq_ctr(), seq_ctr_);
        queue_pdu(pdu);
    }

    DtpcDataPduCollectorStore::instance()->update(this);
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::send_ack(DtpcProtocolDataUnit* pdu, BundleRef& rcvd_bref)
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());


    // ** Implementation decision is to set ACK bundle characteristics from the received bundle


    BundleRef bref("DtpcDataPduCollector::send_ack");
    bref = new Bundle();


    // load the EIDs
    bref->mutable_source()->assign(pdu->local_eid());

    if (pdu->remote_eid().scheme() == IPNScheme::instance()) {
        u_int64_t node = 0;
        u_int64_t service = 0;
        if (! IPNScheme::parse(pdu->remote_eid(), &node, &service)) {
            bref->mutable_dest()->assign(pdu->remote_eid());
        } else {
            // If we received a PDU from an ION node using the transmit 
            // service number (128) then we need to send back the ACK to
            // the receive service number (129)
            if (service == DtpcDaemon::params_.ipn_transmit_service_number) {
                service = DtpcDaemon::params_.ipn_receive_service_number;
            }
            IPNScheme::format(bref->mutable_dest(), node, service);
        }
    } else {
        bref->mutable_dest()->assign(pdu->remote_eid());
    }
    bref->mutable_replyto()->assign(rcvd_bref->replyto());
    bref->mutable_custodian()->assign(EndpointID::NULL_EID());
    bref->set_singleton_dest(true);

    // set the priority code
    bref->set_priority(rcvd_bref->priority());
    
    //XXX/dz placeholder to set the ECOS priority when it is implemented
    //bref->set_ecos_flags(rcvd_bref->ecos_flags());
    //bref->set_ecos_ordinal(rcvd_bref->ecos_ordinal());


    // set the bref flags
    bref->set_custody_requested(false);
    bref->set_receive_rcpt(rcvd_bref->receive_rcpt());
    bref->set_delivery_rcpt(rcvd_bref->delivery_rcpt());
    bref->set_forward_rcpt(rcvd_bref->forward_rcpt());
    bref->set_custody_rcpt(rcvd_bref->custody_rcpt());
    bref->set_deletion_rcpt(rcvd_bref->deletion_rcpt());

    bref->set_expiration(rcvd_bref->expiration());
    bref->set_app_acked_rcpt(false);


    // generate the DPDU Header Fields which will be the bundle payload
    buf_.clear();
    buf_.reserve(80);
    
    // First field/byte has bit flags
    //     Version Number  2 bits  
    //     Reserved        5 bits
    //     PDU Type        1 bit  (0=data, 1=ack)
    // reference Table A4-1
    char* ptr = buf_.tail_buf(1);
    *ptr = 0;
    *ptr |= (DtpcProtocolDataUnit::DTPC_VERSION_NUMBER << DtpcProtocolDataUnit::DTPC_VERSION_BIT_SHIFT);
    *ptr |= DtpcProtocolDataUnit::DTPC_PDU_TYPE_ACK;
    buf_.incr_len(1);

    // Next field is the Profile ID (SDNV)
    int len = SDNV::encoding_len(profile_id_);
    ptr = buf_.tail_buf(len);
    len = SDNV::encode(profile_id_, ptr, len);
    ASSERT(len > 0);
    buf_.incr_len(len);

    // Next field is the Payload Sequence Number (SDNV) that was received
    len = SDNV::encoding_len(pdu->seq_ctr());
    ptr = buf_.tail_buf(len);
    len = SDNV::encode(pdu->seq_ctr(), ptr, len);
    ASSERT(len > 0);
    buf_.incr_len(len);

    // fill the bundle payload with buf_
    bref->mutable_payload()->set_length(buf_.len());
    bref->mutable_payload()->set_data(buf_.buf(), buf_.len());

    log_debug("send_ack - posting bundle to send (%s~%"PRIi32"~%"PRIu64")", 
              remote_eid().c_str(), profile_id_, pdu->seq_ctr());

    BundleDaemon::post(new BundleReceivedEvent(bref.object(), EVENTSRC_APP));
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::deliver_pdu(DtpcProtocolDataUnit* pdu)
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());

    // break up the Data PDU into Topic Blocks and deliver
    u_char* payload_buf = pdu->buf()->buf();
    size_t payload_idx = pdu->topic_block_index();
    size_t payload_len = pdu->buf()->len() - payload_idx; 

    DtpcTopicCollector* collector = NULL;
    DtpcApplicationDataItem* adi = NULL;
    bool valid_topic;
    bool is_valid = true;
    // loop through the topics - structure has already been validated so we should not
    // break out of the processing but the checks are left in just to be safe
    while (is_valid && payload_len > 0) {
        // make sure we have a valid Topic ID SDNV and also check the actual value here
        u_int32_t topic_id;
        int num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &topic_id);
        if (-1 == num_bytes) {
            log_err("deliver_pdu - invalid Topic ID SDNV - idx %zu  len: %zu", payload_idx, payload_len);
            is_valid = false;
            break;
        }
        payload_idx += num_bytes;
        payload_len -= num_bytes;

        valid_topic = validate_topic_id(topic_id);

        // the Payload Record Count is an SDNV defining the number of 
        // Application Data Items that follow
        u_int64_t rec_count;
        num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &rec_count);
        if (-1 == num_bytes) {
            log_err("deliver_pdu - invalid Payload Record Count SDNV "
                    "- abort DTPC processing");
            is_valid = false;
            break;
        }
        if (0 == rec_count) {
            log_err("deliver_pdu - invalid Topic Block with zero records "
                    "- abort DTPC processing");
            is_valid = false;
            break;
        }
        payload_idx += num_bytes;
        payload_len -= num_bytes;

        // loop through the Application Data Items for this topic
        while (rec_count > 0) {
            size_t adi_len;
            num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &adi_len);
            if (-1 == num_bytes) {
                log_err("deliver_pdu - invalid Application Data Item Length SDNV "
                        "- abort DTPC processing");
                is_valid = false;
                break;
            }
            payload_idx += num_bytes;
            payload_len -= num_bytes;

            if (adi_len > payload_len) {
                log_err("deliver_pdu - invalid Application Data Item structure "
                        "(ADI Len: %zu, Payload Remaining: %zu) "
                        "- abort DTPC processing", adi_len, payload_len);
                is_valid = false;
                break;
            } else {
                // if invalid topic then we skip these ADIs to get to the next topic
                if (valid_topic) {
                    if (NULL == collector) {
                        collector = new DtpcTopicCollector(topic_id);
                        collector->set_remote_eid(pdu->remote_eid());
                        collector->set_expiration_ts(pdu->expiration_ts());
                    }
                    adi = new DtpcApplicationDataItem(pdu->remote_eid(), topic_id);
                    adi->reserve(adi_len);
                    adi->set_data(adi_len, &payload_buf[payload_idx]);
                    collector->deliver_data_item(adi);
                }

                payload_idx += adi_len;
                payload_len -= adi_len;
                --rec_count;
            }
        }

        if (is_valid && NULL != collector) {
            log_debug("deliver topic (%"PRIu32") to collector", topic_id);
            deliver_topic_collector(collector);
            collector = NULL;
        }
    }

    delete pdu;

    // clean up in case of an unlikely abort
    delete collector;

    // bump the sequence counter and check to see if any other queued PDUs can be delivered
    ++seq_ctr_;

    check_for_deliverable_pdu();
}


//----------------------------------------------------------------------
void
DtpcDataPduCollector::check_for_deliverable_pdu()
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());

    if (! pdu_map_.empty()) {
        DtpcPduSeqCtrIterator iter = pdu_map_.begin();
        DtpcProtocolDataUnit* pdu = iter->second;

        if (pdu->seq_ctr() == seq_ctr_) {
            if (NULL != timer_) {
                timer_->cancel();
                timer_ = NULL;
                memset(&expiration_time_, 0, sizeof(expiration_time_));
            }
            
            pdu_map_.erase(iter);
            deliver_pdu(pdu);

        } else if (NULL == timer_) {
            // wait for up to 1 second before expiration for prior PDUs 
            // to arrive then give up and deliver what we have            
            expiration_time_.tv_sec = pdu->expiration_ts() - 1;
            expiration_time_.tv_usec = 0;
    
            // start a timer ticking for this payload
            timer_ = new DtpcDeliverPduTimer(pdu->collector_key(), pdu->seq_ctr());
            timer_->schedule_at(&expiration_time_);
        }
    }
}


//----------------------------------------------------------------------
bool
DtpcDataPduCollector::validate_topic_id(u_int32_t topic_id)
{
    log_debug("validate_topic_id - validate Topic ID: %"PRIu32, topic_id);

    bool result = true;
    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    if (! toptab->find(topic_id) ) {
        if (DtpcDaemon::params_.require_predefined_topics_) {
            // on the fly topics not allowed
            log_err("validate_topic_id - Topic Block rejected - no predefined Topic ID: %"PRIu32,
                    topic_id);
            result = false;
        } else {
            // add the on the fly topic
	    oasys::StringBuffer errmsg;
            if (! toptab->add(&errmsg, topic_id, false, "<on the fly API topic received>")) {
                log_err("validate_topic_id - error adding on the fly Topic ID: %"PRIu32,
                        topic_id);
                result = false;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
DtpcDataPduCollector::deliver_topic_collector(DtpcTopicCollector* collector)
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());

    DtpcTopic* topic = DtpcTopicTable::instance()->get(collector->topic_id());
    if (NULL == topic) {
        log_err("deliver_topic_collector - Topic ID: %"PRIu32" not found - abort delivery",
                collector->topic_id());
        delete collector;
    } else {
        topic->deliver_topic_collector(collector);
    }
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::queue_pdu(DtpcProtocolDataUnit* pdu)
{
    // caller should have gotten the lock
    ASSERT(lock_.is_locked_by_me());

    DtpcPduSeqCtrInsertResult result = pdu_map_.insert(DtpcPduSeqCtrPair(pdu->seq_ctr(), pdu));
    DtpcPduSeqCtrIterator iter = result.first;

    // if this is now the first entry in the list then we need to 
    // cancel the delivery timer and start a new one
    if (iter == pdu_map_.begin()) {
        if (NULL != timer_) {
            timer_->cancel();
            timer_ = NULL;
            memset(&expiration_time_, 0, sizeof(expiration_time_));
        }

        // wait for up to 1 second before expiration for prior PDUs 
        // to arrive then give up and deliver what we have            
        expiration_time_.tv_sec = pdu->expiration_ts() - 1;
        expiration_time_.tv_usec = 0;

        // start a timer ticking for this payload
        timer_ = new DtpcDeliverPduTimer(pdu->collector_key(), pdu->seq_ctr());
        timer_->schedule_at(&expiration_time_);
    }
}

//----------------------------------------------------------------------
void 
DtpcDataPduCollector::timer_expired(u_int64_t seq_ctr)
{
    oasys::ScopeLock l(&lock_, "timer_expired");

    if (! pdu_map_.empty()) {
        DtpcPduSeqCtrIterator iter = pdu_map_.begin();
        DtpcProtocolDataUnit* pdu = iter->second;

        if (pdu->seq_ctr() == seq_ctr) {
            timer_ = NULL;
            memset(&expiration_time_, 0, sizeof(expiration_time_));

            pdu_map_.erase(iter);

            // update our sequence counter and deliver the PDU
            seq_ctr_ = seq_ctr;
            deliver_pdu(pdu);
        }

        // update the data store
        DtpcDataPduCollectorStore::instance()->update(this);
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
