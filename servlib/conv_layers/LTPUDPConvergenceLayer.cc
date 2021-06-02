/* 
 *    Copyright 2015 United States Government as represented by NASA
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
 *    results, designs or products rsulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <iostream>
// #include <sys/timeb.h>
#include <climits>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/io/IO.h>

#include "LTPUDPConvergenceLayer.h"

#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/FormatUtils.h"

namespace dtn {

class LTPUDPConvergenceLayer::Params LTPUDPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = (int) bucket_type_;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("inact_intvl", &inactivity_intvl_);
    a->process("retran_intvl", &retran_intvl_);
    a->process("retran_retries", &retran_retries_);
    a->process("remote_engine_id", &remote_engine_id_);
    a->process("comm_aos", &comm_aos_);
    a->process("max_sessions", &max_sessions_);
    a->process("seg_size", &seg_size_);
    a->process("agg_size", &agg_size_);
    a->process("agg_time", &agg_time_);
    a->process("ccsds_compatible", &ccsds_compatible_);
    a->process("sendbuf", &sendbuf_);
    a->process("recvbuf", &recvbuf_);
    a->process("queued_bytes_quota", &ltp_queued_bytes_quota_);
    a->process("bytes_per_checkpoint", &bytes_per_checkpoint_);
    a->process("use_files_xmit", &use_files_xmit_);
    a->process("use_files_recv", &use_files_recv_);
    a->process("keep_aborted_files", &keep_aborted_files_);
    a->process("use_diskio_kludge", &use_diskio_kludge_);
    a->process("dir_path", &dir_path_);
    // clear_stats_ not needed here
    // hexdump_ not needed here
    // dump_sessions_ not needed here
    // dump_segs_ not needed here
    a->process("icipher_suite", &inbound_cipher_suite_);
    a->process("icipher_keyid", &inbound_cipher_key_id_);
    a->process("icipher_engine",  &inbound_cipher_engine_);
    a->process("ocipher_suite", &outbound_cipher_suite_);
    a->process("ocipher_keyid", &outbound_cipher_key_id_);
    a->process("ocipher_engine",  &outbound_cipher_engine_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPConvergenceLayer()
    : IPConvergenceLayer("LTPUDPConvergenceLayer", "ltpudp")
{
    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::~LTPUDPConvergenceLayer()
{
}

//----------------------------------------------------------------------
CLInfo*
LTPUDPConvergenceLayer::new_link_params()
{
    return new LTPUDPConvergenceLayer::Params(LTPUDPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&LTPUDPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char* argv[],
                                  const char** invalidp)
{
    char icipher_from_engine[50];
    char ocipher_from_engine[50];
    oasys::OptParser p;
    uint32_t tmp_bucket_type = 0;
    uint64_t tmp_engine_id = 0;
    bool dummy_ion_comp = false;

    bool use_files = false;
    bool use_files_set = false;

    std::string ocipher_engine_str = "";
    std::string icipher_engine_str = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    //p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    //p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::UIntOpt("bucket_type", &tmp_bucket_type));
    p.addopt(new oasys::RateOpt("rate", &params->rate_));
    p.addopt(new oasys::UInt64Opt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::UIntOpt("inact_intvl", &params->inactivity_intvl_));
    p.addopt(new oasys::UIntOpt("retran_intvl", &params->retran_intvl_));
    p.addopt(new oasys::UIntOpt("retran_retries", &params->retran_retries_));
    p.addopt(new oasys::UInt64Opt("remote_engine_id", &params->remote_engine_id_));
    p.addopt(new oasys::BoolOpt("hexdump",&params->hexdump_));
    p.addopt(new oasys::BoolOpt("comm_aos",&params->comm_aos_));
    p.addopt(new oasys::UIntOpt("max_sessions", &params->max_sessions_));
    p.addopt(new oasys::UIntOpt("seg_size", &params->seg_size_));
    p.addopt(new oasys::BoolOpt("ccsds_compatible", &params->ccsds_compatible_));
    p.addopt(new oasys::UInt64Opt("agg_size", &params->agg_size_));
    p.addopt(new oasys::UIntOpt("agg_time", &params->agg_time_));
    p.addopt(new oasys::UIntOpt("recvbuf", &params->recvbuf_));
    p.addopt(new oasys::UIntOpt("sendbuf", &params->sendbuf_));
    p.addopt(new oasys::BoolOpt("clear_stats", &params->clear_stats_));
    p.addopt(new oasys::BoolOpt("dump_sessions", &params->dump_sessions_));
    p.addopt(new oasys::BoolOpt("dump_segs", &params->dump_segs_));
    p.addopt(new oasys::SizeOpt("queued_bytes_quota", &params->ltp_queued_bytes_quota_));
    p.addopt(new oasys::SizeOpt("bytes_per_checkpoint", &params->bytes_per_checkpoint_));

    // use_files must be first otherwise it triggers on a partial match with use_files_xmit
    p.addopt(new oasys::BoolOpt("use_files", &use_files, "", &use_files_set));
    p.addopt(new oasys::BoolOpt("use_files_xmit", &params->use_files_xmit_));
    p.addopt(new oasys::BoolOpt("use_files_recv", &params->use_files_recv_));
    p.addopt(new oasys::BoolOpt("keep_aborted_files", &params->keep_aborted_files_));
    p.addopt(new oasys::BoolOpt("use_diskio_kludge", &params->use_diskio_kludge_));

    p.addopt(new oasys::StringOpt("dir_path", &params->dir_path_));

    p.addopt(new oasys::IntOpt("xmit_test", &params->xmit_test_));
    p.addopt(new oasys::IntOpt("recv_test", &params->recv_test_));

    p.addopt(new oasys::IntOpt("ocipher_suite", &params->outbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("ocipher_keyid", &params->outbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("ocipher_engine", &ocipher_engine_str));
    p.addopt(new oasys::IntOpt("icipher_suite", &params->inbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("icipher_keyid", &params->inbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("icipher_engine", &icipher_engine_str));

    // for backward compatibility
    p.addopt(new oasys::UInt64Opt("engine_id", &tmp_engine_id));
    p.addopt(new oasys::BoolOpt("ion_comp",&dummy_ion_comp));

    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        log_err("Error parsing parameters - invalid value: %s", *invalidp);
    } else if (count < argc) {
        argc -= count;
        log_err("Warning parsing parameters - ignored %d unrecognized parameter(s): ", argc);
        for (count=0; count<argc; ++count) {
            log_err("        %d) %s", count+1, argv[count]);
        }

        log_err("Issue command \"link options ltpudp\" or \"interface options ltpudp\" for list of valid parameters");
    }

//    argc -= count;
//    if (! p.parse(argc, argv, invalidp)) {
//        log_err("Unrecognized parameter or invalid value: %s", *invalidp);
//        //continue processing instead of returning false for error to
//        // prevent aborting on startup for minor errors
//    }

    if (dummy_ion_comp) {
        log_always("configuration parameter \"ion_comp\" is now depricated and can be removed");
    }

    if (use_files_set) {
        params->use_files_xmit_ = use_files;
        params->use_files_recv_ = use_files;
    }

    if (params->remote_engine_id_ == 0) {
        if (tmp_engine_id != 0) {
            params->remote_engine_id_ = tmp_engine_id;

            log_err("Warning - \"remote_engine_id\" not configured but depricated \"engine_id\" was"
                    " - using engine_id value (%zu) as remote_engine_id", params->remote_engine_id_);
        }
    }


    if ((tmp_bucket_type == 0) || (tmp_bucket_type == 1)) {
        params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) tmp_bucket_type;
    } else {
        log_err("parse_params: bucket type must be 0=standard or 1=leaky - using standard");
        params->bucket_type_ = oasys::RateLimitedSocket::STANDARD;
    }

    if (icipher_engine_str.length() > 0) {
        params->inbound_cipher_engine_.assign(icipher_engine_str);
    } else {
        sprintf(icipher_from_engine, "%" PRIu64, params->remote_engine_id_);
        params->inbound_cipher_engine_ = icipher_from_engine;
    } 
    if (ocipher_engine_str.length() > 0) {
        params->outbound_cipher_engine_.assign(ocipher_engine_str);
    } else {
        sprintf(ocipher_from_engine, "%" PRIu64, params->remote_engine_id_);
        params->outbound_cipher_engine_ = ocipher_from_engine;
    } 

    if (params->outbound_cipher_suite_ == 0 || params->outbound_cipher_suite_ == 1 ) {
         if (params->outbound_cipher_key_id_ == -1)return false;
    } 
    if (params->inbound_cipher_suite_ == 0 || params->inbound_cipher_suite_ == 1 ) {
         if (params->inbound_cipher_key_id_ == -1)return false;
    } 

    return true;
};


//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("LTP over UDP Convergence Layer [%s] - valid Link options:\n\n", name());
    buf.append("<next hop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.append("CLA specific options:\n");

    //buf.append("    local_addr <IP address>            - IP address to bind to (not used for a link)\n");
    //buf.append("    local_port <U16>                   - Port to bind to (not used for a link)\n");

    //buf.append("    remote_addr <IP address>           - IP address to connect to (extracted from nexthop)\n");
    //buf.append("    remote_port <U16>                  - Port to connect to (extracted from nexthop (default: 1113))\n");

    buf.append("  UDP related params:\n");
    buf.append("    rate <Rate>                        - transmit rate throttle in bits per second (default: 0 = no throttle)\n");
    buf.append("    bucket_type <0 or 1>               - throttle token bucket type: 0=standard, 1=leaky (default: 0)\n");
    buf.append("    bucket_depth <U64>                 - throttle token bucket depth in bits (default: 524280 = 64K * 8)\n");
    //buf.append("    recvbuf <U32>                      - socket receive buffer size  (default: 0 = operating system managed)\n");
    buf.append("    sendbuf <U32>                      - socket send buffer size  (default: 0 = operating system managed)\n");


    buf.append("  LTP params:\n");
    buf.append("    remote_engine_id <U64>             - LTP Engine ID of remote peer (must be specified)\n");
    buf.append("    engine_id <U64>                    - LTP Engine ID of remote peer [depricated - sets remote_engine_id])\n");
    buf.append("    max_sessions <U32>                 - maximum number of active outgoing LTP sessions (default: 100)\n");
    buf.append("    agg_size <U64>                     - accumulate bundles for an LTP session until agg_size bytes is reached (default: 1000000)\n");
    buf.append("    agg_time <U32>                     - accumulate bundles for an LTP session for up to agg_time milliseconds (default: 500)\n");
    buf.append("    seg_size <U32>                     - maximum size of the data portion of an LTP segment in bytes (sefault: 1400)\n");
    buf.append("    ccsds_compatible <Bool>            - whether to use CCSDS 734.1-B-1 compatiibility which specifies client service ID 2 for Data Aggregation \n");
    buf.append("                                            (default: false  which is compatible with ION thruough at least version 4.4.0)\n");
    buf.append("    inact_intvl <U32>                  - cancel incoming session after inactivity interval in seconds (default: 30)\n"); 
    buf.append("    retran_intvl <U32>                 - retransmit interval in seconds for LTP segments (default: 7)\n"); 
    buf.append("    retran_retries <U32>               - number of retransmit retries before cancelling LTP session (default: 7)\n");
    buf.append("    queued_bytes_quota <Size>          - discard DataSegments if queued bytes exceeds quota (default: 2G; 0=no limit)\n");
    buf.append("    bytes_per_checkpoint <Size>        - when to generate intermediate LTP checkpoints (default: 0=none)\n");
    buf.append("                                             (not supported by ION at least through 3.7.3 / 4.0.2)\n");
    buf.append("                                             (10M is a good starting point for use with DTNME)\n");
    buf.append("    use_files_xmit <Bool>              - whether to use files to store outgoing LTP session data (default: false)\n");
    buf.append("    use_files_recv <Bool>              - whether to use files to store incoming LTP session data (default: false)\n");
    buf.append("    use_files <Bool>                   - overrides and sets both use_files_xmit and use_files_recv to the specified valeu\n");
    buf.append("    keep_aborted_files <Bool>          - whether to keep session files that had BP protocol errors for analysis\n");
    buf.append("    use_diskio_kludge <Bool>           - whether to use Disk I/O kludge that was needed on the development system (Default: false)\n");
    buf.append("    dir_path <directory>               - path to directory to use for storing LTP session data\n");
    buf.append("    hexdump <Bool>                     - whether to log incoming and outgoing data in hex for debugging (default: false)\n");
    buf.append("    comm_aos <Bool>                    - communications AOS flag (false pauses LTP processing) - \"link reconfigure\" can be used to change it\n");
    buf.append("    clear_stats <Bool>                 - used with \"link reconfigure\" to clear LTP statistics between test runs\n");
    buf.append("    dump_sessions <Bool>               - whether to include LTP Session detail in the link dump report (default: true)\n");
    buf.append("    dump_segs <Bool>                   - whether to include LTP Red Segment detail in the link dump report (default:i false)\n");
    buf.append("    xmit_test <I32>                    - Transmit Test modes:   n <= 0     : normal operation (default: 0)\n");
    buf.append("                                                                n >= 1     : drop every nth packet to transmit\n");
    buf.append("    recv_test <I32>                    - Transmit Test modes:   n <= 0     : normal operation (default: 0)\n");
    buf.append("                                                                n >= 1     : drop every nth packet to transmit\n");

#ifdef MSFC_LTP_AUTH
    buf.append("  LTP authentication params (see help security for configuring keys):\n");
    buf.append("    ocipher_suite <I32>                - outgoing cipher suite (default: -1 = none)\n");
    buf.append("    ocipher_keyid <I32>                - outgoing cipher suite key ID (default: -1 = none)\n");
    buf.append("    ocipher_engine <string>            - outgoing cipher LTP engine name (typically the local engine ID)\n");
    buf.append("    icipher_suite <I32>                - incoming cipher suite (default: -1 = none)\n");
    buf.append("    icipher_keyid <I32>                - incoming cipher suite key ID (default: -1 = none)\n");
    buf.append("    icipher_engine <string>            - incoming cipher LTP engine name (typically the remote engine ID)\n");
#endif


    buf.append("\nOptions for all links:\n");
   
    buf.append("    remote_eid <Endpoint ID>           - Remote Endpoint ID\n");
    buf.append("    reliable <Bool>                    - Whether the CLA is considered reliable (default: false)\n");
    buf.append("    nexthop <ip_address:port>          - IP address and port of remote node (positional in link add command)\n");
    buf.append("    mtu <U64>                          - Max size for outgoing bundle triggers proactive fragmentation (default: 0 = no limit)\n");
    buf.append("    min_retry_interval <U32>           - Minimum seconds to try to reconnect (default: 5)\n");
    buf.append("    max_retry_interval <U32>           - Maximum seconds to try to reconnect (default: 600)\n");
    buf.append("    idle_close_time <U32>              - Seconds without receiving data to trigger disconnect (default: 0 = no limit)\n");
    buf.append("    potential_downtime <U32>           - Seconds of potential downtime for routing algorithms (default: 30)\n");
    buf.append("    prevhop_hdr <Bool>                 - Whether to include the Previous Node Block (default: true)\n");
    buf.append("    cost <U32>                         - Abstract figure for use with routing algorithms (default: 100)\n");
    buf.append("    qlimit_enabled <Bool>              - Whether Queued Bundle Limits are in use by router (default: false)\n");
    buf.append("    qlimit_bundles_high <U64>          - Maximum number of bundles to queue on a link (default: 10)\n");
    buf.append("    qlimit_bytes_high <U64>            - Maximum payload bytes to queue on a link (default: 1 MB)\n");
    buf.append("    qlimit_bundles_low <U64>           - Low watermark number of bundles to add more bundles to link queue (default: 5)\n");
    buf.append("    qlimit_bytes_low <U64>             - Low watermark of payload bytes to add more bundles to link queue (default: 512 KB)\n");
    buf.append("    bp6_redirect <link_name>           - Redirect BP6 bundles to specified link (usually for Bundle In Bundle Encapsulation)\n");
    buf.append("    bp7_redirect <link_name>           - Redirect BP7 bundles to specified link (usually for Bundle In Bundle Encapsulation)\n");
    buf.append("\n");

    buf.append("          (parameters of type <U64>, <Rate> and <Size> can include magnitude character (K, M or G):   125G = 125,000,000,000)\n");

    buf.append("\n");
    buf.append("Example:\n");
    buf.append("link add ltp_22 192.168.0.2:1113 ALWAYSON ltpudp remote_engine_id=22 seg_size=16000 rate=150000000\n");
    buf.append("    (create a link named \"ltp_22\" to transmit to peer with LTP Engine ID 22 at IP address 192.168.0.2 port 1113\n");
    buf.append("     using a segment size of 16000 bytes and transmit rate of 150 Mbps)\n");
    buf.append("\n");
    buf.append("NOTE: The local LTP Engine ID is the node number extracted from the configured \"route local_eid_ipn\" parameter\n");
    buf.append("      if not specified using the \"route set local_lt_engine_id\" command in the startup configuration file. \n");
    buf.append("\n");
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("LTP over UDP Convergence Layer [%s] - valid Interface options:\n\n", name());
    buf.appendf("CLA specific options:\n");

    buf.appendf("  UDP related params:\n");
    buf.appendf("    local_addr <IP address>            - IP address of interface on which to listen (default: 0.0.0.0 = all interfaces)\n");
    buf.appendf("    local_port <U16>                   - Port on which to listen (default: 1113)\n");
    buf.appendf("    recvbuf <U32>                      - socket receive buffer size  (default: 0 = operating system managed)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("interface add in_ltp ltpudp local_addr=192.168.0.1 local_port=1113\n");
    buf.appendf("    (create an interface named \"in_ltp\" to listen for LTP over UDP packets on network interface\n");
    buf.appendf("     with IP address 192.168.0.1 using port 1113)\n");
    buf.appendf("\n");
    buf.appendf("NOTE: The local LTP Engine ID is the node number extracted from the configured \"route local_eid_ipn\" parameter\n");
    buf.appendf("      if not specified using the \"route set local_lt_engine_id\" command in the startup configuration file. \n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)

    Params *params = new Params(defaults_);
    const char* invalid;
    if (!parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params->local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    // create a new server socket for the requested interface

    Receiver* receiver = new Receiver(params, this);

    receiver->logpathf("%s/iface/receiver/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params->local_addr_, params->local_port_) != 0) {
        delete params;
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params->remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params->remote_addr_, params->remote_port_) != 0) {
            delete params;
            return false; // error log already emitted
        }
    }
   
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    delete params;

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::interface_activate(Interface* iface)
{
    log_debug("activating interface %s", iface->name().c_str());

    // start listening and then start the thread to loop calling accept()
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt_from_io();
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params * params = &((Receiver*)iface->cl_info())->cla_params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("\tinact_intvl: %d", params->inactivity_intvl_);
    buf->appendf("\tretran_intvl: %d", params->retran_intvl_);
    buf->appendf("\tretran_retries: %d\n", params->retran_retries_);

    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d - socket recvbuf size: %u\n",
                     intoa(params->remote_addr_), params->remote_port_,
                     params->recvbuf_actual_size_);
    } else {
        buf->appendf("\tlistening - socket recvbuf size: %u\n",
                     params->recvbuf_actual_size_);
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = dynamic_cast<Params *>(new_link_params());
    ASSERT(params != nullptr);

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &params->remote_addr_, &params->remote_port_);

    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPUDPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[]) 
{
    (void) link;

    const char* invalid;

    Params* official_params = (Params*)link->cl_info();

    Params* new_params = new Params();
    *new_params = *official_params;

    if (! parse_params(new_params, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }

    // let the Sender process the new params before we set the shared [old] version
    SPtr_LTPUDPSender sptr_sender;
    sptr_sender = std::dynamic_pointer_cast<LTPUDPSender>(link->contact()->sptr_cl_info());

    if (sptr_sender == nullptr) {
        log_err("%s: Error converting Contact.cl_info to LTPUDPSender*", __func__);
    } else {
        sptr_sender->reconfigure(new_params);
    }

    *official_params = *new_params;

    delete new_params;

    if (official_params->clear_stats_) {
        BundleDaemon::instance()->ltp_engine()->clear_stats(official_params->remote_engine_id_);
        official_params->clear_stats_ = false;
    }

    BundleDaemon::instance()->ltp_engine()->link_reconfigured(official_params->remote_engine_id_);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                         AttributeVector& params)
{
    Params* official_params = (Params*)link->cl_info();

    Params* new_params = new Params();
    *new_params = *official_params;


    AttributeVector::iterator iter;

    for (iter = params.begin(); iter != params.end(); iter++) {
        if (iter->name().compare("comm_aos") == 0) { 
            new_params->comm_aos_ = iter->bool_val();
            log_debug("reconfigure_link - new comm_aos = %s", new_params->comm_aos_ ? "true": "false");

        } else if (iter->name().compare("rate") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                new_params->rate_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                new_params->rate_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                new_params->rate_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                new_params->rate_ = iter->int64_val();
            }
            log_debug("reconfigure_link - new rate = %" PRIu64, new_params->rate_);

        } else if (iter->name().compare("bucket_depth") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                new_params->bucket_depth_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                new_params->bucket_depth_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                new_params->bucket_depth_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                new_params->bucket_depth_ = iter->int64_val();
            }
            log_debug("reconfigure_link - new bucket depth = %" PRIu64, new_params->bucket_depth_);

        } else if (iter->name().compare("bucket_type") == 0) { 
            new_params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) iter->int_val();
            log_debug("reconfigure_link - new bucket type = %d", (int) new_params->bucket_type_);

        } else if (iter->name().compare("clear_stats") == 0) { 
            new_params->clear_stats_ = iter->bool_val();
        }
        // any others to change on the fly through the External Router interface?

        else {
            log_crit("reconfigure_link - unknown parameter: %s", iter->name().c_str());
        }
    }    

    // let the Sender process the new params before we set the shared [old] version
    SPtr_LTPUDPSender sptr_sender;
    sptr_sender = std::dynamic_pointer_cast<LTPUDPSender>(link->contact()->sptr_cl_info());

    if (sptr_sender == nullptr) {
        log_err("%s: Error converting Contact.cl_info to LTPUDPSender*", __func__);
    } else {
        sptr_sender->reconfigure(new_params);
    }

    *official_params = *new_params;
    delete new_params;

    if (official_params->clear_stats_) {
        BundleDaemon::instance()->ltp_engine()->clear_stats(official_params->remote_engine_id_);
        official_params->clear_stats_ = false;
    }

    BundleDaemon::instance()->ltp_engine()->link_reconfigured(official_params->remote_engine_id_);

}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    Params* params = (Params*)link->cl_info();

    buf->appendf("remote_engine_id: %ld\n",params->remote_engine_id_);
    buf->appendf("local_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("remote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);


    SPtr_LTPUDPSender sptr_sender;
    sptr_sender = std::dynamic_pointer_cast<LTPUDPSender>(link->contact()->sptr_cl_info());

    if (sptr_sender == nullptr) {
        log_err("%s: Error converting Contact.cl_info to LTPUDPSender*", __func__);

        buf->appendf("rate: %" PRIu64 " (%s)\n", params->rate_, FORMAT_AS_RATE(params->rate_).c_str());
        if (0 == params->bucket_type_) 
            buf->appendf("bucket_type: TokenBucket\n");
        else
            buf->appendf("bucket_type: TokenBucketLeaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", params->bucket_depth_);
    } else {
        sptr_sender->dump_link(buf);
    }

    buf->appendf("inact_intvl: %u seconds\n", params->inactivity_intvl_);
    buf->appendf("retran_intvl: %u seconds\n", params->retran_intvl_);
    buf->appendf("retran_retries: %u\n", params->retran_retries_);
    buf->appendf("agg_size: %zu (%s) bytes\n", params->agg_size_, FORMAT_WITH_MAG(params->agg_size_).c_str());
    buf->appendf("agg_time: %u milliseconds\n", params->agg_time_);
    buf->appendf("seg_size: %u bytes\n", params->seg_size_);
    buf->appendf("ccsds_compatible: %s\n", params->ccsds_compatible_?"true":"false");
    buf->appendf("max_sessions: %u\n", params->max_sessions_);
    buf->appendf("hexdump: %s\n", params->hexdump_?"true":"false");
    buf->appendf("use_files_xmit: %s\n", params->use_files_xmit_?"true":"false");
    buf->appendf("use_files_recv: %s\n", params->use_files_recv_?"true":"false");
    buf->appendf("keep_aborted_files: %s\n", params->keep_aborted_files_?"true":"false");
    buf->appendf("use_diskio_kludge: %s\n", params->use_diskio_kludge_?"true":"false");


    if (params->dir_path_.length() == 0) {
        buf->appendf("dir_path: %s    [full paths:  ./tmp_ltp_<engine_id>/<files> ]\n", 
                     params->dir_path_.c_str());
    } else {
        buf->appendf("dir_path: %s    [full paths:  %s/tmp_ltp_<engine_id>/<files> ]\n", 
                     params->dir_path_.c_str(),  params->dir_path_.c_str());
    }


    buf->appendf("bytes_per_checkpoint: %zu  (%s)\n", params->bytes_per_checkpoint_, 
                 FORMAT_WITH_MAG(params->bytes_per_checkpoint_).c_str());
    buf->appendf("queued_bytes_quota: %zu  (%s)\n", params->ltp_queued_bytes_quota_, 
                 FORMAT_WITH_MAG(params->ltp_queued_bytes_quota_).c_str());
    buf->appendf("xmit_test: %d  (0=normal; >=1 = drop every nth packet to transmit))\n", params->xmit_test_);
    buf->appendf("recv_test: %d  (0=normal; >=1 = drop every nth packet received))\n", params->recv_test_);
    buf->appendf("dump_sessions: %s\n", params->dump_sessions_?"true":"false");
    buf->appendf("dump_segs: %s\n", params->dump_segs_?"true":"false");
    buf->appendf("comm_aos: %s\n", params->comm_aos_?"true":"false");

    if (params->outbound_cipher_suite_ != -1) {
        buf->appendf("outbound_cipher_suite: %d\n",(int) params->outbound_cipher_suite_);
        if (params->outbound_cipher_suite_ != 255) {
            buf->appendf("outbound_cipher_key_id: %d\n",(int) params->outbound_cipher_key_id_);
            buf->appendf("outbound_cipher_engine_: %s\n", params->outbound_cipher_engine_.c_str());
        }
    }

    if (params->inbound_cipher_suite_ != -1) {
        buf->appendf("inbound_cipher_suite: %d\n",(int) params->inbound_cipher_suite_);
        if (params->inbound_cipher_suite_ != 255) {
            buf->appendf("inbound_cipher_key_id_: %d\n",(int) params->inbound_cipher_key_id_);
            buf->appendf("inbound_cipher_engine_: %s\n", params->inbound_cipher_engine_.c_str());
        }
    }

    if (sptr_sender != nullptr) {
        sptr_sender->dump_stats(buf);
    }

    BundleDaemon::instance()->ltp_engine()->dump_link_stats(params->remote_engine_id_, buf);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    Params* params = (Params*)link->cl_info();

    if (params->remote_engine_id_ == 0) {
        log_err("LTPUDPConvergenceLayer::open_contact: "
              "remote_engine_id not configured - aboritng");
        return false;
    }

    log_debug("LTPUDPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());


    // Initialize and start the LTPUDPSender thread
    
    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("can't lookup hostname in next hop address '%s'",
                link->nexthop());
        return false;
    }

    // if the port wasn't specified, use the default
    if (port == 0) {
        port = LTPUDPCL_DEFAULT_PORT;
    }

    next_hop_addr_  = addr;
    next_hop_port_  = port;
    next_hop_flags_ = MSG_DONTWAIT;

    // create a new sender
    SPtr_LTPUDPSender sptr_sender = std::make_shared<LTPUDPSender>(contact, this);

    if (!sptr_sender->init(params, addr, port)) {
        log_err("error initializing LTP Sender scoket");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        return false;
    }

    // get a pointer as a generic LTPCLSenderIF that the LTPEngine understands
    SPtr_LTPCLSenderIF sptr_ltpclif = std::static_pointer_cast<LTPCLSenderIF>(sptr_sender);

    if (BundleDaemon::instance()->ltp_engine()->register_engine(sptr_ltpclif)) {
        sptr_sender->start();

        // get a pointer as a generic CLInfo that the Contact understands
        SPtr_CLInfo sptr_clinfo = std::static_pointer_cast<CLInfo>(sptr_sender);
        contact->set_sptr_cl_info(sptr_clinfo);

        BundleDaemon::post(new ContactUpEvent(link->contact()));

        return true;
    } else {
        log_err("Unable to register remote LTP Engine ID (%" PRIu64 ") - already registered by a different link",
                params->remote_engine_id_);

        return false;
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    SPtr_LTPUDPSender sptr_sender;
    sptr_sender = std::dynamic_pointer_cast<LTPUDPSender>(contact->sptr_cl_info());


    if (sptr_sender == nullptr) {
        log_err("%s: Error converting Contact.cl_info to LTPUDPSender*", __func__);
    } else {

        log_always("dzdebug - %s - unregister LTPCL sender", __func__);


        sptr_sender->shutdown();

        BundleDaemon::instance()->ltp_engine()->unregister_engine(sptr_sender->Remote_Engine_ID());

        contact->release_sptr_cl_info();
    }

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;
    (void) bundle;
    return;
}

 //----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::pop_queued_bundle(const LinkRef& link)
{
    bool result = false;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
   
    static int counter = 0;

    Params* params = (Params*)link->cl_info();

    if (!params) return result;

    if (!params->comm_aos_) return result;

    BundleRef bref = link->queue()->front();
    if (bref != NULL) {
        log_debug("About to send a bundle (%d)", counter);
        counter++;
        result = (BundleDaemon::instance()->ltp_engine()->send_bundle(bref, params->remote_engine_id_) > 0);
    }

    return result;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::Receiver(LTPUDPConvergenceLayer::Params* params, LTPUDPConvergenceLayer * parent)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/ltpudp/receiver")),
      UDPClient("/dtn/cl/ltpudp/receiver"),
      Thread("LTPUDPConvergenceLayer::Receiver")
{
    logfd_         = false;
    cla_params_    = *params;
    parent_        = parent;

    // bump up the receive buffer size
    params_.recv_bufsize_ = cla_params_.recvbuf_;

}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::~Receiver()
{
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::run()
{
    char threadname[16] = "LTPCLARcvr";
    pthread_setname_np(pthread_self(), threadname);
   
    SPtr_LTPEngine ltp_engine = BundleDaemon::instance()->ltp_engine();

    int result;
    socklen_t len = sizeof(result);
    if (::getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &result, &len) != 0) {
        log_err("LTPUDP Receiver  - error getting socket receive buffer length");
    } else {
        log_always("LTPUDP Receiver socket buffer length = %d", result);
    }
    cla_params_.recvbuf_actual_size_ = result;



    int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char buf[MAX_UDP_PACKET];

    while (!should_stop()) {
        
        ret = recvfrom((char*)buf, MAX_UDP_PACKET, 0, &addr, &port); 
        if (ret <= 0) {    
            if (errno == EINTR) {
                continue;
            }
            log_err("error in recvfrom(): %d %s", 
                    errno, strerror(errno));
            close();
            break;
        }
        
        if (!should_stop() ) {
            ltp_engine->post_data (buf, ret);
        }
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::check_ready_for_bundle()
{
    bool result = false;
    if (ready_for_bundles_) {
        result = parent_->pop_queued_bundle(link_ref_);
    }
    return result;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::PostTransmitProcessing(BundleRef& bref, bool isred, 
                                                       uint64_t expected_bytes,  bool success)
{
    LinkRef link = contact_->link();

    log_debug("PostTransmitProcessing..."); 

    // xmit blocks always deleted in LTPEngine::process_bundles()
    bool blocks_deleted = true;

    if (expected_bytes > 0) { 
        log_debug("Session LTPUDPSender PostTransmitProcessing (%s) = %d", (isred ? "red": "green"),(int) expected_bytes);
        if (isred) {
            BundleDaemon::post(
                new BundleTransmittedEvent(bref.object(), contact_, 
                                           link, expected_bytes,
                                           expected_bytes, success, blocks_deleted));
        } else  {
            //XXX/dz TODO: green is not really reliable 
            // (how would link distinguish between red/green in the is_reliable() function?
            // add parameter to the event?
            BundleDaemon::post(
                new BundleTransmittedEvent(bref.object(), contact_, 
                                           link, expected_bytes, 
                                           expected_bytes, success, blocks_deleted));
        }
    } else {
            // signaling failure so External Router can reroute the bundle
            log_debug("Session LTPUDPSender PostTransmitProcessing = failure");
            BundleDaemon::post(
                new BundleTransmittedEvent(bref.object(), contact_, 
                                           link, 0, 0, success, blocks_deleted));
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::LTPUDPSender::Delete_Xmit_Blocks(const BundleRef&  bref)
{
    LinkRef link = contact_->link();
    BundleProtocol::delete_blocks(bref.object(), link);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::AOS()
{
    return params_->comm_aos_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::LTPUDPSender::Inbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::LTPUDPSender::Inbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::LTPUDPSender::Remote_Engine_ID()
{
    return params_->remote_engine_id_;
}
//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::LTPUDPSender::Inbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::LTPUDPSender::Max_Sessions()
{
    return params_->max_sessions_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::LTPUDPSender::Agg_Time()
{
    return params_->agg_time_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::LTPUDPSender::Seg_Size()
{
    return params_->seg_size_;
}
//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::LTPUDPSender::Agg_Size()
{
    return params_->agg_size_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::CCSDS_Compatible()
{
    return params_->ccsds_compatible_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::LTPUDPSender::Ltp_Queued_Bytes_Quota()
{
    return params_->ltp_queued_bytes_quota_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::LTPUDPSender::Bytes_Per_CheckPoint()
{
    return params_->bytes_per_checkpoint_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::Use_Files_Xmit()
{
    return params_->use_files_xmit_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::Use_Files_Recv()
{
    return params_->use_files_recv_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::Keep_Aborted_Files()
{
    return params_->keep_aborted_files_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::Use_DiskIO_Kludge()
{
    return params_->use_diskio_kludge_;
}

//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::LTPUDPSender::Dir_Path()
{
    return params_->dir_path_;
}

//----------------------------------------------------------------------
int32_t
LTPUDPConvergenceLayer::LTPUDPSender::Recv_Test()
{
    return params_->recv_test_;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::LTPUDPSender::Del_From_Queue(const BundleRef& bref)
{
    link_ref_->del_from_queue(bref);
}

//----------------------------------------------------------------------
size_t LTPUDPConvergenceLayer::LTPUDPSender::Get_Bundles_Queued_Count()
{
    return link_ref_->queue()->size();
}

//----------------------------------------------------------------------
size_t LTPUDPConvergenceLayer::LTPUDPSender::Get_Bundles_InFlight_Count()
{
    return link_ref_->inflight()->size();
}

//----------------------------------------------------------------------
bool LTPUDPConvergenceLayer::LTPUDPSender::Del_From_InFlight_Queue(const BundleRef& bref)
{
    return link_ref_->del_from_inflight(bref);
}
//----------------------------------------------------------------------
bool LTPUDPConvergenceLayer::LTPUDPSender::Dump_Sessions()
{
    return params_->dump_sessions_;
}
//----------------------------------------------------------------------
bool LTPUDPConvergenceLayer::LTPUDPSender::Dump_Segs()
{
    return params_->dump_segs_;
}
//----------------------------------------------------------------------
bool LTPUDPConvergenceLayer::LTPUDPSender::Hex_Dump()
{
    return params_->hexdump_;
}
//----------------------------------------------------------------------
void LTPUDPConvergenceLayer::LTPUDPSender::Add_To_Inflight(const BundleRef& bref) 
{
    contact_->link()->add_to_inflight(bref);
}
//----------------------------------------------------------------------
SPtr_BlockInfoVec LTPUDPConvergenceLayer::LTPUDPSender::GetBlockInfoVec(Bundle * bundle)
{
    return bundle->xmit_blocks()->find_blocks(contact_->link());
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::LTPUDPSender::Inactivity_Intvl()
{
    return params_->inactivity_intvl_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::LTPUDPSender::Retran_Retries()
{
    return params_->retran_retries_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::LTPUDPSender::Retran_Intvl()
{
    return params_->retran_intvl_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::LTPUDPSender::Outbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::LTPUDPSender::Outbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::LTPUDPSender::Outbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPSender::LTPUDPSender(const ContactRef& contact,LTPUDPConvergenceLayer * parent)
    : Logger("LTPUDPConvergenceLayer::LTPUDPSender", "/dtn/cl/ltpudp/sender/%p", this),
      Thread("LTPUDPConvergenceLayer::LTPUDPSender"),
      admin_eventq_("/dtn/cl/ltpudp/sender/adminq"),
      ds_high_eventq_("/dtn/cl/ltpudp/sender/dshighq"),
      ds_low_eventq_("/dtn/cl/ltpudp/sender/dslowq"),
      socket_(logpath_),
      contact_(contact.object(), "LTPUDPCovergenceLayer::LTPUDPSender")
{
    rate_socket_              = nullptr;
    link_ref_                 = contact_->link();
    ready_for_bundles_        = false;
    parent_                   = parent;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPSender::~LTPUDPSender()
{
    shutdown();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::shutdown()
{
    poller_->shutdown();

    // wait for administrative type segments to be transmitted
    if (params_->comm_aos_) {
        while (admin_eventq_.size() > 0) {
            usleep(100000);
        }
    }

    this->set_should_stop();
    while (!is_stopped()) {
        usleep(10000);
    }

    oasys::ScopeLock l(&eventq_lock_, __func__);

    MySendObject *event;
    while (admin_eventq_.try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (ds_high_eventq_.try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (ds_low_eventq_.try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::init(Params* params, in_addr_t addr, u_int16_t port)
{
    params_ = params;

    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);
    socket_.params_.send_bufsize_ = params->sendbuf_;
    socket_.init_socket();

    int bufsize = 0; 
    socklen_t optlen = sizeof(bufsize);
    if (::getsockopt(socket_.fd(), SOL_SOCKET, SO_SNDBUF, &bufsize, &optlen) < 0) {
        log_warn("error getting SO_SNDBUF: %s", strerror(errno));
    } else {
        log_always("LTPUDPConvergenceLayer::LTPUDPSender socket send_buffer size = %d", bufsize);
    }

    // do not bind or connect the socket
    if (params->rate_ != 0) {

        rate_socket_ = std::unique_ptr<oasys::RateLimitedSocket> (
                                         new oasys::RateLimitedSocket(logpath_, 0, 0, 
                                                                      params->bucket_type_) );
 
        rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        rate_socket_->bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params->bucket_depth_);
        }
 
        log_debug("init: LTPUDPSender rate controller after initialization: rate %" PRIu64 " depth %" PRIu64,
                  rate_socket_->bucket()->rate(),
                  rate_socket_->bucket()->depth());
    }

    use_rate_socket_ = (params->rate_ > 0);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::run()
{
    char threadname[16] = "LTPCLASndr";
    pthread_setname_np(pthread_self(), threadname);
   
    int ret;

    MySendObject* event; 
    bool got_event = false;

    poller_ = std::unique_ptr<SendPoller>(new SendPoller(this, contact_->link()));
    poller_->start();


    // only polling if all queues go empty
    admin_eventq_.notify_when_empty();
    ds_high_eventq_.notify_when_empty();
    ds_low_eventq_.notify_when_empty();


    struct pollfd pollfds[3];

    struct pollfd* admin_event_poll = &pollfds[0];
    admin_event_poll->fd = admin_eventq_.read_fd();
    admin_event_poll->events = POLLIN; 
    admin_event_poll->revents = 0;

    struct pollfd* ds_high_event_poll = &pollfds[1];
    ds_high_event_poll->fd = ds_high_eventq_.read_fd();
    ds_high_event_poll->events = POLLIN; 
    ds_high_event_poll->revents = 0;

    struct pollfd* ds_low_event_poll = &pollfds[2];
    ds_low_event_poll->fd = ds_low_eventq_.read_fd();
    ds_low_event_poll->events = POLLIN; 
    ds_low_event_poll->revents = 0;

    const char* event_priority = nullptr;
    bool okay_to_send = true;

    ssize_t cc = 0;

    while (!should_stop()) {

        if (!got_event) {
            ret = oasys::IO::poll_multiple(pollfds, 3, 10, NULL);

            if (ret == oasys::IOINTR) {
                log_err("run(): LTPUDPSender interrupted");
                set_should_stop();
                continue;
            }

            if (ret == oasys::IOERROR) {
                log_err("run(): LTPUDPSender I/O error");
                set_should_stop();
                continue;
            }
        }


        if (!params_->comm_aos_)
        {
            // not currently in contact so sleep and try again later
            usleep(100000);
            continue; 
        }

        if(!contact_->link()->get_contact_state()){
            // not currently in configured for contact so sleep and try again later
            usleep(100000);
            continue;
        }

        // process the event with the highest priority
        eventq_lock_.lock(__func__);
        got_event = false;
        if (admin_eventq_.try_pop(&event)) {
            event_priority = "high priority admin";
            got_event = true;
        } else if (ds_high_eventq_.try_pop(&event)) {
            event_priority = "higher priority DS resend";
            got_event = true;
        } else if (ds_low_eventq_.try_pop(&event)) {
            event_priority = "low priority DS send";
            got_event = true;
        }
        eventq_lock_.unlock();

        if (got_event) {

            ASSERT(event != NULL)

            okay_to_send = true;

            std::string* xmit_str = nullptr;

            ssize_t xmit_len;
            const char* str_data;
            if (event->str_data_) {
                str_data = event->str_data_->data();
                xmit_len = event->str_data_->size();

                if (event->timer_) {
                    event->timer_->start();
                }
            } else {
                // prevent LTPEngine from modifying this segment 
                // while it is being processed for transmission
                // (LTP Engine can change it to a checkpoint while it is queued,
                //  but, now it is too late and it will have to resend it)
                oasys::ScopeLock scoplok(event->sptr_ds_seg_->lock(), __func__);

                // The LTP session may have been cancelled or completed
                // while this segment was queued for transmission
                if (event->sptr_ds_seg_->IsDeleted()) {
                    if (event->sptr_ds_seg_->Retransmission_Timer_Raw_Ptr() != nullptr) {
                        // XXX/dz This should not happen [any longer ;)]
                        //        but if the timer is not cancelled then the segment and the timer leak
                        log_err("not sending deleted segment %s-%zu Chkpt: %zu  that has a Retransmission Timer",
                                   event->sptr_ds_seg_->session_key_str().c_str(), 
                                   event->sptr_ds_seg_->Offset(),
                                   event->sptr_ds_seg_->Checkpoint_ID());

                        event->sptr_ds_seg_->Set_Deleted();  // cancels the timer
                    }

                    okay_to_send = false;
                } else {
                    // xmit_str maintains the new string in scope until it is transmitted
                    // NOTE: the xmit_len may differ from event->bytes_queued if the
                    //       LTPEngine changed it to a checkpoint while it was queued
                    xmit_str = event->sptr_ds_seg_->asNewString();
                    if (xmit_str == nullptr) {
                        // Error reading from an LTP Session Data file
                        okay_to_send = false;
                        SPtr_LTPEngine ltp_engine = BundleDaemon::instance()->ltp_engine();

                        ltp_engine->force_cancel_by_sender(event->sptr_ds_seg_.get());
                    } else {
                        str_data = xmit_str->data();
                        xmit_len = xmit_str->size();

                        if (event->timer_) {
                            event->timer_->start();
                        } else {
                            // in case the segment was changed to a Checkpoint while it was queued
                            event->sptr_ds_seg_->Start_Retransmission_Timer();
                        }

                        event->sptr_ds_seg_->Set_Queued_To_Send(false);
                    }
                }
            }


            if (okay_to_send && (params_->xmit_test_ > 0)) {
                // drop every nth packet
                okay_to_send = (packets_transmitted_ % (uint32_t) params_->xmit_test_) != 
                                       (uint32_t) (params_->xmit_test_ - 1);

                if (!okay_to_send) {
                    ++packets_dropped_for_xmit_test_;
                }
            }
            ++packets_transmitted_;


            if (okay_to_send) {
                if (use_rate_socket_) {
                    cc = rate_socket_->sendto((char*)str_data, xmit_len, 0,
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              true);  // block until data can be sent
                } else {
                    cc = socket_.sendto((char*)str_data, xmit_len, 0,
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (cc == xmit_len) {
                    //log_debug("Send: transmitted %s %zd byte segment to %s:%d",
                    //          event_priority, cc, intoa(params_->remote_addr_), params_->remote_port_);
                } else {
                    log_err("Send: error sending %s segment to %s:%d (wrote %zd/%zd): %s",
                            event_priority, intoa(params_->remote_addr_), params_->remote_port_,
                            cc, xmit_len, strerror(errno));
                }
            }


            eventq_lock_.lock(__func__);
            ASSERT(eventq_bytes_ >= event->bytes_queued_);
            eventq_bytes_ -= event->bytes_queued_;
            eventq_lock_.unlock();


            // clean up
            if (event->str_data_) {
                delete event->str_data_;
            } else if (xmit_str != nullptr) {
                delete xmit_str;
            }

            event->sptr_ds_seg_.reset();
            delete event;
        }
    }
}
 
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::Send_Admin_Seg_Highest_Priority(std::string * send_data, SPtr_LTPTimer timer, bool back)
{
    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_       = send_data;
        obj->bytes_queued_   = send_data->length();
        obj->timer_          = timer;

        oasys::ScopeLock l(&eventq_lock_, __func__);

	    if (back) {
	        admin_eventq_.push_back(obj);
        } else {
	        admin_eventq_.push_front(obj);
        }

        eventq_bytes_ += obj->bytes_queued_;
        if (eventq_bytes_ > eventq_bytes_max_)
        {
            eventq_bytes_max_ = eventq_bytes_;
        }

    } else {
        delete send_data;
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::Send_DataSeg_Higher_Priority(SPtr_LTPDataSegment sptr_ds_seg, SPtr_LTPTimer timer)
{
    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_       = nullptr;
        obj->bytes_queued_   = sptr_ds_seg->Transmit_Length();
        obj->sptr_ds_seg_    = sptr_ds_seg;
        obj->timer_          = timer;

        oasys::ScopeLock l(&eventq_lock_, __func__);

        ds_high_eventq_.push_back(obj);

        eventq_bytes_ += obj->bytes_queued_;
        if (eventq_bytes_ > eventq_bytes_max_)
        {
            eventq_bytes_max_ = eventq_bytes_;
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::Send_DataSeg_Low_Priority(SPtr_LTPDataSegment sptr_ds_seg, SPtr_LTPTimer timer)
{
    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_       = nullptr;
        obj->bytes_queued_   = sptr_ds_seg->Transmit_Length();
        obj->sptr_ds_seg_    = sptr_ds_seg;
        obj->timer_          = timer;

        oasys::ScopeLock l(&eventq_lock_, __func__);

        ds_low_eventq_.push_back(obj);

        eventq_bytes_ += obj->bytes_queued_;
        if (eventq_bytes_ > eventq_bytes_max_)
        {
            eventq_bytes_max_ = eventq_bytes_;
        }
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::LTPUDPSender::reconfigure(Params* new_params)
{
    if (new_params->rate_ == 0) {
        // possibly switching from the rate socket to the regular socket
        // should be no issue  - just change the flag
        use_rate_socket_ = false;
    } else {
        if (!use_rate_socket_) {
            // not currently using the rate socket so we can destroy it if it exists
            // and generate a new one in case the bucket type is being changed
            rate_socket_.reset();
        }

        if (rate_socket_ == nullptr) {
            rate_socket_ = std::unique_ptr<oasys::RateLimitedSocket> (
                                         new oasys::RateLimitedSocket(logpath_, 0, 0, 
                                                                      new_params->bucket_type_) );
 
            rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        } else if (new_params->bucket_type_ != params_->bucket_type_) {
            // allow updating the rate and the bucket depth on the fly but not the bucket type
            // >> set rate to zero and then reconfigure the bucket type to change it
            log_err("Warning - Rate Limited Socket's Bucket Type cannot be changed on the fly - set rate to zero first");
        }

        rate_socket_->bucket()->set_rate(new_params->rate_);

        if (new_params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(new_params->bucket_depth_);
        }
 
        log_debug("reconfigure: new rate controller values: rate %" PRIu64 " depth %" PRIu64,
                  rate_socket_->bucket()->rate(),
                  rate_socket_->bucket()->depth());

        use_rate_socket_ = true;
    }

    return true;
}


//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::dump_link(oasys::StringBuffer* buf)
{
    if (!use_rate_socket_) {
        buf->appendf("rate: unlimited\n");
    } else {
        uint64_t rate =  rate_socket_->bucket()->rate();
        buf->appendf("rate: %" PRIu64 " (%s)\n", rate, FORMAT_AS_RATE(rate).c_str());
        if (rate_socket_->bucket()->is_type_standard())
            buf->appendf("bucket_type: Standard\n");
        else
            buf->appendf("bucket_type: Leaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", rate_socket_->bucket()->depth());
    }
}


//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::dump_stats(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&eventq_lock_, __func__);

    buf->appendf("\nTransmit queues - admin: %zu  resend DS: %zu  send DS: %zu   Total bytes: %zu (%s) Max: %zu (%s)\n\n", 
                 admin_eventq_.size(), ds_high_eventq_.size(), ds_low_eventq_.size(), 
                 eventq_bytes_, FORMAT_WITH_MAG(eventq_bytes_).c_str(),
                 eventq_bytes_max_, FORMAT_WITH_MAG(eventq_bytes_max_).c_str());
    
}


//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPSender::SendPoller::SendPoller(LTPUDPSender* parent, const LinkRef& link)
    : Logger("LTPUDPConvergenceLayer::LTPUDPSender::SendPoller",
             "/dtn/cl/ltpudp/sender/poller/%p", this),
      Thread("LTPUDPConvergenceLayer::LTPUDPSender::SendPoller")
{
    log_debug("Poller created");
    parent_   = parent;
    link_ref_ = link;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPSender::SendPoller::~SendPoller()
{
    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::SendPoller::shutdown()
{
    this->set_should_stop();

    while (!is_stopped()) {
        usleep(10000);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::LTPUDPSender::SendPoller::run()
{
    char threadname[16] = "LTPCLASndrPollr";
    pthread_setname_np(pthread_self(), threadname);
   
    log_debug("Poller started");
    while (!should_stop()) {
        if (!parent_->check_ready_for_bundle()) {
            usleep(10000);
        }
    }
}




} // namespace dtn

