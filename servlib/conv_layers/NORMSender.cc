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
#include <oasys/util/Random.h>
#include <oasys/util/StringUtils.h>
#include <oasys/io/NetUtils.h>
#include "bundling/BundleDaemon.h"
#include "NORMConvergenceLayer.h"
#include "NORMSessionManager.h"
#include "NORMReceiver.h"
#include "NORMSender.h"

namespace dtn {

//----------------------------------------------------------------------
NORMSender::NORMSender(NORMParameters *params,
                       const ContactRef& contact,
                       SendStrategy *strategy)
    : Thread("NORMSender"),
      Logger("NORMSender", "/dtn/cl/norm/sender/"),
      link_params_(params),
      contact_(contact.object(), "NORMSender"),
      strategy_(strategy),
      contact_up_(false),
      //transmitting_(false),
      closing_session_(false)
{
    ASSERT(strategy_);
    commandq_ = new oasys::MsgQueue<CLMsg>(logpath());
}

//----------------------------------------------------------------------
NORMSender::~NORMSender()
{
    if (timer_)
        timer_->cancel();
    really_close_contact();
    delete strategy_;
    delete commandq_;
}

//----------------------------------------------------------------------
bool
NORMSender::init()
{
    log_debug("initializing sender");

    // configure the sender
    NormSetTxRobustFactor(norm_session(), link_params_->tx_robust_factor());
    apply_cc();
    NormSetGroupSize(norm_session(), link_params_->group_size());
    NormSetBackoffFactor(norm_session(), link_params_->backoff_factor());
    NormSetTxCacheBounds(norm_session(),
                         link_params_->tx_cache_size_max(),
                         link_params_->tx_cache_count_min(),
                         link_params_->tx_cache_count_max());
    NormSetAutoParity(norm_session(), link_params_->auto_parity());
    apply_tos();

    // begin participating as a Norm sender
    if (! NormStartSender(norm_session(),
                         (NormSessionId)oasys::Random::rand(),
                          link_params_->fec_buf_size(),
                          link_params_->segment_size(),
                          link_params_->block_size(),
                          link_params_->num_parity())) {
        return false;
    }

    // initialize the keepalive timer
    timer_ = new KeepaliveTimer(this);
    timer_->schedule_in(link_params_->keepalive_intvl());

    return true;
}

//----------------------------------------------------------------------
void
NORMSender::set_bundle_sent_time()
{
    bundle_sent_ = oasys::Time::now();
}

//----------------------------------------------------------------------
NormSessionHandle 
NORMSender::norm_session()
{
    return link_params_->norm_session();
}

//----------------------------------------------------------------------
NORMSender*
NORMSender::norm_sender()
{
    return link_params_->norm_sender();
}

//----------------------------------------------------------------------
NORMReceiver*
NORMSender::norm_receiver()
{
    return link_params_->norm_receiver();
}

//----------------------------------------------------------------------
void
NORMSender::really_close_contact()
{
    log_debug("closing norm session");

    closing_session_ = true;

    // set a flag for the receiver thread to stop
    norm_receiver()->set_should_stop();

    // unregister the receiver from the session manager
    NORMSessionManager::instance()->
        remove_receiver(norm_receiver());

    while (! norm_receiver()->is_stopped()) {
        oasys::Thread::yield();
    }

    // free norm receiver thread
    link_params_->set_norm_receiver(0);
    delete norm_receiver();

    // stop the sender thread
    set_should_stop();
    commandq_->push_back(
        NORMSender::CLMsg(NORMSender::CLMSG_INVALID)); 

    // destroy the norm session
    NormStopSender(norm_session());
    NormDestroySession(norm_session());
    link_params_->set_norm_session(NORM_SESSION_INVALID);

    link_params_->set_norm_sender(0);
}

//----------------------------------------------------------------------
void
NORMSender::apply_cc()
{
    if (link_params_->cc()) {
        NormSetCongestionControl(norm_session(), true);
        NormSetTxRateBounds(norm_session(), -1.0, link_params_->rate());
    } else {
        NormSetTxRate(norm_session(), link_params_->rate());
    }
}

//----------------------------------------------------------------------
void
NORMSender::apply_tos()
{
    static u_int8_t ecn_capable = 0x02;
    u_int8_t tos = link_params_->tos() << 2;

    if (link_params_->ecn()) {
        NormSetEcnSupport(norm_session(),
                          link_params_->ecn(),
                          link_params_->ecn());
        tos = tos | ecn_capable;
    }

    NormSetTOS(norm_session(), tos);
}

//----------------------------------------------------------------------
void
NORMSender::KeepaliveTimer::timeout(const struct timeval &now)
{
    static size_t heartbeat_len = strlen(KEEPALIVE_STR);
    (void)now;

    Contact *contact = sender_->contact_.object();
    if (contact && contact->link()->isopen() &&
        sender_->bundle_sent_time().elapsed_ms() >=
        sender_->link_params_->keepalive_intvl()) {

        if (contact->link()->type() == Link::OPPORTUNISTIC) {
            char *heartbeat = (char *)malloc(sizeof(char) * heartbeat_len);
            strncpy(heartbeat, KEEPALIVE_STR, heartbeat_len);
        
            NormSendCommand(sender_->norm_session(), heartbeat, heartbeat_len);
            free(heartbeat);
        }
    }

    sender_->strategy_->timeout_bottom_half(sender_);
    schedule_in(sender_->link_params_->keepalive_intvl());
}

//----------------------------------------------------------------------
void
NORMSender::run()
{
    while (1) {
        if (should_stop()) return;

        CLMsg msg = commandq_->pop_blocking();
        switch(msg.type_) {
        case CLMSG_BUNDLE_QUEUED:
            strategy_->handle_bundle_queued(this);
            break;
        case CLMSG_CANCEL_BUNDLE:
            strategy_->handle_cancel_bundle(
                contact_->link(), msg.bundle_.object());
            break;
        case CLMSG_WATERMARK:
            strategy_->handle_watermark(this);
            break;
        case CLMSG_BREAK_CONTACT:
            contact_up_ = false;
            strategy_->handle_close_contact(
                this, contact_->link());
            // drain the command queue
            while (commandq_->try_pop(&msg)) {}
        default:
            break;
        }

        oasys::Thread::yield();
    }
}

//----------------------------------------------------------------------
void
NORMSender::handle_bundle_queued()
{
    if (contact_up_) {
        oasys::ScopeLock l(contact_->link()->queue()->lock(),
                           "NORMSender::handle_bundle_queued");

        const LinkRef link = contact_->link();
        BundleRef bref("NORMSender::handle_bundle_queued");
        bref = link->queue()->front();

        if (bref == NULL) {
            log_debug("NORMSender::run -- no bundles queued on link");
            return;
        }

        BlockInfoVec* blocks = bref->xmit_blocks()->find_blocks(contact_->link());
        ASSERT(blocks != NULL);

        size_t total_len = BundleProtocol::total_length(blocks);
        ASSERT(total_len <= pow(2, 32));

        move_bundle_to_inflight(bref, total_len);
        l.unlock();

        strategy_->send_bundle(this, bref.object(), blocks, total_len);
   }
}

//----------------------------------------------------------------------
void
NORMSender::move_bundle_to_inflight(BundleRef &bref, size_t length)
{
    const LinkRef link = contact_->link();
    link->del_from_queue(bref, length);
    link->add_to_inflight(bref, length);
}

//----------------------------------------------------------------------
void
NORMSender::move_bundle_to_link(Bundle *bundle, size_t length)
{
    const LinkRef link = contact_->link();
    BundleRef bref("NORMSender::move_to_link");
    bref = bundle;
    link->del_from_inflight(bref, length);
    link->add_to_queue(bref, length);
}

//----------------------------------------------------------------------
NormObjectHandle
NORMSender::enqueue_data(Bundle *bundle, BlockInfoVec *blocks,
                         size_t length, size_t offset, bool *last,
                         BundleInfo *info, size_t info_length)
{
    u_char *buf = (u_char*)malloc(length);
    ASSERT(buf != NULL);

    size_t res = BundleProtocol::produce(bundle, blocks, buf,
                                         offset, length, last);

    if (res < length) {
        ASSERT(*last);
        buf = (u_char*)realloc(buf, res);

        if (info) {
            //adjust the chunk length to actual size
            info->length_ = htonl(res);
        }
    }

    // write the bundle out to the NORM protocol engine
    NormObjectHandle oh = NormDataEnqueue(norm_session(), (char*)buf, res,
                                          (char *)info, info_length);
    if (oh == NORM_OBJECT_INVALID) {
        free(buf);
    }

    return oh;
}

//----------------------------------------------------------------------
SendStrategy::SendStrategy()
    : Logger("SendStrategy", "/dtn/cl/norm/sender/")
{
}

//----------------------------------------------------------------------
void
SendStrategy::handle_bundle_queued(NORMSender *sender)
{
    sender->handle_bundle_queued();
}

//----------------------------------------------------------------------
void
SendStrategy::handle_cancel_bundle(const LinkRef &link, Bundle *bundle)
{
    (void)link;
    (void)bundle;
}

//----------------------------------------------------------------------
void
SendStrategy::handle_close_contact(NORMSender *sender, const LinkRef &link)
{
    (void)link;

    NormSetGrttProbingMode(sender->norm_session(), NORM_PROBE_NONE);
}

//----------------------------------------------------------------------
void
SendStrategy::handle_watermark(NORMSender *sender)
{
    (void)sender;
}

//----------------------------------------------------------------------
void
SendStrategy::timeout_bottom_half(NORMSender *sender)
{
    (void)sender;
}

//----------------------------------------------------------------------
void
SendBestEffort::send_bundle(NORMSender *sender, Bundle *bundle,
                            BlockInfoVec *blocks, size_t total_len)
{
    // write the bundle out to the NORM protocol engine
    bool complete = false;
    NormObjectHandle oh = sender->enqueue_data(bundle, blocks, total_len, 0, &complete);
    ASSERT(complete);

    if (oh == NORM_OBJECT_INVALID) {
        log_warn("send_bundle: NormDataEnqueue failed for bundle %i, countMax exceeded?",
                bundle->bundleid());
        sender->move_bundle_to_link(bundle, total_len);
        return;
    }

    BundleDaemon::post(
        new BundleTransmittedEvent(bundle, sender->contact(),
                                   sender->contact()->link(), total_len, 0));

    sender->set_bundle_sent_time();

    log_info("send_bundle: successfully sent bundle length %d",
             total_len);
}

//----------------------------------------------------------------------
SendReliable::NORMBundle::NORMBundle(Bundle *bundle,
    const ContactRef &contact, const LinkRef &link,
    size_t total_len)
    : bundle_(bundle, "SendReliable::NORMBundle"),
      contact_(contact), link_(link), total_len_(total_len),
      sent_(false)
{
}

//----------------------------------------------------------------------
SendReliable::SendReliable()
    : bundle_tx_(0),
      watermark_object_(0),
      watermark_object_candidate_(0),
      watermark_result_(NORM_ACK_INVALID),
      watermark_request_(false),
      watermark_pending_(false),
      watermark_complete_notifier_(new oasys::Notifier(logpath_)),
      num_tx_pending_(0),
      lock_(logpath_)
{
}

//----------------------------------------------------------------------
SendReliable::~SendReliable()
{
    oasys::ScopeLock l(&lock_, "NORMSender::~NORMSender");
    erase(begin(), end());
    delete watermark_complete_notifier_;
    l.unlock();
}

//----------------------------------------------------------------------
SendReliable::iterator
SendReliable::begin()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

