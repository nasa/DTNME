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

//#define _BSD_SOURCE
#include <endian.h>

#include <netinet/tcp.h>
#include <sys/poll.h>
#include <stdlib.h>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/HexDumpBuffer.h>

#ifdef WOLFSSL_TLS_ENABLED
#    include <wolfssl/options.h>
#    include <wolfssl/ssl.h>
#endif // WOLFSSL_TLS_ENABLED

#include "TCPConvergenceLayer.h"
#include "IPConvergenceLayerUtils.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "routing/BundleRouter.h"
#include "storage/BundleStore.h"

namespace dtn {

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPLinkParams::TCPLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults)
{
}

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPLinkParams::TCPLinkParams(const TCPLinkParams& other)
    : StreamLinkParams(false)
{
    *this = other;
}

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPLinkParams::~TCPLinkParams()
{
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::TCPLinkParams::serialize(oasys::SerializeAction *a)
{
    log_debug_p("TCPLinkParams", "TCPLinkParams::serialize");
    StreamConvergenceLayer::StreamLinkParams::serialize(a);

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
    a->process("delay_for_tcpcl3", &delay_for_tcpcl3_);
    a->process("max_rcv_seg_len", &max_rcv_seg_len_);
    a->process("max_rcv_bundle_size", &max_rcv_bundle_size_);
    a->process("tls_enabled", &tls_enabled_);
    a->process("require_tls", &require_tls_);

    a->process("tls_iface_cert_file", &tls_iface_cert_file_);
    a->process("tls_iface_cert_chain_file", &tls_iface_cert_chain_file_);
    a->process("tls_iface_private_key_file", &tls_iface_private_key_file_);
    a->process("tls_iface_verify_cert_file", &tls_iface_verify_cert_file_);
    a->process("tls_iface_verify_certs_dir", &tls_iface_verify_certs_dir_);

    a->process("tls_link_cert_file", &tls_link_cert_file_);
    a->process("tls_link_cert_chain_file", &tls_link_cert_chain_file_);
    a->process("tls_link_private_key_file", &tls_link_private_key_file_);
    a->process("tls_link_verify_cert_file", &tls_link_verify_cert_file_);
    a->process("tls_link_verify_certs_dir", &tls_link_verify_certs_dir_);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::TCPConvergenceLayer()
    : StreamConvergenceLayer("TCPConvergenceLayer", "tcp", TCPCL_VERSION),
      default_link_params_(true)
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
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == nullptr);

    log_always("init_link: adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    TCPLinkParams* params = dynamic_cast<TCPLinkParams *>(new_link_params());
    ASSERT(params != nullptr);

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
    ASSERT(params != nullptr);

    oasys::OptParser p;
    
    p.addopt(new oasys::BoolOpt("hexdump", &params->hexdump_));
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
//    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_)); uses nexthop
//    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_)); uses nexthop
    p.addopt(new oasys::UIntOpt("delay_for_tcpcl3", &params->delay_for_tcpcl3_));
    p.addopt(new oasys::BoolOpt("tls_enabled", &params->tls_enabled_));
    p.addopt(new oasys::BoolOpt("require_tls", &params->require_tls_));
    p.addopt(new oasys::UInt64Opt("max_rcv_seg_len", &params->max_rcv_seg_len_));
    p.addopt(new oasys::UInt64Opt("max_rcv_bundle_size", &params->max_rcv_bundle_size_));

    p.addopt(new oasys::StringOpt("tls_link_cert_file", &params->tls_link_cert_file_));
    p.addopt(new oasys::StringOpt("tls_link_cert_chain_file", &params->tls_link_cert_chain_file_));
    p.addopt(new oasys::StringOpt("tls_link_private_key_file", &params->tls_link_private_key_file_));
    p.addopt(new oasys::StringOpt("tls_link_verify_cert_file", &params->tls_link_verify_cert_file_));
    p.addopt(new oasys::StringOpt("tls_link_verify_certs_dir", &params->tls_link_verify_certs_dir_));

    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        log_err("Error parsing parameters - invalid value: %s", *invalidp);
    } else {
        argc -= count;

        // continue up to parse the parent class
        StreamConvergenceLayer::parse_link_params(lparams, argc, argv, invalidp);
    }

    
    if (params->local_addr_ == INADDR_NONE) {
        params->local_addr_ = INADDR_ANY;
        log_err("invalid local address setting of INADDR_NONE - resetting to default INADDR_ANY");
    }

    if (!params->segment_ack_enabled_) {
        log_always("Warning - forcing segment_ack_enabled to true for TCP CLv4\n");
        params->segment_ack_enabled_ = true; // always true with TCPCLv4
    }

#ifndef WOLFSSL_TLS_ENABLED
    if (params->tls_enabled_) {
        log_err("TLS cannot be enabled unless compiled with the --with-wolfssl option - TLS is disabled");
        params->tls_enabled_ = false;
    }
#endif // WOLFSSL_TLS_ENABLED

    return true; // prevent abort at startup on error
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);

    StreamConvergenceLayer::dump_link(link, buf);
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(link->cl_info());
    ASSERT(params != nullptr);
    
    buf->appendf("peer_tcpcl_version: %u\n", params->peer_tcpcl_version_);
    buf->appendf("local_addr: %s\n", intoa(params->local_addr_));
    buf->appendf("remote_addr: %s\n", intoa(params->remote_addr_));
    buf->appendf("remote_port: %u\n", params->remote_port_);
    buf->appendf("delay_for_tcpcl3: %u\n", params->delay_for_tcpcl3_);
    buf->appendf("max_rcv_seg_len: %" PRIu64 "\n", params->max_rcv_seg_len_);
    buf->appendf("max_rcv_bundle_size: %" PRIu64 "\n", params->max_rcv_bundle_size_);
    buf->appendf("segment_length: %" PRIu64 "\n", params->segment_length_);
    buf->appendf("max_inflight_bundles: %u\n", params->max_inflight_bundles_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("reactive_frag_enabled: %s\n", params->reactive_frag_enabled_ ? "true": "false");

    buf->appendf("tls_link_cert_file: %s\n", params->tls_link_cert_file_.c_str());
    buf->appendf("tls_link_cert_chain_file: %s\n", params->tls_link_cert_chain_file_.c_str());
    buf->appendf("tls_link_private_key_file: %s\n", params->tls_link_private_key_file_.c_str());
    buf->appendf("tls_link_verify_cert_file: %s\n", params->tls_link_verify_cert_file_.c_str());
    buf->appendf("tls_link_verify_certs_dir: %s\n", params->tls_link_verify_certs_dir_.c_str());
    buf->appendf("tls_enabled: %s\n", params->tls_enabled_ ? "true": "false");
    buf->appendf("require_tls: %s\n", params->require_tls_ ? "true": "false");
    buf->appendf("tls_active: %s\n", params->tls_active_ ? "true": "false");
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("TCP Convergence Layer version 4 [%s] - valid Link options:\n\n", name());
    buf.appendf("<nexthop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.appendf("CLA specific options:\n");

    buf.appendf("    delay_for_tcpcl3 <U32>             - seconds to delay for TCPCL3 contact header from peer (default: 5)\n");
    buf.appendf("    max_rcv_seg_len <U64>              - Max length of an incoming data segment (default: 10 MB)\n");
    buf.appendf("    max_rcv_bundle_size <U64>          - Max length of an incoming bundle (default: 2 GB)\n");

    buf.appendf("    tls_enabled <Bool>                 - Whether to allow TLS connections (default: false)\n");
    buf.appendf("    require_tls <Bool>                 - Whether to require only TLS connections (default: false)\n");
    buf.appendf("    tls_link_cert_file <Path>          - Path to certificate (PEM) Link will use for TLS\n"
                "                                         (overrides tls_link_cert_chain_file)\n");
    buf.appendf("    tls_link_cert_chain_file <Path>    - Path to chain certificate (PEM) Link will use for TLS\n");
    buf.appendf("    tls_link_private_key_file <Path>   - Path to private key (PEM) Link will use for TLS\n");
    buf.appendf("    tls_link_verify_cert_file <Path>   - Path to verification certificate(s) (PEM) Link will use for TLS\n"
                "                                         (overrides tls_link_verify_certs_dir)\n");
    buf.appendf("    tls_link_verify_certs_dir <Path>   - Path to directory of verification certificates (PEM) Link will use for TLS\n");

    buf.appendf("    local_addr <IP address>            - IP address to bind to (optional; seldom used for a link)\n");
    buf.appendf("    local_port <U16>                   - Port to bind to (optional; seldom used for a link)\n");

    buf.appendf("    keepalive_interval <U16>           - Seconds between sending keepalive packets (default: 10)\n");
    buf.appendf("    break_contact_on_keepalive_fault <Bool>  - Whether to break contact on keepalive fault (default: true)\n");
    buf.appendf("                                                 (ION stops sending while processing large CFDP files and\n");
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
    buf.append("          (parameters of type <U64> and <Size> can include magnitude character (K, M or G):   125G = 125,000,000,000)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("link add tcp_823 192.168.0.4:4556 ALWAYSON tcp tls_enabled=false\n");
    buf.appendf("    (create a link named \"tcp_823\" using the tcp convergence layer to connect to peer at IP address 192.168.0.4 port 4556\n");
    buf.appendf("     without using TLS for security)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
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
    buf.appendf("                                                 (ION stops sending while processing large CFDP files and\n");
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
    buf.appendf("interface add tcp_in tcp local_addr=0.0.0.0 local_port=4556 tls_enabled=false\n");
    buf.appendf("    (create an interface named \"tcp_in\" using the tcp convergence layer to receive connections on all network interfaces on port 4556\n");
    buf.appendf("     without using TLS for security)\n");
    buf.appendf("\n");
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
    ASSERT(params != nullptr);

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
    ASSERT(params != nullptr);
    return new Connection4(this, params);
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams *>(new_link_params());
    ASSERT(params != nullptr);

    log_debug("adding interface %s", iface->name().c_str());

    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::UIntOpt("delay_for_tcpcl3", &params->delay_for_tcpcl3_));
    p.addopt(new oasys::UInt64Opt("segment_length", &params->segment_length_));
    p.addopt(new oasys::UInt16Opt("keepalive_interval", &params->keepalive_interval_));
    p.addopt(new oasys::BoolOpt("tls_enabled", &params->tls_enabled_));
    p.addopt(new oasys::BoolOpt("require_tls", &params->require_tls_));
    p.addopt(new oasys::UInt64Opt("max_rcv_seg_len", &params->max_rcv_seg_len_));
    p.addopt(new oasys::UInt64Opt("max_rcv_bundle_size", &params->max_rcv_bundle_size_));

    p.addopt(new oasys::StringOpt("tls_iface_cert_file", &params->tls_iface_cert_file_));
    p.addopt(new oasys::StringOpt("tls_iface_cert_chain_file", &params->tls_iface_cert_chain_file_));
    p.addopt(new oasys::StringOpt("tls_iface_private_key_file", &params->tls_iface_private_key_file_));
    p.addopt(new oasys::StringOpt("tls_iface_verify_cert_file", &params->tls_iface_verify_cert_file_));
    p.addopt(new oasys::StringOpt("tls_iface_verify_certs_dir", &params->tls_iface_verify_certs_dir_));


    const char* invalidp = nullptr;
    int count = p.parse_and_shift(argc, argv, &invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;
    
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }

    // continue up to parse the parent class
    bool parse_result = StreamConvergenceLayer::parse_link_params(params, argc, argv, &invalidp);

    if (!params->segment_ack_enabled_) {
        log_always("Warning - forcing segment_ack_enabled to true for TCP CLv4\n");
        params->segment_ack_enabled_ = true; // always true with TCPCLv4
    }

    // check that the local interface / port are valid
    if (params->local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    if (!parse_result) {
        log_err("error parsing interface options: invalid option '%s'", invalidp);
        return false;
    }

#ifndef WOLFSSL_TLS_ENABLED
    if (params->tls_enabled_) {
        log_err("TLS cannot be enabled unless compiled with the --with-wolfssl option - TLS is disabled");
        params->tls_enabled_ = false;
    }
#endif // WOLFSSL_TLS_ENABLED


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
    ASSERT(listener != nullptr);
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
    ASSERT(listener != nullptr);
    listener->dump_interface(buf);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Listener::Listener(TCPConvergenceLayer* cl, TCPLinkParams* params)
    : TCPServerThread("TCPConvergenceLayer::Listener",
                      "/dtn/cl/tcp/listener"),
      cl_(cl),
      params_(params)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Listener::~Listener()
{
    delete params_;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Listener::dump_interface(oasys::StringBuffer* buf)
{
    buf->appendf("\tlocal_addr: %s local_port: %d segment_ack_enabled: %s, tls_enabled: %s\n",
                 intoa(local_addr()), local_port(), 
                 params_->segment_ack_enabled_ ? "true" : "false",
                 params_->tls_enabled_ ? "true" : "false");
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);

    TCPLinkParams *params = new TCPLinkParams(false);
    *params = *params_;

    Connection0* conn = new Connection0(cl_, params, fd, addr, port);
    conn->start();
}






//----------------------------------------------------------------------
TCPConvergenceLayer::Connection0::Connection0(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection0",
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
TCPConvergenceLayer::Connection0::Connection0(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params,
                                            int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection0",
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
    sock_->params_.recv_bufsize_ = 128 * 1024;
    sock_->params_.send_bufsize_ = 128 * 1024;
    sock_->configure();
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection0::~Connection0()
{
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::serialize(oasys::SerializeAction *a)
{
    (void) a;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;
    
    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;
    
    poll_timeout_ = 5000; // hard coded 5 seconds
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::connect()
{
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::accept()
{
    if (sock_->state() != oasys::IPSocket::ESTABLISHED)
    {
      log_err ("accept: socket not in ESTABLISHED state - closing socket");
      disconnect();
      return;
    }
    
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::disconnect()
{
    if (sock_ != nullptr) {
        if (sock_->state() != oasys::IPSocket::CLOSED) {
            sock_->close();
        }
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::handle_contact_initiation()
{
    ASSERT(! contact_up_);

    /*
     * First check for valid magic number.
     */
    uint32_t magic = 0;
    size_t len_needed = sizeof(magic) + 1; // + 1 to get to the TCPCL version byte

    if (recvbuf_.fullbytes() < len_needed) {
        log_debug("handle_contact_initiation: not enough data received "
                  "(need > %zu, got %zu)",
                  len_needed, recvbuf_.fullbytes());
        return;
    }

    contact_initiation_received_ = true ;

    memcpy(&magic, recvbuf_.start(), sizeof(magic));
    magic = ntohl(magic);
   
    if (magic != MAGIC) {
        log_err("remote sent magic number 0x%.8x, expected 0x%.8x "
                 "-- disconnecting.", magic, MAGIC);
        break_contact(ContactEvent::CL_ERROR);
        oasys::Breaker::break_here();
        return;
    }

    peer_tcpcl_version_ = recvbuf_.start()[4];
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::process_data()
{
    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    handle_contact_initiation();
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::handle_poll_activity()
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
    
    // finally, check for incoming data
    if (sock_pollfd_->revents & POLLIN) {
        recv_data();
        process_data();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::run()
{
    char threadname[16] = "TcpCLConnectn0";
    pthread_setname_np(pthread_self(), threadname);

    set_flag(oasys::Thread::DELETE_ON_EXIT);

    initialize_pollfds();
    if (contact_broken_) {
        log_debug("contact_broken set during initialization");
        return;
    }

    peer_tcpcl_version_ = 4; 
    contact_initiation_received_ = false;

    TCPLinkParams* tcpcl_params = dynamic_cast<TCPLinkParams*>(params_);
    uint32_t ms_to_delay = tcpcl_params->delay_for_tcpcl3_ * 1000;

    oasys::Time delay_for_tcpcl3_time = oasys::Time::now();
    

    while (!contact_initiation_received_ && (delay_for_tcpcl3_time.elapsed_ms() < ms_to_delay)) {
        int timeout = 5000; // 5 seconds max wait
        pollfds_[0].revents = 0;

        if (contact_broken_) {
            log_debug("contact_broken set, exiting main loop");
            return;
        }

        int cc = oasys::IO::poll_multiple(pollfds_, num_pollfds_,
                                          timeout, nullptr, logpath_);
                                          
        // check again here for contact broken since we don't want to
        // act on the poll result if the contact is broken
        if (contact_broken_) {
            log_debug("contact_broken set, exiting main loop");
            return;
        }
        
        if (cc > 0)
        {
            handle_poll_activity();
            usleep(1);  //give other threads a chance
        }
        else if (cc != oasys::IOTIMEOUT)
        {
            log_err("unexpected return from poll_multiple: %d", cc);
            break_contact(ContactEvent::BROKEN);
            return;
        }
    }


    // Either no data received in 5 seconds so we assume TCPCL version 4
    // or we have received the contact header which provided the peer's version
    if (BundleDaemon::instance()->shutting_down()) {
        return;
    }


    TCPConvergenceLayer* tcp_cl = dynamic_cast<TCPConvergenceLayer*>(cl_);

    ASSERT(tcpcl_params != nullptr);
    ASSERT(tcp_cl != nullptr);

    if (peer_tcpcl_version_ == 3) {
        Connection3* conn = new Connection3(tcp_cl, tcpcl_params, sock_, nexthop_, recvbuf_);
        conn->start();
    } else {
        Connection4* conn = new Connection4(tcp_cl, tcpcl_params, sock_, nexthop_, recvbuf_);
        conn->start();
    }

    sock_ = nullptr;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::recv_data()
{
    // Connection0 only reads in first 5 bytes which is enough
    // to determine the TCP CL version of the connecting peer
    int toread = 5 - recvbuf_.fullbytes();  // Connection0 only reads in first 5 bytes which is

    if (toread <= 0) return;

    int cc = sock_->read(recvbuf_.end(), toread);

    if (cc < 1) {
        log_err("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    recvbuf_.fill(cc);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection0::break_contact(ContactEvent::reason_t reason)
{
    (void) reason;

    if (breaking_contact_) {
        return;
    }
    breaking_contact_ = true;
    
    // just close the socket at this stage
        
    contact_broken_ = true;

    disconnect();
}








//----------------------------------------------------------------------
TCPConvergenceLayer::Connection3::Connection3(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params,
                                            oasys::TCPClient* sock, 
                                            std::string& nexthop)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection3",
                                         cl->logpath(), cl, params,
                                         true /* call connect() */)
{
    base_logpath_ = cl->logpath();
    base_logpath_.append("/conn/out/tcp4to3/%s/%s");

    // use temp logpath until we have a good connection
    logpathf("%s/conn/out/tcp4to3/%p", cl->logpath(), this);

    cmdqueue_.logpathf("%s/cmdq", logpath_);

    cl_version_ = 3;
    params->peer_tcpcl_version_ = 3;

    // set up the base class' nexthop parameter
    set_nexthop(nexthop.c_str());
    
    // the actual socket
    sock_ = sock;

    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection3::Connection3(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params,
                                            oasys::TCPClient* sock, 
                                            std::string& nexthop,
                                            oasys::StreamBuffer& initial_recvbuf)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection3",
                                         cl->logpath(), cl, params,
                                         false /* call accept() */)
{
    base_logpath_ = cl->logpath();
    base_logpath_.append("/conn/in/tcp4to3/%s/%s");

    // use temp logpath until we have a good connection
    logpathf("%s/conn/in/tcp4to3/%p", cl->logpath(), this);

    cmdqueue_.logpathf("%s/cmdq", logpath_);

    cl_version_ = 3;
    params->peer_tcpcl_version_ = 3;

    // set up the base class' nexthop parameter
    set_nexthop(nexthop.c_str());
    
    sock_ = sock;

    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);

    // load the first 5 bytes that were read in by Connection0 
    memcpy(recvbuf_.start(), initial_recvbuf.start(), initial_recvbuf.fullbytes());
    recvbuf_.fill(initial_recvbuf.fullbytes());
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection3::~Connection3()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection3::serialize(oasys::SerializeAction *a)
{
    TCPLinkParams *params = tcp_lparams();
    if (! params) return;

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
TCPConvergenceLayer::Connection3::initialize_pollfds()
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
TCPConvergenceLayer::Connection3::connect()
{
    if (sock_->state() == oasys::IPSocket::ESTABLISHED) {
        if (!contact_initiated_) {
            initiate_contact();
        }
        return;
    }

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
    ASSERT(contact_ == NULL || contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    int ret = sock_->connect(sock_->remote_addr(), sock_->remote_port());

    if (ret == 0) {
        log_debug("Connection3::connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

        initiate_contact();
        
    } else if (ret == -1 && errno == EINPROGRESS) {
        log_debug("Connection3::connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("Connection3::connection attempt to %s:%d failed... %s",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection3::accept()
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
TCPConvergenceLayer::Connection3::disconnect()
{
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection3::handle_poll_activity()
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
            note_data_rcvd();   // prevent keep_alive timeout
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
TCPConvergenceLayer::Connection3::run()
{
    char threadname[16] = "TcpCLConnectn3";
    pthread_setname_np(pthread_self(), threadname);

    CLConnection::run();
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection3::send_data()
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
TCPConvergenceLayer::Connection3::recv_data()
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











//----------------------------------------------------------------------
TCPConvergenceLayer::Connection4::Connection4(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection4",
                                         cl->logpath(), cl, params,
                                         true /* call connect() */)
{
    base_logpath_ = cl->logpath();
    base_logpath_.append("/conn/out/tcp4/%s/%s");

    // use temp logpath until we have a good connection
    logpathf("%s/conn/out/tcp4/%p", cl->logpath(), this);

    cmdqueue_.logpathf("%s/cmdq", logpath_);

    cl_version_ = 4;
    params->peer_tcpcl_version_ = 4;


    // set up the base class' nexthop parameter
    oasys::StringBuffer nexthop("%s:%d",
                                intoa(params->remote_addr_),
                                params->remote_port_);
    set_nexthop(nexthop.c_str());
    
    // the actual socket
    sock_ = new oasys::TCPClient(logpath_);

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);

    sock_->params_.recv_bufsize_ = 128 * 1024;
    sock_->params_.send_bufsize_ = 128 * 1024;
    sock_->set_nonblocking(true);
    sock_->init_socket();
   
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
TCPConvergenceLayer::Connection4::Connection4(TCPConvergenceLayer* cl,
                                            TCPLinkParams* params,
                                            oasys::TCPClient* sock, 
                                            std::string& nexthop,
                                            oasys::StreamBuffer& initial_recvbuf)
    : StreamConvergenceLayer::Connection("TCPConvergenceLayer::Connection4",
                                         cl->logpath(), cl, params,
                                         false /* call accept() */)
{
    base_logpath_ = cl->logpath();
    base_logpath_.append("/conn/in/tcp4/%s/%s");

    // use temp logpath until we have a good connection
    logpathf("%s/conn/out/tcp4/%p", cl->logpath(), this);

    cmdqueue_.logpathf("%s/cmdq", logpath_);

    
    cl_version_ = 4;
    params->peer_tcpcl_version_ = 4;

    // set up the base class' nexthop parameter
    set_nexthop(nexthop.c_str());
    
    sock_ = sock;

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);

    // load the first 5 bytes that were read in by Connection0 
    memcpy(recvbuf_.start(), initial_recvbuf.start(), initial_recvbuf.fullbytes());
    recvbuf_.fill(initial_recvbuf.fullbytes());
}

//----------------------------------------------------------------------
TCPConvergenceLayer::Connection4::~Connection4()
{
    delete sock_;

#ifdef WOLFSSL_TLS_ENABLED
    if (wolfssl_handle_) {
        wolfSSL_free(wolfssl_handle_);
        wolfssl_handle_ = nullptr;
    }

    if (wolfssl_ctx_) {
        wolfSSL_CTX_free(wolfssl_ctx_);
        wolfssl_ctx_ = nullptr;
    }
#endif // WOLFSSL_TLS_ENABLED
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::serialize(oasys::SerializeAction *a)
{
    TCPLinkParams *params = tcp_lparams();
    if (! params) return;

    a->process("local_addr", oasys::InAddrPtr(&params->local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&params->remote_addr_));
    a->process("remote_port", &params->remote_port_);
    a->process("delay_for_tcpcl3", &params->delay_for_tcpcl3_);
    a->process("tls_enabled", &params->tls_enabled_);
    a->process("require_tls", &params->require_tls_);
    a->process("max_rcv_seg_len", &params->max_rcv_seg_len_);
    a->process("max_rcv_bundle_size", &params->max_rcv_bundle_size_);

    a->process("tls_iface_cert_file", &params->tls_iface_cert_file_);
    a->process("tls_iface_cert_chain_file", &params->tls_iface_cert_chain_file_);
    a->process("tls_iface_private_key_file", &params->tls_iface_private_key_file_);
    a->process("tls_iface_verify_cert_file", &params->tls_iface_verify_cert_file_);
    a->process("tls_iface_verify_certs_dir", &params->tls_iface_verify_certs_dir_);

    a->process("tls_link_cert_file", &params->tls_link_cert_file_);
    a->process("tls_link_cert_chain_file", &params->tls_link_cert_chain_file_);
    a->process("tls_link_private_key_file", &params->tls_link_private_key_file_);
    a->process("tls_link_verify_cert_file", &params->tls_link_verify_cert_file_);
    a->process("tls_link_verify_certs_dir", &params->tls_link_verify_certs_dir_);


    // from StreamLinkParams
    a->process("segment_ack_enabled", &params->segment_ack_enabled_);
    a->process("negative_ack_enabled", &params->negative_ack_enabled_);
    a->process("keepalive_interval", &params->keepalive_interval_);
    a->process("segment_length", &params->segment_length_);

    // from LinkParams
    a->process("reactive_frag_enabled", &params->reactive_frag_enabled_);
    a->process("sendbuf_len", &params->sendbuf_len_);
    a->process("recvbuf_len", &params->recvbuf_len_);
    a->process("data_timeout", &params->data_timeout_);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;
    
    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(params_);
    ASSERT(params != nullptr);
    
    poll_timeout_ = params->data_timeout_;
    
    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::connect()
{
    if (sock_->state() == oasys::IPSocket::ESTABLISHED) {
        if (!contact_initiated_) {
            initiate_contact();
        }
        return;
    }

    contact_initiation_received_ = false;
    contact_initiated_ = false;

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
    ASSERT(params != nullptr);
    sock_->set_remote_addr(params->remote_addr_);
    sock_->set_remote_port(params->remote_port_);

    // start a connection to the other side... in most cases, this
    // returns EINPROGRESS, in which case we wait for a call to
    // handle_poll_activity
    ASSERT(contact_ == nullptr || contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    int ret = sock_->connect(sock_->remote_addr(), sock_->remote_port());

    if (ret == 0) {
        log_debug("Connection4::connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

        //XXX/dz - initiate_contact(); is now delayed to allow for possible receipt of a TCPCL3 contact header
        
    } else if (ret == -1 && errno == EINPROGRESS) {
        log_debug("Connection4::connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("Connection4::connection attempt to %s:%d failed... %s",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::accept()
{
    if (sock_->state() != oasys::IPSocket::ESTABLISHED)
    {
      log_err ("accept: socket not in ESTABLISHED state - closing socket");
      disconnect();
      return;
    }
    
    
    log_debug("accept: got connection from %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::disconnect()
{
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::initiate_contact()
{
    // format the contact header
    TCPv4ContactHeader contacthdr;
    contacthdr.magic   = htonl(MAGIC);
    contacthdr.version = TCPCL_VERSION;
    
    contacthdr.flags = 0;

    TCPLinkParams* params = tcp_lparams();
    
    if (params->tls_enabled_) {
        contacthdr.flags |= TLS_CAPABLE;
    }
    
    // copy the contact header into the send buffer
    ASSERT(sendbuf_.fullbytes() == 0);
    if (sendbuf_.tailbytes() < sizeof(contacthdr)) {
        log_warn("send buffer too short for TCP CLv4 contact header: %zu < needed %zu",
                 sendbuf_.tailbytes(), sizeof(contacthdr));
        sendbuf_.reserve(sizeof(contacthdr));
    }
    
    memcpy(sendbuf_.end(), &contacthdr, sizeof(contacthdr));
    sendbuf_.fill(sizeof(contacthdr));
    

    contact_initiated_ = true;

    // drain the send buffer
    note_data_sent();
    send_data();

}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::handle_contact_initiation()
{
    ASSERT(! contact_up_);

    /*
     * First check for valid magic number.
     */
    uint32_t magic = 0;
    size_t len_needed = sizeof(magic);
    if (recvbuf_.fullbytes() < len_needed) {
        log_debug("handle_contact_initiation: not enough data received for magic number"
                  "(need > %zu, got %zu)",
                  len_needed, recvbuf_.fullbytes());
        return;
    }

    contact_initiation_received_ = true ;

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
    TCPv4ContactHeader incoming_contacthdr;
    len_needed = sizeof(incoming_contacthdr);
    if (recvbuf_.fullbytes() < len_needed) {
        log_debug("handle_contact_initiation: not enough data received for contact header"
                  "(need > %zu, got %zu)",
                  len_needed, recvbuf_.fullbytes());
        return;
    }

    /*
     * Ok, we have enough data, parse the contact header.
     */
    memcpy(&incoming_contacthdr, recvbuf_.start(), sizeof(incoming_contacthdr));

    incoming_contacthdr.magic = ntohl(incoming_contacthdr.magic);

    recvbuf_.consume(sizeof(incoming_contacthdr));


    // server side needs to send its contact header in response
    if (!contact_initiated_) {
        initiate_contact();
    }

    /*
     * In this implementation, we can't handle other versions than our
     * own. We will break contact after sending back our header with the
     * version we support.
     */
    TCPLinkParams* params = tcp_lparams();
    uint8_t cl_version = TCPCL_VERSION;

    if (cl_version != incoming_contacthdr.version) {
        log_err("remote sent version %d, expected version %d "
                 "-- disconnecting.", incoming_contacthdr.version, cl_version);
        break_contact(ContactEvent::CL_VERSION);
        return;
    }

    /*
     * Now do parameter negotiation.
     */
    bool use_tls = (incoming_contacthdr.flags & TLS_CAPABLE) && params->tls_enabled_;

    if (use_tls) {
        if (!active_connector_) {
            if (!start_tls_server())
            {
                break_contact(ContactEvent::CL_ERROR);
                return;
            }
        } else {
            if (!start_tls_client()) {
                break_contact(ContactEvent::CL_ERROR);
                return;
            }
        }
    } else {
        if (params->require_tls_) {
            // aborting because this side requires TLS and other side does not support it
            log_err("Aborting conenction from TCP CL peer that does not support TLS");

            break_contact(ContactEvent::CL_ERROR);
            return;
        }
    }

    initiate_session_negotiation();

    if (recvbuf_.fullbytes() > 0) {
        // sometimes the contact initiation and the session negotiation are received 
        // together and there is no POLLIN to trigger processing the session negotiation
        // so we are doing that here
        process_data();
    }
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::start_tls_server()
{
#ifdef WOLFSSL_TLS_ENABLED
    wolfSSL_Init();

    wolfssl_ctx_ = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
    if (wolfssl_ctx_ == nullptr) {
        log_err("Error creating wolfSSL context for TLSv3 server");
        return false;
    }

    TCPLinkParams* params = tcp_lparams();

    // Load server certificates into WOLFSSL_CTX
    if (!params->tls_iface_cert_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_certificate_file(wolfssl_ctx_, 
                                           params->tls_iface_cert_file_.c_str(), 
                                           SSL_FILETYPE_PEM)) {
            log_err("Error loading server certificate file: %s", 
                    params->tls_iface_cert_file_.c_str());
            return false;
        }
    } else if (!params->tls_iface_cert_chain_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_certificate_chain_file(wolfssl_ctx_, 
                                           params->tls_iface_cert_chain_file_.c_str())) {
            log_err("Error loading server certificate chain file: %s", 
                    params->tls_iface_cert_chain_file_.c_str());
            return false;
        }
    }


    // Load server key into WOLFSSL_CTX
    if (!params->tls_iface_private_key_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_PrivateKey_file(wolfssl_ctx_, 
                                            params->tls_iface_private_key_file_.c_str(), 
                                            SSL_FILETYPE_PEM)) {
            log_err("Error loading server private key file: %s", 
                    params->tls_iface_private_key_file_.c_str());
            return false;
        }
    }


    // create the handle for this connection
    wolfssl_handle_ = wolfSSL_new(wolfssl_ctx_);
    if (wolfssl_handle_ == nullptr) {
        log_err("Error creating server wolfSSL handle object");
        return false;
    }

    int status;

    // configure to ask for the client side certificate
    wolfSSL_CTX_set_verify(wolfssl_ctx_, SSL_VERIFY_PEER, 0);

    // TODO: specify the allowed ciphers?   wolfSSL_CTX_set_cipher_list

    // Load the CA certificate(s) to verify the peer cert (single file or a directory)
    // (a file can contain 1 or more certs)
    if (!params->tls_iface_verify_cert_file_.empty()) {
        wolfSSL_set_verify(wolfssl_handle_, SSL_VERIFY_PEER, nullptr);
        status = wolfSSL_CTX_load_verify_locations(wolfssl_ctx_, 
                                    params->tls_iface_verify_cert_file_.c_str(), nullptr);
        if (status != SSL_SUCCESS) {
            log_err("Error loading verification certificate file: %s", 
                    params->tls_iface_verify_cert_file_.c_str());
            return false;
        }
    } else if (!params->tls_iface_verify_certs_dir_.empty()) {
        wolfSSL_set_verify(wolfssl_handle_, SSL_VERIFY_PEER, nullptr);
        status = wolfSSL_CTX_load_verify_locations(wolfssl_ctx_, nullptr, 
                                                   params->tls_iface_verify_certs_dir_.c_str());
        if (status != SSL_SUCCESS) {
            log_err("Error loading verification certificates from directory: %s", 
                    params->tls_iface_verify_certs_dir_.c_str());
            return false;
        }
    }


    // attach the non-blocking socket
    wolfSSL_set_fd(wolfssl_handle_, sock_->fd());
    wolfSSL_set_using_nonblock(wolfssl_handle_, 1);

    // wait for the TLS handhaking to complete
    status = wolfSSL_accept_TLSv13(wolfssl_handle_);
    while (status != SSL_SUCCESS) {
        if (!wolfSSL_want_read(wolfssl_handle_) && !wolfSSL_want_write(wolfssl_handle_)) {
            int err = wolfSSL_get_error(wolfssl_handle_, 0);
            char buf[80];
            wolfSSL_ERR_error_string(err, buf);
            log_err("Error establishing TLS connection: %s", buf);
            return false;
        }

        usleep(100000);
        status = wolfSSL_accept_TLSv13(wolfssl_handle_);
    }


    tls_active_ = true;
    params->tls_active_ = true;  // indication for "link dump" reporting


    WOLFSSL_X509* peer_cert = wolfSSL_get_peer_certificate(wolfssl_handle_);
    if (peer_cert) {
        int ctr = 0;
        char* altname;
        while ( (altname = wolfSSL_X509_get_next_altname(peer_cert)) != nullptr) {
            ++ctr;
            log_always("Peer certificate - altname = %s", altname);
        }
        wolfSSL_FreeX509(peer_cert);

        if (ctr == 0) {
            log_always("No alternate names available in peer certificate");
        }
    } else {
        log_always("Unable to retrieve the peer certificate");
    }

    return true;

#else

    return false;

#endif // WOLFSSL_TLS_ENABLED
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::start_tls_client()
{
#ifdef WOLFSSL_TLS_ENABLED
    wolfSSL_Init();

    wolfssl_ctx_ = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    if (wolfssl_ctx_ == nullptr) {
        log_err("Error creating wolfSSL context for TLSv3 client");
        return false;
    }

    TCPLinkParams* params = tcp_lparams();

    // TODO: specify the allowed ciphers?   wolfSSL_CTX_set_cipher_list

    // Load client certificate into WOLFSSL_CTX
    if (!params->tls_link_cert_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_certificate_file(wolfssl_ctx_, 
                                           params->tls_link_cert_file_.c_str(), 
                                           SSL_FILETYPE_PEM)) {
            log_err("Error loading client certificate file: %s", 
                    params->tls_link_cert_file_.c_str());
            return false;
        }
    } else if (!params->tls_link_cert_chain_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_certificate_chain_file(wolfssl_ctx_, 
                                           params->tls_link_cert_chain_file_.c_str())) {
            log_err("Error loading client certificate chain file: %s", 
                    params->tls_link_cert_chain_file_.c_str());
            return false;
        }
    }


    // Load client key into WOLFSSL_CTX
    if (!params->tls_link_private_key_file_.empty()) {
        if (SSL_SUCCESS != wolfSSL_CTX_use_PrivateKey_file(wolfssl_ctx_, 
                                           params->tls_link_private_key_file_.c_str(), 
                                           SSL_FILETYPE_PEM)) {
            log_err("Error loading client private key file: %s", 
                    params->tls_link_private_key_file_.c_str());
            return false;
        }
    }


    // create the handle for this connection
    wolfssl_handle_ = wolfSSL_new(wolfssl_ctx_);
    if (wolfssl_handle_ == nullptr) {
        log_err("Error creating client wolfSSL handle object");
        return false;
    }

    int status;

    // Load the CA certificate(s) to verify the peer cert (single file or a directory)
    // (a file can contain 1 or more certs)
    if (!params->tls_link_verify_cert_file_.empty()) {
        wolfSSL_set_verify(wolfssl_handle_, SSL_VERIFY_PEER, nullptr);
        status = wolfSSL_CTX_load_verify_locations(wolfssl_ctx_, 
                                    params->tls_link_verify_cert_file_.c_str(), nullptr);
        if (status != SSL_SUCCESS) {
            log_err("Error loading verification certificate file: %s", 
                    params->tls_link_verify_cert_file_.c_str());
            return false;
        }
    } else if (!params->tls_link_verify_certs_dir_.empty()) {
        wolfSSL_set_verify(wolfssl_handle_, SSL_VERIFY_PEER, nullptr);
        status = wolfSSL_CTX_load_verify_locations(wolfssl_ctx_, nullptr, 
                                                   params->tls_link_verify_certs_dir_.c_str());
        if (status != SSL_SUCCESS) {
            log_err("Error loading verification certificates from directory: %s", 
                    params->tls_link_verify_certs_dir_.c_str());
            return false;
        }
    }


    // attach the non-blocking socket
    wolfSSL_set_fd(wolfssl_handle_, sock_->fd());
    wolfSSL_set_using_nonblock(wolfssl_handle_, 1);

    status = wolfSSL_connect_TLSv13(wolfssl_handle_);
    while (status != SSL_SUCCESS) {
        if (!wolfSSL_want_read(wolfssl_handle_) && !wolfSSL_want_write(wolfssl_handle_)) {
            int err = wolfSSL_get_error(wolfssl_handle_, 0);
            char buf[80];
            wolfSSL_ERR_error_string(err, buf);
            log_err("Error establishing TLS connection: %s", buf);
            return false;
        }

        usleep(100000);
        status = wolfSSL_connect_TLSv13(wolfssl_handle_);
    }


    tls_active_ = true;
    params->tls_active_ = true;  // indication for "link dump" reporting
    

    WOLFSSL_X509* peer_cert = wolfSSL_get_peer_certificate(wolfssl_handle_);
    if (peer_cert) {
        char* subject = wolfSSL_X509_get_subjectCN(peer_cert);
        log_always("Peer certificate - subject = %s", subject);

        int ctr = 0;
        char* altname;
        while ( (altname = wolfSSL_X509_get_next_altname(peer_cert)) != nullptr) {
            ++ctr;
            log_always("Peer certificate - altname = %s", altname);
        }
        wolfSSL_FreeX509(peer_cert);

        if (ctr == 0) {
            log_always("No alternate names available in peer certificate");
        }
    } else {
        log_always("Unable to retrieve the peer certificate");
    }

    return true;

#else

    return false;

#endif // WOLFSSL_TLS_ENABLED
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::initiate_session_negotiation()
{
    size_t local_eid_len;

    TCPLinkParams* params = tcp_lparams();
    BundleDaemon* bd = BundleDaemon::instance();

    if(!bd->params_.announce_ipn_)
    {
        local_eid_len = bd->local_eid()->length();
    } else {
        local_eid_len = bd->local_eid_ipn()->length();
    }


    // calculate size of the SESS_INIT message (no session extension items)
    size_t need_bytes = 1 + 2 + 8 + 8 + 2 + local_eid_len + 4 + 0;

    if (sendbuf_.tailbytes() < need_bytes) {
        log_warn("send buffer too short for SESS_INIT msg: %zu < needed %zu",
                 sendbuf_.tailbytes(), need_bytes);
        sendbuf_.reserve(need_bytes);
    }
  
    // Message Header
    *sendbuf_.end() = TCPV4_MSG_TYPE_SESS_INIT;
    sendbuf_.fill(1);

    // Keepalive Interval
    uint16_t u16 = htons(params->keepalive_interval_);
    memcpy(sendbuf_.end(), &u16, sizeof(u16));
    sendbuf_.fill(sizeof(u16));
   
    // Segment MRU
    uint64_t u64 = htobe64(params->max_rcv_seg_len_);
    memcpy(sendbuf_.end(), &u64, sizeof(u64));
    sendbuf_.fill(sizeof(u64));

    // Transfer MRU
    u64 = htobe64(params->max_rcv_bundle_size_);
    memcpy(sendbuf_.end(), &u64, sizeof(u64));
    sendbuf_.fill(sizeof(u64));

    // Node ID length
    u16 = htons(local_eid_len);
    memcpy(sendbuf_.end(), &u16, sizeof(u16));
    sendbuf_.fill(sizeof(u16));

    // the Node ID itself
    if(!bd->params_.announce_ipn_)
    {
        memcpy(sendbuf_.end(), bd->local_eid()->c_str(), local_eid_len);
    } else {
        memcpy(sendbuf_.end(), bd->local_eid_ipn()->c_str(), local_eid_len);
    }
    sendbuf_.fill(local_eid_len);

    // Session Exension Items Length (none)
    uint32_t u32 = 0;
    memcpy(sendbuf_.end(), &u32, sizeof(u32));
    sendbuf_.fill(sizeof(u32));

    // Session Extension Items (none)

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
}


//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::handle_session_negotiation()
{
    if (contact_up_) {
        log_err("Received SESS_INIT message after session already established - closing connection");

        break_contact(ContactEvent::CL_ERROR);   // TODO: TCPv4 specifies "Contact Failure"...
        return true;
    }

    // do we have enough bytes to read the length of the Node ID?
    size_t need_bytes = 1 + 2 + 8 + 8 + 2;    //  + local_eid_len + 4 + 0;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_session_negotiation: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type;
    uint16_t keepalive_interval;
    uint64_t max_seg_len;
    uint64_t max_bundle_size;
    uint16_t node_id_len;
    const char* node_id_ptr;
    uint32_t extensions_len;
    u_char* extensions_ptr;


    u_char* buf = (u_char*) recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_SESS_INIT);
    ++buf;

    memcpy(&keepalive_interval, buf, sizeof(keepalive_interval));
    keepalive_interval = ntohs(keepalive_interval);
    buf += sizeof(keepalive_interval);

    memcpy(&max_seg_len, buf, sizeof(max_seg_len));
    max_seg_len = be64toh(max_seg_len);
    buf += sizeof(max_seg_len);

    memcpy(&max_bundle_size, buf, sizeof(max_bundle_size));
    max_bundle_size = be64toh(max_bundle_size);
    buf += sizeof(max_bundle_size);

    memcpy(&node_id_len, buf, sizeof(node_id_len));
    node_id_len = ntohs(node_id_len);
    buf += sizeof(node_id_len);

    // determine if we have enough data to read the length of the Session Extension Items
    need_bytes += node_id_len + 4;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_session_negotiation: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    node_id_ptr = (const char*) buf;
    buf += node_id_len;

    memcpy(&extensions_len, buf, sizeof(extensions_len));
    extensions_len = ntohl(extensions_len);
    buf += sizeof(extensions_len);

    // determine if we have enough data to read n the Session Extension Items
    need_bytes += extensions_len;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_session_negotiation: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    extensions_ptr = buf;
    //TODO: parse the Session Extension Items even though we don't recognize any at this time
    //      - check for critical flag and abort since we don't support it
    //parse_session_extension_items(extensions_ptr, extensions_len);
    (void) extensions_ptr;


    // advance pointer in the recvbuf
    recvbuf_.consume(need_bytes);



    /*
     * parse the peer node's eid and give it to the base
     * class to handle (i.e. by linking us to a Contact if we don't
     * have one).
     */
    std::string eid_str(node_id_ptr, node_id_len);
    SPtr_EID sptr_peer_eid = BD_MAKE_EID(eid_str);
    if (!sptr_peer_eid->valid()) {
        log_err("protocol error: invalid endpoint id '%s' (len %llu)",
                sptr_peer_eid->c_str(), U64FMT(node_id_len));
        break_contact(ContactEvent::CL_ERROR);
        return true;
    }

    if (!find_contact(sptr_peer_eid)) {
        ASSERT(contact_ == nullptr);
        log_debug("TCPConvergenceLayer::Connection4::"
                  "handle_contact_initiation: failed to find contact");
        break_contact(ContactEvent::CL_ERROR);
        return true;
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
    

    // change the logpath to output the peer_eid
    logpathf(base_logpath_.c_str(), link->name(), sptr_peer_eid->c_str());
    cmdqueue_.logpathf("%s/cmdq", logpath_);
    sock_->logpathf("%s/sock", logpath_);



    TCPLinkParams* params = tcp_lparams();


    params->keepalive_interval_ = std::min(params->keepalive_interval_, keepalive_interval);

    /*
     * Make sure to readjust poll_timeout in case we have a smaller
     * keepalive interval than data timeout
     */
    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
     
    params->segment_length_ = std::min(params->segment_length_, max_seg_len);

    link->params().mtu_ = max_bundle_size;

    /*
     * Finally, we note that the contact is now up.
     */
    contact_up();


    // If external router - delay 5 seconds befoe reading bundles to give time for contact up to be processed?
    int secs_to_delay = BundleDaemon::instance()->router()->delay_after_contact_up();
    if (0 != secs_to_delay) {
        sleep(secs_to_delay);
    }

    return true;
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::handle_data_segment(uint8_t flags)
{
    // do we have enough bytes to read the length of the Node ID?
    size_t need_bytes = 1 + 1 + 8;  // [+ 4 + extension_items] + 8 + data_contents;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_data_segment: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type = 0;
    uint64_t transfer_id = 0;
    uint32_t extensions_len = 0;
    u_char* extensions_ptr = nullptr;
    uint64_t data_len = 0;

    bool extitm_critical_unhandled = false;
    uint64_t extitm_transfer_len = 0;

    u_char* buf = (u_char*) recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_XFER_SEGMENT);
    ++buf;

    // overwrite the dummy flags that were passed in
    flags = *buf;
    ++buf;

    memcpy(&transfer_id, buf, sizeof(transfer_id));
    transfer_id = be64toh(transfer_id);
    buf += sizeof(transfer_id);

    if (flags & BUNDLE_START) {

        // extension items only present if bundle_start bit is set
        // enough data to read the Length of the Extension Items?
        need_bytes += 4;
        if (recvbuf_.fullbytes() < need_bytes) {
            log_debug("handle_data_segment: not enough data received "
                      "(need > %zu, got %zu)",
                      need_bytes, recvbuf_.fullbytes());
            return false;
        }

        memcpy(&extensions_len, buf, sizeof(extensions_len));
        extensions_len = ntohl(extensions_len);
        buf += sizeof(extensions_len);
    
        // enough data to read the Extension Items?
        need_bytes += extensions_len;
        if (recvbuf_.fullbytes() < need_bytes) {
            log_debug("handle_data_segment: not enough data received "
                      "(need > %zu, got %zu)",
                      need_bytes, recvbuf_.fullbytes());
            return false;
        }

        extensions_ptr = buf;
        buf += extensions_len; // skip over the extensions for now

        if (!parse_xfer_segment_extension_items(extensions_ptr, extensions_len, 
                                                extitm_critical_unhandled, extitm_transfer_len))
        {
            log_err("protocol error: "
                    "got BUNDLE_START before bundle completed");
            break_contact(ContactEvent::CL_ERROR);
            return false;

        }
    }

    // enough data to read the Length of the Data?
    need_bytes += 8;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_data_segment: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    memcpy(&data_len, buf, sizeof(data_len));
    data_len = be64toh(data_len);
    buf += sizeof(data_len);

    recvbuf_.consume(need_bytes);


    // already refusing this bundle?
    if (transfer_id == refuse_transfer_id_) {
        recv_segment_todo_ = data_len;
        recv_segment_to_refuse_ = data_len;
        if (flags & BUNDLE_END) {
            // this is the last segment we have to skip over
            refuse_transfer_id_ = 0;
        }
        return handle_data_todo();
    }


   if (extitm_critical_unhandled) {
        refuse_bundle(transfer_id, TCPV4_REFUSE_REASON_EXTENSION_FAILURE);

        recv_segment_todo_ = data_len;
        recv_segment_to_refuse_ = data_len;
        if (flags & BUNDLE_END) {
            // this is the last segment we have to skip over
            refuse_transfer_id_ = 0;
        }
        return handle_data_todo();
   }



    IncomingBundle* incoming = nullptr;
    if (flags & BUNDLE_START)  // flag bits are the same as in TCPCLv3 defined in StreamConvergenceLayer.h
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
                if (incoming->payload_bytes_reserved_ > 0) {
                    // return previously reserveed space
                    BundleStore* bs = BundleStore::instance();
                    bs->release_payload_space(incoming->payload_bytes_reserved_);
                }
                incoming->payload_bytes_reserved_ = 0;
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
            incoming = new IncomingBundle(new Bundle(BundleProtocol::BP_VERSION_UNKNOWN));
            incoming_.push_back(incoming);
        }


        if (extitm_transfer_len == 0) {
            if (flags & BUNDLE_END) {
                // segment contains the entire bundle so we can use the data_len
                extitm_transfer_len = data_len;
            }
        }


        if (extitm_transfer_len > 0) {
            //XXX/dz - TODO: check for length greater than MTU ???

            BundleStore* bs = BundleStore::instance();

            // total length was provided as an extension item so we can reserve payload space
            uint64_t    tmp_payload_storage_bytes_reserved = incoming->payload_bytes_reserved_;


            ASSERT((tmp_payload_storage_bytes_reserved == 0) || (tmp_payload_storage_bytes_reserved == extitm_transfer_len));

            int32_t     ctr = 0;
            while (!should_stop() && (++ctr < 50) && (tmp_payload_storage_bytes_reserved < extitm_transfer_len))
            {
                if (bs->try_reserve_payload_space(extitm_transfer_len)) {
                    tmp_payload_storage_bytes_reserved = extitm_transfer_len;
                } else {
                    usleep(100000);
                }
            }
        
            if (!should_stop()) {
                if (tmp_payload_storage_bytes_reserved == 0) {
                    log_info("TCP CL unable to reserve %" PRIu64 " bytes of payload space for 5 seconds"
                             " - refusing bundle in transaction ID: %" PRIu64, 
                             extitm_transfer_len, transfer_id);
                    refuse_bundle(transfer_id, TCPV4_REFUSE_REASON_NO_RESOURCES);

                    recv_segment_todo_ = data_len;
                    recv_segment_to_refuse_ = data_len;
                    if (flags & BUNDLE_END) {
                        // this is the last segment we have to skip over
                        refuse_transfer_id_ = 0;
                    }
                    return handle_data_todo();
                }

                incoming->payload_bytes_reserved_ = tmp_payload_storage_bytes_reserved;
            }
        }

        incoming->transfer_id_ = transfer_id;
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

    if (data_len == 0) {
        log_err("protocol error -- zero length segment");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    size_t segment_offset = incoming->bytes_received_;

    SPtr_AckObject ackptr = std::make_shared<AckObject>();
    ackptr->flags_ = flags;
    ackptr->transfer_id_ = incoming->transfer_id_;
    ackptr->acked_len_ = segment_offset + data_len;
    incoming->pending_acks_.push_back(ackptr);


    // if this is the last segment for the bundle, we calculate and
    // store the total length in the IncomingBundle structure so
    // send_pending_acks knows when we're done.
    if (flags & BUNDLE_END)
    {
        incoming->total_length_ = segment_offset + data_len;
    }
    
    recv_segment_todo_ = data_len;
    return handle_data_todo();
}

//----------------------------------------------------------------------
// returns false on protocol error 
// critical_item_unhandled set to true indicates bundle should be refused
bool
TCPConvergenceLayer::Connection4::parse_xfer_segment_extension_items(u_char* buf, uint32_t len, 
                                                                    bool& critical_item_unhandled,
                                                                    uint64_t& extitm_transfer_len)
{
    bool result = true;

    critical_item_unhandled = false;
    extitm_transfer_len = 0;

    while (len > 0) {
        uint8_t flags = 0;
        uint16_t item_type = 0;
        uint16_t item_length = 0;

        if (len < 5) {
            log_err("parse_xfer_segment_extension_items: not enough data to read the length of the item value");
            result = false;
            break;
        }

        flags = *buf;
        buf += 1;
        len -= 1;

        memcpy(&item_type, buf, sizeof(item_type));
        buf += sizeof(item_type);
        len -= sizeof(item_type);
        item_type = ntohs(item_type);

        memcpy(&item_length, buf, sizeof(item_length));
        buf += sizeof(item_length);
        len -= sizeof(item_length);
        item_length = ntohs(item_length);

        if (len < item_length) {
            log_err("parse_xfer_segment_extension_items: not enough data to read the item value");
            result = false;
            break;
        }

        if (item_type == TCPV4_EXTENSION_TYPE_TRANSFER_LENGTH) {
            if (extitm_transfer_len != 0) {
                log_err("parse_xfer_segment_extension_items: Error - Received multiple instances of"
                        " the Transfer Length Extension");
                result = false;
                break;
            }

            // we recognize and handle this type
            if (item_length != 8) {
                log_err("parse_xfer_segment_extension_items: Error - Transfer Length Extension is not 8 bytes");
                result = false;
                break;
            }

            memcpy(&extitm_transfer_len, buf, sizeof(extitm_transfer_len));
            buf += sizeof(extitm_transfer_len);
            len -= sizeof(extitm_transfer_len);

            extitm_transfer_len = be64toh(extitm_transfer_len);

        } else if (flags & TCPV4_EXTENSION_FLAG_CRITICAL) {
            critical_item_unhandled = true;
            log_err("parse_xfer_segment_extension_items: Error - Unrecognized Critical Extension Item (Type: %u)"
                    " - refusing bundle", item_type);

        } else {
            log_warn("parse_xfer_segment_extension_items: Error - Unrecognized Extension Item (Type: %u)"
                    " - ignoring and continuing", item_type);
        }
    }

    return result;
}

//----------------------------------------------------------------------
uint32_t
TCPConvergenceLayer::Connection4::encode_transfer_length_extension(u_char* buf, uint32_t len, 
                                                                  uint64_t transfer_length)
{
    uint32_t result = 0;

    uint32_t need_bytes = 1 + 2 + 2 + 8;
    if (len >= need_bytes) {
        uint8_t flags = 0;
        uint16_t item_type = TCPV4_EXTENSION_TYPE_TRANSFER_LENGTH;
        uint16_t item_length = 8;  // size of a uint64_t

        *buf = flags;
        buf += 1;
        result += 1;

        item_type = htons(item_type);
        memcpy(buf, &item_type, sizeof(item_type));
        buf += sizeof(item_type);
        result += sizeof(item_type);

        item_length = htons(item_length);
        memcpy(buf, &item_length, sizeof(item_length));
        buf += sizeof(item_length);
        result += sizeof(item_length);


        transfer_length = htobe64(transfer_length);
        memcpy(buf, &transfer_length, sizeof(transfer_length));
        buf += sizeof(transfer_length);
        result += sizeof(transfer_length);

        ASSERT(result == need_bytes);
    }

    return result;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::refuse_bundle(uint64_t transfer_id, uint8_t reason)
{
    // skip additional segments that match this transaction ID
    refuse_transfer_id_ = transfer_id;


    // calculate size of the XFER_REFUSE message
    size_t need_bytes = 1 + 1 + 8;

    char buf[need_bytes];

    // Message Header
    buf[0] = TCPV4_MSG_TYPE_XFER_REFUSE;

    // Reason Code
    buf[1] = reason;
   
    // Transfer ID
    uint64_t u64 = htobe64(transfer_id);
    memcpy(&buf[2], &u64, sizeof(u64));

    // queue msg to be sent
    admin_msg_list_.push_back(new std::string(buf, need_bytes));
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::send_msg_reject(uint8_t msg_type, uint8_t reason)
{
    // calculate size of the MSG_REJECT message
    size_t need_bytes = 1 + 1 + 1;

    char buf[need_bytes];

    // Message Header
    buf[0] = TCPV4_MSG_TYPE_MSG_REJECT;

    // Reason Code
    buf[1] = reason;
   
    // Msg Type that caused the issue
    buf[2] = msg_type;

    // queue msg to be sent
    admin_msg_list_.push_back(new std::string(buf, need_bytes));
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::handle_ack_segment(uint8_t flags)
{
    // do we have enough bytes to read the entire message?
    size_t need_bytes = 1 + 1 + 8 + 8;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_ack_segment: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type = 0;
    uint64_t transfer_id = 0;
    uint64_t acked_len = 0;

    char* buf = recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_XFER_ACK);
    ++buf;

    // overwrite the dummy flags that were passed in
    // - the flags match those from the XFER_SEGMENT but we don't 
    //   currently use them
    flags = *buf;
    ++buf;
    (void) flags;

    memcpy(&transfer_id, buf, sizeof(transfer_id));
    transfer_id = be64toh(transfer_id);
    buf += sizeof(transfer_id);

    memcpy(&acked_len, buf, sizeof(acked_len));
    acked_len = be64toh(acked_len);
    buf += sizeof(acked_len);

    recvbuf_.consume(need_bytes);

    if (inflight_.empty()) {
        log_err("protocol error: got ack segment with no inflight bundle");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    InFlightBundle* inflight = inflight_.front();

    // verify transaction IDs match
    if (transfer_id != inflight->transfer_id_) {
        log_err("protocol error: got ack segment for %zu bytes but unexpected Transfer ID - expected: %zu got: %zu",
                acked_len, inflight->transfer_id_, transfer_id);
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }


    size_t ack_begin;
    DataBitmap::iterator i = inflight->ack_data_.begin();
    if (i == inflight->ack_data_.end()) {
        ack_begin = 0;
    } else {
        i.skip_contiguous();
        ack_begin = *i + 1;
    }

    if (acked_len < ack_begin) {
        log_err("protocol error: got ack for length %" PRIu64 " but already acked up to %zu",
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
TCPConvergenceLayer::Connection4::handle_refuse_bundle(uint8_t flags)
{
    (void) flags;

    // do we have enough bytes to read the entire message?
    size_t need_bytes = 1 + 1 + 8;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_refuse_bundle: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type = 0;
    uint8_t reason = 0;
    uint64_t transfer_id = 0;

    char* buf = recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_XFER_REFUSE);
    ++buf;

    reason = *buf;
    ++buf;

    memcpy(&transfer_id, buf, sizeof(transfer_id));
    transfer_id = be64toh(transfer_id);
    buf += sizeof(transfer_id);

    recvbuf_.consume(need_bytes);

    if (inflight_.empty()) {
        log_err("protocol error: got XFER_REFUSE segment with no inflight bundle");
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }


    // search inflight list to find Transfer ID
    uint64_t first_id = 0;
    uint64_t last_id = 0;
    bool found = false;
    size_t ctr = 0;
    InFlightBundle* inflight;
    InFlightList::iterator iter = inflight_.begin();

    while (iter != inflight_.end()) {
        inflight = *iter;
        if (ctr++ == 0) {
            first_id = inflight->transfer_id_;
        }
        last_id = inflight->transfer_id_;

        if (inflight->transfer_id_ == transfer_id) {
            found = true;
            break;
        }

        ++iter;
    }

    if (!found) {
        log_err("protocol error: got XFER_REFUSE segment for unexpected Transfer ID (%" PRIu64 
                ") expected  range from %" PRIu64 " to %" PRIu64,
                transfer_id, first_id, last_id);
        break_contact(ContactEvent::CL_ERROR);
        return false;
    }

    log_warn("TCPCLv4 - inflight list bundle (#%zu) refused (transfer id: %" PRIu64 " - %s): *%p", 
             ctr, transfer_id, tcpv4_refusal_reason_to_str(reason), inflight->bundle_.object());

    inflight->bundle_refused_ = true;

    if (inflight == current_inflight_) {
        finish_bundle(inflight);
        check_completed(inflight);
    } else {
        BundleTransmittedEvent* event_to_post;
        event_to_post = new BundleTransmittedEvent(inflight->bundle_.object(),
                                                   contact_,contact_->link(),
                                                   0, 0, false, true);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

        inflight_.erase(iter);
        delete inflight;
    }

    return true;
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::handle_reject_msg()
{
    // do we have enough bytes to read the entire message?
    size_t need_bytes = 1 + 1 + 1;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_reject_msg: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type = 0;
    uint8_t reason = 0;
    uint8_t msg_type_caused_issue = 0;

    char* buf = recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_MSG_REJECT);
    ++buf;

    reason = *buf;
    ++buf;

    msg_type_caused_issue = *buf;
    ++buf;

    recvbuf_.consume(need_bytes);

    log_err("Received MSG_REJECT from peer on msg type; %u - reason: %s",
            msg_type_caused_issue, msg_reject_reason_to_str(reason));

    return true;
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::handle_shutdown(uint8_t flags)
{
    log_debug("got SESS_TERM message");

    // do we have enough bytes to read the entire message?
    size_t need_bytes = 1 + 1 + 1;
    if (recvbuf_.fullbytes() < need_bytes) {
        log_debug("handle_shutdown: not enough data received "
                  "(need > %zu, got %zu)",
                  need_bytes, recvbuf_.fullbytes());
        return false;
    }

    uint8_t msg_type = 0;
    uint8_t reason = 0;

    char* buf = recvbuf_.start();

    msg_type = *buf;
    ASSERT(msg_type == TCPV4_MSG_TYPE_SESS_TERM);
    ++buf;

    // overwrite the dummy flags that were passed in
    flags = *buf;
    ++buf;
    (void) flags;

    reason = *buf;
    ++buf;
    if (reason > TCPV4_SHUTDOWN_REASON_RESOURCE_EXHAUSTION) {
        reason = TCPV4_SHUTDOWN_REASON_UNKNOWN;
    }

    recvbuf_.consume(need_bytes);


    log_always("got SESS_TERM (%s)", tcpv4_shutdown_reason_to_str(reason));

    break_contact(ContactEvent::SHUTDOWN);
    
    return false;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::process_data()
{
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
    if (!contact_initiation_received_) {
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
        
        uint8_t type  = *recvbuf_.start();
        uint8_t flags = 0;

        bool ok;
        switch (type) {
        case TCPV4_MSG_TYPE_SESS_INIT:
            ok = handle_session_negotiation();
            break;

        case TCPV4_MSG_TYPE_XFER_SEGMENT:
            if (delay_reads_to_free_some_storage_) {
                ok = false;
            } else {
                ok = handle_data_segment(flags);
            }
            break;

        case TCPV4_MSG_TYPE_XFER_ACK:
            ok = handle_ack_segment(flags);
            break;

        case TCPV4_MSG_TYPE_XFER_REFUSE:
            ok = handle_refuse_bundle(flags);
            break;

        case TCPV4_MSG_TYPE_KEEPALIVE:
            // same processing as in StreamCOnvergenceLayer
            ok = handle_keepalive(flags);
            break;

        case TCPV4_MSG_TYPE_SESS_TERM:
            ok = handle_shutdown(flags);
            break;

        case TCPV4_MSG_TYPE_MSG_REJECT:
            ok = handle_reject_msg();
            break;

        default:
            log_err("invalid CL message type code 0x%x", type);
            send_msg_reject(type, TCPV4_MSG_REJECT_REASON_UNKNOWN);
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
TCPConvergenceLayer::Connection4::handle_poll_activity_while_waiting_for_tcpcl3_contact()
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
                log_debug("delayed_connect to %s:%d succeeded while delaying for TCPCL3 contact header",
                          intoa(sock_->remote_addr()), sock_->remote_port());
            } else {
                log_info("Connection4::connection attempt to %s:%d failed while delaying for TCPCL3 contact header... %s",
                         intoa(sock_->remote_addr()), sock_->remote_port(),
                         strerror(errno));
                break_contact(ContactEvent::BROKEN);
            }

            return;
        }
    }
    
    // finally, check for incoming data
    if (sock_pollfd_->revents & POLLIN) {
        // if we got data then peer is not TCPCL4 which 
        // waits for contact header before replying

        // hand off to a TCPCL3 Connection
        TCPConvergenceLayer* tcp_cl = dynamic_cast<TCPConvergenceLayer*>(cl_);
        TCPLinkParams* tcpcl_params = dynamic_cast<TCPLinkParams*>(params_);

        ASSERT(tcpcl_params != nullptr);
        ASSERT(tcp_cl != nullptr);

        Connection3* conn = new Connection3(tcp_cl, tcpcl_params, sock_, nexthop_);
        conn->set_contact(contact_);
        contact_->set_cl_info(nullptr);
        contact_->set_cl_info(conn);
        conn->start();
        sock_ = nullptr;
        

        contact_broken_ = true;
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::handle_poll_activity()
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
                log_debug("delayed_connect to %s:%d succeeded after delay for TCPCL3 contact header",
                          intoa(sock_->remote_addr()), sock_->remote_port());
                initiate_contact();
                
            } else {
                //log_info("connection attempt to %s:%d failed... %s",
                log_debug("connection attempt to %s:%d failed after delay for TCPCL3 contact header... %s",
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
            // this can happen if the BundleRestagingDaemon refuses a bundle
            // after the TCP CL has reserved the payload quota space
            note_data_rcvd();   // prevent keep_alive timeout
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
TCPConvergenceLayer::Connection4::run()
{
    char initial_threadname[16] = "TcpCL4Delay";
    pthread_setname_np(pthread_self(), initial_threadname);

    if (active_connector_) {

        bool continue_with_tcpcl4 = run_delay_for_tcpcl3();
        if (!continue_with_tcpcl4) {
            return;
        }
    }

    char threadname[16] = "TcpCLConnectn4";
    pthread_setname_np(pthread_self(), threadname);

    CLConnection::run();
}




//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::run_delay_for_tcpcl3()
{
    char threadname[16] = "TcpCLConn4delay";
    pthread_setname_np(pthread_self(), threadname);

    initialize_pollfds();
    if (contact_broken_) {
        log_debug("contact_broken set during initialization");
        return false;
    }

    // make the socket connection
    connect();

    TCPLinkParams* tcpcl_params = dynamic_cast<TCPLinkParams*>(params_);
    uint32_t ms_to_delay = tcpcl_params->delay_for_tcpcl3_ * 1000;

    oasys::Time delay_for_tcpcl3_time = oasys::Time::now();
    
    while (!contact_initiation_received_ && (delay_for_tcpcl3_time.elapsed_ms() < ms_to_delay)) {
        int timeout = 5000; // 5 seconds max wait
        pollfds_[0].revents = 0;

        if (contact_broken_) {
            log_debug("contact_broken set while delaying for TCPCL3 contact header, exiting main loop");
            return false;
        }

        int cc = oasys::IO::poll_multiple(pollfds_, num_pollfds_,
                                          timeout, nullptr, logpath_);
                                          
        // check again here for contact broken since we don't want to
        // act on the poll result if the contact is broken
        if (contact_broken_) {
            log_debug("contact_broken set while delaying for TCPCL3 contact header, exiting main loop");
            return false;
        }
        
        if (cc > 0)
        {
            handle_poll_activity_while_waiting_for_tcpcl3_contact();
            if (contact_broken_) {
                // either kicked off a TCPCL3 Connection or error
                set_flag(DELETE_ON_EXIT);
                return false;
            }
            usleep(1);  //give other threads a chance
        }
        else if (cc != oasys::IOTIMEOUT)
        {
            log_err("unexpected return from poll_multiple while delaying for TCPCL3 contact: %d", cc);
            break_contact(ContactEvent::BROKEN);
            return false;
        }
    }

    // reset the num_pollfds so the CLConnection::run method can initialize properly
    num_pollfds_ = 0;
    return true;
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::send_keepalive()
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

    *(sendbuf_.end()) = TCPV4_MSG_TYPE_KEEPALIVE;
    sendbuf_.fill(1);

    ::gettimeofday(&keepalive_sent_, 0);

    // don't note_data_sent() here since keepalive messages shouldn't
    // be counted for keeping an idle link open
    send_data();
}

//----------------------------------------------------------------------
bool
TCPConvergenceLayer::Connection4::send_next_segment(InFlightBundle* inflight)
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

    // begin calculating bytes needed for the msg up to the data itself
    size_t need_bytes = 1 + 1 + 8 + 8;  // Msg Header + Flags + Transfer ID + Data Length  (no extension items)

    uint32_t extensions_len = 0;
    bool include_extension_items = false;


    if (bytes_sent == 0) {
        flags |= BUNDLE_START;
    }
    
    if (params->segment_length_ >= inflight->total_length_ - bytes_sent) {
        flags |= BUNDLE_END;
        segment_len = inflight->total_length_ - bytes_sent;
    } else {
        segment_len = params->segment_length_;
    }
   
    if (bytes_sent == 0) {
        include_extension_items = true;
        need_bytes += 4;  // add in length of extensions items

        if ((flags & BUNDLE_END) == 0) {
            extensions_len = 13;
            need_bytes += extensions_len;  // add in the data for the Transfer Length extension
        }
    }


    if (sendbuf_.tailbytes() < need_bytes) {
        //log_debug("send_next_segment: "
        //          "not enough space for segment header [need %zu, have %zu]",
        //          1 + sdnv_len, sendbuf_.tailbytes());
        return false;
    }
    
    // Message Header
    *sendbuf_.end() = TCPV4_MSG_TYPE_XFER_SEGMENT;
    sendbuf_.fill(1);

    // Flags
    *sendbuf_.end() = flags;
    sendbuf_.fill(1);
   
    // Transfer ID
    uint64_t u64 = htobe64(inflight->transfer_id_);
    memcpy(sendbuf_.end(), &u64, sizeof(u64));
    sendbuf_.fill(sizeof(u64));

    // Extension Items if any
    if (include_extension_items) {
        uint32_t u32 = htonl(extensions_len);
        memcpy(sendbuf_.end(), &u32, sizeof(u32));
        sendbuf_.fill(sizeof(u32));

        if (extensions_len > 0) {
             u_char* extensions_ptr = (u_char*) sendbuf_.end();
            if (extensions_len != encode_transfer_length_extension(extensions_ptr, extensions_len, 
                                                                   inflight->total_length_))
            {
                log_err("send_next_segment - error encoding Transfer Length extension - abort");
                break_contact(ContactEvent::BROKEN);
                return false;
            }
            sendbuf_.fill(extensions_len);
        }
    }

    // Data Length
    u64 = htobe64(segment_len);
    memcpy(sendbuf_.end(), &u64, sizeof(u64));
    sendbuf_.fill(sizeof(u64));

    send_segment_todo_ = segment_len;

    // send_data_todo actually does the deed
    return send_data_todo(inflight);
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::queue_acks_for_incoming_bundle(IncomingBundle* incoming)
{
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != NULL); 

    while (!incoming->pending_acks_.empty()) {
        SPtr_AckObject in_ackptr = incoming->pending_acks_.front();

        if(params->segment_ack_enabled_)
        {
            // make sure we have space in the send buffer
            size_t need_bytes = 1 + 1 + 8 + 8;

            char buf[need_bytes];

            // msg header
            buf[0] = TCPV4_MSG_TYPE_XFER_ACK;

            // flags from XFER_SEGMENT we are ACKing
            buf[1] = in_ackptr->flags_;

            // Transfer ID
            uint64_t u64 = htobe64(in_ackptr->transfer_id_);
            memcpy(&buf[2], &u64, sizeof(u64));

            // Acknowledged Length
            u64 = htobe64(in_ackptr->acked_len_);
            memcpy(&buf[10], &u64, sizeof(u64));

            admin_msg_list_.push_back(new std::string(buf, need_bytes));
        }

        incoming->acked_length_ = in_ackptr->acked_len_;

        incoming->pending_acks_.pop_front();
    }
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::send_data()
{
    if (tls_active_) {
        send_data_tls();
        return;
    }



    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    if (contact_broken_) {
        if (BundleDaemon::instance()->shutting_down()) {
            return;
        }
        ASSERT(! contact_broken_);
    }

    u_int towrite = sendbuf_.fullbytes();
    if (params_->test_write_limit_ != 0) {
        towrite = std::min(towrite, params_->test_write_limit_);
    }
    
    //log_debug("send_data: trying to drain %u bytes from send buffer...",
    //          towrite);
    if (towrite == 0) {
        if (BundleDaemon::instance()->shutting_down()) {
            return;
        }
        ASSERT(towrite > 0);
    }

    int cc = sock_->write(sendbuf_.start(), towrite);


    if (cc > 0) {
        //log_debug("send_data: wrote %d/%zu bytes from send buffer",
        //           cc, sendbuf_.fullbytes());

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
TCPConvergenceLayer::Connection4::send_data_tls()
{
#ifdef WOLFSSL_TLS_ENABLED
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    u_int towrite = sendbuf_.fullbytes();
    if (params_->test_write_limit_ != 0) {
        towrite = std::min(towrite, params_->test_write_limit_);
    }
    
    //log_debug("send_data_tls: trying to drain %u bytes from send buffer...",
    //          towrite);
    ASSERT(towrite > 0);

    int cc = wolfSSL_write(wolfssl_handle_, sendbuf_.start(), towrite); 

    if (cc > 0) {
        //log_debug("send_data_tls: wrote %d/%zu bytes from send buffer",
        //          cc, sendbuf_.fullbytes());
        if (tcp_lparams()->hexdump_) {
            oasys::HexDumpBuffer hex;
            hex.append((u_char*)sendbuf_.start(), cc);
            log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }
        
        sendbuf_.consume(cc);
        
        if (sendbuf_.fullbytes() != 0) {
            log_debug("send_data_tls: incomplete write, setting POLLOUT bit");
            sock_pollfd_->events |= POLLOUT;

        } else {
            if (sock_pollfd_->events & POLLOUT) {
                //log_debug("send_data_tls: drained buffer, clearing POLLOUT bit");
                sock_pollfd_->events &= ~POLLOUT;
            }
        }
    } else if (wolfSSL_want_write(wolfssl_handle_)) {
        //log_debug("send_data_tls: write returned EWOULDBLOCK, setting POLLOUT bit");

        sock_pollfd_->events |= POLLOUT;
        
    } else {
        int err = wolfSSL_get_error(wolfssl_handle_, 0);
        char buf[80];
        wolfSSL_ERR_error_string(err, buf);
        log_err("send_data_tls: breaking contact after error writing to TLS socker: %s", buf);
        break_contact(ContactEvent::BROKEN);
    }
#endif // WOLFSSL_TLS_ENABLED
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::recv_data()
{
    if (tls_active_) {
        recv_data_tls();
        return;
    }

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

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::recv_data_tls()
{
#ifdef WOLFSSL_TLS_ENABLED
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
    int cc = wolfSSL_read(wolfssl_handle_, recvbuf_.end(), toread);

    if (cc < 1) {
        if (!wolfSSL_want_read(wolfssl_handle_)) {
            int err = wolfSSL_get_error(wolfssl_handle_, 0);
            char buf[80];
            wolfSSL_ERR_error_string(err, buf);
            log_err("iBreaking contact after error reading TLS socket: %s", buf);
            break_contact(ContactEvent::BROKEN);
        }
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
#endif // WOLFSSL_TLS_ENABLED
}

//----------------------------------------------------------------------
void
TCPConvergenceLayer::Connection4::break_contact(ContactEvent::reason_t reason)
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
    uint8_t shutdown_reason = TCPV4_SHUTDOWN_REASON_UNKNOWN;
    uint8_t flags = 0;

    switch (reason) {
    case ContactEvent::USER:
        // if the user is closing this link, we say that we're busy
        send_shutdown = true;
        shutdown_reason = TCPV4_SHUTDOWN_REASON_BUSY;
        break;
        
    case ContactEvent::IDLE:
        // if we're idle, indicate as such
        send_shutdown = true;
        shutdown_reason = TCPV4_SHUTDOWN_REASON_IDLE_TIMEOUT;
        break;
        
    case ContactEvent::SHUTDOWN:
        // if the other side shuts down first, we send the
        // corresponding SHUTDOWN byte for a clean handshake, but
        // don't give any more reason
        send_shutdown = true;
        flags = TCPV4_SHUTDOWN_FLAG_REPLY;
        break;
        
    case ContactEvent::BROKEN:
    case ContactEvent::CL_ERROR:
        // no shutdown 
        send_shutdown = false;
        break;

    case ContactEvent::CL_VERSION:
        // version mismatch
        send_shutdown = true;
        shutdown_reason = TCPV4_SHUTDOWN_REASON_VERSION_MISMATCH;
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

        *sendbuf_.end() = TCPV4_MSG_TYPE_SESS_TERM;
        sendbuf_.fill(1);

        *sendbuf_.end() = flags;
        sendbuf_.fill(1);

        *sendbuf_.end() = shutdown_reason;
        sendbuf_.fill(1);

        send_data();
    }
        
    contact_broken_ = true;
        
    log_debug("break_contact: %s", ContactEvent::reason_to_str(reason));

    if (reason != ContactEvent::BROKEN) {
        if (tls_active_) {
#ifdef WOLFSSL_TLS_ENABLED
            wolfSSL_shutdown(wolfssl_handle_);

            wolfSSL_free(wolfssl_handle_);
            wolfssl_handle_ = nullptr;

            wolfSSL_CTX_free(wolfssl_ctx_);
            wolfssl_ctx_ = nullptr;
#endif // WOLFSSL_TLS_ENABLED
        }

        disconnect();
    }

    // if the connection isn't being closed by the user, we need to
    // notify the daemon that either the contact ended or the link
    // became unavailable before a contact began.
    //
    // we need to check that there is in fact a contact, since a
    // connection may be accepted and then break before establishing a
    // contact
    if ((reason != ContactEvent::USER) && (contact_ != NULL)) {
        LinkStateChangeRequest* event_to_post;
        event_to_post = new LinkStateChangeRequest(contact_->link(),
                                                   Link::CLOSED,
                                                   reason);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    }
}

} // namespace dtn
