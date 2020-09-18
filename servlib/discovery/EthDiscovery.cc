/*
 *    Copyright 2013 Lana Black <sickmind@lavabit.com>
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

#include <cstring>
#include <climits>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <poll.h>

#include <oasys/io/IO.h>
#include <oasys/util/OptParser.h>
#include <bundling/BundleDaemon.h>
#include <contacts/ContactManager.h>
#include <contacts/InterfaceTable.h>
#include <conv_layers/EthConvergenceLayer.h>

#include "EthDiscovery.h"
#include "EthAnnounce.h"

namespace dtn {

EthDiscovery::EthDiscovery(const std::string& name)
    : Discovery(name, "eth"),
      oasys::Thread("EthDiscovery", DELETE_ON_EXIT)
{
    shutdown_ = false;
}

bool EthDiscovery::configure(int argc, const char *argv[])
{
    if(oasys::Thread::started()) {
        log_warn("reconfiguration of EthDiscovery no supported");
        return false;
    }

    oasys::OptParser p;

    p.addopt(new oasys::StringOpt("interface", &iface_));

    const char *invalid;
    if(!p.parse(argc, argv, &invalid)) {
        log_err("bad option for Ethernet discovery: %s", invalid);
        return false;
    }

    // Check the interface
    if(!iface_.empty()) {
        oasys::StringBuffer iface_list;

        InterfaceTable::instance()->list(&iface_list);
        if(strstr(iface_list.c_str(), iface_.c_str()) == NULL) {
            // No interface found
            log_err("bad interface name");
            return false;
        }
    } else {
        log_err("Ethernet discovery requires interface name");
        return false;
    }


    log_debug("starting thread");
    start();

    return true;
}

void EthDiscovery::run()
{
    struct sockaddr_ll ifaddr;
    struct ifreq req;
    struct pollfd fds;
    struct iovec iov[2];
    struct ether_header ethhdr;
    struct ::ether_addr src_addr;
    u_char buf[1500];   // Buffer to store beacons
    int cc;

    log_debug("discovery thread running");

    sock_ = socket(PF_PACKET, SOCK_RAW, htons(ETHERTYPE_DTN_BEACON));
    if(sock_ < 0) {
        log_err("socket() failed: %s", strerror(errno));
        return;
    }

    memset(&req, 0, sizeof(req));
    strcpy(req.ifr_name, iface_.c_str());
    ioctl(sock_, SIOCGIFHWADDR, &req);
    memcpy(&src_addr.ether_addr_octet, req.ifr_hwaddr.sa_data, 6);

    memset(&req, 0, sizeof(req));
    strcpy(req.ifr_name, iface_.c_str());
    ioctl(sock_, SIOCGIFINDEX, &req);

    memset(&ifaddr, 0, sizeof(ifaddr));
    ifaddr.sll_family = AF_PACKET;
    ifaddr.sll_protocol = htons(ETHERTYPE_DTN_BEACON);
    ifaddr.sll_ifindex = req.ifr_ifindex;

    if(bind(sock_, (struct sockaddr*) &ifaddr, sizeof(ifaddr)) == -1) {
        log_err("bind() failed: %s", strerror(errno));
        return;
    }

    fds.fd = sock_;
    fds.events = POLLIN;

    memset(iov, 0, sizeof(iov));

    iov[0].iov_base = &ethhdr;
    iov[0].iov_len = sizeof(ethhdr);

    // The following code is an adapted copy of the same code from IPDiscovery.cc

    while(true) {
        if(shutdown_) break;

        memset(&ethhdr.ether_dhost, 0xff, sizeof(ethhdr.ether_dhost));
        memcpy(&ethhdr.ether_shost, src_addr.ether_addr_octet, 6);
        ethhdr.ether_type = htons(ETHERTYPE_DTN_BEACON);

        iov[0].iov_base = &ethhdr;
        iov[0].iov_len = sizeof(ethhdr);

        iov[1].iov_base = buf;

        u_int timeout = UINT_MAX;
        for(iterator iter = list_.begin(); iter != list_.end(); ++iter) {
            EthAnnounce* announce = dynamic_cast<EthAnnounce*>(*iter);

            u_int remaining = announce->interval_remaining();

            if(remaining) {
                log_debug("announce not ready: %u ms remaining", remaining);
                if(remaining < timeout)
                    timeout = remaining;
            } else {
                int len;

                len = announce->format_advertisement(buf, sizeof(buf));
                iov[1].iov_len = len;

                cc = oasys::IO::writevall(sock_, iov, 2);
                if(cc != (int)sizeof(ethhdr)+len) {
                    log_err("send failed: %s", strerror(errno));
                    return;
                }

                timeout = announce->interval();
            }
        }

        // We cannot obviously wait UINT_MAX milliseconds
        cc = poll(&fds, 1, timeout == UINT_MAX ? 1 : timeout);
        if(cc == 0) {
            continue;
        } else if(cc < 0) {
            if(errno == EINTR) {
                continue;
            } else {
                log_err("poll failed: %s", strerror(errno));
                return;
            }
        }

        iov[1].iov_len = sizeof(buf);

        cc = oasys::IO::readv(sock_, iov, 2);
        if(cc < 0) {
            log_err("readvall() failed");
            return;
        } else if(cc < (int)sizeof(ethhdr)) {
            log_warn("corrupt packet received, ignore");
            continue;
        }

        std::string nexthop;
        EndpointID remote_eid;

        if(!parse_advertisement(buf, cc - sizeof(ethhdr), ethhdr.ether_shost, nexthop, remote_eid)) {
            log_warn("unable to parse beacon");
            continue;
        }

        if(remote_eid.equals(BundleDaemon::instance()->local_eid())) {
            log_debug("ignoring our own beacon");
            continue;
        }

        handle_neighbor_discovered("eth", nexthop, remote_eid);
    }

}

bool EthDiscovery::parse_advertisement(u_char *bp, size_t len, u_char *remote_addr, std::string& nexthop, EndpointID& remote_eid)
{
    if(len < sizeof(struct EthBeaconHeader))
        return false;

    struct EthBeaconHeader *beacon = reinterpret_cast<struct EthBeaconHeader*>(bp);

    if(beacon->version != ETHCL_VERSION)
        return false;
    if(beacon->type != ETHCL_BEACON) {
        log_warn("Received non beacon packet with type %u", beacon->type);
        return false;
    }

    size_t eid_len = ntohs(beacon->eid_len);
    std::string eid(beacon->sender_eid, eid_len);

    oasys::StringBuffer buf("%x:%x:%x:%x:%x:%x", remote_addr[0], remote_addr[1], remote_addr[2],
                                                 remote_addr[3], remote_addr[4], remote_addr[5]);
    nexthop.assign(buf.c_str());

    return remote_eid.assign(eid);
}

// Have to reimplement this method to fill the interface name
// in the link parameters structure
void
EthDiscovery::handle_neighbor_discovered(const std::string& cl_type,
                                         const std::string& cl_addr,
                                         const EndpointID& remote_eid)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(cl_type.c_str());
    if (cl == NULL) {
        log_err("handle_neighbor_discovered: "
                "unknown convergence layer type '%s'", cl_type.c_str());
        return;
    }

    // Look for match on convergence layer and remote EID
    LinkRef link("Announce::handle_neighbor_discovered");
    link = cm->find_link_to(cl, "", remote_eid);

    if (link == NULL) {
        link = cm->new_opportunistic_link(cl, cl_addr, remote_eid);
        if (link == NULL) {
            log_debug("Discovery::handle_neighbor_discovered: "
                      "failed to create opportunistic link");
            return;
        }
    }

    ASSERT(link != NULL);
    ASSERT(link->cl_info() != NULL)

    EthConvergenceLayer::Params* link_params = dynamic_cast<EthConvergenceLayer::Params*>(link->cl_info());
    link_params->if_name_ = iface_;

    if (!link->isavailable())
    {
        // request to set link available
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::AVAILABLE,
                                       ContactEvent::DISCOVERY)
            );
    }
}


} //dtn