    return sent_cache_.begin();
}

//----------------------------------------------------------------------
SendReliable::iterator
SendReliable::end()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

    return sent_cache_.end();
}

//----------------------------------------------------------------------
SendReliable::const_iterator
SendReliable::begin() const
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

    return sent_cache_.begin();
}

//----------------------------------------------------------------------
SendReliable::const_iterator
SendReliable::end() const
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

    return sent_cache_.end();
}

//----------------------------------------------------------------------
size_t
SendReliable::size()
{
    return sent_cache_.size();
}

//----------------------------------------------------------------------
SendReliable::iterator
SendReliable::erase(iterator pos)
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

    delete *pos;
    return sent_cache_.erase(pos);
}

//----------------------------------------------------------------------
SendReliable::iterator
SendReliable::erase(iterator first, iterator last)
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock NORMSender object list before using iterator");

        SendReliable::iterator i = first;
        while (i != last) {
            i = erase(i);
        }

    return i;
}

//----------------------------------------------------------------------
void
SendReliable::handle_bundle_queued(NORMSender *sender)
{
    if (bundle_tx_) {
        if (sender->bundle_sent_time().elapsed_ms() >=
            sender->link_params()->inter_object_pause()) {
            send_bundle_chunk();
        } else {
            bundle_tx_->sender_->commandq_->push_back(
                NORMSender::CLMsg(NORMSender::CLMSG_BUNDLE_QUEUED));
        }
    } else {
        sender->handle_bundle_queued();
    }
}

