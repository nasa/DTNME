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


#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "UDPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/FormatUtils.h"

namespace dtn {

class UDPConvergenceLayer::Params UDPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = (int) bucket_type_;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("recvbuf", &recvbuf_);
    a->process("sendbuf", &sendbuf_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;
    }
}

//----------------------------------------------------------------------
UDPConvergenceLayer::UDPConvergenceLayer()
    : IPConvergenceLayer("UDPConvergenceLayer", "udp")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = UDPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.bucket_type_              = (oasys::RateLimitedSocket::BUCKET_TYPE) 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.bucket_depth_             = 0; // default
    defaults_.recvbuf_                  = 0; // OS managed
    defaults_.sendbuf_                  = 0; // OS managed
    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
//    next_hop_flags_                     = 0;
}

//----------------------------------------------------------------------
CLInfo*
UDPConvergenceLayer::new_link_params()
{
    return new UDPConvergenceLayer::Params(UDPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&UDPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;
    int temp = 0;
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::IntOpt("bucket_type", &temp));
    p.addopt(new oasys::RateOpt("rate", &params->rate_));
    p.addopt(new oasys::UInt64Opt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::UIntOpt("recvbuf", &params->recvbuf_));
    p.addopt(new oasys::UIntOpt("sendbuf", &params->sendbuf_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;

    return true;
};

//----------------------------------------------------------------------
void
UDPConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("UDP Convergence Layer [%s] - valid Link options:\n\n", name());
    buf.appendf("<next hop> format for \"link add\" command is ip_address:port or hostname:port\n\n");
    buf.appendf("CLA specific options:\n");

    buf.appendf("    local_addr <IP address>            - IP address to bind to (usually not needed for a link)\n");
    buf.appendf("    local_port <U16>                   - Port to bind to (usually not needed for a link)\n");

    buf.appendf("    rate <U64>                         - Transmit rate throttle in bits per second (default: 0 = no throttle)\n");
    buf.appendf("    bucket_type <0 or 1>               - Throttle token bucket type: 0=standard, 1=leaky (default: 0)\n");
    buf.appendf("    bucket_depth <U64>                 - Throttle token bucket depth in bits (default: 524280 = 64K * 8)\n");
    buf.appendf("    sendbuf <U32>                      - Size of socket send buffer in bytes (default: 0 = OS managed)\n");


    buf.appendf("\nOptions for all links:\n");
    
    buf.appendf("    remote_eid <Endpoint ID>           - Remote Endpoint ID\n");
    buf.appendf("    reliable <Bool>                    - Whether the CLA is considered reliable (default: false)\n");
    buf.appendf("    nexthop <ip_address:port>          - IP address and port of remote node (positional in link add command)\n");
    buf.appendf("    mtu <U64>                          - Max size for outgoing bundle triggers proactive fragmentation (default: 0 = no limit)\n");
    buf.appendf("                                           (recommend 65000 to keep it less than the max UDP packet size)\n");
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
    buf.appendf("link add udp_6 192.168.0.6:4556 ALWAYSON udp mtu=65000 rate=150000000\n");
    buf.appendf("    (create a link named \"udp_6\" to transmit UDP to peer at IP address 192.168.0.6 port 4556\n");
    buf.appendf("     with a transmit rate of 150 Mbps and bundles greater than 65000 bytes will be fragmented prior to sending)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("UDP Convergence Layer [%s] - valid Interface options:\n\n", name());
    buf.appendf("CLA specific options:\n");

    buf.appendf("  UDP related params:\n");
    buf.appendf("    local_addr <IP address>            - IP address of interface on which to listen (default: 0.0.0.0 = all interfaces)\n");
    buf.appendf("    local_port <U16>                   - Port on which to listen (default: 4556)\n");
    buf.appendf("    remote_addr <IP address>           - Only accept packets from specified IP address (usually not needed)\n");
    buf.appendf("    remote_port <U16>                  - Only accept packets from specified port (usually not needed)\n");
    buf.appendf("    recvbuf <U32>                      - Size of socket receive buffer in bytes (default: 0 = OS managed)\n");

    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("interface add udprcvr udp local_addr=192.168.0.1 local_port=4556\n");
    buf.appendf("    (create an interface named \"udprcvr\" to listen for UDP packets on network interface\n");
    buf.appendf("     with IP address 192.168.0.1 using port 4556)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = UDPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }
    
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params);
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
    
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::interface_activate(Interface* iface)
{
    // start listening and then start the thread to loop calling accept()
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::interface_down(Interface* iface)
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
UDPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Receiver*)iface->cl_info())->cla_params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d  recvbuf: %u\n",
                 intoa(params->local_addr_), params->local_port_, params->recvbuf_);
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tlistening\n");
    }
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == nullptr);
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &addr, &port);

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);
    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %" PRIu64 " > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);

    log_debug("UDPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(nullptr);
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
    buf->appendf("rate: %" PRIu64 " (%s)\n", params->rate_, FORMAT_AS_RATE(params->rate_).c_str());
    buf->appendf("bucket_depth: %" PRIu64 "\n", params->bucket_depth_);
    buf->appendf("sendbuf: %u\n", params->sendbuf_);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);
    
    log_debug("UDPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());
    
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
        port = UDPCL_DEFAULT_PORT;
    }

    next_hop_addr_  = addr;
    next_hop_port_  = port;
