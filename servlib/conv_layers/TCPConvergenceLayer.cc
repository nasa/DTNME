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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sys/poll.h>
#include <stdlib.h>

#include <oasys/io/NetUtils.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/HexDumpBuffer.h>

#include "TCPConvergenceLayer.h"
#include "IPConvergenceLayerUtils.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

TCPConvergenceLayer::TCPLinkParams
    TCPConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPLinkParams::TCPLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults),
      hexdump_(false),
      local_addr_(INADDR_ANY),
      remote_addr_(INADDR_NONE),
      remote_port_(TCPCL_DEFAULT_PORT)
{
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::TCPLinkParams::serialize(oasys::SerializeAction *a)
{
    log_debug_p("TCPLinkParams", "TCPLinkParams::serialize");
    StreamConvergenceLayer::StreamLinkParams::serialize(a);
    a->process("hexdump", &hexdump_);
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPConvergenceLayer()
    : StreamConvergenceLayer("TCPConvergenceLayer", "tcp", TCPCL_VERSION)
{
}

//----------------------------------------------------------------------
CLInfo*
TCPConvergenceLayer::new_link_params()
{
    return new TCPLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    TCPLinkParams* params = dynamic_cast<TCPLinkParams *>(new_link_params());
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

	// Calls the StreamConvergenceLayer method
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
TCPConvergenceLayer::parse_link_params(LinkParams* lparams,
                                       int argc, const char** argv,
                                       const char** invalidp)
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;
    
    p.addopt(new oasys::BoolOpt("hexdump", &params->hexdump_));
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;
    
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }
    
    // continue up to parse the parent class
    return StreamConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    StreamConvergenceLayer::dump_link(link, buf);
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("hexdump: %s\n", params->hexdump_ ? "enabled" : "disabled");
    buf->appendf("local_addr: %s\n", intoa(params->local_addr_));
    buf->appendf("remote_addr: %s\n", intoa(params->remote_addr_));
    buf->appendf("remote_port: %u\n", params->remote_port_);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::parse_nexthop(const LinkRef& link, LinkParams* lparams)
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(lparams);
    ASSERT(params != NULL);

    if (params->remote_addr_ == INADDR_NONE || params->remote_port_ == 0)
    {
        if (! IPConvergenceLayerUtils::parse_nexthop(logpath_, link->nexthop(),
                                                     &params->remote_addr_,
                                                     &params->remote_port_)) {
            return false;
        }
    }
    
    if (params->remote_addr_ == INADDR_ANY ||
        params->remote_addr_ == INADDR_NONE)
    {
        log_warn("can't lookup hostname in next hop address '%s'",
                 link->nexthop());
        return false;
    }
    
    // if the port wasn't specified, use the default
    if (params->remote_port_ == 0) {
        params->remote_port_ = TCPCL_DEFAULT_PORT;
    }
    
    return true;
}

//----------------------------------------------------------------------
CLConnection*
TCPConvergenceLayer::new_connection(const LinkRef& link, LinkParams* p)
{
    (void)link;
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    in_addr_t local_addr = INADDR_ANY;
    u_int16_t local_port = TCPCL_DEFAULT_PORT;
    u_int32_t segment_len = TCPConvergenceLayer::default_link_params_.segment_length_;
    u_int32_t keepalive_interval = TCPConvergenceLayer::default_link_params_.keepalive_interval_;

    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &local_addr));
    p.addopt(new oasys::UInt16Opt("local_port", &local_port));
    p.addopt(new oasys::UIntOpt("segment_length", &segment_len));
    p.addopt(new oasys::UIntOpt("keepalive_interval", &keepalive_interval));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }
    
    TCPConvergenceLayer::default_link_params_.segment_length_ = segment_len;
    TCPConvergenceLayer::default_link_params_.keepalive_interval_ = keepalive_interval;

    // check that the local interface / port are valid
    if (local_addr == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }

    if (local_port == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    int ret = listener->bind(local_addr, local_port);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);
        
        ret = listener->bind(local_addr, local_port);
    }

    if (ret != 0) {
        return false; // error already logged
    }

    // start listening and then start the thread to loop calling accept()

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);
    
    return true;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::interface_activate(Interface* iface)
{
    // start listening and then start the thread to loop calling accept()
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    listener->listen();
    listener->start();
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::interface_down(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    
    TCPLinkParams* params = &default_link_params_;
    buf->appendf("\tlocal_addr: %s local_port: %d segment_ack_enabled: %u\n",
                 intoa(listener->local_addr()), listener->local_port(), 
                 params->segment_ack_enabled_);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Listener::Listener(TCPConvergenceLayer* cl)
    : TCPServerThread("TCPConvergenceLayer::Listener",
                      "/dtn/cl/tcp/listener"),
      cl_(cl)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);

    TCPLinkParams *params = new TCPLinkParams(false);
    *params = default_link_params_;

    Connection* conn = new Connection(cl_, params, fd, addr, port);
    conn->start();
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection::Connection(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection",
                                         cl->logpath(), cl, params,
                                         true /* call connect() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);


    cmdqueue_.logpathf("%s", logpath_);

    // set up the base class' nexthop parameter
    oasys::StringBuffer nexthop("%s:%d",
                                intoa(params->remote_addr_),
                                params->remote_port_);
    set_nexthop(nexthop.c_str());
    
    // the actual socket
    sock_ = new oasys::TCPClient(logpath_);

    //dzdebug
    //sock_->params_.tcp_nodelay_ = true;

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
    if (params->local_addr_ != INADDR_ANY)
    {
        if (sock_->bind(params->local_addr_, 0) != 0) {
            log_err("error binding to %s: %s",
                    intoa(params->local_addr_),
                    strerror(errno));
        }
    }
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection::Connection(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params,
                                            int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection",
                                         cl->logpath(), cl, params,
                                         false /* call accept() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);
    
    // set up the base class' nexthop parameter
    oasys::StringBuffer nexthop("%s:%d", intoa(remote_addr), remote_port);
    set_nexthop(nexthop.c_str());
    
    sock_ = new oasys::TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::serialize(oasys::SerializeAction *a)
{
    TCPLinkParams *params = tcp_lparams();
    if (! params) return;

    a->process("hexdump", &params->hexdump_);
    a->process("local_addr", oasys::InAddrPtr(&params->local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&params->remote_addr_));
    a->process("remote_port", &params->remote_port_);

    // from StreamLinkParams
    a->process("segment_ack_enabled", &params->segment_ack_enabled_);
    a->process("negative_ack_enabled", &params->negative_ack_enabled_);
    a->process("keepalive_interval", &params->keepalive_interval_);
    a->process("segment_length", &params->segment_length_);

    // from LinkParams
    a->process("reactive_frag_enabled", &params->reactive_frag_enabled_);
    a->process("sendbuf_length", &params->sendbuf_len_);
    a->process("recvbuf_length", &params->recvbuf_len_);
    a->process("data_timeout", &params->data_timeout_);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;
    
    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(params_);
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
TCPConvergenceLayer::Connection::connect()
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
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(params_);
    ASSERT(params != NULL);
    sock_->set_remote_addr(params->remote_addr_);
    sock_->set_remote_port(params->remote_port_);

    // start a connection to the other side... in most cases, this
    // returns EINPROGRESS, in which case we wait for a call to
    // handle_poll_activity
    log_debug("connect: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    ASSERT(contact_ == NULL || contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    int ret = sock_->connect(sock_->remote_addr(), sock_->remote_port());

    if (ret == 0) {
        log_debug("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

        initiate_contact();
        
    } else if (ret == -1 && errno == EINPROGRESS) {
        log_debug("connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("connection attempt to %s:%d failed... %s",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::accept()
{
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);
    
    log_debug("accept: got connection from %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    initiate_contact();
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::disconnect()
{
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::handle_poll_activity()
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
        //log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;
            
        if (sock_->state() == oasys::IPSocket::CONNECTING) {
            int result = sock_->async_connect_result();
            if (result == 0 && sendbuf_.fullbytes() == 0) {
                log_debug("delayed_connect to %s:%d succeeded",
                          intoa(sock_->remote_addr()), sock_->remote_port());
                initiate_contact();
                
            } else {
                log_info("connection attempt to %s:%d failed... %s",
                         intoa(sock_->remote_addr()), sock_->remote_port(),
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
        if (delay_reads_to_free_some_storage_ > 0) {
            if (--delay_reads_to_free_some_storage_ == 0) {
                 // try to accept the last incoming bundle again
                if (NULL != incoming_bundle_to_retry_to_accept_) {
                    IncomingBundle* incoming = incoming_bundle_to_retry_to_accept_;
                    incoming_bundle_to_retry_to_accept_ = NULL;
                    retry_to_accept_incoming_bundle(incoming);
                }
            }
            return;
        }

        recv_data();
        process_data();

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
TCPConvergenceLayer::Connection::send_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    u_int towrite = sendbuf_.fullbytes();
    if (params_->test_write_limit_ != 0) {
        towrite = std::min(towrite, params_->test_write_limit_);
    }
    
    //log_debug("send_data: trying to drain %u bytes from send buffer...",
    //          towrite);
    ASSERT(towrite > 0);

    int cc = sock_->write(sendbuf_.start(), towrite);
    if (cc > 0) {
        //log_debug("send_data: wrote %d/%zu bytes from send buffer",
        //          cc, sendbuf_.fullbytes());
        if (tcp_lparams()->hexdump_) {
            oasys::HexDumpBuffer hex;
            hex.append((u_char*)sendbuf_.start(), cc);
            log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }
        
        sendbuf_.consume(cc);
        
        if (sendbuf_.fullbytes() != 0) {
            log_debug("send_data: incomplete write, setting POLLOUT bit");
            sock_pollfd_->events |= POLLOUT;

        } else {
            if (sock_pollfd_->events & POLLOUT) {
                //log_debug("send_data: drained buffer, clearing POLLOUT bit");
                sock_pollfd_->events &= ~POLLOUT;
            }
        }
    } else if (errno == EWOULDBLOCK) {
        //log_debug("send_data: write returned EWOULDBLOCK, setting POLLOUT bit");
        sock_pollfd_->events |= POLLOUT;
        
    } else {
        log_info("send_data: remote connection unexpectedly closed: %s",
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection::recv_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);
    
    // this shouldn't ever happen
    if (recvbuf_.tailbytes() == 0) {
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

    //log_debug("recv_data: draining up to %u bytes into recv buffer...", toread);
    int cc = sock_->read(recvbuf_.end(), toread);
    if (cc < 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    //log_debug("recv_data: read %d bytes, rcvbuf has %zu bytes",
    //          cc, recvbuf_.fullbytes());
    if (tcp_lparams()->hexdump_) {
        oasys::HexDumpBuffer hex;
        hex.append((u_char*)recvbuf_.end(), cc);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }
    recvbuf_.fill(cc);
}

} // namespace dtn