//----------------------------------------------------------------------
void
SendReliable::send_bundle(NORMSender *sender, Bundle *bundle,
                          BlockInfoVec *blocks, size_t total_len)
{
    oasys::ScopeLock l(lock(), "SendReliable::send_bundle");

    // if bundle is found in the sent cache, don't do anything
    // we're working on it...

    iterator i = this->begin();
    iterator end = this->end();
    for (; i != end; ++i) {
        if ((*i)->bundle_->bundleid() == bundle->bundleid()) {
            return;
        }
    }

    // this is a new bundle

    // create a new cache entry in the sent cache
    sent_cache_.push_back(
        new NORMBundle(bundle, sender->contact(),
                       sender->contact()->link(),
                       total_len));
    NORMBundle *norm_bundle = sent_cache_.back();

    u_int32_t object_size = sender->link_params()->object_size();

    if ((object_size == 0) || (total_len <= object_size)) {
        // do not partition this bundle into multiple norm objects
        bool complete = false;
        NormObjectHandle oh = sender->enqueue_data(bundle, blocks, total_len, 0, &complete);
        ASSERT(complete);

        if (oh == NORM_OBJECT_INVALID) {
            log_warn("send_bundle_chunk: NormDataEnqueue failed for bundle %i, countMax exceeded?",
                    bundle->bundleid());
            sent_cache_.pop_back();
            delete norm_bundle;
            sender->move_bundle_to_link(bundle, total_len);
            return;
        } else {
            log_info("send_bundle: successfully sent bundle %i of length %d",
                     bundle->bundleid(), total_len);
            norm_bundle->sent_ = true;
            norm_bundle->handle_list_.push_back(oh);
            return send_bundle_complete(sender, norm_bundle);
        }
    }

    bundle_tx_ = new BundleTransmitState(bundle, blocks, norm_bundle,
                                         total_len, 1, 0, object_size,
                                         sender); 

    l.unlock();
    send_bundle_chunk();
}

