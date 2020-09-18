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

// Only works on Linux (for now)
#ifdef __linux__

#include <sys/poll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

#include <oasys/io/NetUtils.h>
#include <oasys/io/IO.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "EthConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"

using namespace oasys;
namespace dtn {

/******************************************************************************
 *
 * EthConvergenceLayer::Params
 *
 *****************************************************************************/
class EthConvergenceLayer::Params EthConvergenceLayer::defaults_;

void
EthConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
	a->process("interface", &if_name_);
}

/******************************************************************************
 *
 * EthConvergenceLayer
 *
 *****************************************************************************/

EthConvergenceLayer::EthConvergenceLayer()
    : ConvergenceLayer("EthConvergenceLayer", "eth")
{
    defaults_.if_name_ = "";
}

//----------------------------------------------------------------------
CLInfo*
EthConvergenceLayer::new_link_params()
{
    return new EthConvergenceLayer::Params(EthConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&EthConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
/**
 * Parse variable args into a parameter structure.
 */
bool
EthConvergenceLayer::parse_params(Params* params,
                                  int argc, const char* argv[],
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::StringOpt("interface", &params->if_name_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
/* 
 *   Start listening to, and sending beacons on, the provided interface.
 *
 *   For now, we support interface strings on the form
 *   string://eth0
 *   
 *   this should change further down the line to simply be
 *    eth0
 *  
 */

bool
EthConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    Params params = EthConvergenceLayer::defaults_;
    const char *invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // grab the interface name out of the string:// 

    // XXX/jakob - this fugly mess needs to change when we get the
    // config stuff right
    const char* if_name=iface->name().c_str()+strlen("string://");
    log_info("EthConvergenceLayer::interface_up(%s).", if_name);
    
    Receiver* receiver = new Receiver(if_name, &params);
    receiver->logpathf("/cl/eth");
    //dzdebug receiver->start();
    iface->set_cl_info(receiver);

    return true;
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::interface_activate(Interface* iface)
{
    Receiver *receiver = (Receiver *)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::interface_down(Interface* iface)
{
  // XXX/jakob - need to keep track of the Beacon and Receiver threads for each 
  //             interface and kill them.
  // NOTIMPLEMENTED;

    Receiver *receiver = (Receiver *)iface->cl_info();
    receiver->set_should_stop();
    //receiver->interrupt();
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }
    delete receiver;

    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    Params* link_params = dynamic_cast<Params*>(link->cl_info());

    log_debug("EthConvergenceLayer::open_contact: "
              "opening contact to link *%p", link.object());

    // parse out the address from the contact nexthop
    eth_addr_t addr;
    if (!EthernetScheme::parse(link->nexthop(), &addr)) {
        log_err("EthConvergenceLayer::open_contact: "
                "next hop address '%s' not a valid eth uri", link->nexthop());
        return false;
    }
    
    // create a new connection for the contact
    Sender* sender = new Sender(link_params->if_name_.c_str(),
                                link->contact());
    contact->set_cl_info(sender);

    sender->logpathf("/cl/eth");

    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::close_contact(const ContactRef& contact)
{  
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {            
        contact->set_cl_info(NULL);
        delete sender;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    Params* params = dynamic_cast<Params *>(new_link_params());
    ASSERT(params != NULL);

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
EthConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != NULL);

    buf->appendf("interface: %s\n", params->if_name_.c_str());
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    log_debug("EthConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    if (link->cl_info() != NULL) {
        delete link->cl_info();
        link->set_cl_info(NULL);
    }
}

//----------------------------------------------------------------------
/**
 * Send bundles queued up for the contact.
 */
void
EthConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    const ContactRef contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    ASSERT(contact == sender->contact_);

    sender->send_bundle(bundle);
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::is_queued(const LinkRef& contact, Bundle* bundle)
{
    (void)contact;
    (void)bundle;

    /// The Ethernet convergence layer does not maintain an output queue.
    return false;
}

/******************************************************************************
 *
 * EthConvergenceLayer::Receiver
 *
 *****************************************************************************/
EthConvergenceLayer::Receiver::Receiver(const char* if_name,
                                        EthConvergenceLayer::Params* params)
  : Logger("EthConvergenceLayer::Receiver", "/dtn/cl/eth/receiver"),
    Thread("EthConvergenceLayer::Receiver")
{
    memset(if_name_,0, IFNAMSIZ);
    strcpy(if_name_,if_name);
    Thread::flags_ |= INTERRUPTABLE;
    (void)params;
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    Bundle* bundle = NULL;       
    EthCLHeader ethclhdr;
    size_t bundle_len;
    //struct ether_header* ethhdr=(struct ether_header*)bp;
    
    log_debug("Received DTN packet on interface %s, %zu.",if_name_, len);    

    // copy in the ethcl header.
    if (len < sizeof(EthCLHeader)) {
        log_err("process_data: "
                "incoming packet too small (len = %zu)", len);
        return;
    }
    memcpy(&ethclhdr, bp+sizeof(struct ether_header), sizeof(EthCLHeader));

    // check for valid magic number and version
    if (ethclhdr.version != ETHCL_VERSION) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", ethclhdr.version, ETHCL_VERSION);
        return;
    }

    if(ethclhdr.type == ETHCL_BUNDLE) {
        // infer the bundle length based on the packet length minus the
        // eth cl header
        bundle_len = len - sizeof(EthCLHeader) - sizeof(struct ether_header);
        
        log_debug("process_data: got ethcl header -- bundle id %d, length %zu",
                  ntohl(ethclhdr.bundle_id), bundle_len);
        
        // skip past the cl header
        bp  += (sizeof(EthCLHeader) + sizeof(struct ether_header));
        len -= (sizeof(EthCLHeader) + sizeof(struct ether_header));

        bundle = new Bundle();
        bool complete = false;
        int cc = BundleProtocol::consume(bundle, bp, len, &complete);

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

        log_debug("process_data: new bundle id %" PRIbid " arrival, bundle length %zu",
                  bundle->bundleid(), bundle_len);
        
        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, 
                                    bundle_len, EndpointID::NULL_EID()));
    }
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::Receiver::run()
{
    int sock;
    int cc;
    int flags;
    struct sockaddr_ll iface;
    unsigned char buffer[MAX_ETHER_PACKET];

    if((sock = socket(PF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        log_err("EthConvergenceLayer::Receiver::run() " 
                "Couldn't open socket.");       
        exit(1);
    }

    // Make the socket non-blocking
    flags = fcntl(sock, F_GETFL, 0);
    if(flags == -1) {
        perror("fcntl");
        log_err("EthConvergenceLayer::Receiver::run() "
                "Couldn't get socket flags.");
        exit(1);
    }
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
   
    // figure out the interface index of the device with name if_name_
    struct ifreq req;
    strcpy(req.ifr_name, if_name_);
    ioctl(sock, SIOCGIFINDEX, &req);

    memset(&iface, 0, sizeof(iface));
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    iface.sll_ifindex=req.ifr_ifindex;
   
    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }

    log_warn("Reading from socket...");
    while(true) {
        cc=read (sock, buffer, MAX_ETHER_PACKET);
        if(cc<=0) {
            // EAGAIN means we haven't received anything yet
            if(errno != EAGAIN) {
                perror("EthConvergenceLayer::Receiver::run()");
                exit(1);
            }
        } else {
            struct ether_header* hdr=(struct ether_header*)buffer;
  
            if(ntohs(hdr->ether_type)==ETHERTYPE_DTN) {
                process_data(buffer, cc);
            }
            else if(ntohs(hdr->ether_type)!=0x800)
            {
                log_err("Got non-DTN packet in Receiver, type %4X.",
                        ntohs(hdr->ether_type));
                // exit(1);
            }
        }

        if(should_stop())
            break;

        struct timespec ts = { 0, 1*1000*1000 };  // 1 msec
        nanosleep(&ts, NULL);
    }
}

/******************************************************************************
 *
 * EthConvergenceLayer::Sender
 *
 *****************************************************************************/

/**
 * Constructor for the active connection side of a connection.
 */
EthConvergenceLayer::Sender::Sender(const char* if_name,
                                    const ContactRef& contact)
    : Logger("EthConvergenceLayer::Sender", "/dtn/cl/eth/sender"),
      contact_(contact.object(), "EthConvergenceLayer::Sender")
{
    ASSERT(strlen(if_name) > 0);

    struct ifreq req;
    struct sockaddr_ll iface;
    LinkRef link = contact->link();

    memset(src_hw_addr_.octet, 0, 6); // determined in Sender::run()
    EthernetScheme::parse(link->nexthop(), &dst_hw_addr_);

    strcpy(if_name_, if_name);
    sock_ = 0;

    memset(&req, 0, sizeof(req));
    memset(&iface, 0, sizeof(iface));

    // Get and bind a RAW socket for this contact. XXX/jakob - seems
    // like it'd be enough with one socket per interface, not one per
    // contact. figure this out some time.
    if((sock_ = socket(AF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        exit(1);
    }

    // get the interface name from the contact info
    strcpy(req.ifr_name, if_name_);

    // ifreq the interface index for binding the socket    
    ioctl(sock_, SIOCGIFINDEX, &req);
    
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    iface.sll_ifindex=req.ifr_ifindex;
        
    // store away the ethernet address of the device in question
    if(ioctl(sock_, SIOCGIFHWADDR, &req))
    {
        perror("ioctl");
        exit(1);
    } 
    memcpy(src_hw_addr_.octet,req.ifr_hwaddr.sa_data,6);    

    if (bind(sock_, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }
}
        
//----------------------------------------------------------------------
/* 
 * Send one bundle.
 */
bool 
EthConvergenceLayer::Sender::send_bundle(const BundleRef& bundle) 
{
    int cc;
    struct iovec iov[3];
        
    EthCLHeader ethclhdr;
    struct ether_header hdr;

    memset(iov,0,sizeof(iov));
    
    // iovec slot 0 holds the ethernet header

    iov[0].iov_base = (char*)&hdr;
    iov[0].iov_len = sizeof(struct ether_header);

    // write the ethernet header

    memcpy(hdr.ether_dhost,dst_hw_addr_.octet,6);
    memcpy(hdr.ether_shost,src_hw_addr_.octet,6); // Sender::hw_addr
    hdr.ether_type=htons(ETHERTYPE_DTN);
    
    // iovec slot 1 for the eth cl header

    iov[1].iov_base = (char*)&ethclhdr;
    iov[1].iov_len  = sizeof(EthCLHeader);
    
    // write the ethcl header

    ethclhdr.version    = ETHCL_VERSION;
    ethclhdr.type       = ETHCL_BUNDLE;
    ethclhdr.bundle_id  = htonl(bundle->bundleid());

    // iovec slot 2 for the bundle
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);

    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, MAX_ETHER_PACKET);
        return -1;
    }

    iov[2].iov_base = (char *)buf_;
    iov[2].iov_len  = total_len;
    
    // We're done assembling the packet. Now write the whole thing to
    // the socket!
    log_info("Sending bundle out interface %s",if_name_);

    cc=IO::writevall(sock_, iov, 3);
    if(cc<0) {
        perror("send");
        log_err("Send failed!\n");
    }    
    log_info("Sent packet, size: %d",cc );

    // move the bundle off the link queue and onto the inflight queue
    contact_->link()->del_from_queue(bundle, total_len);
    contact_->link()->add_to_inflight(bundle, total_len);

    // check that we successfully wrote it all
    bool ok;
    int total = sizeof(EthCLHeader) + sizeof(struct ether_header) + total_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (wrote %d/%d): %s",
                cc, total, strerror(errno));
        ok = false;
    } else {
        // cons up a transmission event and pass it to the router
        // since this is an unreliable protocol, acked_len = 0, and
        // ack = false
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle.object(), contact_,contact_->link(),
                                       total_len, false));
        ok = true;
    }

    return ok;
}

} // namespace dtn

#endif // __linux
