/*
 *    Copyright 2007-2010 Darren Long, darren.long@mac.com
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


#include <sys/poll.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

#include <oasys/io/NetUtils.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/HexDumpBuffer.h>
#include <oasys/util/CRC32.h>

#include "AX25CMConvergenceLayer.h"
#include "IPConvergenceLayerUtils.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

#include <iostream>
#include <sstream>

namespace dtn {

AX25CMConvergenceLayer::AX25CMLinkParams 
                            AX25CMConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
AX25CMConvergenceLayer::AX25CMLinkParams::AX25CMLinkParams(bool init_defaults)
    :   SeqpacketLinkParams(init_defaults),
        hexdump_(false),
        local_call_("NO_CALL"),
        remote_call_("NO_CALL"),
        digipeater_("NO_CALL"),
        axport_("None")
{
    SeqpacketLinkParams::keepalive_interval_=30;
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::AX25CMLinkParams::serialize(oasys::SerializeAction *a)
{
    SeqpacketConvergenceLayer::SeqpacketLinkParams::serialize(a);
    a->process("hexdump", &hexdump_);
    a->process("local_call", &local_call_);
    a->process("remote_call", &remote_call_);
    a->process("digipeater", &digipeater_);
    a->process("axport", &axport_);

}

//----------------------------------------------------------------------
AX25CMConvergenceLayer::AX25CMConvergenceLayer()
    : SeqpacketConvergenceLayer("AX25CMConvergenceLayer", "ax25cm", AX25CMCL_VERSION)
{
    log_debug("AX25CMConvergenceLayer instantiated. ***");

}

//----------------------------------------------------------------------
CLInfo*
AX25CMConvergenceLayer::new_link_params()
{
    return new AX25CMLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::init_link(const LinkRef& link,
                                  int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams *>(new_link_params());
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

	// Calls the SeqpacketConvergenceLayer method
    if (! finish_init_link(link, params)) {
        log_err("error in finish_init_link");
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::parse_link_params(LinkParams* lparams,
                                        int argc, const char** argv,
                                        const char** invalidp)
{
    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("hexdump", &params->hexdump_));
    p.addopt(new oasys::StringOpt("local_call", &params->local_call_));    
    p.addopt(new oasys::StringOpt("remote_call", &params->remote_call_));
    p.addopt(new oasys::StringOpt("digipeater", &params->digipeater_));
    p.addopt(new oasys::StringOpt("axport", &params->axport_));

    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;

    if (params->local_call_ == "NO_CALL") {
        log_err("invalid local callsign setting of NO_CALL");
        return false;
    }

    if (params->remote_call_ == "NO_CALL") {
        log_err("invalid remote callsign setting of NO_CALL");
        return false;
    }

    if (params->axport_ == "None") {
        log_err("invalid local axport setting of None");
        return false;
    }

    // continue up to parse the parent class
    return SeqpacketConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    SeqpacketConvergenceLayer::dump_link(link, buf);

    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(link->cl_info());
    ASSERT(params != NULL);

    buf->appendf("hexdump: %s\n", params->hexdump_ ? "enabled" : "disabled");
    buf->appendf("local_call: %s\n", params->local_call_.c_str());
    buf->appendf("remote_call: %s\n", params->remote_call_.c_str());
    buf->appendf("digipeater: %s\n", params->digipeater_.c_str());
    buf->appendf("axport: %s\n", params->axport_.c_str());
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::parse_nexthop(const LinkRef& link, LinkParams* lparams)
{
    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(lparams);
    ASSERT(params != NULL);

    if (params->remote_call_ == "NO_CALL" || params->axport_ == "None")
    {
        if (! AX25ConvergenceLayerUtils::parse_nexthop(logpath_, link->nexthop(),
                                                     &params->local_call_,
                                                     &params->remote_call_,
                                                     &params->digipeater_,
                                                     &params->axport_)) {
            return false;
        }
    }

    //std::cout<<"local_call:"<<params->local_call_<<std::endl;
    //std::cout<<"axport:"<<params->axport_<<std::endl;
    //std::cout<<"remote_call:"<<params->remote_call_<<std::endl;   

    if (params->remote_call_ == "NO_CALL")    {
        log_warn("can't lookup callsign in next hop address '%s'",
                 link->nexthop());
        return false;
    }

    // make sure the port was specified
    if (params->axport_ == "None") {
        log_err("axport not specified in next hop address '%s'",
                link->nexthop());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
CLConnection*
AX25CMConvergenceLayer::new_connection(const LinkRef& link, LinkParams* p)
{
    (void)link;
    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    std::string local_call = "NO_CALL";
    std::string axport = "None";

    oasys::OptParser p;
    p.addopt(new oasys::StringOpt("local_call", &local_call));
    p.addopt(new oasys::StringOpt("axport", &axport));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (local_call == "NO_CALL") {
        log_err("invalid local call setting of NO_CALL");
        return false;
    }

    if (axport == "None") {
        log_err("invalid local axport setting of None");
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());

    int ret = listener->bind(axport, local_call);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                        "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                        "waiting for 10 seconds then trying again");
        sleep(10);

        ret = listener->bind(axport, local_call);    }

    if (ret != 0) {
        return false; // error already logged
    }

    // start listening and then start the thread to loop calling accept()
    //dzdebug listener->listen();
    //dzdebug listener->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);

    return true;
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::interface_activate(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    listener->listen();
    listener->start();
}

//----------------------------------------------------------------------
bool
AX25CMConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);

    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);

    buf->appendf("\tlocal_call: %s axport: %s\n",
                    listener->local_call().c_str(), listener->axport().c_str());
}

//----------------------------------------------------------------------
AX25CMConvergenceLayer::Listener::Listener(AX25CMConvergenceLayer* cl)
    :    AX25ConnectedModeServerThread("AX25CMConvergenceLayer::Listener",
                        "/dtn/cl/ax25cm/listener"), cl_(cl)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Listener::accepted(int fd, const std::string& addr)
{
    log_debug("new connection from %s", addr.c_str());

    Connection* conn =
        new Connection(cl_, &AX25CMConvergenceLayer::default_link_params_,
                       fd, local_call(), addr, axport());
    conn->start();
}

//----------------------------------------------------------------------
AX25CMConvergenceLayer::Connection::Connection(AX25CMConvergenceLayer* cl,
                                            AX25CMLinkParams* params)
    : SeqpacketConvergenceLayer::Connection("AX25CMConvergenceLayer::Connection",
                                            cl->logpath(), cl, params,
                                            true /* call connect() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);

    // set up the base class' nexthop parameter
    std::stringstream ss;
    ss<<params->local_call_<<":"<<params->remote_call_;
    if(params->digipeater_ != "NO_CALL")
    {
        ss<<","<<params->digipeater_;
    }
    ss<<":"<<params->axport_<<std::ends;
    oasys::StringBuffer nexthop("%s", ss.str().c_str());
    set_nexthop(nexthop.c_str());

    // the actual socket
    sock_ = new oasys::AX25ConnectedModeClient(logpath_);

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);

    sock_->init_socket();
    sock_->set_nonblocking(true);

    // if the parameters specify a local address, do the bind here --
    // however if it fails, we can't really do anything about it, so
    // just log and go on
    if (params->local_call_ != "NO_CALL")
    {
        if (sock_->bind(params->axport_, params->local_call_) != 0) {
            log_err("error binding to %s axport=%s : %s",
                    params->local_call_.c_str(),params->axport_.c_str(),
                    strerror(errno));
        }
    }
}

//----------------------------------------------------------------------
AX25CMConvergenceLayer::Connection::Connection(AX25CMConvergenceLayer* cl,
                                               AX25CMLinkParams* params,
                                               int fd, 
                                               const std::string& local_call,
                                               const std::string& addr,
                                               const std::string& axport)
    : SeqpacketConvergenceLayer::Connection("AX25CMConvergenceLayer::Connection",
                                         cl->logpath(), cl, params,
                                         false /* call accept() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);

    // set up the base class' nexthop parameter
    std::stringstream ss;
    ss<<local_call<<":"<<addr<<":"<<axport<<std::ends;
    oasys::StringBuffer nexthop("%s", ss.str().c_str());
    set_nexthop(nexthop.c_str());

    sock_ = new oasys::AX25ConnectedModeClient(fd, addr, logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);
}

//----------------------------------------------------------------------
AX25CMConvergenceLayer::Connection::~Connection()
{
    sock_->shutdown(SHUT_RDWR);
    delete sock_;
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;

    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;

    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(params_);
    ASSERT(params != NULL);

    poll_timeout_ = params->data_timeout_;

    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::connect()
{
    // the first thing we do is try to parse the next hop address...
    // if we're unable to do so, the link can't be opened.
    if (! cl_->parse_nexthop(contact_->link(), params_)) {
        log_info("can't resolve nexthop address '%s'",
                 contact_->link()->nexthop());
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // cache the remote addr and port in the fields in the socket
    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(params_);
    ASSERT(params != NULL);
    sock_->set_remote_call(params->remote_call_);
    sock_->set_axport(params->axport_);
    //sock_->set_via_route(params->digipeater_);
    // start a connection to the other side... in most cases, this
    // returns EINPROGRESS, in which case we wait for a call to
    // handle_poll_activity
    log_debug("connect: connecting to %s axport=%s...",
              sock_->remote_call().c_str(), sock_->axport().c_str());
    ASSERT(contact_ == NULL || contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::AX25Socket::ESTABLISHED);

    std::vector<std::string> rr;
    std::string rp = sock_->axport();
    std::string rc = sock_->remote_call();
    if(params->digipeater_ != "NO_CALL")
    {
        rr.push_back(params->digipeater_);
    }
    
    int ret = sock_->oasys::AX25Socket::connect(rp, rc, rr);

    if (ret == 0) {
        log_debug("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::AX25Socket::ESTABLISHED);

        initiate_contact();

    } else if (ret == -1 && errno == EINPROGRESS) {
        log_debug("connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("connection attempt to %s axport=%s failed... %s",
                 sock_->remote_call().c_str(), sock_->axport().c_str(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
        // DML - Attempted bug fix hack here below
        disconnect();
    }
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::accept()
{
    ASSERT(sock_->state() == oasys::AX25Socket::ESTABLISHED);

    log_debug("accept: got connection from %s axport=%s...",
              sock_->remote_call().c_str(), sock_->axport().c_str());
    initiate_contact();
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::process_data()
{

    log_always("AX25CMConvergenceLayer::Connection::process_data() called");
    SeqpacketConvergenceLayer::Connection::process_data();

}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::disconnect()
{
    if (sock_->state() != oasys::AX25Socket::CLOSED) {
        log_debug("closing socket");
        sock_->close();
    }
    else {
        log_debug("attempting to close socket in state oasys::AX25Socket::CLOSED");
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::handle_poll_activity()
{
    if (sock_pollfd_->revents & POLLHUP) {
        log_info("remote socket closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if (sock_pollfd_->revents & POLLERR) {
        log_info("error condition on remote socket -- returned POLLERR");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // first check for write readiness, meaning either we're getting a
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked
    if (sock_pollfd_->revents & POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;

        if (sock_->state() == oasys::AX25Socket::CONNECTING) {
            int result = sock_->async_connect_result();
            if (result == 0 && sendbuf_.fullbytes() == 0) {
                log_debug("delayed_connect to %s axport=%s succeeded",
                          sock_->remote_call().c_str(), sock_->axport().c_str());
                initiate_contact();

            } else {
                log_info("connection attempt to %s axport=%s failed... %s",
                          sock_->remote_call().c_str(), sock_->axport().c_str(),
                         strerror(errno));
                break_contact(ContactEvent::BROKEN);
            }

            return;
        }

        send_data();
    }

    //check that the connection was not broken during the data send
    if (contact_broken_)
    {
        return;
    }

    // finally, check for incoming data
    if (sock_pollfd_->revents & POLLIN) {
        recv_data();
        this->process_data();

        // Sanity check to make sure that there's space in the buffer
        // for a subsequent read_data() call
        if (recvbuf_.tailbytes() == 0) {
            log_err("process_data left no space in recvbuf!!");
        }

        if (contact_up_ && ! contact_broken_) {
            check_keepalive();
        }

    }

}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::send_data()
{

    // DML: If we have any sequence delimiters on the queue, then try and send the first sequence,
    // and if not, all we can do here is try and send the whole buffer.  Whichever we send,
    // the whole thing should go through the socket, or it is a protocol error.
    // When we've selected either the first sequence in the queue or the entire buffer for
    // sending, then we'll create a temporary buffer for the payload, calculate the CRC, append it,
    // and try and send the packet payload through the socket.
    // If it works, then we'll pop the sequence off the queue, consume the appropriate length of
    // data from the buffer and be done.
    // If we get a WOULDBLOCK and we're not sending a sequence, then push a sequence on the queue.
    // If we get a WOULDBLOCK, and we are sending a sequence, then leave the sequence on the queue.
    // We have to recalculate the CRC every time we try and send the same payload.  Shame.


    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    AX25CMLinkParams* params = dynamic_cast<AX25CMLinkParams*>(params_);
    ASSERT(params != NULL);
    u_int towrite = 0;
    u_int payload_length = 0;

//    if (params_->test_write_limit_ != 0) {
//        towrite = std::min(towrite, params_->test_write_limit_);
//    }       

    //  see if we have any length delimiters queued from previous attempts where EWOULDBLOCK
    //  was set. if so, only send that much data through the socket write and leave the rest
    // for subsequent calls to take care of.

    ASSERT(!sendbuf_sequence_delimiters_.empty() );
    payload_length = sendbuf_sequence_delimiters_.front();
    log_debug("send_data: trying to drain %u bytes from pending sequence in send buffer...",
               payload_length);        
  

    ASSERT(payload_length > 0);
    //ASSERT(towrite <= params->segment_length_);

    log_debug("generating CRC32 for payload length: %u", payload_length);    
    oasys::CRC32 crc;
    crc.update(sendbuf_.start(), payload_length);
    u_int crc_generated = htonl(crc.value());
    log_debug("appending CRC32 to payload: %x", crc.value());
    towrite = payload_length + sizeof(u_int);
    oasys::StreamBuffer temp(towrite);
    ASSERT(temp.tailbytes() >= payload_length);
    memcpy(temp.end(), sendbuf_.start(), payload_length);
    temp.fill(payload_length);
    ASSERT(temp.tailbytes() >= sizeof(crc_generated));  
    memcpy(temp.end(), reinterpret_cast<char*>(&crc_generated), sizeof(crc_generated));
    temp.fill(sizeof(crc_generated));

    if (ax25cm_lparams()->hexdump_) {
        log_always("send_data sending %i bytes as below...",towrite);
        oasys::HexDumpBuffer hex;
        hex.append((u_char*)temp.start(), towrite);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    int cc = sock_->write(temp.start(), towrite);

    // we really don't want to have leftovers with SOCK_SEQPACKET       
    if (static_cast<u_int>(cc) == towrite) {
        log_debug("send_data: wrote %d/%zu bytes from send buffer", cc, sendbuf_.fullbytes());
       
        sendbuf_.consume(payload_length);

        //  if there's a delimiter on the queue, we've now consumed it, so pop the queue...
        if( !sendbuf_sequence_delimiters_.empty() ) {
            ASSERT(sendbuf_sequence_delimiters_.front() + sizeof(crc_generated) == static_cast<u_int>(cc));
            // well, the assert kicked in too often.   so I'm just gonna
            // declare a protocl error and ditch the link
            if(sendbuf_sequence_delimiters_.front() + sizeof(crc_generated) != static_cast<u_int>(cc))
            {
                std::stringstream ss;
                ss<<"CL attempted to send a "<<sendbuf_sequence_delimiters_.front()+ sizeof(crc_generated);
                ss<<" byte packet, but only "<<cc<<" bytes were sent"<<std::ends;
                log_err(ss.str().c_str());            
                log_err("CL Protocol error: send_buf underrun breaks SOCK_SEQPACKET SEMANTICS");
                break_contact(ContactEvent::CL_ERROR);
                return;
            }
            else
            {

                log_info("removing pending sequence: %u from sequence delimiters queue, queue depth now: %u",
                    sendbuf_sequence_delimiters_.front(),   sendbuf_sequence_delimiters_.size()-1); 
                sendbuf_sequence_delimiters_.pop();                 
            }

        }

        if (sendbuf_.fullbytes() != 0) {            
            log_info("send_data: incomplete write (%u bytes remain in %u segments), setting POLLOUT bit",
                        sendbuf_.fullbytes(), sendbuf_sequence_delimiters_.size());
            sock_pollfd_->events |= POLLOUT;

            ASSERT(!sendbuf_sequence_delimiters_.empty() );        
            ASSERT(sendbuf_sequence_delimiters_.front() <= sendbuf_.fullbytes());        

        } 
        else 
        {
            if (sock_pollfd_->events & POLLOUT) {
                ASSERT(!sendbuf_sequence_delimiters_.empty() );        
                log_debug("send_data: drained buffer, clearing POLLOUT bit");
                sock_pollfd_->events &= ~POLLOUT;
                // if we get here, the queue of delimiters should be empty ...
                ASSERT(sendbuf_sequence_delimiters_.empty()); 
            }
        }
    } 
    else if (errno == EWOULDBLOCK) {
        ASSERT(cc < 0 );

        ASSERT(!sendbuf_sequence_delimiters_.empty() );            
        log_info("send_data: write returned EWOULDBLOCK with %u bytes queued, in %u segments - setting POLLOUT bit",
                    sendbuf_.fullbytes(), sendbuf_sequence_delimiters_.size());
        sock_pollfd_->events |= POLLOUT;
        // so, we're gong to record the length of the send_buf contents
        // so we can extract the right ammount of data next time round to maintain SEQ_PACKET
        // sematics, but only if we're not trying to service the sendbuf_sequence_delimiters_ queue

    } 
    else {
        log_info("send_data: whilst sending %i bytes of data, with %i bytes buffered, remote connection unexpectedly closed: %s",
                    towrite,
                    sendbuf_.fullbytes(),
                    strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
AX25CMConvergenceLayer::Connection::recv_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    // this shouldn't ever happen
    if (recvbuf_.tailbytes() < 256) {
        log_err("no space in receive buffer to accept data!!!");
        return;
    }

    if (params_->test_read_delay_ != 0) {
        log_debug("recv_data: sleeping for test_read_delay msecs %u",
                  params_->test_read_delay_);

        usleep(params_->test_read_delay_ * 1000);
    }


    u_int toread = recvbuf_.tailbytes();
    if (params_->test_read_limit_ != 0) {
        toread = std::min(toread, params_->test_read_limit_);
    }

    log_debug("recv_data: draining up to %u bytes into recv buffer...", toread);
    int cc = sock_->read(recvbuf_.end(), toread);
    if (cc < 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("recv_data: read %d bytes, rcvbuf has %zu bytes",
                cc, recvbuf_.fullbytes());
    if (ax25cm_lparams()->hexdump_) {
        oasys::HexDumpBuffer hex;
        hex.append((u_char*)recvbuf_.end(), cc);
        log_always("recv_data received %i bytes as below...",cc);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    oasys::CRC32 crc;
    if(static_cast<uint>(cc) <= sizeof(oasys::CRC32::CRC_t)) {
        // DML: I had an assert here to see if we ever get 'packets' that are smaller than
        // the CRC size.  Well, we did, and I can't for the life of me figure out why.
        // So, we have to protect ourselves from this kind of thing happening, and for
        // now I think the thing to do is to disconnect the other end, because obviously
        // their AX.25 CL implementation sucks ;-) or there's a problem somewhere else
        // in the stack or kit. Still, bye-bye time.
        log_err("CL Protocol error: Format error in recv_data");
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    // check the CRC is good
    uint crc_offset = static_cast<uint>(cc) - sizeof(oasys::CRC32::CRC_t);
    crc.update(recvbuf_.start(), crc_offset);
    uint crc_calculated = crc.value();
    uint crc_received = *reinterpret_cast<uint*>(recvbuf_.start() + crc_offset);
    crc_received = ntohl(crc_received);
    log_debug("crc received: %x, crc calculated: %x", crc_received, crc_calculated);
    if(crc_received != crc_calculated) {
        log_err("CL Protocol error: CRC failure detected in recv_data");
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    recvbuf_.fill(cc- sizeof(oasys::CRC32::CRC_t));
}

/**
 * Parse a next hop address specification of the form
 * LOCAL_CALL:REMOTE_CALL:AXPORT or REMOTE_CALL<,DIGIPEATER>:axport
 *
 * @return true if the conversion was successful, false
 */
