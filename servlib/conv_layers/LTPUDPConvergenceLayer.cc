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
    a->process("hexdump", &hexdump_);
    a->process("comm_aos", &comm_aos_);
    a->process("max_sessions", &max_sessions_);
    a->process("seg_size", &seg_size_);
    a->process("agg_size", &agg_size_);
    a->process("agg_time", &agg_time_);
    a->process("ccsds_compatible", &ccsds_compatible_);
    a->process("sendbuf", &sendbuf_);
    a->process("recvbuf", &recvbuf_);
    // clear_stats_ not needed here
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

    std::string ocipher_engine_str = "";
    std::string icipher_engine_str = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    //p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    //p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::UIntOpt("bucket_type", &tmp_bucket_type));
    p.addopt(new oasys::UInt64Opt("rate", &params->rate_));
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
    buf.appendf("<next hop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.appendf("CLA specific options:\n");

    //buf.appendf("    local_addr <IP address>            - IP address to bind to (not used for a link)\n");
    //buf.appendf("    local_port <U16>                   - Port to bind to (not used for a link)\n");

    //buf.appendf("    remote_addr <IP address>           - IP address to connect to (extracted from nexthop)\n");
    //buf.appendf("    remote_port <U16>                  - Port to connect to (extracted from nexthop (default: 1113))\n");

    buf.appendf("  UDP related params:\n");
    buf.appendf("    rate <U64>                         - transmit rate throttle in bits per second (default: 0 = no throttle)\n");
    buf.appendf("    bucket_type <0 or 1>               - throttle token bucket type: 0=standard, 1=leaky (default: 0)\n");
    buf.appendf("    bucket_depth <U64>                 - throttle token bucket depth in bits (default: 524280 = 64K * 8)\n");
    //buf.appendf("    recvbuf <U32>                      - socket receive buffer size  (default: 0 = operating system managed)\n");
    buf.appendf("    sendbuf <U32>                      - socket send buffer size  (default: 0 = operating system managed)\n");


    buf.appendf("  LTP params:\n");
    buf.appendf("    remote_engine_id <U64>             - LTP Engine ID of remote peer (must be specified)\n");
    buf.appendf("    engine_id <U64>                    - LTP Engine ID of remote peer [depricated - sets remote_engine_id])\n");
    buf.appendf("    max_sessions <U32>                 - maximum number of active outgoing LTP sessions (sefault: 100)\n");
    buf.appendf("    agg_size <U64>                     - accumulate bundles for an LTP session until agg_size bytes is reached (default: 1000000)\n");
    buf.appendf("    agg_time <U32>                     - accumulate bundles for an LTP session for up to agg_time milliseconds (default: 500)\n");
    buf.appendf("    seg_size <U32>                     - maximum LTP segment size in bytes (sefault: 1400)\n");
    buf.appendf("    ccsds_compatible <Bool>            - whether to use CCSDS 734.1-B-1 compatiibility which specifies client service ID 2 for Data Aggregation \n");
    buf.appendf("                                            (default: false  which is compatible with ION thruough at least version 4.4.0)\n");
    buf.appendf("    inact_intvl <U32>                  - cancel incoming session after inactivity interval in seconds (default: 30)\n"); 
    buf.appendf("    retran_intvl <U32>                 - retransmit interval in seconds for LTP segments (default: 7)\n"); 
    buf.appendf("    retran_retries <U32>               - number of retransmit retries before cancelling LTP session (default: 7)\n");
    buf.appendf("    hexdump <Bool>                     - whether to log incoming and outgoing data in hex for debugging (default: false)\n");
    buf.appendf("    comm_aos <Bool>                    - communications AOS flag (false pauses LTP processing) - \"link reconfigure\" can be used to change it\n");
    buf.appendf("    clear_stats <Bool>                 - used with \"link reconfigure\" to clear LTP statistics between test runs\n");

#ifdef MSFC_LTP_AUTH
    buf.appendf("  LTP authentication params (see help security for configuring keys):\n");
    buf.appendf("    ocipher_suite <I32>                - outgoing cipher suite (default: -1 = none)\n");
    buf.appendf("    ocipher_keyid <I32>                - outgoing cipher suite key ID (default: -1 = none)\n");
    buf.appendf("    ocipher_engine <string>            - outgoing cipher LTP engine name (typically the local engine ID)\n");
    buf.appendf("    icipher_suite <I32>                - incoming cipher suite (default: -1 = none)\n");
    buf.appendf("    icipher_keyid <I32>                - incoming cipher suite key ID (default: -1 = none)\n");
    buf.appendf("    icipher_engine <string>            - incoming cipher LTP engine name (typically the remote engine ID)\n");
#endif


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
    buf.appendf("link add ltp_22 192.168.0.2:1113 ALWAYSON ltpudp remote_engine_id=22 seg_size=16000 rate=150000000\n");
    buf.appendf("    (create a link named \"ltp_22\" to transmit to peer with LTP Engine ID 22 at IP address 192.168.0.2 port 1113\n");
    buf.appendf("     using a segment size of 16000 bytes and transmit rate of 150 Mbps)\n");
    buf.appendf("\n");
    buf.appendf("NOTE: The local LTP Engine ID is the node number extracted from the configured \"route local_eid_ipn\" parameter\n");
    buf.appendf("      if not specified using the \"route set local_lt_engine_id\" command in the startup configuration file. \n");
    buf.appendf("\n");
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
    buf->appendf("\tinactivity_intvl: %d", params->inactivity_intvl_);
    buf->appendf("\tretran_intvl: %d", params->retran_intvl_);
    buf->appendf("\tretran_retries: %d\n", params->retran_retries_);

    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected\n");
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
    SenderRef* sref_ptr = dynamic_cast<SenderRef*>(link->contact()->cl_info());

    if (sref_ptr == nullptr) {
        log_err("%s: Error converting Contact.cl_info to SenderRef*", __func__);
    } else {
        sref_ptr->reconfigure(new_params);
    }

    *official_params = *new_params;

    if (official_params->clear_stats_) {
        //XXX/dz TODO move this to the Sender
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
    SenderRef* sref_ptr = dynamic_cast<SenderRef*>(link->contact()->cl_info());

    if (sref_ptr == nullptr) {
        log_err("%s: Error converting Contact.cl_info to SenderRef*", __func__);
    } else {
        sref_ptr->reconfigure(new_params);
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


    SenderRef* sref_ptr = dynamic_cast<SenderRef*>(link->contact()->cl_info());

    if (sref_ptr == nullptr) {
        log_err("%s: Error converting Contact.cl_info to SenderRef*", __func__);

        buf->appendf("transmit rate: %" PRIu64 "\n", params->rate_);
        if (0 == params->bucket_type_) 
            buf->appendf("bucket_type: TokenBucket\n");
        else
            buf->appendf("bucket_type: TokenBucketLeaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", params->bucket_depth_);
    } else {
        sref_ptr->dump_link(buf);
    }

    buf->appendf("inactivity_intvl: %u\n", params->inactivity_intvl_);
    buf->appendf("retran_intvl: %u\n", params->retran_intvl_);
    buf->appendf("retran_retries: %u\n", params->retran_retries_);
    buf->appendf("agg_size: %zu (bytes)\n", params->agg_size_);
    buf->appendf("agg_time: %u (milliseconds)\n", params->agg_time_);
    buf->appendf("seg_size: %u (bytes)\n", params->seg_size_);
    buf->appendf("ccsds_compatible: %s\n", params->ccsds_compatible_?"true":"false");
    buf->appendf("max_sessions: %u\n", params->max_sessions_);
    buf->appendf("comm_aos: %d\n", (int) params->comm_aos_);
    buf->appendf("hexdump: %d\n", (int) params->hexdump_);

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


    // Initialize and start the Sender thread
    
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

    // create a new sender structure
    SenderRef* contact_sref_ptr = new SenderRef(contact, this);
  
    if (!contact_sref_ptr->init(params, addr, port)) {
        log_err("error initializing scoket");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        delete contact_sref_ptr;
        return false;
    }
        
    // create a new reference to the Sender to hand off to the LTP Engine
    // it is up to the LTP Engine to delete this object
    QPtr_LTPCLSenderIF ltp_sref_qptr (new SenderRef(contact_sref_ptr));
    SPtr_LTPCLSenderIF ltp_sref_sptr (std::move(ltp_sref_qptr));

    if (BundleDaemon::instance()->ltp_engine()->register_engine(ltp_sref_sptr)) {
        contact_sref_ptr->start();
        contact->set_cl_info(contact_sref_ptr);

        BundleDaemon::post(new ContactUpEvent(link->contact()));

        return true;
    } else {
        log_err("Unable to register remote LTP Engine ID (%" PRIu64 ") - already registered by a different link",
                params->remote_engine_id_);

        delete contact_sref_ptr;

        return false;
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    SenderRef* sref_ptr = dynamic_cast<SenderRef*>(contact->cl_info());

    if (sref_ptr == nullptr) {
        log_err("%s: Error converting Contact.cl_info to SenderRef*", __func__);
    } else {
        sref_ptr->shutdown();

        BundleDaemon::instance()->ltp_engine()->unregister_engine(sref_ptr->Remote_Engine_ID());

        delete sref_ptr;
        contact->set_cl_info(nullptr);
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

    const ContactRef& contact = link->contact();

    SenderRef* sref_ptr = dynamic_cast<SenderRef*>(contact->cl_info());

    if (sref_ptr == nullptr) {
        log_crit("%s: Error converting Contact.cl_info to SenderRef*", __func__);
    } else {
        BundleRef bref = link->queue()->front();
        if (bref != NULL) {
            log_debug("About to send a bundle (%d)", counter);
            counter++;
            result = (BundleDaemon::instance()->ltp_engine()->send_bundle(bref, params->remote_engine_id_) > 0);
        }
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
LTPUDPConvergenceLayer::Sender::check_ready_for_bundle()
{
    bool result = false;
    if (ready_for_bundles_) {
        result = parent_->pop_queued_bundle(link_ref_);
    }
    return result;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::PostTransmitProcessing(BundleRef& bref, bool isred, 
                                                       uint64_t expected_bytes,  bool success)
{
    LinkRef link = contact_->link();

    log_debug("PostTransmitProcessing..."); 

    // xmit blocks always deleted in LTPEngine::process_bundles()
    bool blocks_deleted = true;

    if (expected_bytes > 0) { 
        log_debug("Session Sender PostTransmitProcessing (%s) = %d", (isred ? "red": "green"),(int) expected_bytes);
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
            log_debug("Session Sender PostTransmitProcessing = failure");
            BundleDaemon::post(
                new BundleTransmittedEvent(bref.object(), contact_, 
                                           link, 0, 0, success, blocks_deleted));
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Delete_Xmit_Blocks(const BundleRef&  bref)
{
    LinkRef link = contact_->link();
    BundleProtocol::delete_blocks(bref.object(), link);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::AOS()
{
    return params_->comm_aos_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::Sender::Inbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::Sender::Inbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::Sender::Remote_Engine_ID()
{
    return params_->remote_engine_id_;
}
//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::Sender::Inbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::Sender::Max_Sessions()
{
    return params_->max_sessions_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::Sender::Agg_Time()
{
    return params_->agg_time_;
}
//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::Sender::Seg_Size()
{
    return params_->seg_size_;
}
//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::Sender::Agg_Size()
{
    return params_->agg_size_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::CCSDS_Compatible()
{
    return params_->ccsds_compatible_;
}

//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::Sender::Ltp_Queued_Bytes_Quota()
{
    return params_->ltp_queued_bytes_quota_;
}

void LTPUDPConvergenceLayer::Sender::Del_From_Queue(const BundleRef& bref)
{
    link_ref_->del_from_queue(bref);
}

size_t LTPUDPConvergenceLayer::Sender::Get_Bundles_Queued_Count()
{
    return link_ref_->queue()->size();
}

size_t LTPUDPConvergenceLayer::Sender::Get_Bundles_InFlight_Count()
{
    return link_ref_->inflight()->size();
}

bool LTPUDPConvergenceLayer::Sender::Del_From_InFlight_Queue(const BundleRef& bref)
{
    return link_ref_->del_from_inflight(bref);
}
//----------------------------------------------------------------------
bool LTPUDPConvergenceLayer::Sender::Hex_Dump()
{
    return params_->hexdump_;
}
//----------------------------------------------------------------------
void LTPUDPConvergenceLayer::Sender::Add_To_Inflight(const BundleRef& bref) 
{
    contact_->link()->add_to_inflight(bref);
}
//----------------------------------------------------------------------
SPtr_BlockInfoVec LTPUDPConvergenceLayer::Sender::GetBlockInfoVec(Bundle * bundle)
{
    return bundle->xmit_blocks()->find_blocks(contact_->link());
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::Sender::Inactivity_Intvl()
{
    return params_->inactivity_intvl_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::Sender::Retran_Retries()
{
    return params_->retran_retries_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::Sender::Retran_Intvl()
{
    return params_->retran_intvl_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
uint32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::Sender(const ContactRef& contact,LTPUDPConvergenceLayer * parent)
    : Logger("LTPUDPConvergenceLayer::Sender", "/dtn/cl/ltpudp/sender/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender"),
      high_eventq_("/dtn/cl/ltpudp/sender/hmsgq"),
      low_eventq_("/dtn/cl/ltpudp/sender/lmsgq"),
      socket_(logpath_),
      contact_(contact.object(), "LTPUDPCovergenceLayer::Sender")
{
    rate_socket_              = nullptr;
    link_ref_                 = contact_->link();
    ready_for_bundles_        = false;
    parent_                   = parent;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::~Sender()
{
    shutdown();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::shutdown()
{
    poller_->shutdown();

    this->set_should_stop();
    while (!is_stopped()) {
        usleep(10000);
    }

    MySendObject *event;
    while (high_eventq_.try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (low_eventq_.try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::init(Params* params, in_addr_t addr, u_int16_t port)
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
        log_always("LTPUDPConvergenceLayer::Sender socket send_buffer size = %d", bufsize);
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
 
        log_debug("init: Sender rate controller after initialization: rate %" PRIu64 " depth %" PRIu64,
                  rate_socket_->bucket()->rate(),
                  rate_socket_->bucket()->depth());
    }

    use_rate_socket_ = (params->rate_ > 0);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::run()
{
    char threadname[16] = "LTPCLASndr";
    pthread_setname_np(pthread_self(), threadname);
   
    struct timespec sleep_time = { 0, 100000000 }; // 1 tenth of a second

    int ret;

    MySendObject *hevent; 
    MySendObject *levent; 
    int cc = 0;
    // block on input from the socket and
    // on input from the bundle event list

    poller_ = std::unique_ptr<SendPoller>(new SendPoller(this, contact_->link()));
    poller_->start();

    struct pollfd pollfds[2];

    struct pollfd* high_event_poll = &pollfds[0];

    high_event_poll->fd = high_eventq_.read_fd();
    high_event_poll->events = POLLIN; 
    high_event_poll->revents = 0;

    struct pollfd* low_event_poll = &pollfds[1];

    low_event_poll->fd = low_eventq_.read_fd();
    low_event_poll->events = POLLIN; 
    low_event_poll->revents = 0;

    while (!should_stop()) {

        ret = oasys::IO::poll_multiple(pollfds, 2, 10, NULL);

        if (ret == oasys::IOINTR) {
            log_err("run(): Sender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("run(): Sender I/O error");
            set_should_stop();
            continue;
        }

        if (!params_->comm_aos_)
        {
            // not currently in contact so sleep and try again later
            nanosleep(&sleep_time,NULL);
            continue; 
        }

        if(!contact_->link()->get_contact_state()){
          nanosleep(&sleep_time,NULL);
          continue;
        }

        // check for an event
        if (high_event_poll->revents & POLLIN) {
            if (high_eventq_.try_pop(&hevent)) {
                ASSERT(hevent != NULL)
        
                if (hevent->timer_) {
                    hevent->timer_->start();
                }
               
                if (use_rate_socket_) {
                    cc = rate_socket_->sendto(const_cast< char * >(hevent->str_data_->data()),
                                              hevent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              true);  // iblock until data can be sent
                } else {
  
                    cc = socket_.sendto(const_cast< char * > (hevent->str_data_->data()), 
                                        hevent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (cc == (int)hevent->str_data_->size()) {
                    log_debug("Send: transmitted high priority %d byte segment to %s:%d",
                              cc, intoa(params_->remote_addr_), params_->remote_port_);
                } else {
                    log_err("Send: error sending high priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, hevent->str_data_->size(), strerror(errno));
                }

                oasys::ScopeLock l(&eventq_lock_, __func__);
                eventq_bytes_ -= hevent->str_data_->length();

                delete hevent->str_data_;
                delete hevent; 
            }

            // loop around to see if there is another high priority segment
            // to transmit before checking for low priority segements 
            continue; 
        }


        // check for a low priority event (new Data Segment)
        if (low_event_poll->revents & POLLIN) {
            if (low_eventq_.try_pop(&levent)) {

                ASSERT(levent != NULL)

                if (levent->timer_) {
                    levent->timer_->start();
                }
               
                if (use_rate_socket_) {
                    cc = rate_socket_->sendto(const_cast< char * >(levent->str_data_->data()),
                                              levent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              true);  // iblock until data can be sent
                } else {
                    cc = socket_.sendto(const_cast< char * > (levent->str_data_->data()), 
                                        levent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (cc == (int)levent->str_data_->size()) {
                    log_debug("Send: transmitted low priority %d byte segment to %s:%d",
                              cc, intoa(params_->remote_addr_), params_->remote_port_);
                } else {
                    log_err("Send: error sending low priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, levent->str_data_->size(), strerror(errno));
                }

                oasys::ScopeLock l(&eventq_lock_, __func__);
                eventq_bytes_ -= levent->str_data_->length();

                delete levent->str_data_;
                delete levent;
            }
        }
    }
}
 
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send(std::string * send_data, SPtr_LTPTimer& timer, bool back)
{
    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_       = send_data;
        obj->timer_          = timer;

        oasys::ScopeLock l(&eventq_lock_, __func__);

	    if (back) {
	        high_eventq_.push_back(obj);
        } else {
	        high_eventq_.push_front(obj);
        }

        eventq_bytes_ += send_data->length();
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
LTPUDPConvergenceLayer::Sender::Send_Low_Priority(std::string * send_data, SPtr_LTPTimer& timer)
{
    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_     = send_data;
        obj->timer_        = timer;

        oasys::ScopeLock l(&eventq_lock_, __func__);

        low_eventq_.push_back(obj);

        eventq_bytes_ += send_data->length();
        if (eventq_bytes_ > eventq_bytes_max_)
        {
            eventq_bytes_max_ = eventq_bytes_;
        }

    } else {
        delete send_data;
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::reconfigure(Params* new_params)
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
LTPUDPConvergenceLayer::Sender::dump_link(oasys::StringBuffer* buf)
{
    if (!use_rate_socket_) {
        buf->appendf("transmit rate: unlimited\n");
    } else {
        buf->appendf("transmit rate: %" PRIu64 "\n", rate_socket_->bucket()->rate());
        if (rate_socket_->bucket()->is_type_standard())
            buf->appendf("bucket_type: Standard\n");
        else
            buf->appendf("bucket_type: Leaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", rate_socket_->bucket()->depth());
    }
}


//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::SendPoller(Sender* parent, const LinkRef& link)
    : Logger("LTPUDPConvergenceLayer::Sender::SendPoller",
             "/dtn/cl/ltpudp/sender/poller/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender::SendPoller")
{
    log_debug("Poller created");
    parent_   = parent;
    link_ref_ = link;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::~SendPoller()
{
    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SendPoller::shutdown()
{
    this->set_should_stop();

    while (!is_stopped()) {
        usleep(10000);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SendPoller::run()
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





//----------------------------------------------------------------------
LTPUDPConvergenceLayer::SenderRef::SenderRef(const ContactRef& contact, LTPUDPConvergenceLayer *ltpcl)
{
    sender_instance_ = std::make_shared<Sender>(contact, ltpcl);
}


//----------------------------------------------------------------------
LTPUDPConvergenceLayer::SenderRef::SenderRef(const SenderRef& sref)
{
    sender_instance_ = sref.sender_instance_;
}
        

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::SenderRef::SenderRef(SenderRef* sref_ptr)
{
    sender_instance_ = sref_ptr->sender_instance_;
}
        

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::SenderRef::~SenderRef()
{
}


//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::init(Params* params, in_addr_t addr, u_int16_t port)
{
    return sender_instance_->init(params, addr, port);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::start()
{
    sender_instance_->start();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::shutdown()
{
    sender_instance_->shutdown();
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::reconfigure(Params* new_params)
{
    return sender_instance_->reconfigure(new_params);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::dump_link(oasys::StringBuffer* buf)
{
    sender_instance_->dump_link(buf);
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::SenderRef::get_incoming_list_size()
{
    return sender_instance_->get_incoming_list_size();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::get_cla_stats(oasys::StringBuffer& buf)
{
    sender_instance_->get_cla_stats(buf);
}


//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::SenderRef::Remote_Engine_ID()
{
    return sender_instance_->Remote_Engine_ID();
}


//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Set_Ready_For_Bundles(bool input_flag)
{
    return sender_instance_->Set_Ready_For_Bundles(input_flag);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Send(std::string *send_data, SPtr_LTPTimer& timer, bool back)
{
    sender_instance_->Send(send_data, timer, back);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Send_Low_Priority(std::string *send_data, SPtr_LTPTimer& timer)
{
    return sender_instance_->Send_Low_Priority(send_data, timer);
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Retran_Intvl()
{
    return sender_instance_->Retran_Intvl();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Retran_Retries()
{
    return sender_instance_->Retran_Retries();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Inactivity_Intvl()
{
    return sender_instance_->Inactivity_Intvl();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Add_To_Inflight(const BundleRef& bref)
{
    sender_instance_->Add_To_Inflight(bref);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Del_From_Queue(const BundleRef&  bref)
{
    sender_instance_->Del_From_Queue(bref);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::Del_From_InFlight_Queue(const BundleRef&  bref)
{
    return sender_instance_->Del_From_InFlight_Queue(bref);
}

size_t LTPUDPConvergenceLayer::SenderRef::Get_Bundles_Queued_Count()
{
    return sender_instance_->Get_Bundles_Queued_Count();
}

size_t LTPUDPConvergenceLayer::SenderRef::Get_Bundles_InFlight_Count()
{
    return sender_instance_->Get_Bundles_InFlight_Count();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::Delete_Xmit_Blocks(const BundleRef&  bref)
{
    sender_instance_->Delete_Xmit_Blocks(bref);
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Max_Sessions()
{
    return sender_instance_->Max_Sessions();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Agg_Time()
{
    return sender_instance_->Agg_Time();
}

//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::SenderRef::Agg_Size()
{
    return sender_instance_->Agg_Size();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Seg_Size()
{
    return sender_instance_->Seg_Size();
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::CCSDS_Compatible()
{
    return sender_instance_->CCSDS_Compatible();
}

//----------------------------------------------------------------------
uint64_t
LTPUDPConvergenceLayer::SenderRef::Ltp_Queued_Bytes_Quota()
{
    return sender_instance_->Ltp_Queued_Bytes_Quota();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::SenderRef::PostTransmitProcessing(BundleRef& bref, bool red, 
                                                          uint64_t expected_bytes, bool success)
{
    sender_instance_->PostTransmitProcessing(bref, red, expected_bytes, success);
}

//----------------------------------------------------------------------
SPtr_BlockInfoVec
LTPUDPConvergenceLayer::SenderRef::GetBlockInfoVec(Bundle * bundle)
{
    return sender_instance_->GetBlockInfoVec(bundle);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::Hex_Dump()
{
    return sender_instance_->Hex_Dump();
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::SenderRef::AOS()
{
    return sender_instance_->AOS();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Inbound_Cipher_Suite() 
{
    return sender_instance_->Inbound_Cipher_Suite();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Inbound_Cipher_Key_Id()
{
    return sender_instance_->Inbound_Cipher_Key_Id();
}

//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::SenderRef::Inbound_Cipher_Engine()
{
    return sender_instance_->Inbound_Cipher_Engine();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Outbound_Cipher_Suite( )
{
    return sender_instance_->Outbound_Cipher_Suite();
}

//----------------------------------------------------------------------
uint32_t
LTPUDPConvergenceLayer::SenderRef::Outbound_Cipher_Key_Id()
{
    return sender_instance_->Outbound_Cipher_Key_Id();
}

//----------------------------------------------------------------------
std::string&
LTPUDPConvergenceLayer::SenderRef::Outbound_Cipher_Engine()
{
    return sender_instance_->Outbound_Cipher_Engine();
}

} // namespace dtn

