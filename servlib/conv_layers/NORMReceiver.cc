/*
 * Copyright 2008 The MITRE Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The US Government will not be charged any license fee and/or royalties
 * related to this software. Neither name of The MITRE Corporation; nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 */

/*
 * This product includes software written and developed 
 * by Brian Adamson and Joe Macker of the Naval Research 
 * Laboratory (NRL).
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(NORM_ENABLED)

#include <normApi.h>
#include "bundling/BundleDaemon.h"
#include "NORMConvergenceLayer.h"
#include "NORMSender.h"
#include "NORMReceiver.h"

namespace dtn {

//----------------------------------------------------------------------
NORMReceiver::NORMReceiver(NORMParameters *params, const LinkRef &link,
                           ReceiveLoop *strategy)
    : Thread("NORMReceiver"),
      Logger("NORMReceiver", "/dtn/cl/norm/receiver/"),
      link_params_(params),
      strategy_(strategy),
      remote_eid_(link->remote_eid()),
      timer_(0)
{
    ASSERT(strategy_);
    eventq_ = new oasys::MsgQueue<NormEvent>(logpath(), &lock_, false);
    inactivity_timer_start(link);
}

//----------------------------------------------------------------------
NORMReceiver::NORMReceiver(NORMParameters *params, ReceiveLoop *strategy)
    : Thread("NORMReceiver"),
      Logger("NORMReceiver", "/dtn/cl/norm/receiver/"),
      link_params_(params),
      strategy_(strategy),
      remote_eid_()
{
    ASSERT(strategy_);
    eventq_ = new oasys::MsgQueue<NormEvent>(logpath(), &lock_, false);
}

//----------------------------------------------------------------------
NORMReceiver::~NORMReceiver() {
    if (timer_)
        timer_->cancel();
    delete strategy_;
    delete eventq_;
    delete link_params_;
}

//----------------------------------------------------------------------
NormSessionHandle
NORMReceiver::norm_session()
{
    return link_params_->norm_session();
}

//----------------------------------------------------------------------
NORMSender*
NORMReceiver::norm_sender()
{
    return link_params_->norm_sender();
}

//----------------------------------------------------------------------
void
NORMReceiver::run()
{
    strategy_->run(this);
}

//----------------------------------------------------------------------
void
NORMReceiver::inactivity_timer_start(const LinkRef &link)
{
    ASSERT(timer_ == 0);
    if (link->type() != Link::ALWAYSON) {
        // initialize the inactivity timer
        timer_ = new InactivityTimer(this);
        timer_->schedule_in(LINK_DEAD_MULTIPLIER *
                            link_params_->keepalive_intvl());
    }
}

//----------------------------------------------------------------------
BundleRef
NORMReceiver::find_bundle(const BundleTimestamp& creation_ts) const
{
    BundleList *pending = BundleDaemon::instance()->pending_bundles();
    return pending->find(remote_eid_, creation_ts);
}

//----------------------------------------------------------------------
void
NORMReceiver::inactivity_timer_reschedule()
{
    // must check for a non-null timer_ since we could
    // be attempting to reschedule just after a timeout
    if ((norm_sender()->contact()->link()->type() != Link::ALWAYSON) &&
         timer_) {
        // reset the Inactivity timer
        log_debug("rescheduling norm inactivity timer");
        timer_->cancel();
        timer_ = new InactivityTimer(this);
        timer_->schedule_in(LINK_DEAD_MULTIPLIER *
                            link_params_->keepalive_intvl());
    }
}

//----------------------------------------------------------------------
void
NORMReceiver::process_data(u_char *bundle_buf, size_t len)
{
    // we should now have a full bundle
    Bundle* bundle = new Bundle();
    
    bool complete = false;
    int cc = BundleProtocol::consume(bundle, bundle_buf, len, &complete);

    if (cc < 0) {
        log_err("process_data: bundle protocol error");
        delete bundle;
        return;
    }

    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, remote_eid_));
}

//----------------------------------------------------------------------
void
NORMReceiver::InactivityTimer::timeout(
    const struct timeval &now)
{
    (void)now;
    NORMSender *sender = receiver_->norm_sender();

    ContactRef contact = sender->contact();
    LinkRef link = contact->link();
    log_debug("norm session inactive, closing link %s", link->name());
    BundleDaemon::instance()->post_at_head(
        new LinkStateChangeRequest(link, Link::CLOSED, ContactEvent::BROKEN));

    receiver_->timer_ = 0;
    delete this;
}

//----------------------------------------------------------------------
ReceiveLoop::ReceiveLoop()
    : Logger("ReceiveLoop", "/dtn/cl/norm/receiver/")
{
}

//----------------------------------------------------------------------
void
ReceiveOnly::run(NORMReceiver *receiver)
{
    while (1) {
        if (receiver->should_stop()) return;

        NormEvent event = receiver->eventq_->pop_blocking();

        log_debug("\nNORMConvergenceLayer event dump (type = %i session = %p "
                  "sender = %p object = %p)",
                  event.type, event.session, event.sender, event.object);
    
        if (event.type == NORM_RX_OBJECT_COMPLETED) {
            u_char *bp = (u_char*)NormDataAccessData(event.object);
            size_t len = (size_t)NormObjectGetSize(event.object);
            receiver->process_data(bp, len);

            NormObjectRelease(event.object);
        }

    }
}

//----------------------------------------------------------------------
ReceiveWatermark::ReceiveWatermark(SendReliable *send_strategy)
    : send_strategy_(send_strategy), link_open_(true)
{
    ASSERT(send_strategy_);
}

//----------------------------------------------------------------------
void
ReceiveWatermark::run(NORMReceiver *receiver)
{
    while (1) {
        if (receiver->should_stop()) return;

        Contact *contact = receiver->norm_sender()->contact().object();
        if (contact && contact->link()->isopen()) {
            if (! link_open_) {
                link_open_ = true;
                handle_open_contact(receiver, contact->link());
            }
        } else {
            if (link_open_) {
                link_open_ = false;
                handle_close_contact(receiver);
            }
        }

        NormEvent event = receiver->eventq_->pop_blocking();

        log_debug("\nNORMConvergenceLayer event dump (type = %i session = %p "
                  "sender = %p object = %p)",
                  event.type, event.session, event.sender, event.object);

        switch (event.type) {
        case NORM_REMOTE_SENDER_ACTIVE: {
            if (link_open_) {
                receiver->inactivity_timer_reschedule();
            }
            break;
        }

        case NORM_GRTT_UPDATED: {
            if (link_open_ && (event.sender != NORM_NODE_INVALID)) {
                receiver->inactivity_timer_reschedule();
            }
            break;
        }

        case NORM_TX_WATERMARK_COMPLETED: {
            ASSERT(receiver->norm_sender());
            NormAckingStatus status = NormGetAckingStatus(receiver->norm_session());
            send_strategy_->set_watermark_result(status);

            if (status == NORM_ACK_SUCCESS) {
                log_debug("WATERMARK_COMPLETED: tx_cache size = %zu", send_strategy_->size());
            }

            send_strategy_->watermark_complete_notifier()->notify();
            break;
        }

        case NORM_TX_OBJECT_PURGED: {
            ASSERT(receiver->norm_sender());
            oasys::ScopeLock l(send_strategy_->lock(), "NORMReceiver::run");
            SendReliable::iterator i = send_strategy_->begin();
            SendReliable::iterator end = send_strategy_->end();

            for (; i != end; ++i) {
                if ((*i)->handle_list_.back() == event.object) {
                    log_debug("purging bundle id: %hu",
                              (*i)->bundle_->bundleid());
                    send_strategy_->erase(i);
                    break;
                }
            }
            break;
        }

        case NORM_REMOTE_SENDER_NEW: {
            ASSERT(receiver->norm_sender());
            ASSERT(contact);
            log_info("new NORM remote sender %lu on link %s", 
                      (unsigned long)NormNodeGetId(event.sender),
                      contact->link()->name());
            break;
        }

        case NORM_REMOTE_SENDER_PURGED: {
            ASSERT(receiver->norm_sender());
            log_info("NORM remote sender %lu on link %s is gone", 
                     (unsigned long)NormNodeGetId(event.sender),
                     contact->link()->name());

            NormRemoveAckingNode(event.session, NormNodeGetId(event.sender));
            break;
        }

        case NORM_RX_CMD_NEW: {
            static size_t keepalive_len = strlen(KEEPALIVE_STR);

            if (link_open_) {
                receiver->inactivity_timer_reschedule();
            }

            char cmd[keepalive_len];
            if (NormNodeGetCommand(event.sender, cmd, (unsigned int *) &keepalive_len)) {
                if (strncmp(cmd, KEEPALIVE_STR, keepalive_len) != 0) {
                    log_debug("received unknown norm command!");
                }
            }

            break;
        }

        case NORM_RX_OBJECT_NEW:
        case NORM_RX_OBJECT_UPDATED: {
            if (link_open_) {
                receiver->inactivity_timer_reschedule();
            }
            break;
        }

        case NORM_RX_OBJECT_COMPLETED: {
            if (link_open_) {
                receiver->inactivity_timer_reschedule();
            }

            u_char *bp = (u_char*)NormDataAccessData(event.object);
            size_t len = (size_t)NormObjectGetSize(event.object);
            typedef NORMConvergenceLayer::BundleInfo BundleInfo;

            // process Norm info
            if (NormObjectHasInfo(event.object)) {
                char char_info[sizeof(BundleInfo)];
                unsigned short info_len = sizeof(BundleInfo);
                unsigned short ret =
                    NormObjectGetInfo(event.object, char_info, info_len);
                ASSERT(ret == sizeof(BundleInfo));

                BundleInfo *info = (BundleInfo*)char_info;
                BundleTimestamp ts(ntohl(info->seconds_), ntohl(info->seqno_));
                log_debug("processing remote bundle with timestamp %u:%u, chunk %hu",
                          ts.seconds_, ts.seqno_, ntohs(info->chunk_));
                reassembly_map_t::iterator i =
                    reassembly_map_.find(ts);

                if (i == reassembly_map_.end()) {
                    // new bundle
                    reassembly_map_[ts] =
                        new BundleReassemblyBuf(receiver);
                    reassembly_map_[ts]->add_fragment(ntohl(info->total_length_),
                                                      ntohl(info->object_size_),
                                                      ntohl(info->frag_offset_),
                                                      ntohl(info->payload_offset_));
                }

                BundleReassemblyBuf *bundle_buf = reassembly_map_[ts];
                bundle_buf->set(bp, len, ntohl(info->object_size_),
                                ntohl(info->frag_offset_),
                                ntohs(info->chunk_),
                                ntohl(info->total_length_),
                                ntohl(info->payload_offset_));

                if (bundle_buf->check_completes()) {
                    log_debug("processing of remote bundle with timestamp %u:%u complete!",
                              ts.seconds_, ts.seqno_);
                    delete bundle_buf;
                    reassembly_map_.erase(ts);
                }
            } else {
                receiver->process_data(bp, len);
            }

            NormObjectRelease(event.object);
            break;
        }

        case NORM_TX_QUEUE_EMPTY: {
            send_strategy_->num_tx_pending() = 0;
            break;
        }

        case NORM_CC_ACTIVE:
        case NORM_CC_INACTIVE:
        case NORM_LOCAL_SENDER_CLOSED:
        case NORM_REMOTE_SENDER_INACTIVE:
        case NORM_TX_FLUSH_COMPLETED:
        case NORM_RX_OBJECT_INFO:
        case NORM_RX_OBJECT_ABORTED:
        case NORM_TX_OBJECT_SENT:
        case NORM_TX_QUEUE_VACANCY:
        case NORM_TX_CMD_SENT:
        case NORM_TX_RATE_CHANGED:
        case NORM_EVENT_INVALID: // trigger to unblock and exit
            break;
        default: {
            log_err("NOTREACHED: %d", event.type);
            NOTREACHED;
            break;
        }
        }
    }
}

//----------------------------------------------------------------------
ReceiveWatermark::BundleReassemblyBuf::BundleReassemblyBuf(NORMReceiver *receiver)
    : Logger("NORMReceiver::BundleReassemblyBuf",
             "/dtn/cl/norm/receiver/reassemblybuf"),
      receiver_(receiver)
{
}

//----------------------------------------------------------------------
ReceiveWatermark::BundleReassemblyBuf::~BundleReassemblyBuf()
{
    frag_list_t::iterator i = frag_list_.begin();
    frag_list_t::iterator end = frag_list_.end();

    for (; i != end; ++i) {
        free((*i).bundle_);
    }
}

//----------------------------------------------------------------------
void
ReceiveWatermark::BundleReassemblyBuf::add_fragment(u_int32_t length,
                                                    u_int32_t object_size,
                                                    u_int32_t frag_offset,
                                                    u_int32_t payload_offset)
{
    if (frag_list_.size() != 0) {
        Fragment &prev_frag = frag_list_.back();
        prev_frag.length_ = prev_frag.payload_offset_ + frag_offset;
        prev_frag.rcvd_chunks_.resize(num_chunks(prev_frag.length_,
                                                 prev_frag.object_size_));
        prev_frag.bundle_ =
            (u_char*)realloc(prev_frag.bundle_, prev_frag.length_);
    }

    frag_list_.push_back(Fragment(length, object_size, frag_offset, payload_offset));
    Fragment &frag = frag_list_.back();

    u_int16_t chunks = num_chunks(length, object_size);
    for (int i = 0; i < chunks; ++i) {
        frag.rcvd_chunks_.push_back(false);
    }

    frag.bundle_ = (u_char*)malloc(frag.length_);
}

//----------------------------------------------------------------------
void
ReceiveWatermark::BundleReassemblyBuf::set(const u_char *buf, size_t length,
                                           u_int32_t object_size, u_int32_t frag_offset,
                                           u_int16_t chunk, u_int32_t total_length,
                                           u_int32_t payload_offset)
{
retry:
    frag_list_t::iterator i = frag_list_.begin();
    frag_list_t::iterator end = frag_list_.end();

    for (; i != end; ++i) {
        if ((*i).frag_offset_ == frag_offset) {
            ASSERT(chunk <= (u_int16_t)(*i).rcvd_chunks_.size());
            ASSERT(length <= object_size);

            size_t offset = (chunk - 1) * object_size;
            memcpy(&((*i).bundle_)[offset], buf, length);
            ((*i).rcvd_chunks_)[chunk - 1] = true;
            break;
        }
    }

    if (i == end) {
        add_fragment(total_length, object_size, frag_offset, payload_offset);
        goto retry;
    }
}

//----------------------------------------------------------------------
bool
ReceiveWatermark::BundleReassemblyBuf::check_completes(bool link_up)
{
    u_int32_t object_size = receiver_->link_params()->object_size();

    frag_list_t::iterator begin = frag_list_.begin();
    frag_list_t::iterator end = frag_list_.end();
    frag_list_t::iterator i = begin;

    // check for and deliver completed bundle fragments
    for (; i != end; ++i) {
        if (! (*i).complete_) {
            std::vector<bool>::iterator j = (*i).rcvd_chunks_.begin();
            std::vector<bool>::iterator end = (*i).rcvd_chunks_.end();
    
            u_int32_t rcvd_len = 0;
            for (; j != end; ++j) {
                if ((*j) == false) {
                    break;
                }

                // this calculation is inaccurate for the last
                // chunk, but isn't used if the fragment is complete
                rcvd_len += object_size;
            }
    
            if (j == end) {
                // all bundle chunks received
                (*i).complete_ = true;
                ReceiveWatermark::process_data(receiver_, (*i).bundle_, (*i).length_);
            } else if ((! link_up) && rcvd_len > 0) {
                // the contact is down so process a partial fragment
                ReceiveWatermark::process_data(receiver_, (*i).bundle_, rcvd_len);
            }
        }
    }

    // check for a fully received bundle
    i = begin;
    for (; i != end; ++i) {
        if ((*i).complete_ == false) break;
    }

    return (i == end) ? true : false;
}

//----------------------------------------------------------------------
u_int16_t
ReceiveWatermark::BundleReassemblyBuf::num_chunks(u_int32_t length,
                                                  u_int32_t object_size)
{
    if (length % object_size == 0) {
        return length/object_size;
    }

    return length/object_size + 1;
}

//----------------------------------------------------------------------
ReceiveWatermark::BundleReassemblyBuf::Fragment::Fragment(
    u_int32_t length, u_int32_t object_size,
    u_int32_t frag_offset, u_int32_t payload_offset)
    : length_(length), object_size_(object_size),
      frag_offset_(frag_offset), payload_offset_(payload_offset),
      complete_(false)
{
}

//----------------------------------------------------------------------
void
ReceiveWatermark::handle_open_contact(NORMReceiver *receiver,
                                      const LinkRef &link)
{
    receiver->inactivity_timer_start(link);
}

//----------------------------------------------------------------------
void
ReceiveWatermark::handle_close_contact(NORMReceiver *receiver)
{
    reassembly_map_t::iterator i = reassembly_map_.begin();
    reassembly_map_t::iterator end = reassembly_map_.end();

    while (i != end) {
        if (((*i).second)->check_completes(false)) {
            delete (*i).second;
            reassembly_map_t::iterator remove = i;
            ++i;
            reassembly_map_.erase(remove);
        } else {
            ++i;
        }
    }

    // kill the InactivityTimer
    if (receiver->timer_) {
        receiver->timer_->cancel();
        receiver->timer_ = 0;
    }
}

} // namespace dtn
#endif // NORM_ENABLED
