/*
 *    Copyright 2009-2010 Darren Long, darren.long@mac.com
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

#include <oasys/util/OptParser.h>
#include "SeqpacketConvergenceLayer.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "bundling/TempBundle.h"
#include "contacts/ContactManager.h"

namespace dtn {

//----------------------------------------------------------------------
SeqpacketConvergenceLayer::SeqpacketLinkParams::SeqpacketLinkParams(bool init_defaults)
    : LinkParams(init_defaults),
      segment_ack_enabled_(true),
      negative_ack_enabled_(true),
      keepalive_interval_(10),
      segment_length_(4096),
      ack_window_(8)
{
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::SeqpacketLinkParams::serialize(oasys::SerializeAction *a)
{
	log_debug_p("StreamLinkParams", "StreamLinkParams::serialize");
	ConnectionConvergenceLayer::LinkParams::serialize(a);
	a->process("segment_ack_enabled", &segment_ack_enabled_);
	a->process("negative_ack_enabled", &negative_ack_enabled_);
	a->process("keepalive_interval", &keepalive_interval_);
	a->process("segment_length", &segment_length_);
	a->process("ack_window", &ack_window_);
}

//----------------------------------------------------------------------
SeqpacketConvergenceLayer::SeqpacketConvergenceLayer(const char* logpath,
                                               const char* cl_name,
                                               u_int8_t    cl_version)
    : ConnectionConvergenceLayer(logpath, cl_name),
      cl_version_(cl_version)
{
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::parse_link_params(LinkParams* lparams,
                                          int argc, const char** argv,
                                          const char** invalidp)
{
    // all subclasses should create a params structure that derives
    // from SeqpacketLinkParams
    SeqpacketLinkParams* params = dynamic_cast<SeqpacketLinkParams*>(lparams);
    ASSERT(params != NULL);
                               
    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("segment_ack_enabled",
                                &params->segment_ack_enabled_));
    
    p.addopt(new oasys::BoolOpt("negative_ack_enabled",
                                &params->negative_ack_enabled_));
    
    p.addopt(new oasys::UIntOpt("keepalive_interval",
                                &params->keepalive_interval_));
    
    p.addopt(new oasys::UIntOpt("segment_length",
                                &params->segment_length_));
                                
    p.addopt(new oasys::UIntOpt("ack_window",
                                &params->ack_window_)); 
    
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
SeqpacketConvergenceLayer::finish_init_link(const LinkRef& link,
                                         LinkParams* lparams)
{
    SeqpacketLinkParams* params = dynamic_cast<SeqpacketLinkParams*>(lparams);
    ASSERT(params != NULL);

    // make sure to set the reliability bit in the link structure
    if (params->segment_ack_enabled_) {
        link->set_reliable(true);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    ConnectionConvergenceLayer::dump_link(link, buf);
    
    SeqpacketLinkParams* params =
        dynamic_cast<SeqpacketLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("segment_ack_enabled: %u\n", params->segment_ack_enabled_);
    buf->appendf("negative_ack_enabled: %u\n", params->negative_ack_enabled_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("segment_length: %u\n", params->segment_length_);
    buf->appendf("ack_window: %u\n", params->ack_window_);
    buf->appendf("cl_version: %u\n", cl_version_);
}

//----------------------------------------------------------------------
SeqpacketConvergenceLayer::Connection::Connection(const char* classname,
                                               const char* logpath,
                                               SeqpacketConvergenceLayer* cl,
                                               SeqpacketLinkParams* params,
                                               bool active_connector)
    : CLConnection(classname, logpath, cl, params, active_connector),
      current_inflight_(NULL),
      send_segment_todo_(0),
      recv_segment_todo_(0),
      breaking_contact_(false),
      contact_initiated_(false),
      ack_window_todo_(0)
{
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::initiate_contact()
{
    size_t local_eid_len;

    log_debug("initiate_contact called");

    // format the contact header
    ContactHeader contacthdr;
    contacthdr.magic   = htonl(MAGIC);
    contacthdr.version = ((SeqpacketConvergenceLayer*)cl_)->cl_version_;
    
    contacthdr.flags = 0;

    SeqpacketLinkParams* params = seqpacket_lparams();
    
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
    BundleDaemon* bd = BundleDaemon::instance();

    if(!bd->params_.announce_ipn_)
        local_eid_len = bd->local_eid().length();
    else 
        local_eid_len = bd->local_eid_ipn().length();

    size_t sdnv_len = SDNV::encoding_len(local_eid_len);
    
    if (sendbuf_.tailbytes() < sdnv_len + local_eid_len) {
        log_warn("send buffer too short: %zu < needed %zu",
                 sendbuf_.tailbytes(), sdnv_len + local_eid_len);
        sendbuf_.reserve(sizeof(ContactHeader) + sdnv_len + local_eid_len);
    }
    
    sdnv_len = SDNV::encode(local_eid_len,
                            (u_char*)sendbuf_.end(),
                            sendbuf_.tailbytes());
    sendbuf_.fill(sdnv_len);

    if(!bd->params_.announce_ipn_)
        memcpy(sendbuf_.end(), bd->local_eid().data(), local_eid_len);
    else
        memcpy(sendbuf_.end(), bd->local_eid_ipn().data(), local_eid_len);

    sendbuf_.fill(local_eid_len);
    
    sendbuf_sequence_delimiters_.push(sizeof(ContactHeader) + sdnv_len + local_eid_len); 
    log_info("adding pending sequence: %zu to sequence delimiters queue, queue depth: %zu",
                sizeof(ContactHeader) + sdnv_len + local_eid_len, 
                sendbuf_sequence_delimiters_.size());      

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
SeqpacketConvergenceLayer::Connection::handle_contact_initiation()
{
    ASSERT(! contact_up_);

    /*
     * First check for valid magic number.
     */
    u_int32_t magic = 0;
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
        log_warn("remote sent magic number 0x%.8x, expected 0x%.8x "
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
    u_int64_t peer_eid_len;
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
    u_int8_t cl_version = ((SeqpacketConvergenceLayer*)cl_)->cl_version_;
    if (contacthdr.version < cl_version) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", contacthdr.version, cl_version);
        break_contact(ContactEvent::CL_VERSION);
        return;
    }

    /*
     * Now do parameter negotiation.
     */
    SeqpacketLinkParams* params = seqpacket_lparams();
    
    // DML - tweaked to use std::max instead of std::min.  We want to be
    // conservative about channel usage.  If we time out, that is too bad.
    // Reason for this hack is that the listener sends out a keepalive in its
    // contact header before it knows that the link in question should have a 
    // non-default keepalive_interval, and uses the default, which is lower
    // than what we want, hence the need to use max. Perhaps a better bet is to
    // send out a contact header from the listener after receiving the 
    // inbound contact header from the initiator.  Or, we could simply increase
    // the default timeout.
    
    params->keepalive_interval_ =
        std::max(params->keepalive_interval_,
                 (u_int)contacthdr.keepalive_interval);

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
    EndpointID peer_eid;
    if (! peer_eid.assign(recvbuf_.start(), peer_eid_len)) {
        log_err("protocol error: invalid endpoint id '%s' (len %llu)",
                peer_eid.c_str(), U64FMT(peer_eid_len));
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    if (!find_contact(peer_eid)) {
        ASSERT(contact_ == NULL);
        log_debug("SeqpacketConvergenceLayer::Connection::"
                  "handle_contact_initiation: failed to find contact");
        break_contact(ContactEvent::CL_ERROR);
        return;
    }
    recvbuf_.consume(peer_eid_len);

    /*
     * Make sure that the link's remote eid field is properly set.
     */
    LinkRef link = contact_->link();
    if (link->remote_eid().str() == EndpointID::NULL_EID().str()) {
        link->set_remote_eid(peer_eid);
    } else if (link->remote_eid() != peer_eid) {
        log_warn("handle_contact_initiation: remote eid mismatch: "
                 "link remote eid was set to %s but peer eid is %s",
                 link->remote_eid().c_str(), peer_eid.c_str());
    }
    
    /*
     * Finally, we note that the contact is now up.
     */
    contact_up();
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::handle_bundles_queued()
{
    // since the main run loop checks the link queue to see if there
    // are bundles that should be put in flight, we simply log a debug
    // message here. the point of the message is to kick the thread
    // out of poll() which forces the main loop to check the queue
    log_debug("handle_bundles_queued: %u bundles on link queue",
              contact_->link()->bundles_queued());
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::send_pending_data()
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
        ASSERT(current_inflight_ != NULL);        
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
    if (current_inflight_ == NULL) {
        sent_data = start_next_bundle();
    } else {
        // otherwise send the next segment of the current bundle
        sent_data = send_next_segment(current_inflight_);
    }

    return sent_ack || sent_data;
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::send_pending_acks()
{
    if (contact_broken_ || incoming_.empty()) {
        return false; // nothing to do
    }
    IncomingBundle* incoming = incoming_.front();
    DataBitmap::iterator iter = incoming->ack_data_.begin();
    bool generated_ack = false;

    size_t encoding_len, totol_ack_len=0;
    
    // DML TODO: the bitmask stuff incoming->ack_data_
    // seems nugatory, so perhaps it can go.  I've definitely broken it, but
    // it doesn't stop the this working anyway.
    // If it does go, then perhaps the while loop can go too, as data segments
    // should always be received in order and without scope gaps.
 
    // when data segment headers are received, the last bit of the
    // segment is marked in ack_data, thus if there's nothing in
    // there, we don't need to send out an ack.
    if (iter == incoming->ack_data_.end() || incoming->rcvd_data_.empty()) {
        goto check_done;
    }
    
    // however, we have to be careful to check the recv_data as well
    // to make sure we've actually gotten the segment, since the bit
    // in ack_data is marked when the segment is begun, not when it's
    // completed
    
    while (1) {
        size_t rcvd_bytes  = incoming->rcvd_data_.num_contiguous();
        size_t ack_len     = rcvd_bytes; // DML hack // *iter + 1; 
        //size_t segment_len = ack_len - incoming->acked_length_;
        //(void)segment_len;

        SeqpacketLinkParams* params = seqpacket_lparams();
        
        // DML - If we have a whole bundle's worth of data we want to ack now
        // otherwise, we want to see if we have a whole window's worth to ack,
        // and if we have, ack that.  If not, we'll deal with it later.
        
        // DML -If we don't have a full bundle or we have haven't reached the
        // ack window yet, bail. The ack_window_todo attribute is decremented 
        // or set to zero in handle_data_segment().
        if(0 != ack_window_todo_) {
            log_debug("send_pending_acks: "
                      "waiting to send ack for window %u segments "
                      "since need %u more segments",
                      params->ack_window_, ack_window_todo_);
            break;
        }
        else {
                        
            // we need to reinitialise the ack_window_todo_
            ack_window_todo_ = params->ack_window_;
        }

        // make sure we have space in the send buffer
        encoding_len = 1 + SDNV::encoding_len(ack_len);
        if (encoding_len > sendbuf_.tailbytes()) {
            log_debug("send_pending_acks: "
                      "no space for ack in buffer (need %zu, have %zu)",
                      encoding_len, sendbuf_.tailbytes());
            break;
        }
        

               
        if (totol_ack_len + encoding_len > params->segment_length_ ) {
            log_debug("send_pending_acks: "
                      "no space for additional ack in segment sized %u, sending %zu bytes)",
                      params->segment_length_ , totol_ack_len);
            break;
        }        
        
        log_debug("send_pending_acks: "
                  "sending ack length %zu "
                  "[range %u..%u] ack_data *%p",
                  ack_len, incoming->acked_length_, *iter,
                  &incoming->ack_data_);
        
        *sendbuf_.end() = ACK_SEGMENT;
        int len = SDNV::encode(ack_len, (u_char*)sendbuf_.end() + 1,
                               sendbuf_.tailbytes() - 1);
        ASSERT(encoding_len = len + 1);
        sendbuf_.fill(encoding_len);
        totol_ack_len += encoding_len;

        generated_ack = true;
        incoming->acked_length_ = ack_len;
        incoming->ack_data_.clear(*iter);
        iter = incoming->ack_data_.begin();
        
        if (iter == incoming->ack_data_.end()) {
            // XXX/demmer this should check if there's another bundle
            // with acks we could send
            break;
        }
        
        log_debug("send_pending_acks: "
                  "found another segment (%u)", *iter);
    }
    
    if (generated_ack) {
        sendbuf_sequence_delimiters_.push(totol_ack_len); // may hold many segments 
        log_info("adding pending sequence: %zu to sequence delimiters queue, queue depth: %zu",
                    totol_ack_len, sendbuf_sequence_delimiters_.size());      

        send_data();
        note_data_sent();
    }

    // now, check if a) we've gotten everything we're supposed to
    // (i.e. total_length_ isn't zero), and b) we're done with all the
    // acks we need to send
 check_done:
    if ((incoming->total_length_ != 0) &&
        (incoming->total_length_ == incoming->acked_length_))
    {
        log_debug("send_pending_acks: acked all %u bytes of bundle %" PRIbid,
                  incoming->total_length_, incoming->bundle_->bundleid());
        
        incoming_.pop_front();
        delete incoming;
    }
    else
    {
        log_debug("send_pending_acks: "
                  "still need to send acks -- acked_range %u",
                  incoming->ack_data_.num_contiguous());
    }

    // return true if we've sent something
    return generated_ack;
}
         
//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::start_next_bundle()
{
    ASSERT(current_inflight_ == NULL);

    if (! contact_up_) {
        log_debug("start_next_bundle: contact not yet set up");
        return false;
    }
    
    const LinkRef& link = contact_->link();
    BundleRef bundle("StreamCL::Connection::start_next_bundle");

    // try to pop the next bundle off the link queue and put it in
    // flight, making sure to hold the link queue lock until it's
    // safely on the link's inflight queue
    oasys::ScopeLock l(link->queue()->lock(),
                       "StreamCL::Connection::start_next_bundle");

    bundle = link->queue()->front();
    if (bundle == NULL) {
        log_debug("start_next_bundle: nothing to start");
        return false;
    }

    InFlightBundle* inflight = new InFlightBundle(bundle.object());
    log_debug("trying to find xmit blocks for bundle id:%" PRIbid " on link %s",
              bundle->bundleid(), link->name());
    inflight->blocks_ = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(inflight->blocks_ != NULL);
    inflight->total_length_ = BundleProtocol::total_length(inflight->blocks_);
    inflight_.push_back(inflight);
    current_inflight_ = inflight;

    link->add_to_inflight(bundle, inflight->total_length_);
    link->del_from_queue(bundle, inflight->total_length_);

    // release the lock before calling send_next_segment since it
    // might take a while
    l.unlock();
    
    // now send the first segment for the bundle
    return send_next_segment(current_inflight_);
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::send_next_segment(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    ASSERT(send_segment_todo_ == 0);

    SeqpacketLinkParams* params = seqpacket_lparams();

    size_t bytes_sent = inflight->sent_data_.empty() ? 0 :
                        inflight->sent_data_.last() + 1;
    
    if (bytes_sent == inflight->total_length_) {
        log_debug("send_next_segment: "
                  "already sent all %zu bytes, finishing bundle",
                  bytes_sent);
        ASSERT(inflight->send_complete_);
        return finish_bundle(inflight);
    }

    u_int8_t flags = 0;
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
        log_debug("send_next_segment: "
                  "not enough space for segment header [need %zu, have %zu]",
                  1 + sdnv_len, sendbuf_.tailbytes());
        return false;
    }
    
    log_debug("send_next_segment: "
              "starting %zu byte segment [block byte range %zu..%zu]",
              segment_len, bytes_sent, bytes_sent + segment_len);

    u_char* bp = (u_char*)sendbuf_.end();
    *bp++ = DATA_SEGMENT | flags;
    int cc = SDNV::encode(segment_len, bp, sendbuf_.tailbytes() - 1);
    ASSERT(cc == (int)sdnv_len);
    bp += sdnv_len;

    sendbuf_.reserve(1 + sdnv_len + segment_len);    
    sendbuf_.fill(1 + sdnv_len);
    sendbuf_sequence_delimiters_.push(1 + sdnv_len + segment_len); // may hold many segments 
    log_info("adding pending sequence: %lu to sequence delimiters queue, queue depth: %zu",
                static_cast<unsigned long>(1 + sdnv_len + segment_len), sendbuf_sequence_delimiters_.size());  

    send_segment_todo_ = segment_len;

    // send_data_todo actually does the deed
    return send_data_todo(inflight);
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::send_data_todo(InFlightBundle* inflight)
{
    ASSERT(send_segment_todo_ != 0);

    // loop since it may take multiple calls to send on the socket
    // before we can actually drain the todo amount
    while (send_segment_todo_ != 0 && sendbuf_.tailbytes() != 0) {
        size_t bytes_sent = inflight->sent_data_.empty() ? 0 :
                            inflight->sent_data_.last() + 1;
        size_t send_len   = std::min(send_segment_todo_, sendbuf_.tailbytes());
    
        Bundle* bundle       = inflight->bundle_.object();
        BlockInfoVec* blocks = inflight->blocks_;

        size_t ret =
            BundleProtocol::produce(bundle, blocks, (u_char*)sendbuf_.end(),
                                    bytes_sent, send_len,
                                    &inflight->send_complete_);
        ASSERT(ret == send_len);
        sendbuf_.fill(send_len);
        inflight->sent_data_.set(bytes_sent, send_len);
    
        log_debug("send_data_todo: "
                  "sent %zu/%zu of current segment from block offset %zu "
                  "(%zu todo), updated sent_data *%p",
                  send_len, send_segment_todo_, bytes_sent,
                  send_segment_todo_ - send_len, &inflight->sent_data_);
        
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
SeqpacketConvergenceLayer::Connection::finish_bundle(InFlightBundle* inflight)
{
    ASSERT(inflight->send_complete_);
    
    ASSERT(current_inflight_ == inflight);
    current_inflight_ = NULL;
    
    check_completed(inflight);

    return true;
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::check_completed(InFlightBundle* inflight)
{
    // we can pop the inflight bundle off of the queue and clean it up
    // only when both finish_bundle is called (so current_inflight_ no
    // longer points to the inflight bundle), and after the final ack
    // for the bundle has been received (determined by looking at
    // inflight->ack_data_)

    if (current_inflight_ == inflight) {
        log_debug("check_completed: bundle %" PRIbid " still waiting for finish_bundle",
                  inflight->bundle_->bundleid());
        return;
    }

    u_int32_t acked_len = inflight->ack_data_.num_contiguous();
    if (acked_len < inflight->total_length_) {
        log_debug("check_completed: bundle %" PRIbid " only acked %u/%u",
                  inflight->bundle_->bundleid(),
                  acked_len, inflight->total_length_);
        return;
    }

    log_debug("check_completed: bundle %" PRIbid " transmission complete",
              inflight->bundle_->bundleid());
    ASSERT(inflight == inflight_.front());
    inflight_.pop_front();
    delete inflight;
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::send_keepalive()
{
    // there's no point in putting another byte in the buffer if
    // there's already data waiting to go out, since the arrival of
    // that data on the other end will do the same job as the
    // keepalive byte
    if (sendbuf_.fullbytes() != 0) {
        log_debug("send_keepalive: "
                  "send buffer has %zu bytes queued, suppressing keepalive",
                  sendbuf_.fullbytes());
        return;
    }
    ASSERT(sendbuf_.tailbytes() > 0);

    // similarly, we must not send a keepalive if send_segment_todo_ is
    // nonzero, because that would likely insert the keepalive in the middle
    // of a bundle currently being sent -- verified in check_keepalive
    ASSERT(send_segment_todo_ == 0);

    ::gettimeofday(&keepalive_sent_, 0);

    *(sendbuf_.end()) = KEEPALIVE;
    sendbuf_.fill(1);

    // don't note_data_sent() here since keepalive messages shouldn't
    // be counted for keeping an idle link open
    sendbuf_sequence_delimiters_.push(1); // may hold many segments 
    log_info("adding pending sequence: %u to sequence delimiters queue, queue depth: %zu",
                1, sendbuf_sequence_delimiters_.size());     
    send_data();
}
//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::handle_cancel_bundle(Bundle* bundle)
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
                        log_debug("handle_cancel_bundle: bundle %" PRIbid " "
                                  "already in flight, can't cancel send",
                                  bundle->bundleid());
                        return;
                    }
                    current_inflight_ = NULL;
                }
                
                log_debug("handle_cancel_bundle: "
                          "bundle %" PRIbid " not yet in flight, cancelling send",
                          bundle->bundleid());
                inflight_.erase(iter);
                delete inflight;
                BundleDaemon::post(
                    new BundleSendCancelledEvent(bundle, contact_->link()));
                return;
            } else {
                log_debug("handle_cancel_bundle: "
                          "bundle %" PRIbid " already in flight, can't cancel send",
                          bundle->bundleid());
                return;
            }
        }
    }

    log_warn("handle_cancel_bundle: "
             "can't find bundle %" PRIbid " in the in flight list", bundle->bundleid());
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::handle_poll_timeout()
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
        sleep(1);
        return;
    }
    
    // avoid performing connection timeout operations on
    // connections which have not been initiated yet
    if (!contact_initiated_)
    {
        return;
    }

    struct timeval now;
    u_int elapsed, elapsed2;

    SeqpacketLinkParams* params = dynamic_cast<SeqpacketLinkParams*>(params_);
    ASSERT(params != NULL);
    
    ::gettimeofday(&now, 0);
    
    // check that it hasn't been too long since we got some data from
    // the other side
    elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
    if (elapsed > params->data_timeout_) {
        log_info("handle_poll_timeout: no data heard for %d msecs "
                 "(keepalive_sent %u.%u, data_rcvd %u.%u, now %u.%u, poll_timeout %d) "
                 "-- closing contact",
                 elapsed,
                 (u_int)keepalive_sent_.tv_sec,
                 (u_int)keepalive_sent_.tv_usec,
                 (u_int)data_rcvd_.tv_sec, (u_int)data_rcvd_.tv_usec,
                 (u_int)now.tv_sec, (u_int)now.tv_usec,
                 poll_timeout_);
            
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    //make sure the contact still exists
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(),"SeqpacketConvergenceLayer::Connection::handle_poll_timeout");
    if (contact_ == NULL)
    {
        return;
    }

    // check if the connection has been idle for too long
    // (on demand links only)
    if (contact_->link()->type() == Link::ONDEMAND) {
        u_int idle_close_time = contact_->link()->params().idle_close_time_;

        elapsed  = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, data_sent_);
        
        if (idle_close_time != 0 &&
            (elapsed > idle_close_time * 1000) &&
            (elapsed2 > idle_close_time * 1000))
        {
            log_info("closing idle connection "
                     "(no data received for %d msecs or sent for %d msecs)",
                     elapsed, elapsed2);
            break_contact(ContactEvent::IDLE);
            return;
        } else {
            log_debug("connection not idle: recvd %d / sent %d <= timeout %d",
                      elapsed, elapsed2, idle_close_time * 1000);
        }
    }

    // check if it's time for us to send a keepalive (i.e. that we
    // haven't sent some data or another keepalive in at least the
    // configured keepalive_interval)
    check_keepalive();
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::check_keepalive()
{
    struct timeval now;
    u_int elapsed, elapsed2;

    SeqpacketLinkParams* params = dynamic_cast<SeqpacketLinkParams*>(params_);
    ASSERT(params != NULL);

    ::gettimeofday(&now, 0);
    
    if (params->keepalive_interval_ != 0) {
        elapsed  = TIMEVAL_DIFF_MSEC(now, data_sent_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, keepalive_sent_);

        // XXX/demmer this is bogus -- we should really adjust
        // poll_timeout to take into account the next time we should
        // send a keepalive
        // 
        // give a 500ms fudge to the keepalive interval to make sure
        // we send it when we should
        if (std::min(elapsed, elapsed2) > ((params->keepalive_interval_ * 1000) - 500))
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
SeqpacketConvergenceLayer::Connection::process_data()
{
    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    log_debug("processing up to %zu bytes from receive buffer",
              recvbuf_.fullbytes());

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
        
        u_int8_t type  = *recvbuf_.start() & 0xf0;
        u_int8_t flags = *recvbuf_.start() & 0x0f;

        log_debug("recvbuf has %zu full bytes, dispatching to handler routine",
                  recvbuf_.fullbytes());
        bool ok;
        switch (type) {
        case DATA_SEGMENT:
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
                log_warn("process_data: "
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
SeqpacketConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting data_rcvd");
    ::gettimeofday(&data_rcvd_, 0);
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::note_data_sent()
{
    log_debug("noting data_sent");
    ::gettimeofday(&data_sent_, 0);
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_data_segment(u_int8_t flags)
{
    SeqpacketLinkParams* params = dynamic_cast<SeqpacketLinkParams*>(params_);
    ASSERT(params != NULL);

    IncomingBundle* incoming = NULL;
    if (flags & BUNDLE_START)
    {
        // make sure we're done with the last bundle if we got a new
        // BUNDLE_START flag... note that we need to be careful in
        // case there's not enough data to decode the length of the
        // segment, since we'll be called again
        bool create_new_incoming = true;
        if (!incoming_.empty()) {
            incoming = incoming_.back();

            if (incoming->rcvd_data_.empty() &&
                incoming->ack_data_.empty())
            {
                log_debug("found empty incoming bundle for BUNDLE_START");
                create_new_incoming = false;
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
            log_debug("got BUNDLE_START segment, creating new IncomingBundle");
            IncomingBundle* incoming = new IncomingBundle(new Bundle());
            incoming_.push_back(incoming);
            ack_window_todo_ = params->ack_window_; // start counting towards the ack window now
        }
        ack_window_todo_ = params->ack_window_; // start counting towards the ack window now

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
    u_int32_t segment_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1,
                                &segment_len);

    if (sdnv_len < 0) {
        log_debug("handle_data_segment: "
                  "too few bytes in buffer for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);
    
    if (segment_len == 0) {
        log_err("protocol error -- zero length segment");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    size_t segment_offset = incoming->rcvd_data_.num_contiguous();
    log_debug("handle_data_segment: "
              "got segment of length %u at offset %zu ",
              segment_len, segment_offset);
    
    incoming->ack_data_.set(segment_offset + segment_len - 1);

    log_debug("handle_data_segment: "
              "updated ack_data (segment_offset %zu) *%p ack_data *%p",
              segment_offset, &incoming->rcvd_data_, &incoming->ack_data_);


    // if this is the last segment for the bundle, we calculate and
    // store the total length in the IncomingBundle structure so
    // send_pending_acks knows when we're done.
    if (flags & BUNDLE_END)
    {
        incoming->total_length_ = incoming->rcvd_data_.num_contiguous() +
                                  segment_len;
        
        log_debug("got BUNDLE_END: total length %u",
                  incoming->total_length_);
                  
        ack_window_todo_ = 0; // trigger an ack now
    }
    else {
            ASSERT(0 != ack_window_todo_);        
            ack_window_todo_--; // count this towards the window
    }
    
    recv_segment_todo_ = segment_len;
    return handle_data_todo();
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_data_todo()
{
    // We shouldn't get ourselves here unless there's something
    // incoming and there's something left to read
    ASSERT(!incoming_.empty());
    ASSERT(recv_segment_todo_ != 0);
    
    // Note that there may be more than one incoming bundle on the
    // IncomingList. There's always only one (at the back) that we're
    // reading in data for, the rest are waiting for acks to go out
    IncomingBundle* incoming = incoming_.back();
    size_t rcvd_offset    = incoming->rcvd_data_.num_contiguous();
    size_t rcvd_len       = recvbuf_.fullbytes();
    size_t chunk_len      = std::min(rcvd_len, recv_segment_todo_);

    if (rcvd_len == 0) {
        return false; // nothing to do
    }
    
    log_debug("handle_data_todo: "
              "reading todo segment %zu/%zu at offset %zu",
              chunk_len, recv_segment_todo_, rcvd_offset);

    bool last;
    int cc = BundleProtocol::consume(incoming->bundle_.object(),
                                     (u_char*)recvbuf_.start(),
                                     chunk_len, &last);
    if (cc < 0) {
        log_err("protocol error parsing bundle data segment");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    ASSERT(cc == (int)chunk_len);

    recv_segment_todo_ -= chunk_len;
    recvbuf_.consume(chunk_len);

    incoming->rcvd_data_.set(rcvd_offset, chunk_len);
    
    log_debug("handle_data_todo: "
              "updated recv_data (rcvd_offset %zu) *%p ack_data *%p",
              rcvd_offset, &incoming->rcvd_data_, &incoming->ack_data_);
    
    if (recv_segment_todo_ == 0) {
        check_completed(incoming);
        return true; // completed segment
    }

    return false;
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::check_completed(IncomingBundle* incoming)
{
    u_int32_t rcvd_len = incoming->rcvd_data_.num_contiguous();

    // if we don't know the total length yet, we haven't seen the
    // BUNDLE_END message
    if (incoming->total_length_ == 0) {
        return;
    }
    
    u_int32_t formatted_len =
        BundleProtocol::total_length(&incoming->bundle_->recv_blocks());
    
    log_debug("check_completed: rcvd %u / %u (formatted length %u)",
              rcvd_len, incoming->total_length_, formatted_len);

    if (rcvd_len < incoming->total_length_) {
        return;
    }
    
    if (rcvd_len > incoming->total_length_) {
        log_err("protocol error: received too much data -- "
                "got %u, total length %u",
                rcvd_len, incoming->total_length_);

        // we pretend that we got nothing so the cleanup code in
        // ConnectionCL::close_contact doesn't try to post a received
        // event for the bundle
protocol_err:
        incoming->rcvd_data_.clear();
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    // validate that the total length as conveyed by the convergence
    // layer matches the length according to the bundle protocol
    if (incoming->total_length_ != formatted_len) {
        log_err("protocol error: CL total length %u "
                "doesn't match bundle protocol total %u",
                incoming->total_length_, formatted_len);
        goto protocol_err;
        
    }
    
    BundleDaemon::post(
        new BundleReceivedEvent(incoming->bundle_.object(),
                                EVENTSRC_PEER,
                                incoming->total_length_,
                                contact_->link()->remote_eid(),
                                contact_->link().object()));
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_ack_segment(u_int8_t flags)
{
    (void)flags;
    u_char* bp = (u_char*)recvbuf_.start();
    u_int32_t acked_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1, &acked_len);
    
    if (sdnv_len < 0) {
        log_debug("handle_ack_segment: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
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
        // DML - Hack - not sure if commenting this out is a good idea, we'll see ...        
        //break_contact(ContactEvent::CL_ERROR);
        return false;
    }
    
    inflight->ack_data_.set(0, acked_len);

    // now check if this was the last ack for the bundle, in which
    // case we can pop it off the list and post a
    // BundleTransmittedEvent
    if (acked_len == inflight->total_length_) {
        log_debug("handle_ack_segment: got final ack for %zu byte range -- "
                  "acked_len %u, ack_data *%p",
                  (size_t)acked_len - ack_begin,
                  acked_len, &inflight->ack_data_);

        inflight->transmit_event_posted_ = true;
        
        BundleDaemon::post(
            new BundleTransmittedEvent(inflight->bundle_.object(),
                                       contact_,
                                       contact_->link(),
                                       inflight->sent_data_.num_contiguous(),
                                       inflight->ack_data_.num_contiguous()));

        // might delete inflight
        check_completed(inflight);
        
    } else {
        log_debug("handle_ack_segment: "
                  "got acked_len %u (%zu byte range) -- ack_data *%p",
                  acked_len, (size_t)acked_len - ack_begin, &inflight->ack_data_);
    }

    return true;
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_refuse_bundle(u_int8_t flags)
{
    (void)flags;
    log_debug("got refuse_bundle message");
    log_err("REFUSE_BUNDLE not implemented");
    break_contact(ContactEvent::CL_ERROR);
    return true;
}
//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_keepalive(u_int8_t flags)
{
    (void)flags;
    log_debug("got keepalive message");
    recvbuf_.consume(1);
    return true;
}

//----------------------------------------------------------------------
void
SeqpacketConvergenceLayer::Connection::break_contact(ContactEvent::reason_t reason)
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
        int seqsize = 1;

        if (shutdown_reason != SHUTDOWN_NO_REASON) {
            *sendbuf_.end() = shutdown_reason;
            sendbuf_.fill(1);
            seqsize=2;
        }
        sendbuf_sequence_delimiters_.push(seqsize); // may hold many segments 

        send_data();
    }
        
    CLConnection::break_contact(reason);
}

//----------------------------------------------------------------------
bool
SeqpacketConvergenceLayer::Connection::handle_shutdown(u_int8_t flags)
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

    u_int16_t delay = 0;
    if (flags & SHUTDOWN_HAS_DELAY)
    {
        memcpy(&delay, recvbuf_.start(), 2);
        delay = ntohs(delay);
        recvbuf_.consume(2);
    }

    log_info("got SHUTDOWN (%s) [reconnect delay %u]",
             shutdown_reason_to_str(reason), delay);

    break_contact(ContactEvent::SHUTDOWN);
    
    return false;
}

} // namespace dtn
