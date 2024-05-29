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

#include <netinet/tcp.h>
#include <sys/poll.h>
#include <stdlib.h>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/HexDumpBuffer.h>

#include "TCPConvergenceLayerV3.h"
#include "IPConvergenceLayerUtils.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

TCPConvergenceLayerV3::TCPLinkParams
    TCPConvergenceLayerV3::default_link_params_(true);

//----------------------------------------------------------------------
TCPConvergenceLayerV3::TCPLinkParams::TCPLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults),
      hexdump_(false),
      local_addr_(INADDR_ANY),
      remote_addr_(INADDR_NONE),
      remote_port_(TCPCL_DEFAULT_PORT),

      recv_bufsize_(0),      //dzdebug
      send_bufsize_(0)
{
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::TCPLinkParams::serialize(oasys::SerializeAction *a)
{
    log_debug_p("TCPLinkParams", "TCPLinkParams::serialize");
    StreamConvergenceLayer::StreamLinkParams::serialize(a);
    a->process("hexdump", &hexdump_);
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
}

//----------------------------------------------------------------------
TCPConvergenceLayerV3::TCPConvergenceLayerV3()
    : StreamConvergenceLayer("TCPConvergenceLayerV3", "tcp3", TCPCL_VERSION)
{
}

//----------------------------------------------------------------------
CLInfo*
TCPConvergenceLayerV3::new_link_params()
{
    return new TCPLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayerV3::init_link(const LinkRef& link,
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
TCPConvergenceLayerV3::parse_link_params(LinkParams* lparams,
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
    p.addopt(new oasys::IntOpt("recv_bufsize", &params->recv_bufsize_));
    p.addopt(new oasys::IntOpt("send_bufsize", &params->send_bufsize_));
    
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
TCPConvergenceLayerV3::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    StreamConvergenceLayer::dump_link(link, buf);
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("local_addr: %s\n", intoa(params->local_addr_));
    buf->appendf("remote_addr: %s\n", intoa(params->remote_addr_));
    buf->appendf("remote_port: %u\n", params->remote_port_);
    buf->appendf("max_inflight_bundles: %u\n", params->max_inflight_bundles_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("break_contact_on_keepalive_fault: %s\n", params->break_contact_on_keepalive_fault_? "true": "false");
    buf->appendf("hexdump: %s\n", params->hexdump_ ? "enabled" : "disabled");
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("TCP Convergence Layer version 3 [%s] - valid Link options:\n\n", name());
    buf.appendf("<nexthop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.appendf("CLA specific options:\n");

    buf.appendf("    local_addr <IP address>            - IP address to bind to (optional; seldom used for a link)\n");
    buf.appendf("    local_port <U16>                   - Port to bind to (optional; seldom used for a link)\n");

    buf.appendf("    keepalive_interval <U16>           - Seconds between sending keepalive packets (default: 10)\n");
    buf.appendf("    break_contact_on_keepalive_fault <Bool>  - Whether to break contact on keepalive fault (default: true)\n");
    buf.appendf("                                                 (ION TCPCLv3 does not actually break contact and \n");
    buf.appendf("                                                  breaking contact may exacerbate the situation. You \n");
    buf.appendf("                                                  could also set the keepalive_interval to zero.)\n");
    buf.appendf("    segment_length <U64>               - Max length of an outgoing data segment  (default: 10MB)\n");
    buf.appendf("    max_inflight_bundles <U32>         - Max number of inflight unacked bundles to allow  (default: 100)\n");

    buf.appendf("    reactive_frag_enabled <Bool>       - Whether to reactively fragment partially sent bundles (default: false)\n");
    buf.appendf("    recvbuf_len <U32>                  - Length of internal receive buffer (not socket buffer) (default: 2048000)\n");
    buf.appendf("    sendbuf_len <U32>                  - Length of internal send buffer (not socket buffer) (default: 2048000)\n");
    buf.appendf("    data_timeout <U32>                 - Milliseconds to wait for socket read before timeout (default: 30000)\n");

    buf.appendf("    test_read_delay <U32>              - (for testing) Milliseconds to delay read between read attempts (default: 0)\n");
    buf.appendf("    test_write_delay <U32>             - (for testing) Milliseconds to delay read between write attempts (default: 0)\n");
    buf.appendf("    test_read_limit <U32>              - (for testing) Max bytes to read from socket at one time (default: 0)\n");
    buf.appendf("    test_write_limit <U32>             - (for testing) Max bytes to write to socket at one time (default: 0)\n");

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
    buf.appendf("Example:\n");
    buf.appendf("link add tcpi3_11 192.168.0.4:4556 ALWAYSON tcp3 \n");
    buf.appendf("    (create a link named \"tcp3_11\" using the tcp3 convergence layer to connect to peer at IP address 192.168.0.4 port 4559)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("TCP Convergence Layer version 4 [%s] - valid Interface options:\n\n", name());
    buf.appendf("CLA specific options:\n");

    buf.appendf("    local_addr <IP address>            - IP address to listen on (default: 0.0.0.0 = all interfaces) \n");
    buf.appendf("    local_port <U16>                   - Port to listen on (default: 4556)\n");
    buf.appendf("    delay_for_tcpcl3 <U32>             - seconds to delay for TCPCL3 contact header from peer (default: 5)\n");
    buf.appendf("    max_rcv_seg_len <U64>              - Max length of an incoming data segment (default: 10 MB)\n");
    buf.appendf("    max_rcv_bundle_size <U64>          - Max length of an incoming bundle (default: 20 GB)\n");

    buf.appendf("    tls_enabled <Bool>                 - Whether to allow TLS connections (default: true)\n");
    buf.appendf("    require_tls <Bool>                 - Whether to require only TLS connections (default: false)\n");
    buf.appendf("    tls_iface_cert_file <Path>         - Path to certificate (PEM) Interface will use for TLS\n"
                "                                         (overrides tls_iface_cert_chain_file)\n");
    buf.appendf("    tls_iface_cert_chain_file <Path>   - Path to chain certificate (PEM) Interface will use for TLS\n");
    buf.appendf("    tls_iface_private_key_file <Path>  - Path to private key (PEM) Interface will use for TLS\n");
    buf.appendf("    tls_iface_verify_cert_file <Path>  - Path to verification certificate(s) (PEM) Interface will use for TLS\n"
                "                                          (overrides tls_iface_verify_certs_dir)\n");
    buf.appendf("    tls_iface_verify_certs_dir <Path>  - Path to directory of verification certificates (PEM) Interface will use for TLS\n");


    buf.appendf("    keepalive_interval <U16>           - Seconds between sending keepalive packets (default: 10)\n");
    buf.appendf("    break_contact_on_keepalive_fault <Bool>  - Whether to break contact on keepalive fault (default: true)\n");
    buf.appendf("                                                 (ION TCPCLv3 does not actually break contact and \n");
    buf.appendf("                                                  breaking contact may exacerbate the situation. You \n");
    buf.appendf("                                                  could also set the keepalive_interval to zero.)\n");
    buf.appendf("    segment_length <U64>               - Max length of an outgoing data segment  (default: 10MB)\n");
    buf.appendf("    max_inflight_bundles <U32>         - Max number of inflight unacked bundles to allow  (default: 100)\n");

    buf.appendf("    reactive_frag_enabled <Bool>       - Whether to reactively fragment partially sent bundles (default: false)\n");
    buf.appendf("    recvbuf_len <U32>                  - Length of internal receive buffer (not socket buffer) (default: 2048000)\n");
    buf.appendf("    sendbuf_len <U32>                  - Length of internal send buffer (not socket buffer) (default: 2048000)\n");
    buf.appendf("    data_timeout <U32>                 - Milliseconds to wait for socket read before timeout (default: 30000)\n");

    buf.appendf("    test_read_delay <U32>              - (for testing) Milliseconds to delay read between read attempts (default: 0)\n");
    buf.appendf("    test_write_delay <U32>             - (for testing) Milliseconds to delay read between write attempts (default: 0)\n");
    buf.appendf("    test_read_limit <U32>              - (for testing) Max bytes to read from socket at one time (default: 0)\n");
    buf.appendf("    test_write_limit <U32>             - (for testing) Max bytes to write to socket at one time (default: 0)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("interface add tcp3_in tcp local_addr=0.0.0.0 local_port=4559\n");
    buf.appendf("    (create an interface named \"tcp3_in\" using the tcp3 convergence layer to receive connections on all network interfaces on port 4559)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayerV3::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayerV3::parse_nexthop(const LinkRef& link, LinkParams* lparams)
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
TCPConvergenceLayerV3::new_connection(const LinkRef& link, LinkParams* p)
{
    (void)link;
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection3(this, params);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayerV3::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    in_addr_t local_addr = INADDR_ANY;
    u_int16_t local_port = TCPCL_DEFAULT_PORT;
    u_int32_t segment_len = TCPConvergenceLayerV3::default_link_params_.segment_length_;
    u_int32_t keepalive_interval = TCPConvergenceLayerV3::default_link_params_.keepalive_interval_;
    int recv_bufsize = 0;   //dzdebug
    int send_bufsize = 0;

    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &local_addr));
    p.addopt(new oasys::UInt16Opt("local_port", &local_port));
    p.addopt(new oasys::UIntOpt("segment_length", &segment_len));
    p.addopt(new oasys::UIntOpt("keepalive_interval", &keepalive_interval));
    p.addopt(new oasys::IntOpt("recv_bufsize", &recv_bufsize));
    p.addopt(new oasys::IntOpt("send_bufsize", &send_bufsize));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }
    
    TCPConvergenceLayerV3::default_link_params_.segment_length_ = segment_len;
    TCPConvergenceLayerV3::default_link_params_.keepalive_interval_ = keepalive_interval;
    TCPConvergenceLayerV3::default_link_params_.recv_bufsize_ = recv_bufsize;
    TCPConvergenceLayerV3::default_link_params_.send_bufsize_ = send_bufsize;

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
TCPConvergenceLayerV3::interface_activate(Interface* iface)
{
    // start listening and then start the thread to loop calling accept()
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    listener->listen();
    listener->start();
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayerV3::interface_down(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::dump_interface(Interface* iface,
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
TCPConvergenceLayerV3::Listener::Listener(TCPConvergenceLayerV3* cl)
    : TCPServerThread("TCPConvergenceLayerV3::Listener",
                      "/dtn/cl/tcp/listener"),
      cl_(cl)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);

    TCPLinkParams *params = new TCPLinkParams(false);
    *params = default_link_params_;

    Connection3* conn = new Connection3(cl_, params, fd, addr, port);
    conn->start();
}

//----------------------------------------------------------------------
TCPConvergenceLayerV3::Connection3::Connection3(TCPConvergenceLayerV3* cl,
                                            TCPLinkParams* params)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayerV3::Connection3",
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
TCPConvergenceLayerV3::Connection3::Connection3(TCPConvergenceLayerV3* cl,
                                            TCPLinkParams* params,
                                            int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayerV3::Connection3",
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


    //dzdebug

    uint32_t bufsize = 128 * 1024;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                     &bufsize, sizeof (bufsize)) < 0)
    {
        log_always("error setting SO_RCVBUF to %d: %s",
             bufsize, strerror(errno));
    } else {
        log_always("Socket RCV buf size set to %u", bufsize);
    }
    
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                     &bufsize, sizeof (bufsize)) < 0)
    {
        log_always("error setting SO_SNDBUF to %d: %s",
             bufsize, strerror(errno));
    } else {
        log_always("Socket SND buf size set to %u", bufsize);
    }
   
     uint32_t enabled = 1;
    if (::setsockopt(fd, IPPROTO_IP, TCP_NODELAY,
                     &enabled, sizeof (enabled)) < 0)
    {
        log_always("error setting TCP_NDELAY to %d: %s",
             bufsize, strerror(errno));
    } else {
        log_always("Socket set to TCP_NODELAY");
    }
    
}

//----------------------------------------------------------------------
TCPConvergenceLayerV3::Connection3::~Connection3()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::serialize(oasys::SerializeAction *a)
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
    a->process("data_timeout", &params->data_timeout_);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::initialize_pollfds()
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
TCPConvergenceLayerV3::Connection3::connect()
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
    log_always("connect: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    ASSERT(contact_ == NULL || contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    int ret = sock_->connect(sock_->remote_addr(), sock_->remote_port());

    if (ret == 0) {
        log_always("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

        initiate_contact();
        
    } else if (ret == -1 && errno == EINPROGRESS) {
        log_always("connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        //log_info("connection attempt to %s:%d failed... %s",
        log_always("connection attempt to %s:%d failed... %s",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::accept()
{
    if (sock_->state() != oasys::IPSocket::ESTABLISHED)
    {
      log_err ("accept: socket not in ESTABLISHED state - closing socket");
      disconnect();
      return;
    }
    
    
    log_debug("accept: got connection from %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    initiate_contact();
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::disconnect()
{
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::handle_poll_activity()
{
    if (sock_pollfd_->revents & POLLHUP) {
        log_err("remote socket closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    if (sock_pollfd_->revents & POLLERR) {
        log_err("error condition on remote socket -- returned POLLERR");
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
                log_always("delayed_connect to %s:%d succeeded",
                          intoa(sock_->remote_addr()), sock_->remote_port());
                initiate_contact();
                
            } else {
                //log_info("connection attempt to %s:%d failed... %s",
                log_always("connection attempt to %s:%d failed... %s",
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
            if (delay_reads_timer_.elapsed_ms() >= 5000) {
                retry_to_accept_incoming_bundle(nullptr);
            }

            if (contact_up_ && ! contact_broken_) {
                check_keepalive();
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
TCPConvergenceLayerV3::Connection3::send_data()
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
        log_err("send_data: remote connection unexpectedly closed: %s",
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayerV3::Connection3::recv_data()
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
    
    u_int toread = recvbuf_.tailbytes();
    if (params_->test_read_limit_ != 0) {
        toread = std::min(toread, params_->test_read_limit_);
    }

    //log_debug("recv_data: draining up to %u bytes into recv buffer...", toread);
    int cc = sock_->read(recvbuf_.end(), toread);

    if (cc < 1) {
        log_err("remote connection unexpectedly closed");
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
