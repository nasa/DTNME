/*
 *    Copyright 2006 Intel Corporation
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

#include <memory>

#include <third_party/oasys/util/OptParser.h>
#include "StreamConvergenceLayer.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "bundling/TempBundle.h"
#include "contacts/ContactManager.h"
#include "storage/BundleStore.h"
#include "routing/BundleRouter.h"

namespace dtn {

//----------------------------------------------------------------------
StreamConvergenceLayer::StreamLinkParams::StreamLinkParams(bool init_defaults)
    : LinkParams(init_defaults),
      segment_ack_enabled_(true),
      negative_ack_enabled_(true),
      keepalive_interval_(10),
      break_contact_on_keepalive_fault_(true),
      segment_length_(10000000),
      max_inflight_bundles_(100)
{
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::StreamLinkParams::serialize(oasys::SerializeAction *a)
{
    log_debug_p("StreamLinkParams", "StreamLinkParams::serialize");
    ConnectionConvergenceLayer::LinkParams::serialize(a);
    a->process("segment_ack_enabled", &segment_ack_enabled_);
    a->process("negative_ack_enabled", &negative_ack_enabled_);
    a->process("keepalive_interval", &keepalive_interval_);
    a->process("break_contact_on_keepalive_fault", &break_contact_on_keepalive_fault_);
    a->process("segment_length", &segment_length_);
    a->process("max_inflight_bundles", &max_inflight_bundles_);
}

//----------------------------------------------------------------------
StreamConvergenceLayer::StreamConvergenceLayer(const char* logpath,
                                               const char* cl_name,
                                               uint8_t    cl_version)
    : ConnectionConvergenceLayer(logpath, cl_name),
      cl_version_(cl_version)
{
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::parse_link_params(LinkParams* lparams,
                                          int argc, const char** argv,
                                          const char** invalidp)
{
    // all subclasses should create a params structure that derives
    // from StreamLinkParams
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(lparams);
    ASSERT(params != nullptr);
                               
    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("segment_ack_enabled",
                                &params->segment_ack_enabled_));
    
    p.addopt(new oasys::BoolOpt("negative_ack_enabled",
                                &params->negative_ack_enabled_));
    
    p.addopt(new oasys::UInt16Opt("keepalive_interval",
                                &params->keepalive_interval_));
    
    p.addopt(new oasys::BoolOpt("break_contact_on_keepalive_fault",
                                &params->break_contact_on_keepalive_fault_));

    p.addopt(new oasys::UInt64Opt("segment_length",
                                &params->segment_length_));
    
    p.addopt(new oasys::UIntOpt("max_inflight_bundles",
                                &params->max_inflight_bundles_));
    
    p.addopt(new oasys::UInt8Opt("cl_version",
                                 &cl_version_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false;
    }
    argc -= count;

    return ConnectionConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                         invalidp);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::finish_init_link(const LinkRef& link,
                                         LinkParams* lparams)
{
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(lparams);
    ASSERT(params != nullptr);

    // make sure to set the reliability bit in the link structure
    if (params->segment_ack_enabled_) {
        link->set_reliable(true);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT((link->cl_info() != nullptr) || (link->sptr_cl_info() != nullptr));

    ConnectionConvergenceLayer::dump_link(link, buf);
  

    StreamLinkParams* params = nullptr;

    if (link->sptr_cl_info() != nullptr) {
        params = dynamic_cast<StreamLinkParams*>(link->sptr_cl_info().get());
    } else {
        params = dynamic_cast<StreamLinkParams*>(link->cl_info());
    }

    ASSERT(params != nullptr);
    
    buf->appendf("segment_ack_enabled: %u\n", params->segment_ack_enabled_);
    buf->appendf("negative_ack_enabled: %u\n", params->negative_ack_enabled_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("break_contact_on_keepalive_fault: %s\n", params->break_contact_on_keepalive_fault_ ? "true" : "false" );
    buf->appendf("segment_length: %" PRIu64 "\n", params->segment_length_);
    buf->appendf("max_inflight_bundles: %" PRIu32 "\n", params->max_inflight_bundles_);
    buf->appendf("cl_version: %u\n", cl_version_);
}

//----------------------------------------------------------------------
StreamConvergenceLayer::Connection::Connection(const char* classname,
                                               const char* logpath,
                                               StreamConvergenceLayer* cl,
                                               StreamLinkParams* params,
                                               bool active_connector)
    : CLConnection(classname, logpath, cl, params, active_connector)
{
    cl_version_ = ((StreamConvergenceLayer*)cl_)->cl_version_;

    bdaemon_ = BundleDaemon::instance();
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::initiate_contact()
{
    size_t local_eid_len;
    size_t sdnv_len;

    // format the contact header
    ContactHeader contacthdr;
    contacthdr.magic   = htonl(MAGIC);
    contacthdr.version = cl_version_;
    
    contacthdr.flags = 0;

    StreamLinkParams* params = stream_lparams();
    
    if (params->segment_ack_enabled_)
        contacthdr.flags |= SEGMENT_ACK_ENABLED;
    
    if (params->reactive_frag_enabled_)
        contacthdr.flags |= REACTIVE_FRAG_ENABLED;
    
    contacthdr.keepalive_interval = htons(params->keepalive_interval_);

    // copy the contact header into the send buffer
    ASSERT(sendbuf_.fullbytes() == 0);
    if (sendbuf_.tailbytes() < sizeof(ContactHeader)) {
        log_warn("send buffer too short: %zu < needed %zu",
                 sendbuf_.tailbytes(), sizeof(ContactHeader));
        sendbuf_.reserve(sizeof(ContactHeader));
    }
    
    memcpy(sendbuf_.start(), &contacthdr, sizeof(ContactHeader));
    sendbuf_.fill(sizeof(ContactHeader));
    
    // follow up with the local endpoint id length + data

    if(!bdaemon_->params_.announce_ipn_)
    {
        local_eid_len = bdaemon_->local_eid()->length();
    } else {
        local_eid_len = bdaemon_->local_eid_ipn()->length();
    }

    sdnv_len = SDNV::encoding_len(local_eid_len);
    
    if (sendbuf_.tailbytes() < sdnv_len + local_eid_len) {
        log_warn("send buffer too short: %zu < needed %zu",
                 sendbuf_.tailbytes(), sdnv_len + local_eid_len);
        sendbuf_.reserve(sdnv_len + local_eid_len);
    }
    
    sdnv_len = SDNV::encode(local_eid_len,
                            (u_char*)sendbuf_.end(),
                            sendbuf_.tailbytes());
    sendbuf_.fill(sdnv_len);
   
    if(!bdaemon_->params_.announce_ipn_)
        memcpy(sendbuf_.end(), bdaemon_->local_eid()->data(), local_eid_len);
    else 
        memcpy(sendbuf_.end(), bdaemon_->local_eid_ipn()->data(), local_eid_len);

    sendbuf_.fill(local_eid_len);

    // drain the send buffer
    note_data_sent();
    send_data();

    /*
     * Now we initialize the various timers that are used for
     * keepalives / idle timeouts to make sure they're not used
     * uninitialized.
     */
    ::gettimeofday(&data_rcvd_, 0);
    ::gettimeofday(&data_sent_, 0);
    ::gettimeofday(&keepalive_sent_, 0);


    // XXX/demmer need to add a test for nothing coming back
    
    contact_initiated_ = true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_contact_initiation()
{
    ASSERT(! contact_up_);

    /*
     * First check for valid magic number.
     */
    uint32_t magic = 0;
    size_t len_needed = sizeof(magic);
    if (recvbuf_.fullbytes() < len_needed) {
 tooshort:
        log_debug("handle_contact_initiation: not enough data received "
                  "(need > %zu, got %zu)",
                  len_needed, recvbuf_.fullbytes());
        return;
    }

    memcpy(&magic, recvbuf_.start(), sizeof(magic));
    magic = ntohl(magic);
   
    if (magic != MAGIC) {
        log_err("remote sent magic number 0x%.8x, expected 0x%.8x "
                 "-- disconnecting.", magic, MAGIC);
        break_contact(ContactEvent::CL_ERROR);
        oasys::Breaker::break_here();
        return;
    }

    /*
     * Now check that we got a full contact header
     */
    len_needed = sizeof(ContactHeader);
    if (recvbuf_.fullbytes() < len_needed) {
        goto tooshort;
    }

    /*
     * Now check for enough data for the peer's eid
     */
    uint64_t peer_eid_len;
    int sdnv_len = SDNV::decode((u_char*)recvbuf_.start() +
                                  sizeof(ContactHeader),
                                recvbuf_.fullbytes() -
                                  sizeof(ContactHeader),
                                &peer_eid_len);
    if (sdnv_len < 0) {
        goto tooshort;
    }
    
    len_needed = sizeof(ContactHeader) + sdnv_len + peer_eid_len;
    if (recvbuf_.fullbytes() < len_needed) {
        goto tooshort;
    }
    
    /*
     * Ok, we have enough data, parse the contact header.
     */
    ContactHeader contacthdr;
    memcpy(&contacthdr, recvbuf_.start(), sizeof(ContactHeader));

    contacthdr.magic              = ntohl(contacthdr.magic);
    contacthdr.keepalive_interval = ntohs(contacthdr.keepalive_interval);

    recvbuf_.consume(sizeof(ContactHeader));
    
    /*
     * In this implementation, we can't handle other versions than our
     * own, but if the other side presents a higher version, we allow
     * it to go through and thereby allow them to downgrade to this
     * version.
     */
    if (contacthdr.version < cl_version_) {
        log_err("remote sent version %d, expected version %d "
                 "-- disconnecting.", contacthdr.version, cl_version_);
        break_contact(ContactEvent::CL_VERSION);
        return;
    }

    /*
     * Now do parameter negotiation.
     */
    StreamLinkParams* params = stream_lparams();
    
    params->keepalive_interval_ = std::min(params->keepalive_interval_, contacthdr.keepalive_interval);

    params->segment_ack_enabled_ = params->segment_ack_enabled_ &&
                                   (contacthdr.flags & SEGMENT_ACK_ENABLED);
    
    params->reactive_frag_enabled_ = params->reactive_frag_enabled_ &&
                                     (contacthdr.flags & REACTIVE_FRAG_ENABLED);

    params->negative_ack_enabled_ = params->negative_ack_enabled_ &&
                                     (contacthdr.flags & NEGATIVE_ACK_ENABLED);

    /*
     * Make sure to readjust poll_timeout in case we have a smaller
     * keepalive interval than data timeout
     */
    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
     
    /*
     * Now skip the sdnv that encodes the peer's eid length since we
     * parsed it above.
     */
    recvbuf_.consume(sdnv_len);

    /*
     * Finally, parse the peer node's eid and give it to the base
     * class to handle (i.e. by linking us to a Contact if we don't
     * have one).
     */
    std::string eid_str(recvbuf_.start(), peer_eid_len);
    SPtr_EID sptr_peer_eid = BD_MAKE_EID(eid_str);
    if (!sptr_peer_eid->valid()) {
        log_err("protocol error: invalid endpoint id '%s' (len %llu)",
                sptr_peer_eid->c_str(), U64FMT(peer_eid_len));
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    if (!find_contact(sptr_peer_eid)) {
        ASSERT(contact_ == nullptr);
        log_err("StreamConvergenceLayer::Connection::"
                  "handle_contact_initiation: failed to find contact");
        break_contact(ContactEvent::CL_ERROR);
        return;
    }
    recvbuf_.consume(peer_eid_len);

    /*
     * Make sure that the link's remote eid field is properly set.
     */
    LinkRef link = contact_->link();
    if (link->remote_eid() == BD_NULL_EID()) {
        link->set_remote_eid(sptr_peer_eid);
    } else if (link->remote_eid() != sptr_peer_eid) {
        //dzdebug log_warn("handle_contact_initiation: remote eid mismatch: "
        log_always("handle_contact_initiation: remote eid mismatch: "
                 "link remote eid was set to %s but peer eid is %s",
                 link->remote_eid()->c_str(), sptr_peer_eid->c_str());
    }


    // change the logpath to output the peer_eid
    if (base_logpath_.length() > 0) {
        logpathf(base_logpath_.c_str(), link->name(), sptr_peer_eid->c_str());
        cmdqueue_.logpathf("%s/cmdq", logpath_);
    } else {
        logpathf("%s/conn3/%s", cl_->logpath(), sptr_peer_eid->c_str());
        cmdqueue_.logpathf("%s/cmdq", logpath_);
    }

    /*
     * Finally, we note that the contact is now up.
     */
    contact_up();


    // If external router - delay 5 seconds befoe reading bundles to give time for contact up to be processed?
    int secs_to_delay = bdaemon_->router()->delay_after_contact_up();
    if (0 != secs_to_delay) {
        sleep(secs_to_delay);
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_bundles_queued()
{
    // since the main run loop checks the link queue to see if there
    // are bundles that should be put in flight, we simply log a debug
    // message here. the point of the message is to kick the thread
    // out of poll() which forces the main loop to check the queue
    //log_debug("handle_bundles_queued: %zu bundles on link queue",
    //          contact_->link()->bundles_queued());
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_pending_data()
{
    // if the outgoing data buffer is full, we can't do anything until
    // we poll()
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    // if we're in the middle of sending a segment, we need to continue
    // sending it. only if we completely send the segment do we fall
    // through to send acks, otherwise we return to try to finish it
    // again later.
    if (send_segment_todo_ != 0) {
        ASSERT(current_inflight_ != nullptr);        
        send_data_todo(current_inflight_);
    }
    
    // see if we're broken or write blocked
    if (contact_broken_ || (send_segment_todo_ != 0)) {
        if (params_->test_write_delay_ != 0) {
            return true;
        }
        
        return false;
    }
    
    // now check if there are acks we need to send -- even if it
    // returns true (i.e. we sent an ack), we continue on and try to
    // send some real payload data, otherwise we could get starved by
    // arriving data and never send anything out.
    bool sent_ack = send_pending_acks();
    
    // if the connection failed during ack transmission, stop
    if (contact_broken_)
    {
        return sent_ack;
    }

    // check if we need to start a new bundle. if we do, then
    // start_next_bundle handles the correct return code
    bool sent_data;
    if (current_inflight_ == nullptr) {
        sent_data = start_next_bundle();
    } else {
        // otherwise send the next segment of the current bundle
        sent_data = send_next_segment(current_inflight_);
    }

    return sent_ack || sent_data;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_pending_acks()
{
    if (contact_broken_ || (admin_msg_list_.size() == 0)) {
        return false; // nothing to do
    }

    bool generated_ack = false;
    std::string* msg = nullptr;;

    while (admin_msg_list_.size() > 0) {
        if (!admin_msg_list_.try_pop(&msg)) {
            break;
        }

        if (msg->length() > sendbuf_.tailbytes()) {
            // not enough space so add back to the queue
            admin_msg_list_.push_front(msg);
            break;
        }

        memcpy(sendbuf_.end(), msg->c_str(), msg->length());
        sendbuf_.fill(msg->length());

        generated_ack = true;

        delete msg;
    }
    
    if (generated_ack) {
        send_data();
        note_data_sent();
    }

    // return true if we've sent something or send buffer filled up with more acks to send
    return generated_ack || (admin_msg_list_.size() > 0);

/*

    if (true) return;


    if (contact_broken_ || ack_list_.empty()) {
        return false; // nothing to do
    }

    bool generated_ack = false;

    AckList::iterator iter = ack_list_.begin();

    while (iter != ack_list_.end()) {
        SPtr_AckObject ackptr = ack_list_.front();

        // make sure we have space in the send buffer
        size_t encoding_len = 1 + SDNV::encoding_len(ackptr->acked_len_);
        if (encoding_len > sendbuf_.tailbytes()) {
            //log_debug("send_pending_acks: "
            //      "no space for ack in buffer (need %zu, have %zu)",
            //      encoding_len, sendbuf_.tailbytes());
            break;
        }

        *sendbuf_.end() = ACK_SEGMENT;
        int len = SDNV::encode(ackptr->acked_len_, (u_char*)sendbuf_.end() + 1,
                               sendbuf_.tailbytes() - 1);
        ASSERT(encoding_len = len + 1);
        sendbuf_.fill(encoding_len);

        generated_ack = true;

        ack_list_.pop_front();
        iter = ack_list_.begin();
    }
    
    if (generated_ack) {
        send_data();
        note_data_sent();
    }

    // return true if we've sent something or send buffer filled up with more acks to send
    return generated_ack || !ack_list_.empty();
*/

}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::queue_acks_for_incoming_bundle(IncomingBundle* incoming)
{
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != nullptr); 
   
    while (!incoming->pending_acks_.empty()) {
        SPtr_AckObject in_ackptr = incoming->pending_acks_.front();

        if(params->segment_ack_enabled_)
        {
            // make sure we have space in the send buffer
            size_t encoding_len = 1 + SDNV::encoding_len(in_ackptr->acked_len_);

            char buf[encoding_len];
            buf[0] = ACK_SEGMENT;
            int len = SDNV::encode(in_ackptr->acked_len_, (u_char*)&buf[1],
                               encoding_len - 1);
            ASSERT(encoding_len = len + 1);

            admin_msg_list_.push_back(new std::string(buf, encoding_len));
        }

        incoming->acked_length_ = in_ackptr->acked_len_;

        incoming->pending_acks_.pop_front();
    }
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::start_next_bundle()
{
    ASSERT(current_inflight_ == nullptr);


    if (! contact_up_) {
        log_debug("start_next_bundle: contact not yet set up");
        return false;
    }
    
    StreamLinkParams* params = stream_lparams();

    if (inflight_.size() >= params->max_inflight_bundles_) {
        return false;
    }


    const LinkRef& link = contact_->link();
    //Check Contact Planner State to see if contact is ready for sending
    if(!link->get_contact_state()){
      return false;
    }

    BundleRef bundle("StreamCL::Connection::start_next_bundle");

    // try to pop the next bundle off the link queue and put it in
    // flight, making sure to hold the link queue lock until it's
    // safely on the link's inflight queue
    bundle = link->queue()->front();
    if (bundle == nullptr) {
        //log_debug("start_next_bundle: nothing to start");
        return false;
    }

    // release the lock before calling send_next_segment since it
    // might take a while


    if (bundle->expired() || bundle->manually_deleting())
    {    
        link->del_from_queue(bundle); 
        return false;
    }    
 
    InFlightBundle* inflight = new InFlightBundle(bundle.object());

    // transfer_Id is used by TCPCL4
    inflight->transfer_id_ = sender_transfer_id_++;

    oasys::ScopeLock scoplok(bundle->lock(), __func__);

    inflight->blocks_ = bundle->xmit_blocks()->find_blocks(link);

    scoplok.unlock();

    if (inflight->blocks_ == nullptr) {
        log_warn("StreamCL:start_next_bundle found bundle without xmit blocks - probably expired: *%p",
                 bundle.object());
        return false;
    }
    inflight->total_length_ = BundleProtocol::total_length(bundle.object(), inflight->blocks_.get());
    inflight_.push_back(inflight);
    current_inflight_ = inflight;

    link->add_to_inflight(bundle);
    link->del_from_queue(bundle);

    // now send the first segment for the bundle
    return send_next_segment(current_inflight_);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_next_segment(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    ASSERT(send_segment_todo_ == 0);

    if (inflight->bundle_refused_) {
        return finish_bundle(inflight);
    }



    StreamLinkParams* params = stream_lparams();

    size_t bytes_sent = inflight->sent_data_.empty() ? 0 :
                        inflight->sent_data_.last() + 1;
    
    if (bytes_sent == inflight->total_length_) {
        //log_debug("send_next_segment: "
        //          "already sent all %zu bytes, finishing bundle",
        //          bytes_sent);
        ASSERT(inflight->send_complete_);
        return finish_bundle(inflight);
    }

    uint8_t flags = 0;
    size_t segment_len;

    if (bytes_sent == 0) {
        flags |= BUNDLE_START;
    }
    
    if (params->segment_length_ >= inflight->total_length_ - bytes_sent) {
        flags |= BUNDLE_END;
        segment_len = inflight->total_length_ - bytes_sent;
    } else {
        segment_len = params->segment_length_;
    }
    
    size_t sdnv_len = SDNV::encoding_len(segment_len);
    
    if (sendbuf_.tailbytes() < 1 + sdnv_len) {
        //log_debug("send_next_segment: "
        //          "not enough space for segment header [need %zu, have %zu]",
        //          1 + sdnv_len, sendbuf_.tailbytes());
        return false;
    }
    
    //log_debug("send_next_segment: "
    //          "starting %zu byte segment [block byte range %zu..%zu]",
    //          segment_len, bytes_sent, bytes_sent + segment_len);

    u_char* bp = (u_char*)sendbuf_.end();
    *bp++ = DATA_SEGMENT | flags;
    int cc = SDNV::encode(segment_len, bp, sendbuf_.tailbytes() - 1);
    ASSERT(cc == (int)sdnv_len);
    bp += sdnv_len;
    
    sendbuf_.fill(1 + sdnv_len);
    send_segment_todo_ = segment_len;

    // send_data_todo actually does the deed
    return send_data_todo(inflight);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_data_todo(InFlightBundle* inflight)
{
    ASSERT(send_segment_todo_ != 0);

    // loop since it may take multiple calls to send on the socket
    // before we can actually drain the todo amount
    while (send_segment_todo_ != 0 && sendbuf_.tailbytes() != 0) {
        size_t bytes_sent = inflight->sent_data_.empty() ? 0 :
                            inflight->sent_data_.last() + 1;
        size_t send_len   = std::min(send_segment_todo_, sendbuf_.tailbytes());
    
        Bundle* bundle       = inflight->bundle_.object();
        SPtr_BlockInfoVec sptr_blocks = inflight->blocks_;

        size_t ret =
            BundleProtocol::produce(bundle, sptr_blocks.get(), (u_char*)sendbuf_.end(),
                                    bytes_sent, send_len,
                                    &inflight->send_complete_);
        ASSERT(ret == send_len);
        sendbuf_.fill(send_len);
        inflight->sent_data_.set(bytes_sent, send_len);
    
        //log_debug("send_data_todo: "
        //          "sent %zu/%zu of current segment from block offset %zu "
        //          "(%zu todo), updated sent_data *%p",
        //          send_len, send_segment_todo_, bytes_sent,
        //          send_segment_todo_ - send_len, &inflight->sent_data_);
        
        send_segment_todo_ -= send_len;

        note_data_sent();
        send_data();

        // XXX/demmer once send_complete_ is true, we could post an
        // event to free up space in the queue for more bundles to be
        // sent down. note that it's possible the bundle isn't really
        // out on the wire yet, but we don't have any way of knowing
        // when it gets out of the sendbuf_ and into the kernel (nor
        // for that matter actually onto the wire), so this is the
        // best we can do for now.
        
        if (contact_broken_)
            return true;

        // if test_write_delay is set, then we only send one segment
        // at a time before bouncing back to poll
        if (params_->test_write_delay_ != 0) {
            log_debug("send_data_todo done, returning more to send "
                      "(send_segment_todo_==%zu) since test_write_delay is non-zero",
                      send_segment_todo_);
            return true;
        }
    }

    return (send_segment_todo_ == 0);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::finish_bundle(InFlightBundle* inflight)
{
    ASSERT(inflight->send_complete_ || inflight->bundle_refused_);
    
    ASSERT(current_inflight_ == inflight);
    current_inflight_ = nullptr;
   
    // It is now safe to release the blocks
    inflight->blocks_ = nullptr;
    BundleProtocol::delete_blocks(inflight->bundle_.object(), contact_->link());
 
    check_completed(inflight);

    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::check_completed(InFlightBundle* inflight)
{
    // we can pop the inflight bundle off of the queue and clean it up
    // only when both finish_bundle is called (so current_inflight_ no
    // longer points to the inflight bundle), and after the final ack
    // for the bundle has been received (determined by looking at
    // inflight->ack_data_)

    if (current_inflight_ == inflight) {
        //log_debug("check_completed: bundle %" PRIbid " still waiting for finish_bundle",
        //          inflight->bundle_->bundleid());
        return;
    }

    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != nullptr);

    if (params->segment_ack_enabled_ && !inflight->bundle_refused_) { 
        uint64_t acked_len = inflight->ack_data_.num_contiguous();

        if (acked_len < inflight->total_length_) {
            //log_debug("check_completed: bundle %" PRIbid " only acked %u/%u",
            //      inflight->bundle_->bundleid(),
            //      acked_len, inflight->total_length_);
            return;
        }
    }

    if (!inflight->transmit_event_posted_)
    {
        inflight->transmit_event_posted_ = true;

        if (!inflight->bundle_refused_) {
            BundleTransmittedEvent* event_to_post;
            event_to_post = new BundleTransmittedEvent(inflight->bundle_.object(),
                                                       contact_,contact_->link(),
                                                       inflight->sent_data_.num_contiguous(),
                                                       inflight->sent_data_.num_contiguous(),
                                                       true, true);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);
        } else {
            BundleTransmittedEvent* event_to_post;
            event_to_post = new BundleTransmittedEvent(inflight->bundle_.object(),
                                                       contact_,contact_->link(),
                                                       0, 0, false, true);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);
        }
    }

    //log_debug("check_completed: bundle %" PRIbid " transmission complete",
    //          inflight->bundle_->bundleid());
    ASSERT(inflight == inflight_.front());
    inflight_.pop_front();

    delete inflight;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::send_keepalive()
{
    // there's no point in putting another byte in the buffer if
    // there's already data waiting to go out, since the arrival of
    // that data on the other end will do the same job as the
    // keepalive byte
    if (sendbuf_.fullbytes() != 0) {
        //log_debug("send_keepalive: "
        //          "send buffer has %zu bytes queued, suppressing keepalive",
        //          sendbuf_.fullbytes());
        return;
    }
    ASSERT(sendbuf_.tailbytes() > 0);

    // similarly, we must not send a keepalive if send_segment_todo_ is
    // nonzero, because that would likely insert the keepalive in the middle
    // of a bundle currently being sent -- verified in check_keepalive
    ASSERT(send_segment_todo_ == 0);

    *(sendbuf_.end()) = KEEPALIVE;
    sendbuf_.fill(1);

    ::gettimeofday(&keepalive_sent_, 0);

    // don't note_data_sent() here since keepalive messages shouldn't
    // be counted for keeping an idle link open
    send_data();
}
//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_cancel_bundle(Bundle* bundle)
{
    // if the bundle is already actually in flight (i.e. we've already
    // sent all or part of it), we can't currently cancel it. however,
    // in the case where it's not already in flight, we can cancel it
    // and accordingly signal with an event
    InFlightList::iterator iter;
    for (iter = inflight_.begin(); iter != inflight_.end(); ++iter) {
        InFlightBundle* inflight = *iter;
        if (inflight->bundle_ == bundle)
        {
            if (inflight->sent_data_.empty()) {
                // this bundle might be current_inflight_ but with no
                // data sent yet; check for this case so we do not have
                // a dangling pointer
                if (inflight == current_inflight_) {
                    // we may have sent a segment length without any bundle
                    // data; if so we must send the segment so we can't
                    // cancel the send now
                    if (send_segment_todo_ != 0) {
                        //log_debug("handle_cancel_bundle: bundle %" PRIbid " "
                        //          "already in flight, can't cancel send",
                        //          bundle->bundleid());
                        return;
                    }
                    current_inflight_ = nullptr;
                }
                
                //log_debug("handle_cancel_bundle: "
                //          "bundle %" PRIbid " not yet in flight, cancelling send",
                //          bundle->bundleid());
                inflight_.erase(iter);
                delete inflight;

                BundleSendCancelledEvent* event_to_post;
                event_to_post = new BundleSendCancelledEvent(bundle, contact_->link());
                SPtr_BundleEvent sptr_event_to_post(event_to_post);
                BundleDaemon::post(sptr_event_to_post);
                return;
            } else {
                //log_debug("handle_cancel_bundle: "
                //          "bundle %" PRIbid " already in flight, can't cancel send",
                //          bundle->bundleid());
                return;
            }
        }
    }

    log_warn("handle_cancel_bundle: "
             "can't find bundle %" PRIbid " in the in flight list", bundle->bundleid());
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_poll_timeout()
{
    // Allow the BundleDaemon to call for a close of the connection if
    // a shutdown is in progress. This must be done to avoid a
    // deadlock caused by simultaneous poll_timeout and close_contact
    // activities.
    //
    // Before we return, sleep a bit to avoid continuous
    // handle_poll_timeout calls
    if (BundleDaemon::shutting_down())
    {
        usleep(100000);
        return;
    }
    
    // avoid performing connection timeout operations on
    // connections which have not been initiated yet
    if (!contact_initiated_)
    {
        return;
    }


    struct timeval now;
    uint elapsed, elapsed2;

    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != nullptr);
    
    ::gettimeofday(&now, 0);
    
    // check that it hasn't been too long since we got some data from
    // the other side
    elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
    if (params->keepalive_interval_ > 0 && elapsed > params->data_timeout_) { 
        if (!active_connector_ || active_connector_expects_keepalive_) {
            if (params->break_contact_on_keepalive_fault_) {
                log_info("handle_poll_timeout: no data heard for %d msecs "
                         "-- closing contact",
                         elapsed);
                break_contact(ContactEvent::BROKEN);
            } else {
                log_info("handle_poll_timeout: no data heard for %d msecs "
                          "-- but confgured to not close contact",
                          elapsed);
                // start the clock ticking again
                note_data_rcvd();
            }
            return;
        }
    }
    
    //make sure the contact still exists
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), __func__);
    if (contact_ == nullptr)
    {
        return;
    }

    // check if the connection has been idle for too long
    // (on demand links only)
    if (contact_->link()->type() == Link::ONDEMAND) {
        uint idle_close_time = contact_->link()->params().idle_close_time_;

        elapsed  = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, data_sent_);
        
        if (idle_close_time != 0 &&
            (elapsed > idle_close_time * 1000) &&
            (elapsed2 > idle_close_time * 1000))
        {
            log_err("closing idle connection "
                     "(no data received for %d msecs or sent for %d msecs)",
                     elapsed, elapsed2);
            break_contact(ContactEvent::IDLE);
            return;
        } else {
            //log_debug("connection not idle: recvd %d / sent %d <= timeout %d",
            //          elapsed, elapsed2, idle_close_time * 1000);
        }
    }

    // check if it's time for us to send a keepalive (i.e. that we
    // haven't sent some data or another keepalive in at least the
    // configured keepalive_interval)
    check_keepalive();
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::check_keepalive()
{
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != nullptr);

    if (params->keepalive_interval_ != 0) {
        struct timeval now;
        uint elapsed, elapsed2;

        ::gettimeofday(&now, 0);
    
        elapsed  = TIMEVAL_DIFF_MSEC(now, data_sent_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, keepalive_sent_);

        // XXX/demmer this is bogus -- we should really adjust
        // poll_timeout to take into account the next time we should
        // send a keepalive
        // 
        // give a 500ms fudge to the keepalive interval to make sure
        // we send it when we should
        if (std::min(elapsed, elapsed2) > (uint)((params->keepalive_interval_ * 1000) - 500))
        {
            // it's possible that the link is blocked while in the
            // middle of a segment, triggering a poll timeout, so make
            // sure not to send a keepalive in this case
            if (send_segment_todo_ != 0) {
                log_debug("not issuing keepalive in the middle of a segment");
                return;
            }
    
            send_keepalive();
        }
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::process_data()
{
    //dzdebug
    //log_debug("%s - entered - recvbuf: size= %zu fullbytes= %zu tailbytes= %zu  recv_segment_todo= %zu",
    //           __func__, recvbuf_.size(), recvbuf_.fullbytes(), recvbuf_.tailbytes(), recv_segment_todo_);

    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    // all data (keepalives included) should be noted since the last
    // reception time is used to determine when to generate new
    // keepalives
    note_data_rcvd();

    // the first thing we need to do is handle the contact initiation
    // sequence, i.e. the contact header and the announce bundle. we
    // know we need to do this if we haven't yet called contact_up()
    if (! contact_up_) {
        handle_contact_initiation();
        return;
    }

    // if a data segment is bigger than the receive buffer. when
    // processing a data segment, we mark the unread amount in the
    // recv_segment_todo__ field, so if that's not zero, we need to
    // drain it, then fall through to handle the rest of the buffer
    if (recv_segment_todo_ != 0) {
        bool ok = handle_data_todo();
        
        if (!ok) {
            if (recvbuf_.fullbytes() == recvbuf_.size()) {
                log_always("process_data (after error from handle_data_todo): "
                         "%zu byte recv buffer full but too small... "
                         "doubling buffer size",
                         recvbuf_.size());
                
                recvbuf_.reserve(recvbuf_.size() * 2);

            } else if (recvbuf_.tailbytes() == 0) {
                // force it to move the full bytes up to the front
                recvbuf_.reserve(recvbuf_.size() - recvbuf_.fullbytes());
                ASSERT(recvbuf_.tailbytes() != 0);
            }
            return;
        }
    }
    
    // now, drain cl messages from the receive buffer. we peek at the
    // first byte and dispatch to the correct handler routine
    // depending on the type of the CL message. we don't consume the
    // byte yet since there's a possibility that we need to read more
    // from the remote side to handle the whole message
    while (recvbuf_.fullbytes() != 0) {
        if (contact_broken_) return;
        
        uint8_t type  = *recvbuf_.start() & 0xf0;
        uint8_t flags = *recvbuf_.start() & 0x0f;


        bool ok;
        switch (type) {
        case DATA_SEGMENT:
            if (delay_reads_to_free_some_storage_) {
                note_data_rcvd(); // prevent keep_alive timerout
                return;
            }

            ok = handle_data_segment(flags);
            break;
        case ACK_SEGMENT:
            ok = handle_ack_segment(flags);
            break;
        case REFUSE_BUNDLE:
            ok = handle_refuse_bundle(flags);
            break;
        case KEEPALIVE:
            ok = handle_keepalive(flags);
            break;
        case SHUTDOWN:
            ok = handle_shutdown(flags);
            break;
        default:
            log_err("invalid CL message type code 0x%x (flags 0x%x)",
                    type >> 4, flags);
            break_contact(ContactEvent::CL_ERROR);
            return;
        }


        // if there's not enough data in the buffer to handle the
        // message, make sure there's space to receive more
        if (! ok) {
            if (recvbuf_.fullbytes() == recvbuf_.size()) {
                log_always("process_data: "
                         "%zu byte recv buffer full but too small for msg %u... "
                         "doubling buffer size",
                         recvbuf_.size(), type);
                
                recvbuf_.reserve(recvbuf_.size() * 2);

            } else if (recvbuf_.tailbytes() == 0) {
                // force it to move the full bytes up to the front
                recvbuf_.reserve(recvbuf_.size() - recvbuf_.fullbytes());
                ASSERT(recvbuf_.tailbytes() != 0);
            }
            
            return;
        }
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::note_data_rcvd()
{
    //log_debug("noting data_rcvd");
    ::gettimeofday(&data_rcvd_, 0);
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::note_data_sent()
{
    //log_debug("noting data_sent");
    ::gettimeofday(&data_sent_, 0);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_segment(uint8_t flags)
{
    IncomingBundle* incoming = nullptr;
    if (flags & BUNDLE_START)
    {
        // make sure we're done with the last bundle if we got a new
        // BUNDLE_START flag... note that we need to be careful in
        // case there's not enough data to decode the length of the
        // segment, since we'll be called again
        bool create_new_incoming = true;
        if (!incoming_.empty()) {
            incoming = incoming_.back();

            if ((incoming->bytes_received_ == 0) && incoming->pending_acks_.empty())
            {
                log_debug("found empty incoming bundle for BUNDLE_START");
                create_new_incoming = false;
                incoming->bundle_complete_ = false;
                incoming->bundle_accepted_ = false;
            }
            else if (incoming->total_length_ == 0)
            {
                log_err("protocol error: "
                        "got BUNDLE_START before bundle completed");
                break_contact(ContactEvent::CL_ERROR);
                return false;
            }
        }

        if (create_new_incoming) {
            //log_debug("got BUNDLE_START segment, creating new IncomingBundle");
            IncomingBundle* incoming = new IncomingBundle(new Bundle(BundleProtocol::BP_VERSION_UNKNOWN));
            incoming_.push_back(incoming);
        }
    }
    else if (incoming_.empty())
    {
        log_err("protocol error: "
                "first data segment doesn't have BUNDLE_START flag set");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    // Note that there may be more than one incoming bundle on the
    // IncomingList, but it's the one at the back that we're reading
    // in data for. Others are waiting for acks to be sent.
    incoming = incoming_.back();
    u_char* bp = (u_char*)recvbuf_.start();

    // Decode the segment length and then call handle_data_todo
    uint64_t segment_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1,
                                &segment_len);

    if (sdnv_len < 0) {
        //log_debug("handle_data_segment: "
        //          "too few bytes in buffer for sdnv (%zu)",
        //          recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);
    
    if (segment_len == 0) {
        log_err("protocol error -- zero length segment");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    size_t segment_offset = incoming->bytes_received_;

    SPtr_AckObject ackptr = std::make_shared<AckObject>();
    ackptr->acked_len_ = segment_offset + segment_len;
    incoming->pending_acks_.push_back(ackptr);


    // if this is the last segment for the bundle, we calculate and
    // store the total length in the IncomingBundle structure so
    // send_pending_acks knows when we're done.
    if (flags & BUNDLE_END)
    {
        incoming->total_length_ = segment_offset + segment_len;
    }
    
    recv_segment_todo_ = segment_len;
    return handle_data_todo();
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_todo()
{
    // We shouldn't get ourselves here unless there's something
    // incoming and there's something left to read
    ASSERT(!incoming_.empty());
    ASSERT(recv_segment_todo_ != 0);


    // Note that there may be more than one incoming bundle on the
    // IncomingList. There's always only one (at the back) that we're
    // reading in data for, the rest are waiting for acks to go out
    IncomingBundle* incoming = incoming_.back();
    size_t rcvd_offset    = incoming->bytes_received_;
    ASSERT(rcvd_offset == incoming->bytes_received_);

    size_t rcvd_len       = recvbuf_.fullbytes();
    size_t chunk_len      = std::min(rcvd_len, recv_segment_todo_);

    if (rcvd_len == 0) {
        return false; // nothing to do
    }
    
    if (recv_segment_to_refuse_ > 0) {
        // skipping data for this segment because bundle was refused
        ASSERT(chunk_len <= recv_segment_to_refuse_);

        recv_segment_todo_ -= chunk_len;
        recv_segment_to_refuse_ -= chunk_len;
        recvbuf_.consume(chunk_len);

        return (recv_segment_todo_ == 0);
    }

    bool last;
    ssize_t cc = BundleProtocol::consume(incoming->bundle_.object(),
                                         (u_char*)recvbuf_.start(),
                                         chunk_len, &last);

    if (cc < 0) {
        log_err("protocol error parsing bundle data segment");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    if (cc == 0) {
        return false;
    }

    //ASSERT(cc == (int)chunk_len); - BPv7 cannot always process all data in a chunk - not an error

    recv_segment_todo_ -= cc;
    recvbuf_.consume(cc);

    incoming->bytes_received_ += cc;

    
    if (recv_segment_todo_ == 0) {
        check_completed(incoming);
        return true; // completed segment
    }

    return false;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::retry_to_accept_incoming_bundle(IncomingBundle* incoming)
{
    (void) incoming;

    if (!incoming_.empty()) {
        IncomingBundle* incoming_bundle = incoming_.front();
        check_completed(incoming_bundle);
    } else {
        log_always("retry_to_accept_incoming_bundle  called but incoming_ list is empty");
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::check_completed(IncomingBundle* incoming)
{
    uint64_t rcvd_len = incoming->bytes_received_;

    // if we don't know the total length yet, we haven't seen the
    // BUNDLE_END message
    if (incoming->total_length_ == 0) {
        return;
    }
    
    uint64_t formatted_len =
        BundleProtocol::total_length(incoming->bundle_.object(), incoming->bundle_->recv_blocks().get());
    
    //if (delay_reads_to_free_some_storage_) {
    //    log_always("check_completed while reads delayed [*%p]:  total_len: %zu formatted_len: %zu rcvd_len: %zu ",
    //                incoming->bundle_.object(), incoming->total_length_, formatted_len, rcvd_len);
    //}

    if (rcvd_len < incoming->total_length_) {
        return;
    }
    
    if (rcvd_len > incoming->total_length_) {
        log_err("protocol error: received too much data -- "
                "got %" PRIu64 ", total length %" PRIu64,
                rcvd_len, incoming->total_length_);

        // we pretend that we got nothing so the cleanup code in
        // ConnectionCL::close_contact doesn't try to post a received
        // event for the bundle
        incoming->bytes_received_ = 0;
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    // validate that the total length as conveyed by the convergence
    // layer matches the length according to the bundle protocol
    if (incoming->total_length_ != formatted_len) {
        log_err("protocol error: CL total length %" PRIu64
                " doesn't match bundle protocol total %" PRIu64,
                incoming->total_length_, formatted_len);

        // we pretend that we got nothing so the cleanup code in
        // ConnectionCL::close_contact doesn't try to post a received
        // event for the bundle
        incoming->bytes_received_ = 0;
        break_contact(ContactEvent::CL_ERROR);
        return;
        
    }

    // bundle is complete. now we just need to wait for presumed acceptance to ACK it
    incoming->bundle_complete_ = true;


    // verify it is okay to accept this bundle
    bool space_reserved = false;  // an output not an input
    bool accept_bundle = bdaemon_->query_accept_bundle_based_on_quotas(incoming->bundle_.object(), 
                                                                       space_reserved,
                                                                       incoming->payload_bytes_reserved_);

    if (should_stop()) {
        return;
    }

    if (!accept_bundle) {
        log_warn("TCP CL Unable to reserve [*%p] payload space for 5 seconds - pausing reading ", 
                 incoming->bundle_.object());
        delay_reads_to_free_some_storage_ = true; // allow some sends to free up some memory
        delay_reads_timer_.get_time();
        incoming_bundle_to_retry_to_accept_ = incoming;
        return;
    }

    
    delay_reads_to_free_some_storage_ = false;
    incoming_bundle_to_retry_to_accept_ = nullptr;

    incoming->bundle_accepted_ = true;  // as far as payload storage is concerned

    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(incoming->bundle_.object(),
                                            EVENTSRC_PEER,
                                            incoming->total_length_,
                                            contact_->link()->remote_eid(),
                                            contact_->link().object());
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);

    queue_acks_for_incoming_bundle(incoming);

    incoming_.pop_front();

    delete incoming;


}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_ack_segment(uint8_t flags)
{
    (void)flags;
    u_char* bp = (u_char*)recvbuf_.start();
    uint32_t acked_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1, &acked_len);
    
    if (sdnv_len < 0) {
        //log_debug("handle_ack_segment: too few bytes for sdnv (%zu)",
        //          recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);

    if (inflight_.empty()) {
        log_err("protocol error: got ack segment with no inflight bundle");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    InFlightBundle* inflight = inflight_.front();

    size_t ack_begin;
    DataBitmap::iterator i = inflight->ack_data_.begin();
    if (i == inflight->ack_data_.end()) {
        ack_begin = 0;
    } else {
        i.skip_contiguous();
        ack_begin = *i + 1;
    }

    if (acked_len < ack_begin) {
        log_err("protocol error: got ack for length %u but already acked up to %zu",
                acked_len, ack_begin);
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }
    
    inflight->ack_data_.set(0, acked_len);

    // now check if this was the last ack for the bundle, in which
    // case we can pop it off the list and post a
    // BundleTransmittedEvent
    if (acked_len == inflight->total_length_) {
        //log_debug("handle_ack_segment: got final ack for %zu byte range -- "
        //          "acked_len %u, ack_data *%p",
        //          (size_t)acked_len - ack_begin,
        //          acked_len, &inflight->ack_data_);

        inflight->transmit_event_posted_ = true;
        
        BundleTransmittedEvent* event_to_post;
        event_to_post = new BundleTransmittedEvent(inflight->bundle_.object(),
                                                   contact_,
                                                   contact_->link(),
                                                   inflight->sent_data_.num_contiguous(),
                                                   inflight->ack_data_.num_contiguous(),
                                                   true, true);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

        // might delete inflight
        check_completed(inflight);
    }

    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_refuse_bundle(uint8_t flags)
{
    (void)flags;
    //log_debug("got refuse_bundle message");
    log_err("REFUSE_BUNDLE not implemented");
    break_contact(ContactEvent::CL_ERROR);
    return true;
}
//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_keepalive(uint8_t flags)
{
    (void)flags;
    recvbuf_.consume(1);
    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::break_contact(ContactEvent::reason_t reason)
{
    // it's possible that we can end up calling break_contact multiple
    // times, if for example we have an error when sending out the
    // shutdown message below. we simply ignore the multiple calls
    if (breaking_contact_) {
        return;
    }
    breaking_contact_ = true;
    
    // we can only send a shutdown byte if we're not in the middle
    // of sending a segment, otherwise the shutdown byte could be
    // interpreted as a part of the payload
    bool send_shutdown = false;
    shutdown_reason_t shutdown_reason = SHUTDOWN_NO_REASON;

    switch (reason) {
    case ContactEvent::USER:
        // if the user is closing this link, we say that we're busy
        send_shutdown = true;
        shutdown_reason = SHUTDOWN_BUSY;
        break;
        
    case ContactEvent::IDLE:
        // if we're idle, indicate as such
        send_shutdown = true;
        shutdown_reason = SHUTDOWN_IDLE_TIMEOUT;
        break;
        
    case ContactEvent::SHUTDOWN:
        // if the other side shuts down first, we send the
        // corresponding SHUTDOWN byte for a clean handshake, but
        // don't give any more reason
        send_shutdown = true;
        break;
        
    case ContactEvent::BROKEN:
    case ContactEvent::CL_ERROR:
        // no shutdown 
        send_shutdown = false;
        break;

    case ContactEvent::CL_VERSION:
        // version mismatch
        send_shutdown = true;
        shutdown_reason = SHUTDOWN_VERSION_MISMATCH;
        break;
        
    case ContactEvent::INVALID:
    case ContactEvent::NO_INFO:
    case ContactEvent::RECONNECT:
    case ContactEvent::TIMEOUT:
    case ContactEvent::DISCOVERY:
        NOTREACHED;
        break;
    }

    // of course, we can't send anything if we were interrupted in the
    // middle of sending a block.
    //
    // XXX/demmer if we receive a SHUTDOWN byte from the other side,
    // we don't have any way of continuing to transmit our own blocks
    // and then shut down afterwards
    if (send_shutdown && 
        sendbuf_.fullbytes() == 0 &&
        send_segment_todo_ == 0)
    {
        log_debug("break_contact: sending shutdown");
        char typecode = SHUTDOWN;
        if (shutdown_reason != SHUTDOWN_NO_REASON) {
            typecode |= SHUTDOWN_HAS_REASON;
        }

        // XXX/demmer should we send a reconnect delay??

        *sendbuf_.end() = typecode;
        sendbuf_.fill(1);

        if (shutdown_reason != SHUTDOWN_NO_REASON) {
            *sendbuf_.end() = shutdown_reason;
            sendbuf_.fill(1);
        }

        send_data();
    }
        
    CLConnection::break_contact(reason);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_shutdown(uint8_t flags)
{
    log_debug("got SHUTDOWN byte");
    size_t shutdown_len = 1;

    if (flags & SHUTDOWN_HAS_REASON)
    {
        shutdown_len += 1;
    }

    if (flags & SHUTDOWN_HAS_DELAY)
    {
        shutdown_len += 2;
    }

    if (recvbuf_.fullbytes() < shutdown_len)
    {
        // rare case where there's not enough data in the buffer
        // to handle the shutdown message data
        log_debug("got %zu/%zu bytes for shutdown data... waiting for more",
                  recvbuf_.fullbytes(), shutdown_len);
        return false; 
    }

    // now handle the message, first skipping the typecode byte
    recvbuf_.consume(1);

    shutdown_reason_t reason = SHUTDOWN_NO_REASON;
    if (flags & SHUTDOWN_HAS_REASON)
    {
        switch ((unsigned char) *recvbuf_.start()) {
        case SHUTDOWN_NO_REASON:
            reason = SHUTDOWN_NO_REASON;
            break;
        case SHUTDOWN_IDLE_TIMEOUT:
            reason = SHUTDOWN_IDLE_TIMEOUT;
            break;
        case SHUTDOWN_VERSION_MISMATCH:
            reason = SHUTDOWN_VERSION_MISMATCH;
            break;
        case SHUTDOWN_BUSY:
            reason = SHUTDOWN_BUSY;
            break;
        default:
            log_err("invalid shutdown reason code 0x%x", *recvbuf_.start());
        }

        recvbuf_.consume(1);
    }

    uint16_t delay = 0;
    if (flags & SHUTDOWN_HAS_DELAY)
    {
        memcpy(&delay, recvbuf_.start(), 2);
        delay = ntohs(delay);
        recvbuf_.consume(2);
    }

    log_always("got SHUTDOWN (%s) [reconnect delay %u]",
             shutdown_reason_to_str(reason), delay);

    break_contact(ContactEvent::SHUTDOWN);
    
    return false;
}

} // namespace dtn