//----------------------------------------------------------------------
void
SendReliable::send_bundle_chunk()
{
    oasys::ScopeLock l(lock(), "SendReliable::send_bundle_chunk");

    typedef NORMConvergenceLayer::BundleInfo BundleInfo;
    
    // generate bundle chunk info
    BundleInfo *info = new BundleInfo(htonl(bundle_tx_->bundle_->creation_ts().seconds_),
                                      htonl(bundle_tx_->bundle_->creation_ts().seqno_),
                                      htonl(bundle_tx_->bundle_->frag_offset()),
                                      htonl(bundle_tx_->total_len_),
                                      htonl(BundleProtocol::payload_offset(bundle_tx_->blocks_)),
                                      htonl(bundle_tx_->object_size_),  //really length of chunk
                                      htonl(bundle_tx_->object_size_),
                                      htons(bundle_tx_->round_));
    
    // write the chunk out to the NORM protocol engine
    bool complete = false;
    size_t offset_save = bundle_tx_->offset_;
    bundle_tx_->offset_ = bundle_tx_->object_size_ * (bundle_tx_->round_ - 1);
    NormObjectHandle oh = bundle_tx_->sender_->enqueue_data(
                               bundle_tx_->bundle_, bundle_tx_->blocks_,
                               bundle_tx_->object_size_, bundle_tx_->offset_,
                               &complete, info, sizeof(BundleInfo));

    if (oh == NORM_OBJECT_INVALID) {
        // in this case we don't put the bundle back on the link queue
        // since a part of the bundle may have already been transmitted
        // try again...
        log_warn("send_bundle_chunk: NormDataEnqueue failed for bundle %i, "
                 "countMax exceeded? -- retrying...",
                 bundle_tx_->bundle_->bundleid());
        bundle_tx_->offset_ = offset_save;
        delete info;
        goto queue_next;
    }
    
    ++num_tx_pending_;
    bundle_tx_->bytes_sent_ += ntohl(info->length_);
    
    // add this chunk handle to the norm bundle
    bundle_tx_->norm_bundle_->handle_list_.push_back(oh);
    send_bundle_complete(bundle_tx_->sender_, bundle_tx_->norm_bundle_);
    
    if (complete) {
        log_info("send_bundle: successfully sent bundle %i of length %d",
                 bundle_tx_->bundle_->bundleid(), bundle_tx_->total_len_);

        bundle_tx_->norm_bundle_->sent_ = true;
        delete bundle_tx_;
        bundle_tx_ = 0;

        return;
    }
    
    ++bundle_tx_->round_;

queue_next:
    bundle_tx_->sender_->commandq_->push_back(
        NORMSender::CLMsg(NORMSender::CLMSG_BUNDLE_QUEUED));
}

