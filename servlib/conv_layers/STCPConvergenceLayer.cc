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

#include <memory>

#include <sys/poll.h>
#include <stdlib.h>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/HexDumpBuffer.h>

#include "STCPConvergenceLayer.h"
#include "IPConvergenceLayerUtils.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "storage/BundleStore.h"

namespace dtn {

STCPConvergenceLayer::STCPLinkParams
    STCPConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
STCPConvergenceLayer::STCPLinkParams::STCPLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults),
      hexdump_(false),
      local_addr_(INADDR_ANY),
      local_port_(STCPCL_DEFAULT_PORT),
      remote_addr_(INADDR_NONE),
      remote_port_(STCPCL_DEFAULT_PORT)
{
    // override StreamLinkParams values
    segment_ack_enabled_ = false;
    negative_ack_enabled_ = false;

    // send keep alives periodically but don't break contact
    keepalive_interval_ = 15;
    break_contact_on_keepalive_fault_ = false;  // send keep alive periodically but don't break
    //segment_length_(8192)

    // override ConnectionConvergenceLayer::LinkParams values
    reactive_frag_enabled_ = false;
    //sendbuf_len_(32768),
    //recvbuf_len_(32768),
    data_timeout_= 30000;  // msec - receiver only!!
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::STCPLinkParams::serialize(oasys::SerializeAction *a)
{
    log_debug_p("STCPLinkParams", "STCPLinkParams::serialize");
    StreamConvergenceLayer::StreamLinkParams::serialize(a);
    a->process("hexdump", &hexdump_);
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
}

//----------------------------------------------------------------------
STCPConvergenceLayer::STCPConvergenceLayer()
    : StreamConvergenceLayer("STCPConvergenceLayer", "stcp", STCPCL_VERSION)
{
}

//----------------------------------------------------------------------
CLInfo*
STCPConvergenceLayer::new_link_params()
{
    return new STCPLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    STCPLinkParams* params = dynamic_cast<STCPLinkParams *>(new_link_params());
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
STCPConvergenceLayer::parse_link_params(LinkParams* lparams,
                                       int argc, const char** argv,
                                       const char** invalidp)
{
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(lparams);
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
STCPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    StreamConvergenceLayer::dump_link(link, buf);
    
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("hexdump: %s\n", params->hexdump_ ? "enabled" : "disabled");
    buf->appendf("local_addr: %s\n", intoa(params->local_addr_));
    buf->appendf("remote_addr: %s\n", intoa(params->remote_addr_));
    buf->appendf("remote_port: %u\n", params->remote_port_);
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("STCP Convergence Layer [%s] - valid Link options:\n\n", name());
    buf.appendf("<nexthop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.appendf("CLA specific options:\n");

    buf.appendf("    remote_addr <IP address>           - IP address to connect to (extracted from nexthop)\n");
    buf.appendf("    remote_port <U16>                  - Port to connect to (extracted from nexthop (default: 4557))\n");
    buf.appendf("    local_addr <IP address>            - IP address to bind to (optional; seldom used for a link)\n");
    buf.appendf("    local_port <U16>                   - Port to bind to (optional; seldom used for a link)\n");
    buf.appendf("    sendbuf_len <U32>                  - Length of internal send buffer (not socket buffer) (default: 2048000)\n");

    buf.appendf("\nOptions for all links:\n");
    
    buf.appendf("    remote_eid <Endpoint ID>           - Remote Endpoint ID\n");
    buf.appendf("    reliable <Bool>                    - Whether the CLA is considered reliable (default: false)\n");
    buf.appendf("    nexthop <ip_address:port>          - IP address and port of remote node (positional in link add command)\n");
    buf.appendf("    mtu <U64>                          - Max size for outgoing bundle triggers proactive fragmentation (default: 0 = no limit)\n");
    buf.appendf("    min_retry_interval <U32>           - Minimum seconds to try to reconnect (default: 5)\n");
    buf.appendf("    max_retry_interval <U32>           - Maximum seconds to try to reconnect (default: 600)\n");
    buf.appendf("    idle_close_time <U32>              - Seconds without receiving data to trigger disconnect (default: 0 = no limit)\n");
    buf.appendf("    potential_downtime <U32>           - Seconds of potential downtime for routing algorithms (default: 30)\n");
    buf.appendf("    prevhop_hdr <Bool>                 - Whether to include the Previous Node Block (default: true)\n");
    buf.appendf("    cost <U32>                         - Abstract figure for use with routing algorithms (default: 100)\n");
    buf.appendf("    qlimit_enabled <Bool>              - Whether Queued Bundle Limits are in use by router (default: false)\n");
    buf.appendf("    qlimit_bundles_high <U64>          - Maximum number of bundles to queue on a link (default: 10)\n");
    buf.appendf("    qlimit_bytes_high <U64>            - Maximum payload bytes to queue on a link (default: 1 MB)\n");
    buf.appendf("    qlimit_bundles_low <U64>           - Low watermark number of bundles to add more bundles to link queue (default: 5)\n");
    buf.appendf("    qlimit_bytes_low <U64>             - Low watermark of payload bytes to add more bundles to link queue (default: 512 KB)\n");
    buf.appendf("    bp6_redirect <link_name>           - Redirect BP6 bundles to specified link (usually for Bundle In Bundle Encapsulation)\n");
    buf.appendf("    bp7_redirect <link_name>           - Redirect BP7 bundles to specified link (usually for Bundle In Bundle Encapsulation)\n");
    buf.appendf("\n");
    buf.append("          (parameters of type <U64> and <Size> can include magnitude character (K, M or G):   125G = 125,000,000,000)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("link add stcp_28 192.168.5.56:4557 ALWAYSON stcp \n");
    buf.appendf("    (create a link named \"stcp3_28\" using the stcp convergence layer to connect to peer at IP address 192.168.5.56 port 4557)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("STCP Convergence Layer [%s] - valid Interface options:\n\n", name());
    buf.appendf("CLA specific options:\n");

    buf.appendf("    local_addr <IP address>            - IP address to listen on (default: 0.0.0.0 = all interfaces) \n");
    buf.appendf("    local_port <U16>                   - Port to listen on (default: 4557)\n");

    buf.appendf("    recvbuf_len <U32>                  - Length of internal receive buffer (not socket buffer) (default: 2048000)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("interface add stcp_in stcp local_addr=0.0.0.0 local_port=4557\n");
    buf.appendf("    (create an interface named \"stcp_in\" using the stcp convergence layer to receive connections on all network interfaces on port 4557)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::parse_nexthop(const LinkRef& link, LinkParams* lparams)
{
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(lparams);
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
        params->remote_port_ = STCPCL_DEFAULT_PORT;
    }
    
    return true;
}

//----------------------------------------------------------------------
CLConnection*
STCPConvergenceLayer::new_connection(const LinkRef& link, LinkParams* p)
{
    (void)link;
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    STCPLinkParams* params = dynamic_cast<STCPLinkParams *>(new_link_params());
    ASSERT(params != nullptr);

    log_debug("adding interface %s", iface->name().c_str());
    params->segment_ack_enabled_ = false;
    params->negative_ack_enabled_ = false;

    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));

    const char* invalidp = nullptr;
    int count = p.parse_and_shift(argc, argv, &invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;
    

    // continue up to parse the parent class
    bool parse_result = StreamConvergenceLayer::parse_link_params(params, argc, argv, &invalidp);

    if (params->segment_ack_enabled_) {
        log_always("Warning - forcing segment_ack_enabled to false for STCP CL\n");
        params->segment_ack_enabled_ = false;
    }

    // check that the local interface / port are valid
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }

    if (params->local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    if (!parse_result) {
        log_err("error parsing interface options: invalid option '%s'", invalidp);
        return false;
    }


    // create a new server socket for the requested interface
    Listener* listener = new Listener(this, params);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    

    int ret = listener->bind(params->local_addr_, params->local_port_);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);
        
        ret = listener->bind(params->local_addr_, params->local_port_);
    }

    if (ret != 0) {
        return false; // error already logged
    }

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);
    
    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::interface_activate(Interface* iface)
{
    // start listening and then start the thread to loop calling accept()
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    listener->listen();
    listener->start();
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::interface_down(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(listener->local_addr()), listener->local_port());
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Listener::Listener(STCPConvergenceLayer* cl, STCPLinkParams* params)
    : TCPServerThread("STCPConvergenceLayer::Listener",
                      "/dtn/cl/tcp/listener"),
      cl_(cl),
      params_(params)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Listener::~Listener()
{
    delete params_;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);

    STCPLinkParams *params = new STCPLinkParams(false);
    *params = *params_;

    Connection* conn = new Connection(cl_, params, fd, addr, port);
    conn->start();
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Connection::Connection(STCPConvergenceLayer* cl,
                                            STCPLinkParams* params)
    : StreamConvergenceLayer::Connection("STCPConvergenceLayer::Connection",
                                         cl->logpath(), cl, params,
                                         true /* call connect() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);

    active_connector_expects_keepalive_ = false; // STCP Sender does not expect keepalives

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
STCPConvergenceLayer::Connection::Connection(STCPConvergenceLayer* cl,
                                            STCPLinkParams* params,
                                            int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : StreamConvergenceLayer::Connection("STCPConvergenceLayer::Connection",
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
STCPConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::run()
{
    char threadname[16] = "STcpCLConnectn";
    pthread_setname_np(pthread_self(), threadname);

    CLConnection::run();
}


//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::serialize(oasys::SerializeAction *a)
{
    STCPLinkParams *params = stcp_lparams();
    if (! params) return;

    a->process("hexdump", &params->hexdump_);
    a->process("local_addr", oasys::InAddrPtr(&params->local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&params->remote_addr_));
    a->process("remote_port", &params->remote_port_);

    // from StreamLinkParams
    a->process("segment_ack_enabled", &params->segment_ack_enabled_);       //XXX/dz false
    a->process("negative_ack_enabled", &params->negative_ack_enabled_);     //XXX/dz false
    a->process("keepalive_interval", &params->keepalive_interval_);     //XXX/dz 0
    a->process("segment_length", &params->segment_length_);     //XXX/dz not used

    // from LinkParams
    a->process("reactive_frag_enabled", &params->reactive_frag_enabled_);     //XXX/dz false
    a->process("sendbuf_len", &params->sendbuf_len_);
    a->process("recvbuf_len", &params->recvbuf_len_);
    a->process("data_timeout", &params->data_timeout_);     //XXX/dz 
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;
    
    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;
    
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(params_);
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
STCPConvergenceLayer::Connection::connect()
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
    STCPLinkParams* params = dynamic_cast<STCPLinkParams*>(params_);
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
STCPConvergenceLayer::Connection::accept()
{
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);
    
    log_debug("accept: got connection from %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    initiate_contact();
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::disconnect()
{
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::handle_poll_activity()
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
        if (delay_reads_to_free_some_storage_) {
            if (delay_reads_timer_.elapsed_ms() >= 2000) {
                delay_reads_to_free_some_storage_ = false;

                 // try to accept the last incoming bundle again
                if (nullptr != incoming_bundle_to_retry_to_accept_) {
                    IncomingBundle* incoming = incoming_bundle_to_retry_to_accept_;
                    incoming_bundle_to_retry_to_accept_ = nullptr;
                    if (incoming->bundle_ != nullptr) {
                        retry_to_accept_incoming_bundle(incoming);
                    } else {
                        log_crit("Unable to retry_to_accept_incoming_bundle because bundle object is null");
                    }
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
bool
STCPConvergenceLayer::Connection::send_next_segment(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    ASSERT(send_segment_todo_ == 0);

    size_t bytes_sent = inflight->sent_data_.empty() ? 0 :
                        inflight->sent_data_.last() + 1;
    
    if (bytes_sent == inflight->total_length_) {
        //log_debug("send_next_segment: "
        //          "already sent all %zu bytes, finishing bundle",
        //          bytes_sent);
        ASSERT(inflight->send_complete_);
        return finish_bundle(inflight);
    }

    // STCP format is a 4 byte (bundle) length followed by the data

    size_t segment_len = inflight->total_length_ - bytes_sent;

    //XXX/dz - TODO: check bundle length <= 4GB
    
    if (bytes_sent == 0) {
        // add the length to the send buf
        if (sendbuf_.tailbytes() < 4) {   // space for the 4 byte length header?
            //log_debug("send_next_segment: "
            //          "not enough space for segment header [need %u, have %zu]",
            //          4, sendbuf_.tailbytes());
            return false;
        }

        u_int32_t nbo_len = htonl(segment_len %0xffffffff);
        u_char* bp = (u_char*)sendbuf_.end();
        memcpy(bp, &nbo_len, sizeof(nbo_len));
        sendbuf_.fill(sizeof(nbo_len));
    }
    
    //log_debug("send_next_segment: "
    //          "starting %zu byte segment [block byte range %zu..%zu]",
    //          segment_len, bytes_sent, bytes_sent + segment_len);

    send_segment_todo_ = segment_len;

    // send_data_todo actually does the deed
    return send_data_todo(inflight);
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::initiate_contact()
{
    // no exchange of info is performed in STCP

    log_debug("initiate_contact called");

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

    // no info exchanged for STCP so we can immediately force a contact_up
    handle_contact_initiation();
}


//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::handle_contact_initiation()
{
    // no exchange of info is performed in STCP
    // - this function will be called when the first batch of data is read
    //   so we process through here and send the rcvbuf back to process_data

    ASSERT(! contact_up_);

    SPtr_EID sptr_peer_eid = BD_MAKE_EID_NULL();

    if (!find_contact(sptr_peer_eid)) {
        ASSERT(contact_ == NULL);
        log_debug("StreamConvergenceLayer::Connection::"
                  "handle_contact_initiation: failed to find contact");
        break_contact(ContactEvent::CL_ERROR);
        return;
    }

    /*
     * Make sure that the link's remote eid field is properly set.
     */
    LinkRef link = contact_->link();
    if (link->remote_eid() == BD_NULL_EID()) {
        link->set_remote_eid(sptr_peer_eid);
    } else if (link->remote_eid() != sptr_peer_eid) {
        log_warn("handle_contact_initiation: remote eid mismatch: "
                 "link remote eid was set to %s but peer eid is %s",
                 link->remote_eid()->c_str(), sptr_peer_eid->c_str());
    }
    
    /*
     * Finally, we note that the contact is now up.
     */
    contact_up();
}




//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::send_data()
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
    //dzdebug1   ASSERT(towrite > 0);
    if (towrite == 0) {
        return;
    }

    int cc = sock_->write(sendbuf_.start(), towrite);
    if (cc > 0) {
        //log_debug("send_data: wrote %d/%zu bytes from send buffer",
        //          cc, sendbuf_.fullbytes());
        if (stcp_lparams()->hexdump_) {
            oasys::HexDumpBuffer hex;
            hex.append((u_char*)sendbuf_.start(), cc);
            log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }
        
        sendbuf_.consume(cc);
        
        if (sendbuf_.fullbytes() != 0) {
            //log_debug("send_data: incomplete write, setting POLLOUT bit");
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
STCPConvergenceLayer::Connection::send_keepalive()
{
     // Only a Sender should send keepalives for STCP
    if (active_connector_) {
        if (sendbuf_.fullbytes() != 0) {
            //log_debug("send_keepalive: "
            //          "send buffer has %zu bytes queued, suppressing keepalive",
            //          sendbuf_.fullbytes());
            return;
        }
        ASSERT(sendbuf_.tailbytes() > 0);

        u_int32_t nbo_len = 0;
        u_char* bp = (u_char*)sendbuf_.end();
        memcpy(bp, &nbo_len, sizeof(nbo_len));
        sendbuf_.fill(sizeof(nbo_len));

        send_data();
    }
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::recv_data()
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
    if (stcp_lparams()->hexdump_) {
        oasys::HexDumpBuffer hex;
        hex.append((u_char*)recvbuf_.end(), cc);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }
    recvbuf_.fill(cc);
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::process_data()
{
    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    //log_debug("processing up to %zu bytes from receive buffer",
    //          recvbuf_.fullbytes());

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
    while (!delay_reads_to_free_some_storage_ && (recvbuf_.fullbytes() != 0)) {
        if (contact_broken_) return;
        
        //log_debug("recvbuf has %zu full bytes, dispatching to handler routine",
        //          recvbuf_.fullbytes());

        u_int8_t flags = 0;  // dummy flags

        bool ok = handle_data_segment(flags);

        // if there's not enough data in the buffer to handle the
        // message, make sure there's space to receive more
        if (! ok) {
            if (recvbuf_.fullbytes() == recvbuf_.size()) {
                log_warn("process_data: "
                         "%zu byte recv buffer full but too small for STCP msg... "
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
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::Connection::handle_data_segment(u_int8_t flags)
{
    (void) flags;

    // Decode the segment length 
    if (recvbuf_.fullbytes() < 4) {
        //log_debug("handle_data_segment: "
        //          "too few bytes in buffer for 4 byte length header (%zu)",
        //          recvbuf_.fullbytes());
        return false;
    }

    u_int32_t bp_i32;
    memcpy(&bp_i32, recvbuf_.start(), sizeof(bp_i32));

    u_int32_t segment_len = ntohl(bp_i32);
    
    recvbuf_.consume(4);
    
    if (segment_len == 0) {
        return true;
    }


    BundleStore* bs = BundleStore::instance();
    IncomingBundle* incoming = NULL;

    // make sure we're done with the last bundle if we got a new
    // BUNDLE_START flag... note that we need to be careful in
    // case there's not enough data to decode the length of the
    // segment, since we'll be called again
    bool create_new_incoming = true;
    if (!incoming_.empty()) {
        incoming = incoming_.back();

        if ((incoming->bytes_received_ == 0) && incoming->pending_acks_.empty())
        {
            //log_debug("found empty incoming bundle for BUNDLE_START");
            create_new_incoming = false;
            incoming->bundle_complete_ = false;
            incoming->bundle_accepted_ = false;

            if (incoming->payload_bytes_reserved_ > 0) {
                bs->try_reserve_payload_space(incoming->payload_bytes_reserved_);

                incoming->payload_bytes_reserved_ = 0;
            }
        }
        else if (incoming->total_length_ == 0)
        {
            log_err("protocol error: "
                    "got BUNDLE_START before bundle completed");
            break_contact(ContactEvent::CL_ERROR);
            return false;
        }
    }

    // pause processing until there is storage available for this bundle
    uint64_t    tmp_payload_storage_bytes_reserved = 0;
    int32_t     ctr = 0;
    while (!should_stop() && (tmp_payload_storage_bytes_reserved != segment_len))
    {
        if (bs->try_reserve_payload_space(segment_len)) {
            tmp_payload_storage_bytes_reserved = segment_len;
        } else {
            if ((++ctr % 100) == 0) {
                log_warn("Unable to reserve space for bundle for 10 seconds - continuing trying");
            }
            usleep(100000); // 1/10th sec
        }
    }
        
    if (should_stop()) {
        return false;
    }
 
    if (create_new_incoming) {
        //log_debug("got BUNDLE_START segment, creating new IncomingBundle");
        incoming = new IncomingBundle(new Bundle(BundleProtocol::BP_VERSION_UNKNOWN));
        incoming_.push_back(incoming);
    }

    // Note that there may be more than one incoming bundle on the
    // IncomingList, but it's the one at the back that we're reading
    // in data for. Others are waiting for acks to be sent.
    incoming = incoming_.back();
    incoming->payload_bytes_reserved_ = tmp_payload_storage_bytes_reserved;

    SPtr_AckObject ackptr = std::make_shared<AckObject>();
    ackptr->acked_len_ = segment_len;
    incoming->pending_acks_.push_back(ackptr);

    incoming->total_length_ = segment_len;
        
    //log_debug("STCP reading bundle of total length %u",
    //          incoming->total_length_);
    
    recv_segment_todo_ = segment_len;
    return handle_data_todo();
}

} // namespace dtn
