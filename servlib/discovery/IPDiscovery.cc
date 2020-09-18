/*
 *    Copyright 2006 Baylor University
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

#include <climits>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include "bundling/BundleDaemon.h"
#include "IPDiscovery.h"
#include "IPAnnounce.h"

extern int errno;

namespace dtn {

const u_int32_t IPDiscovery::DEFAULT_DST_ADDR = 0xffffffff;
const u_int32_t IPDiscovery::DEFAULT_SRC_ADDR = INADDR_ANY;
const u_int IPDiscovery::DEFAULT_MCAST_TTL = 1;

IPDiscovery::IPDiscovery(const std::string& name)
    : Discovery(name,"ip"),
      oasys::Thread("IPDiscovery", DELETE_ON_EXIT),
      socket_("/dtn/discovery/ip/sock")
{
    remote_addr_ = DEFAULT_DST_ADDR;
    local_addr_ = DEFAULT_SRC_ADDR;
    mcast_ttl_ = DEFAULT_MCAST_TTL;
    port_ = 0;
    shutdown_ = false;
    persist_ = false;
}

bool
IPDiscovery::configure(int argc, const char* argv[])
{
    if (oasys::Thread::started())
    {
        log_warn("reconfiguration of IPDiscovery not supported");
        return false;
    }

    oasys::OptParser p;

    bool portSet = false;
    bool unicast = false;
    p.addopt(new oasys::UInt16Opt("port",&port_,"","",&portSet));
    p.addopt(new oasys::InAddrOpt("addr",&remote_addr_));
    p.addopt(new oasys::InAddrOpt("local_addr",&local_addr_));
    p.addopt(new oasys::UIntOpt("multicast_ttl",&mcast_ttl_));
    p.addopt(new oasys::BoolOpt("unicast",&unicast));
    p.addopt(new oasys::BoolOpt("continue_on_error",&persist_));

    const char* invalid;
    if (! p.parse(argc,argv,&invalid))
    {
        log_err("bad option for IP discovery: %s",invalid);
        return false;
    }

    if (! portSet)
    {
        log_err("must specify port");
        return false;
    }

    socket_.set_remote_addr(remote_addr_);

    // Assume everything is broadcast unless unicast flag is set or if
    // the remote address is a multicast address
    static in_addr_t mcast_mask = inet_addr("224.0.0.0");
    if (unicast)
    {
        log_debug("configuring unicast socket for address %s", intoa(remote_addr_));
    }
    else if ((remote_addr_ == 0xffffffff) ||
             ((remote_addr_ & mcast_mask) != mcast_mask))
    {
        log_debug("configuring broadcast socket for address %s", intoa(remote_addr_));
        socket_.params_.broadcast_ = true;
    }
    else
    {
        log_debug("configuring multicast socket for address %s", intoa(remote_addr_));
        socket_.params_.multicast_ = true;
        socket_.params_.mcast_ttl_ = mcast_ttl_;
    }

    // allow new announce registration to upset poll() in run() below
    socket_.set_notifier(new oasys::Notifier(socket_.logpath()));

    // configure base class variables 
    oasys::StringBuffer buf("%s:%d",intoa(local_addr_),port_);
    local_.assign(buf.c_str());

    oasys::StringBuffer to("%s:%d",intoa(remote_addr_),port_);
    to_addr_.assign(to.c_str());

    log_debug("starting thread"); 
    start();

    return true;
}

void
IPDiscovery::run()
{
    log_debug("discovery thread running");
    oasys::ScratchBuffer<u_char*> buf(1024);
    u_char* bp = buf.buf(1024);

    // if link state causes bind to fail, then this seems reasonable
    // otherwise this may be a problem (see bind(2) for list of errors)
    bool ok = (socket_.bind(local_addr_,port_) == 0);
    if (persist_)
    {
        oasys::Notifier* intr = socket_.get_notifier();
        while (! ok)
        {
            // continue on error ... sleep a bit and try again
            intr->wait(NULL,10000); // 10 seconds

            if (shutdown_) return;

            ok = (socket_.bind(local_addr_,port_) == 0);
        }
    }
    else
    if (! ok)
    {
        log_err("bind failed");
        return;
    }

    size_t len = 0;
    int cc = 0;

    while (true)
    {
        if (shutdown_) break;

        // only send out beacon(s) once per interval
        u_int min_diff = INT_MAX;
        for (iterator iter = list_.begin(); iter != list_.end(); iter++)
        {
            IPAnnounce* announce = dynamic_cast<IPAnnounce*>(*iter);
            u_int remaining = announce->interval_remaining();
            if (remaining == 0)
            {
                oasys::UDPClient alt;
                oasys::UDPClient* sock = NULL;
                // need to bind to CL's local addr to send out on correct interface
                // if disc already is, then use its socket
                // else create a new socket that bind()s to the right interface
                log_debug("socket_.local_addr(%s) -- announce->cl_addr(%s)",
                          intoa(socket_.local_addr()), intoa(announce->cl_addr()));
                if (socket_.local_addr() == announce->cl_addr())
                    sock = &socket_;
                else
                {
                    log_debug("setting alt params.");
                    alt.params_ = socket_.params_;
                    alt.set_remote_addr(remote_addr_);
                    log_debug("about to bind alt socket.");
                    if (alt.bind(announce->cl_addr(),port_) != 0)
                    {
                        log_err("failed to bind to %s:%u -- %s (%d)",
                                intoa(announce->cl_addr()),port_,
                                strerror(errno),errno);
                        continue;
                    }
                    sock = &alt;
                }
                log_debug("announce ready for sending");
                len = announce->format_advertisement(bp,1024);
                cc = sock->sendto((char*)bp,len,0,remote_addr_,port_);
                if (cc != (int) len)
                {
                    log_err("sendto failed: %s (%d)",
                            strerror(errno),errno);

                    // quit thread on error
                    if (!persist_) return;
                }
                min_diff = announce->interval();
                alt.close();
            }
            else
            {
                log_debug("announce not ready: %u ms remaining", remaining);
                if (remaining < min_diff) {
                    min_diff = announce->interval_remaining();
                }
            }
        }

        // figure out whatever time is left (if poll has already fired within
        // interval ms) and set up poll timeout
        u_int timeout = min_diff;

        log_debug("polling on socket: timeout %u", timeout);
        cc = socket_.poll_sockfd(POLLIN,NULL,timeout);

        if (shutdown_) {
            log_debug("shutdown bit set, exiting thread");
            break;
        }

        // if timeout, then flip back around and send beacons
        if (cc == oasys::IOTIMEOUT || cc == oasys::IOINTR)
        {
            continue;
        }
        else if (cc < 0)
        {
            log_err("poll error (%d): %s (%d)",
                    cc, strerror(errno), errno);
            return;
        }
        else if (cc == 1)
        {
            in_addr_t remote_addr;
            u_int16_t remote_port;

            cc = socket_.recvfrom((char*)bp,1024,0,&remote_addr,&remote_port);
            if (cc < 0)
            {
                log_err("error on recvfrom (%d): %s (%d)",cc,
                        strerror(errno),errno);
                if (!persist_) return;
                else continue;
            }

            EndpointID remote_eid;
            u_int8_t cl_type;
            std::string nexthop;
            if (!parse_advertisement(bp,cc,remote_addr,cl_type,nexthop,
                                     remote_eid))
            {
                log_warn("unable to parse beacon from %s:%d",
                         intoa(remote_addr),remote_port);
                if (!persist_) return;
                else continue;
            }

            if (remote_eid.equals(BundleDaemon::instance()->local_eid()))
            {
                log_debug("ignoring beacon from self (%s:%d)",
                          intoa(remote_addr),remote_port);
                continue;
            }

            // distribute to all beacons registered for this CL type
            handle_neighbor_discovered(
                    IPDiscovery::type_to_str((IPDiscovery::cl_type_t)cl_type),
                    nexthop, remote_eid);
        }
        else
        {
            PANIC("unexpected result from poll (%d)",cc);
        }
    }
}

bool
IPDiscovery::parse_advertisement(u_char* bp, size_t len,
                                 in_addr_t remote_addr, u_int8_t& cl_type,
                                 std::string& nexthop, EndpointID& remote_eid)
{
    if (len <= sizeof(DiscoveryHeader))
        return false;

    DiscoveryHeader* hdr = (DiscoveryHeader*) bp;
    size_t length = ntohs(hdr->length);
    if (len < length)
        return false;

    in_addr_t cl_addr;
    u_int16_t cl_port;

    cl_type = hdr->cl_type;
    cl_addr = (hdr->inet_addr == INADDR_ANY) ? remote_addr : hdr->inet_addr;
    cl_port = ntohs(hdr->inet_port);

    oasys::StringBuffer buf("%s:%d",intoa(cl_addr),cl_port);
    nexthop.assign(buf.c_str());

    size_t name_len = ntohs(hdr->name_len);
    std::string eidstr(hdr->sender_name,name_len);

    return remote_eid.assign(eidstr);
}

} // namespace dtn