//----------------------------------------------------------------------
void
SendReliable::send_bundle_complete(NORMSender *sender,
                                   NORMBundle *norm_bundle)
{
    watermark_object_candidate_ = norm_bundle;
    sender->set_bundle_sent_time();
}

//----------------------------------------------------------------------
void
SendReliable::bundles_transmitted(NormAckingStatus status)
{
    (void) status;
    oasys::ScopeLock l(lock(), "SendReliable::bundles_transmitted");

    iterator begin = this->begin();
    iterator i = begin;

    // free acknowledged objects
    bool found = false;
    while (! found) {
        NORMBundle::iterator begin = (*i)->begin();
        NORMBundle::iterator end = (*i)->end();
        NORMBundle::iterator j = begin;
        for (; j != end; ++j) {
            if (*(watermark_object_->watermark_) == (*j)) {
                found = true;
                break;
            }
        }

        // if the watermark is *not* on the last bundle chunk
        // erase all the objects that have been acked
        if (found && ((! watermark_object_->sent_) ||
            (watermark_object_->sent_ &&
            (*(watermark_object_->watermark_) != watermark_object_->handle_list_.back())))) {

            // erase bundle chunks up to and incl the watermark
            NORMBundle::iterator watermark_copy = watermark_object_->watermark_;
            ++watermark_copy;
            watermark_object_->handle_list_.erase(begin, watermark_copy); 
            break;
        }

        const LinkRef &link = (*i)->link_;
        const ContactRef &contact = (*i)->contact_;
        size_t total_len = (*i)->total_len_;;
        BundleDaemon::post(
            new BundleTransmittedEvent((*i)->bundle_.object(), contact,
                                       link, total_len, total_len));

        ++i;
    }

    erase(begin, i);
}

//----------------------------------------------------------------------
void
SendReliable::handle_cancel_bundle(const LinkRef &link, Bundle *bundle)
{
    (void) link;
    log_debug("bundle %d already in flight, can't cancel",
              bundle->bundleid());
}

//----------------------------------------------------------------------
void
SendReliable::handle_close_contact(NORMSender *sender, const LinkRef &link)
{
    oasys::ScopeLock l(lock(), "SendReliable::handle_close_contact");

    SendStrategy::handle_close_contact(sender, link);

    iterator begin = this->begin();
    iterator end = this->end();
    iterator i = begin;
    bool found_oldest_bundle = false;

    for (; i != end; ++i) {
        // bundles in the inflight queue have either not been sent
        // or haven't yet been positively acknowledged

        u_int32_t reliable_bytes = 0;

        if (! found_oldest_bundle) {
            u_int32_t non_acked_len = 0;
            found_oldest_bundle = true;

            NORMBundle::iterator j = (*i)->begin();
            NORMBundle::iterator end = (*i)->end();
            for (; j != end; ++j) {
                non_acked_len += NormObjectGetSize(*j);
            }

            if (! (*i)->sent_) {
                ASSERT(bundle_tx_);
                ASSERT((*i)->bundle_->bundleid() ==
                       bundle_tx_->bundle_->bundleid());
                reliable_bytes = bundle_tx_->bytes_sent_ - non_acked_len;
            } else {
                reliable_bytes = (*i)->total_len_ - non_acked_len;
            }

            if (reliable_bytes) {
                BundleDaemon::post(
                    new BundleTransmittedEvent((*i)->bundle_.object(), (*i)->contact_,
                                               (*i)->link_, (*i)->total_len_,
                                               reliable_bytes));
            }
        }

        if (reliable_bytes == 0) {
            // move bundle back to the link queue
            link->del_from_inflight((*i)->bundle_, (*i)->total_len_);
            link->add_to_queue((*i)->bundle_, (*i)->total_len_);
        }

        // cancel all the bundle chunks
        NORMBundle::iterator j = (*i)->begin();
        NORMBundle::iterator end = (*i)->end();
        for (; j != end; ++j) {
            NormObjectCancel(*j);
        }
    }

    erase(begin, end);

    // clear out watermark state, if any, becuase
    // all the norm objects have just been cancelled
    watermark_object_ = 0;
    watermark_object_candidate_ = 0;

    // clear any state on partially transmitted bundles
    if (bundle_tx_) {
        delete bundle_tx_;
        bundle_tx_ = 0;
    }
}