bool
AX25ConvergenceLayerUtils::parse_nexthop(const char* logpath, const char* nexthop,
                            std::string* local_call, std::string* remote_call,
                            std::string* digipeater,std::string* axport)
{
    *local_call = "NO_CALL";
    *remote_call = "NO_CALL";
    *digipeater = "NO_CALL";
    *axport = "None";
    std::string temp = nexthop, temp2;
    //std::cout<<"Nexthop:"<<temp<<std::endl;

    const char* comma = strchr(nexthop, ',');
    const char* colon1 = strchr(nexthop, ':');
    const char* colon2 = strrchr(nexthop, ':');
    
    
    if(comma != NULL)
    {
        // we have a digi to deal with, so we must be the link initiator
        // we need to parse out the remote_call, digipeater and axport
        remote_call->assign(nexthop, comma - nexthop);       
        temp2.assign(comma+1, ( temp.size()-remote_call->size() ) -1);

        colon1 = strchr(temp2.c_str(),':');

        if(colon1 != NULL)
        {
            digipeater->assign(temp2.c_str(),colon1-temp2.c_str());         
            axport->assign(colon1+1, ( temp2.size() -  digipeater->size() ) -1 );
        }
        
        if ("None" == *axport  || "NO_CALL" == *remote_call || "NO_CALL" == *digipeater) {
            log_warn_p(logpath, "invalid remote_call,digipeater:axport in next hop '%s'",
                       nexthop);
            return false;
        }

    }
    else
    {
        //we don't have a digipeater, but we may be the link initiator meaning
        // that we need remote_call and axport, or we're the listener, in which case
        // we need the local_call, remote_call and axport.  if we have two colons,
        // then we are the listener ...
        
        if( colon2 == NULL)
        {
            // we're the initiator  
            //so look for the remote call and axport
            remote_call->assign(nexthop,colon1-nexthop);
            axport->assign(colon1+1,temp.size()-remote_call->size() - 1);
            
            if ("None" == *axport  || "NO_CALL" == *remote_call) {
                log_warn_p(logpath, "invalid remote_call:axport in next hop '%s'",
                           nexthop);
                return false;
            }
            
        }
        else if(colon1 != NULL)
        {
            // we're the listener
            local_call->assign(nexthop,colon1-nexthop);         
            remote_call->assign(colon1+1,colon2-colon1);
            axport->assign(colon2+1,temp.size()-remote_call->size() - local_call->size() -2);
            
            if ("None" == *axport  || "NO_CALL" == *remote_call || "NO_CALL" == *local_call) {
                log_warn_p(logpath, "invalid local_call:remote_call:axport in next hop '%s'",
                           nexthop);
                return false;
            }
            
        }
    }
    
    return true;
}


} // namespace dtn

#endif /* #ifdef OASYS_AX25_ENABLED  */