//    next_hop_flags_ = MSG_DONTWAIT;

    Params* params = (Params*)link->cl_info();
    
    // create a new sender structure
    Sender* sender = new Sender(link, link->contact());

    if (!sender->init(params, addr, port)) {
        log_err("error initializing contact");
        LinkStateChangeRequest* event_to_post;
        event_to_post = new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                                   ContactEvent::NO_INFO);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

        delete sender;
        return false;
    }
    sender->start();
        
    contact->set_cl_info(sender);
    log_debug("UDPConv.. Adding ContactUpEvent ");
    ContactUpEvent* event_to_post;
    event_to_post = new ContactUpEvent(link->contact());
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
    
    // XXX/demmer should this assert that there's nothing on the link
    // queue??
    
    return true;
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {
        sender->set_should_stop();
        while (not sender->is_stopped()) {
            usleep(100000);
        }
        delete sender;
        contact->set_cl_info(nullptr);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;
    (void) bundle;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::bundle_queued_orig(const LinkRef& link, const BundleRef& bundle)
{
    (void) bundle;

    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
   
     
    const ContactRef contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }

    if (link->get_contact_state()) {
        BundleRef lbundle = link->queue()->front();
        ASSERT(contact == sender->contact_);

        int len = sender->send_bundle(lbundle, next_hop_addr_, next_hop_port_);

        if (len > 0) {
            link->del_from_queue(lbundle);
            link->add_to_inflight(lbundle);

            BundleTransmittedEvent* event_to_post;
            event_to_post = new BundleTransmittedEvent(lbundle.object(), contact, link, 
                                                       len, 0, true, false);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);
        }
    }
}

//----------------------------------------------------------------------
UDPConvergenceLayer::Receiver::Receiver(UDPConvergenceLayer::Params* params)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/udp/receiver")),
      UDPClient("/dtn/cl/udp/receiver"),
      Thread("UDPConvergenceLayer::Receiver")
{
    logfd_  = false;
    cla_params_ = *params;

    // apply the configured socket receive buffer size
    params_.recv_bufsize_ = (int) cla_params_.recvbuf_;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    // the payload should contain a full bundle
    Bundle* bundle = new Bundle(BundleProtocol::BP_VERSION_UNKNOWN);
    
    bool complete = false;
    ssize_t cc = BundleProtocol::consume(bundle, bp, len, &complete);

    if (cc < 0) {
        log_err("process_data: bundle protocol error");
        delete bundle;
        return;
    }

    if (!complete) {
        log_err("process_data: incomplete bundle");
        delete bundle;
        return;
    }
    
    //log_debug("process_data: new bundle id %" PRIbid " arrival, length %zu (payload %zu)",
    //          bundle->bundleid(), len, bundle->payload().length());

    BundleDaemon* bdaemon = BundleDaemon::instance();
    bool space_reserved = false;
    uint64_t prev_reserved_space = 0;
    bool accept_bundle = bdaemon->query_accept_bundle_based_on_quotas(bundle, space_reserved, prev_reserved_space);

    if (accept_bundle) {
        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
        BundleReceivedEvent* event_to_post;
        event_to_post = new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, sptr_dummy_prevhop);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    } else {
        bdaemon->release_bundle_without_bref_reserved_space(bundle);

        log_err("Deleting bundle *%p due to storage depletion", bundle);
        delete bundle;
    }
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Receiver::run()
{
    char threadname[16] = "UdpClaRcvr";
    pthread_setname_np(pthread_self(), threadname);
   

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
        
        //log_debug("got %d byte packet from %s:%d",
        //          ret, intoa(addr), port);               
        process_data(buf, ret);
    }
}