//----------------------------------------------------------------------
void
SendReliable::handle_watermark(NORMSender *sender)
{
    if (watermark_pending_) {
        if (watermark_complete_notifier_->wait(NULL, 0)) {
            if (sender->contact_up_ && watermark_object_) {
                switch(watermark_result_) {
                case NORM_ACK_FAILURE:
                case NORM_ACK_PENDING:
                    log_debug("watermark failed "
                              "resetting watermark for object handle: %p",
                              *(watermark_object_->watermark_));
                    NormSetWatermark(sender->norm_session(),
                                     *(watermark_object_->watermark_),
                                     true);
                    return;
                    break;
                case NORM_ACK_SUCCESS:
                    log_debug("watermark success");
                    bundles_transmitted(watermark_result_);
                    watermark_object_ = 0;
                    break;
                case NORM_ACK_INVALID:
                default:
                    break;
                }
            }
        } else {
            return;
        }

        watermark_result_ = NORM_ACK_INVALID;
        watermark_pending_ = false;
    }

    if (sender->contact_up_ && watermark_request_ &&
        watermark_object_candidate_) {

        watermark_object_ = watermark_object_candidate_;
        NORMBundle::iterator end = watermark_object_->end();
        watermark_object_->watermark_ = --end;
        watermark_object_candidate_ = 0;
        watermark_pending_ = true;

        log_debug("setting watermark for object handle: %p",
                  *(watermark_object_->watermark_));
        NormSetWatermark(sender->norm_session(),
                         *(watermark_object_->watermark_),
                         true);

        watermark_request_ = false;
    }
}

//----------------------------------------------------------------------
void
SendReliable::timeout_bottom_half(NORMSender *sender)
{
    Contact *contact = sender->contact_.object();
    if (contact && contact->link()->isopen()) {
        watermark_request_ = true;
        sender->commandq_->push_back(
            NORMSender::CLMsg(NORMSender::CLMSG_WATERMARK)); 
    }
}

//----------------------------------------------------------------------
void
SendReliable::push_acking_nodes(NORMSender *sender)
{
    typedef std::vector<std::string> node_id_vector_t;
    node_id_vector_t node_id_vector;

    oasys::tokenize(sender->link_params()->acking_list(), ",", &node_id_vector);

    node_id_vector_t::iterator i = node_id_vector.begin();
    node_id_vector_t::iterator end = node_id_vector.end();
    for (; i != end; ++i) {
        in_addr_t addr = INADDR_NONE;
        if (oasys::gethostbyname((*i).c_str(), &addr) != 0) {
            log_warn("can't lookup hostname '%s'", (*i).c_str());
            continue;
        }
        if (! NormAddAckingNode(sender->norm_session(), htonl((NormNodeId)addr))) {
            log_err("failed to add acking node %s!", (*i).c_str());
        }
    }
}

//----------------------------------------------------------------------
SendReliable::BundleTransmitState::BundleTransmitState(
    Bundle *bundle, BlockInfoVec *blocks,
    NORMBundle *norm_bundle, size_t total_len,
    u_int16_t round, size_t offset, u_int32_t object_size,
    NORMSender *sender)
    : bundle_(bundle), blocks_(blocks), norm_bundle_(norm_bundle),
      total_len_(total_len), round_(round), offset_(offset),
      object_size_(object_size), sender_(sender),
      bytes_sent_(0)
{
}

} // namespace dtn
#endif // NORM_ENABLED
