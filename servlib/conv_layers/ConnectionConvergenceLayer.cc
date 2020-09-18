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

#include "ConnectionConvergenceLayer.h"
#include "CLConnection.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
ConnectionConvergenceLayer::LinkParams::LinkParams(bool init_defaults)
    : reactive_frag_enabled_(true),
      sendbuf_len_(32768),
      recvbuf_len_(32768),
      data_timeout_(30000), // msec
      test_read_delay_(0),
      test_write_delay_(0),
      test_recv_delay_(0),
      test_read_limit_(0),
      test_write_limit_(0)
{
    (void)init_defaults;
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::LinkParams::serialize(oasys::SerializeAction *a)
{
	log_debug_p("LinkParams", "ConnectionConvergenceLayer::LinkParams::serialize");
	a->process("reactive_frag_enabled", &reactive_frag_enabled_);
	a->process("sendbuf_len", &sendbuf_len_);
	a->process("recvbuf_len", &recvbuf_len_);
	a->process("data_timeout", &data_timeout_);
	a->process("test_read_delay", &test_read_delay_);
	a->process("test_write_delay", &test_write_delay_);
	a->process("test_recv_delay", &test_recv_delay_);
	a->process("test_read_limit", &test_read_limit_);
	a->process("test_write_limit", &test_write_limit_);
}

//----------------------------------------------------------------------
ConnectionConvergenceLayer::ConnectionConvergenceLayer(const char* classname,
                                                       const char* cl_name)
    : ConvergenceLayer(classname, cl_name)
{
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::parse_link_params(LinkParams* params,
                                              int argc, const char** argv,
                                              const char** invalidp)
{
    oasys::OptParser p;
    
    p.addopt(new oasys::BoolOpt("reactive_frag_enabled",
                                &params->reactive_frag_enabled_));
    p.addopt(new oasys::UIntOpt("sendbuf_len", &params->sendbuf_len_));
    p.addopt(new oasys::UIntOpt("recvbuf_len", &params->recvbuf_len_));
    p.addopt(new oasys::UIntOpt("data_timeout", &params->data_timeout_));
    
    p.addopt(new oasys::UIntOpt("test_read_delay",
                                &params->test_read_delay_));
    p.addopt(new oasys::UIntOpt("test_write_delay",
                                &params->test_write_delay_));
    p.addopt(new oasys::UIntOpt("test_recv_delay",
                                &params->test_recv_delay_));
    
    p.addopt(new oasys::UIntOpt("test_read_limit",
                                &params->test_read_limit_));
    p.addopt(new oasys::UIntOpt("test_write_limit",
                                &params->test_write_limit_));
    
    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }
    
    if (params->sendbuf_len_ == 0) {
        *invalidp = "sendbuf_len must not be zero";
        return false;
    }

    if (params->recvbuf_len_ == 0) {
        *invalidp = "recvbuf_len must not be zero";
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::dump_link(const LinkRef& link,
                                      oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("reactive_frag_enabled: %u\n", params->reactive_frag_enabled_);
    buf->appendf("sendbuf_len: %u\n", params->sendbuf_len_);
    buf->appendf("recvbuf_len: %u\n", params->recvbuf_len_);
    buf->appendf("data_timeout: %u\n", params->data_timeout_);
    buf->appendf("test_read_delay: %u\n", params->test_read_delay_);
    buf->appendf("test_write_delay: %u\n", params->test_write_delay_);
    buf->appendf("test_read_limit: %u\n",params->test_read_limit_);
    buf->appendf("test_write_limit: %u\n",params->test_write_limit_);
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::init_link(const LinkRef& link,
                                      int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    LinkParams* params = dynamic_cast<LinkParams *>(new_link_params());
    ASSERT(params != NULL);

    // Try to parse the link's next hop, but continue on even if the
    // parse fails since the hostname may not be resolvable when we
    // initialize the link. Each subclass is responsible for
    // re-checking when opening the link.
    parse_nexthop(link, params);
    
    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    if (! finish_init_link(link, params)) {
        log_err("error in finish_init_link");
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    log_debug("ConnectionConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    if (link->isopen() || link->isopening()) {
        log_debug("ConnectionConvergenceLayer::delete_link: "
                  "link %s open, deleting link state when contact closed",
                  link->name());
        return;
    }

    ASSERT(link->contact() == NULL);
    ASSERT(link->cl_info() != NULL);

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::finish_init_link(const LinkRef& link,
                                             LinkParams* params)
{
    (void)link;
    (void)params;
    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::reconfigure_link(const LinkRef& link,
                                             int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }

    if (link->isopen()) {
        LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
        ASSERT(params != NULL);
        
        CLConnection* conn = dynamic_cast<CLConnection*>(link->contact()->cl_info());
        ASSERT(conn != NULL);
        
        if ((params->sendbuf_len_ != conn->sendbuf_.size()) &&
            (params->sendbuf_len_ >= conn->sendbuf_.fullbytes()))
        {
            log_info("resizing link *%p send buffer from %zu -> %u",
                     link.object(), conn->sendbuf_.size(),
                     params->sendbuf_len_);
            conn->sendbuf_.set_size(params->sendbuf_len_);
        }

        if ((params->recvbuf_len_ != conn->recvbuf_.size()) &&
            (params->recvbuf_len_ >= conn->recvbuf_.fullbytes()))
        {
            log_info("resizing link *%p recv buffer from %zu -> %u",
                     link.object(), conn->recvbuf_.size(),
                     params->recvbuf_len_);
            conn->recvbuf_.set_size(params->recvbuf_len_);
        }
    }

    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("ConnectionConvergenceLayer::open_contact: "
              "opening contact on link *%p", link.object());
    
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    // create a new connection for the contact, set up to use the
    // link's configured parameters
    CLConnection* conn = new_connection(link, params);
    conn->set_contact(contact);
    contact->set_cl_info(conn);
    conn->start();

    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::close_contact(const ContactRef& contact)
{
    log_info("close_contact *%p", contact.object());

    const LinkRef& link = contact->link();
    ASSERT(link != NULL);
    
    CLConnection* conn = dynamic_cast<CLConnection*>(contact->cl_info());
    ASSERT(conn != NULL);

    // if the connection isn't already broken, then we need to tell it
    // to do so
    if (! conn->contact_broken_) {
        conn->cmdqueue_.push_back(
            CLConnection::CLMsg(CLConnection::CLMSG_BREAK_CONTACT));
    }
    
    while (!conn->is_stopped()) {
        log_debug("waiting for connection thread to stop...");
        usleep(100000);
        oasys::Thread::yield();
    }

    // now that the connection thread is stopped, clean up the in
    // flight and incoming bundles
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    while (! conn->inflight_.empty()) {
        CLConnection::InFlightBundle* inflight = conn->inflight_.front();
        u_int32_t sent_bytes  = inflight->sent_data_.num_contiguous();
        u_int32_t acked_bytes = inflight->ack_data_.num_contiguous();

        if ((! params->reactive_frag_enabled_) ||
            (sent_bytes == 0) ||
            (link->is_reliable() && acked_bytes == 0))
        {
            // if we've started the bundle but not gotten anything
            // out, we need to push the bundle back onto the link
            // queue so it's there when the link re-opens
            if (! link->del_from_inflight(inflight->bundle_,
                                          inflight->total_length_) ||
                ! link->add_to_queue(inflight->bundle_,
                                     inflight->total_length_))
            {
                log_warn("inflight queue mismatch for bundle %" PRIbid,
                         inflight->bundle_->bundleid());
            }
            
        } else {
            // otherwise, if part of the bundle has been transmitted,
            // then post the event so that the core system can do
            // reactive fragmentation
            if (! inflight->transmit_event_posted_) {
                BundleDaemon::post(
                    new BundleTransmittedEvent(inflight->bundle_.object(),
                                               contact, link,
                                               sent_bytes, acked_bytes));
            }
        }

        conn->inflight_.pop_front();
        delete inflight;
    }

    // check the tail of the incoming queue to see if there's a
    // partially-received bundle that we need to post a received event
    // for (if reactive fragmentation is enabled)
    if (! conn->incoming_.empty()) {
        CLConnection::IncomingBundle* incoming = conn->incoming_.back();
        if (!incoming->rcvd_data_.empty())
        {  
            size_t rcvd_len = incoming->rcvd_data_.last() + 1;
            
            size_t header_block_length =
                BundleProtocol::payload_offset(&incoming->bundle_->recv_blocks());
        
            if ((incoming->total_length_ == 0) && 
                params->reactive_frag_enabled_ &&
                (rcvd_len > header_block_length))
            {
                log_debug("partial arrival of bundle: "
                          "got %zu bytes [hdr %zu payload %zu]",
                          rcvd_len, header_block_length,
                          incoming->bundle_->payload().length());
             
                BundleDaemon::post(
                    new BundleReceivedEvent(incoming->bundle_.object(),
                                            EVENTSRC_PEER, rcvd_len,
                                            contact->link()->remote_eid(),
                                            contact->link().object()));
            }
        }
    }

    // drain the CLConnection incoming queue
    while (! conn->incoming_.empty()) {
        CLConnection::IncomingBundle* incoming = conn->incoming_.back();
        conn->incoming_.pop_back();
        delete incoming;
    }

    // clear out the connection message queue
    CLConnection::CLMsg msg;
    while (conn->cmdqueue_.try_pop(&msg)) {}
    
    delete conn;
    
    contact->set_cl_info(NULL);

    if (link->isdeleted()) {
        ASSERT(link->cl_info() != NULL);
        delete link->cl_info();
        link->set_cl_info(NULL);
    }

    return true;
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::bundle_queued(const LinkRef& link,
                                          const BundleRef& bundle)
{
    (void)bundle;
    //log_debug("ConnectionConvergenceLayer::bundle_queued: "
    //          "queued *%p on *%p", bundle.object(), link.object());

    if (! link->isopen()) {
        return;
    }

    ASSERT(!link->isdeleted());
    
    const ContactRef contact = link->contact();
    //dzdebug ASSERT(contact != NULL);
    if (contact == nullptr) {
        log_err("ConnectionConvergenceLayer::bundle_queued called - contact is null - return");
        return;
    }

    CLConnection* conn = dynamic_cast<CLConnection*>(contact->cl_info());
    ASSERT(conn != NULL);
    
    // the bundle was previously put on the link queue, so we just
    // kick the connection thread in case it's idle.
    //
    // note that it's possible the bundle was already picked up and
    // taken off the link queue by the connection thread, so don't
    // assert here.
    conn->cmdqueue_.push_back(
        CLConnection::CLMsg(CLConnection::CLMSG_BUNDLES_QUEUED));
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::cancel_bundle(const LinkRef& link,
                                          const BundleRef& bundle)
{
    ASSERT(! link->isdeleted());
    
    // the bundle should be on the inflight queue for cancel_bundle to
    // be called
    if (! bundle->is_queued_on(link->inflight())) {
        log_warn("cancel_bundle *%p not on link %s inflight queue",
                 bundle.object(), link->name());
        return;
    }
    
    if (!link->isopen()) {
        /* 
         * (Taken from jmmikkel checkin comment on BBN source tree)
         *
         * The dtnme internal convergence layer complains and does
         * nothing if you try to cancel a bundle after the link has
         * closed instead of just considering the send cancelled. I
         * believe that posting a BundleCancelledEvent before
         * returning is the correct way to make the cancel actually
         * happen in this situation, as the bundle is removed from the
         * link queue in that event's handler.
         */
        log_warn("cancel_bundle *%p but link *%p isn't open!!",
                 bundle.object(), link.object());
        BundleDaemon::post(new BundleSendCancelledEvent(bundle.object(), link));
        return;
    }
    
    const ContactRef contact = link->contact();
    CLConnection* conn = dynamic_cast<CLConnection*>(contact->cl_info());
    ASSERT(conn != NULL);

    ASSERT(contact->link() == link);
    log_debug("ConnectionConvergenceLayer::cancel_bundle: "
              "cancelling *%p on *%p", bundle.object(), link.object());

    conn->cmdqueue_.push_back(
        CLConnection::CLMsg(CLConnection::CLMSG_CANCEL_BUNDLE, bundle));
}

} // namespace dtn