//----------------------------------------------------------------------
UDPConvergenceLayer::Sender::Sender(const LinkRef& link, const ContactRef& contact)
    : Thread("UDPConvergenceLayer::Sender"),
      Logger("UDPConvergenceLayer::Sender",
             "/dtn/cl/udp/sender/%p", this),
      socket_(logpath_),
      rate_socket_(0),
      link_(link.object(), "UDPCovergenceLayer::Sender"),
      contact_(contact.object(), "UDPCovergenceLayer::Sender")
{
}

UDPConvergenceLayer::Sender::~Sender()
{
    if (rate_socket_) {
        delete rate_socket_;
    }
}


//----------------------------------------------------------------------
bool
UDPConvergenceLayer::Sender::init(Params* params,
                                  in_addr_t addr, u_int16_t port)
    
{
    log_debug("initializing sender");

    cla_params_ = params;
    next_hop_addr_ = addr;
    next_hop_port_ = port;
    
    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);

    // apply the configured socket send buffer size
    socket_.params_.send_bufsize_ = (int) cla_params_->sendbuf_;

    if (params->local_addr_ != INADDR_NONE || params->local_port_ != 0)
    {
        if (socket_.bind(params->local_addr_, params->local_port_) != 0) {
            log_err("error binding to %s:%d: %s",
                    intoa(params->local_addr_), params->local_port_,
                    strerror(errno));
            return false;
        }
    }
   
    socket_.init_socket();
    

//    if (socket_.connect(addr, port) != 0) 
//    {
//        log_err("error issuing udp init_socket to %s:%d: %s",
//                intoa(addr), port, strerror(errno));
//        return false;
//    }

    if (params->rate_ != 0) {

        rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params->bucket_type_);
        rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        rate_socket_->bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params->bucket_depth_);
        }
 
        log_debug("initialized rate controller: rate %llu depth %llu",
                  U64FMT(rate_socket_->bucket()->rate()),
                  U64FMT(rate_socket_->bucket()->depth()));
    }

    return true;
}
    
//----------------------------------------------------------------------
int
UDPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr, u_int16_t next_hop_port)
{
    oasys::ScopeLock scoplok(bundle->lock(), __func__);

    int cc = 0;
//    SPtr_BlockInfoVec sptr_blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    SPtr_BlockInfoVec sptr_blocks = bundle->xmit_blocks()->find_blocks(link_);
    ASSERT(sptr_blocks != nullptr);

    scoplok.unlock();


    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle.object(), sptr_blocks.get(),
                                               buf_, 0, sizeof(buf_),
                                               &complete);

    scoplok.unlock();

    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(bundle.object(), sptr_blocks.get());
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, UDPConvergenceLayer::MAX_BUNDLE_LEN);
        return -1;
    }
        
    // write it out the socket and make sure we wrote it all
    if(cla_params_->rate_ > 0) {
        cc = rate_socket_->sendto((char*)buf_, total_len, 0, next_hop_addr, next_hop_port, true);
    } else {
        cc = socket_.sendto((char*)buf_, total_len, 0, next_hop_addr, next_hop_port);
    }

    //int cc = socket_.write((char*)buf_, total_len);
    if (cc == (int)total_len) {
        log_info("send_bundle: successfully sent bundle (id:%" PRIbid ") length %d", 
                 bundle.object()->bundleid(), cc);
        return total_len;
    } else {
        log_err("send_bundle: error sending bundle (id:%" PRIbid ") (wrote %d/%zu): %s",
                bundle.object()->bundleid(), 
                cc, total_len, strerror(errno));
        return -1;
    }
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Sender::run()
{
    char threadname[16] = "UdpClaSender";
    pthread_setname_np(pthread_self(), threadname);
   

    int len = 0;
    BundleRef bref("UdpClaSender");

    while (!should_stop()) {
        if (link_->get_contact_state()) {
            bref = link_->queue()->front();

            if (bref != nullptr) {
                len = send_bundle(bref, next_hop_addr_, next_hop_port_);

                if (len > 0) {
                    link_->del_from_queue(bref);
                    link_->add_to_inflight(bref);

                    BundleTransmittedEvent* event_to_post;
                    event_to_post = new BundleTransmittedEvent(bref.object(), contact_, link_, 
                                                               len, 0, true, false);
                    SPtr_BundleEvent sptr_event_to_post(event_to_post);
                    BundleDaemon::post(sptr_event_to_post);
                }
            } else {
                usleep(10000);
            }

            bref = nullptr;
        } else {
            usleep(10000);
        }
    }
}


} // namespace dtn
