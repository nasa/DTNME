/*
 *    Copyright 2012 Raytheon BBN Technologies
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
 *
 * IPNDDiscovery.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <climits>
#include <sstream>

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/HexDumpBuffer.h>

#include "contacts/ContactManager.h"
#include "bundling/BundleDaemon.h"

#include "IPNDDiscovery.h"
#include "IPNDAnnouncement.h"
#include "ipnd_srvc/IPNDServiceFactory.h"
#include "ipnd_srvc/IPClaService.h"

extern int errno;

namespace dtn {

const u_int32_t IPNDDiscovery::DEFAULT_DST_ADDR = 0xffffffff;
const u_int32_t IPNDDiscovery::DEFAULT_SRC_ADDR = INADDR_ANY;
const u_int IPNDDiscovery::DEFAULT_MCAST_TTL = 1;
const u_int32_t IPNDDiscovery::DEFAULT_BEACON_PERIOD = 10; // 10 seconds
const double IPNDDiscovery::DEFAULT_BEACON_THRESHOLD = 0.0; // no tracking

IPNDDiscovery::IPNDDiscovery(const std::string& name)
    : Discovery(name,"ipnd"),
      oasys::Thread("IPNDDiscovery"),
      socket_("/dtn/discovery/ipnd/sock")
{
    remote_addr_ = DEFAULT_DST_ADDR;
    local_addr_ = DEFAULT_SRC_ADDR;
    mcast_ttl_ = DEFAULT_MCAST_TTL;
    port_ = 0;
    shutdown_ = false;
    // continue on error by default
    // otherwise, disruptions in network connectivity will cause discovery to die
    persist_ = true;

    beacon_period_ = DEFAULT_BEACON_PERIOD;
    // turn off missed beacon tracking by default
    beacon_threshold_ = DEFAULT_BEACON_THRESHOLD;
    beacon_sequence_num_ = 0;
}

IPNDDiscovery::~IPNDDiscovery() {
    // free all of our services
    ServicesIter iter;
    for(iter = services_.begin(); iter != services_.end(); iter++) {
        delete (*iter).second;
    }

    services_.clear();
}

bool
IPNDDiscovery::announce(const char* name, int argc, const char* argv[]) {
    // we don't use this call for announcements anymore
    log_err("IPND Discovery uses the 'announce-svc' command instead of "
            "'announce'");
    return false;
}

bool
IPNDDiscovery::svc_announce(const char* name, int argc, const char* argv[]) {
    // make sure the requested name isn't already in use
    std::string name_str(name);
    ServicesIter iter = services_.find(name_str);
    if(iter != services_.end()) {
        // already in use
        log_err("Requested service identifier already in use! [%s]",
                name_str.c_str());
        return false;
    } else {
        // good to go
        IPNDService *svc =
                IPNDServiceFactory::instance()->configureService(argc, argv);
        if(svc == NULL) {
            // unrecognized or error
            log_err("Requested service with parameters is unrecognized!");
            return false;
        } else {
            log_info("Successfully configured new service [%s]",
                    name_str.c_str());
            services_[name_str] = svc;
            return true;
        }
    }
}

bool
IPNDDiscovery::configure(int argc, const char* argv[])
{
    if (oasys::Thread::started())
    {
        log_warn("reconfiguration of IPNDDiscovery not supported");
        return false;
    }

    oasys::OptParser p;

    bool portSet = false;
    bool unicast = false;
    p.addopt(new oasys::UInt16Opt("port", &port_, "", "", &portSet));
    p.addopt(new oasys::InAddrOpt("addr", &remote_addr_));
    p.addopt(new oasys::InAddrOpt("local_addr", &local_addr_));
    p.addopt(new oasys::UIntOpt("multicast_ttl", &mcast_ttl_));
    p.addopt(new oasys::BoolOpt("unicast", &unicast));
    p.addopt(new oasys::BoolOpt("continue_on_error", &persist_));
    p.addopt(new oasys::UIntOpt("beacon_period", &beacon_period_));
    p.addopt(new oasys::DoubleOpt("beacon_threshold",&beacon_threshold_));

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

    if (beacon_threshold_ <= 1.0)
    {
        log_info("beacon_threshold <= 1.0, missing beacon detection disabled.");
    }

    socket_.set_remote_addr(remote_addr_);
    // set the local address on the socket so it can be referenced during
    // socket initialization (i.e. when the socket options are set)
    // this is particularly important when using multicast beacons as
    // it offers control over the interface that joins the multicast group
    socket_.set_local_addr(local_addr_);

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
IPNDDiscovery::run()
{
    log_debug("IPND discovery thread running");
    oasys::ScratchBuffer<u_char*> buf(1024);
    u_char* bp = buf.buf(1024);

    // XXX: hack here to disable multicast loop
    // FIXME: this support should really be added to oasys::IPSocket
    if (socket_.params_.multicast_)
    {
        // make sure the socket exists before hacking it
        socket_.init_socket();
        unsigned char no = 0;
        if (::setsockopt(socket_.fd(), IPPROTO_IP, IP_MULTICAST_LOOP, (void*) &no, sizeof(no)) < 0)
        {
            log_warn("Failed to disable loop mode on multicast");
        }
    }

    in_addr_t bind_addr = local_addr_;
    // if we are NOT doing unicast, then do NOT bind to the
    // specified local_addr, as we will not "accept" incoming beacons
    // on the broadcast/multicast address
    if (socket_.params_.broadcast_ || socket_.params_.multicast_)
    {
        // FIXME: perhaps there should be a separate config item for the
        // local "bind" address; for now just bind to the port so that we
        // can catch all incoming beacons
        bind_addr = INADDR_ANY;
    }
    bool bind_success = (socket_.bind(bind_addr,port_) == 0);
    if (persist_)
    {
        // this retry loop assumes that the reason bind is failing is that the
        // interface/link is temp. down and retrying may catch the interface
        // once it comes back up - other errors may not benefit from retrying
        // (e.g. the address is not associated with this machine)
        // (see bind(2) for list of errors)
        oasys::Notifier* intr = socket_.get_notifier();
        while (! bind_success)
        {
            // continue on error ... sleep a bit and try again
            intr->wait(NULL,10000); // 10 seconds
            if (shutdown_)
            {
                return;
            }
            bind_success = (socket_.bind(bind_addr,port_) == 0);
        }
    }
    else if (!bind_success)
    {
        log_err("bind failed, IPND discovery aborted");
        return;
    }
    else
    {
    	log_debug("socket bind to %s:%d", intoa(bind_addr),port_);
    }

    int cc = 0;

    // send initial beacon to initialize start time
    if(!send_beacon(bp, 1024) && !persist_) {
        log_err("Failure sending initial beacon; aborting");
        return;
    }

    while (true)
    {
        if (shutdown_) break;

        // set the poll timeout based on our remaining interval time
        u_int timeout = interval_remaining();

        // is it time to beacon?
        if (timeout == 0)
        {
            // yes, send beacon
            if (!send_beacon(bp, 1024) && !persist_)
            {
                // beacon send failure and we're not persisting; quit
                log_err("Failure sending beacon; aborting");
                break;
            } else {
                // in all other cases reset the timeout to the beacon period
                timeout = beacon_period_ * 1000;
            }
        }

        // check for beacon expirations and adjust timeout as needed
        if (beacon_threshold_ > 1.0) {
            u_int min_neighbor_period = check_beacon_trackers();
            if (min_neighbor_period < timeout)
            {
                timeout = min_neighbor_period;
            }
        }

        // poll for beacons only as long as our current timeout
        log_debug("polling on socket: timeout %u", timeout);
        cc = socket_.poll_sockfd(POLLIN, NULL, timeout);

        if (shutdown_) {
            log_debug("shutdown bit set, exiting thread");
            break;
        }

        // if timeout, then skip processing and send another beacon
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

            cc = socket_.recvfrom((char*)bp, 1024, 0, &remote_addr, &remote_port);
            if (cc < 0)
            {
                log_err("error on recvfrom (%d): %s (%d)",cc,
                        strerror(errno),errno);
                if (!persist_)
                {
                    return;
                }
                else
                {
                    continue;
                }
            }

            std::string error_str;

            if (!process_advertisement(bp, cc, remote_addr, remote_port, error_str))
            {
                log_warn("unable to parse beacon from %s:%d > %s",
                         intoa(remote_addr),remote_port, error_str.c_str());
                // continue
            }
        }
        else
        {
            PANIC("unexpected result from poll (%d)",cc);
        }
    }
}

bool IPNDDiscovery::send_beacon(u_char* packet_buf, size_t packet_buf_len)
{
    size_t len = 0;
    int cc = 0;

    EndpointID local(BundleDaemon::instance()->local_eid());
    // FIXME: need to make the version level configurable in "discovery
    // announce" command?
    IPNDAnnouncement beacon(IPNDAnnouncement::IPND_VERSION_04, local, false);
    // use the next sequence number and increment
    beacon.set_sequence_number(beacon_sequence_num_);
    beacon.set_beacon_period((u_int64_t)beacon_period_);

    // add a service block entry for each "announced" service
    ServicesIter iter;
    log_debug("Adding %u service definitions to beacon", services_.size());
    for(iter = services_.begin(); iter != services_.end(); iter++) {
        beacon.add_service((*iter).second);
    }

    len = beacon.format(packet_buf, packet_buf_len);

    // (agladd) Debug output
    oasys::HexDumpBuffer hdb(len);
    hdb.append(packet_buf, len);
    log_debug("Raw formatted beacon [send]:\n%s", hdb.hexify().c_str());

    oasys::UDPClient* sock = &socket_;
    cc = sock->sendto((char*)packet_buf, len, 0, remote_addr_, port_);
    if (cc != (int) len)
    {
        log_err("sendto failed: %s (%d)", strerror(errno),errno);
        return false;
    }
    else
    {
        // update sequence number on successful send
        if (beacon_sequence_num_ == 0xFFFF)
        {
            // roll-over the sequence number counter
            beacon_sequence_num_ = 0;
        }
        else
        {
            beacon_sequence_num_ += 1;
        }
        // capture the current time to support interval tracking
        ::gettimeofday(&beacon_sent_,0);
        return true;
    }
}

bool IPNDDiscovery::process_advertisement(u_char* buf, size_t len,
        in_addr_t remote_addr, u_int16_t remote_port, std::string& errorStr) {

    // (agladd) Debug output
    oasys::HexDumpBuffer hdb(len);
    hdb.append(buf, len);
    log_debug("Raw formatted beacon [recv]:\n%s", hdb.hexify().c_str());

    // parse beacon
	IPNDAnnouncement announcement;
	std::string bEid = "unknown";
	if (!announcement.parse(buf, len))
	{
		errorStr = "Unrecognized or malformed beacon format.";
		return false;
	}

	// handle EID
	if (announcement.get_flags() & IPNDAnnouncement::BEACON_CONTAINS_EID)
	{
	    if (announcement.get_endpoint_id().equals(
	            BundleDaemon::instance()->local_eid()))
	    {
	        log_debug("Ignoring beacon from self (%s:%d)", intoa(remote_addr),
	                remote_port);
	        return true;
	    } else {
	        bEid = announcement.get_endpoint_id().str();
	    }
	}
	else
	{
	    log_debug("Ignoring beacon with unspecified EID from (%s:%d)",
	            intoa(remote_addr),remote_port);
	    return true;
	}

	// handle services
	// XXX (agladd): Since version 0x04 introduced such major changes, we're
    // currently only implementing the new service block and are not
    // backward compatible with previous versions
	if(announcement.get_version() >= IPNDAnnouncement::IPND_VERSION_04 &&
	        (announcement.get_flags() &
	                IPNDAnnouncement::BEACON_SERVICE_BLOCK)) {
	    // check if we actually got services that we recognize
	    if(announcement.get_services().empty()) {
	        log_debug("No recognized services in beacon from %s (%s:%d)",
	                bEid.c_str(), intoa(remote_addr), remote_port);
	    } else {
	        std::list<IPNDService*> svcs = announcement.get_services();
	        std::list<IPNDService*>::const_iterator iter;
	        for(iter = svcs.begin(); iter != svcs.end(); iter++) {
	            handle_service(announcement, (*iter));
	        }
	    }
	} else if(!(announcement.get_flags() &
	        IPNDAnnouncement::BEACON_SERVICE_BLOCK)) {
	    log_debug("Beacon has no service block");
	} else {
	    log_debug("Ignoring service block [ver=0x%02x, flags=0x%02x]",
	            announcement.get_version(), announcement.get_flags());
	}

	return true;
}

void IPNDDiscovery::handle_service(const IPNDAnnouncement &beacon,
        const IPNDService *svc) {
    // XXX (agladd): Currently only implementing handling for the minimum
    // requirement for IPND (i.e. only the cla-tcp-v6 and cla-udp-v4 services)

    std::string claType, claIp, claPort;
    const TcpV4ClaService *t4;
    const UdpV4ClaService *u4;

    // XXX (agladd): Could try dynamic_casts here instead, but this is
    // sufficient for now.
    switch(svc->getTag()) {
    case ipndtlv::IPNDSDTLV::C_CLA_TCPv4:
        // cla-tcp-v4
        t4 = static_cast<const TcpV4ClaService*>(svc);
        claType = std::string(type_to_str(TCPCL));
        claIp = t4->getIpAddrStr();
        claPort = t4->getPortStr();
        break;
    case ipndtlv::IPNDSDTLV::C_CLA_UDPv4:
        // cla-udp-v4
        u4 = static_cast<const UdpV4ClaService*>(svc);
        claType = std::string(type_to_str(UDPCL));
        claIp = u4->getIpAddrStr();
        claPort = u4->getPortStr();
        break;
    default:
        log_info("Service handler ignoring service [tag=0x%02x]",
                svc->getTag());
        return;
    }

    // check for valid ip/port
    if(!claIp.empty() && !claPort.empty()) {
        // pass discovery down for linking
        std::string nextHop = claIp + ":" + claPort;
        handle_neighbor_discovered(claType, nextHop, beacon.get_endpoint_id());

        // check if we can update our tracking for this neighbor
        if(beacon.get_beacon_period() > 0) {
            update_beacon_tracker(claType, nextHop, beacon.get_endpoint_id(),
                    beacon.get_beacon_period());
        }
    } else {
        log_err("Malformed IP address or port number; cannot process neighbor "
                "CLA discovery!");
    }
}

void IPNDDiscovery::update_beacon_tracker(const std::string& cl_type,
        	   	   	   	   	   	   	      const std::string& cl_addr,
        	   	   	   	   	   	   	      const EndpointID& remote_eid,
        	   	   	   	   	   	   	      const u_int64_t new_period)
{
	// FIXME: The use of a string key could probably be made more efficient
	std::stringstream key_buf;
	key_buf << remote_eid.str() << "|" << cl_type << "|" << cl_addr;

	BeaconInfo* info;
	BeaconInfoMap::iterator found = beacon_info_map_.find(key_buf.str());
	if (found != beacon_info_map_.end())
	{
		info = found->second;
	}
	else
	{
		info = new BeaconInfo();
		info->cl_type = cl_type;
		info->cl_addr = cl_addr;
		info->remote_eid = remote_eid;
		beacon_info_map_[key_buf.str()] = info;
	}
	::gettimeofday(&info->last_beacon_, 0);
	info->last_period_ = new_period;
}

u_int IPNDDiscovery::check_beacon_trackers(void)
{
	u_int min_interval = INT_MAX;
	// cycle through tracked beacons to find expirations
	// use a common time reference as "now"
	struct timeval now;
	struct timeval diff;
	::gettimeofday(&now, 0);
	for (BeaconInfoMap::iterator iter = beacon_info_map_.begin();
			iter != beacon_info_map_.end(); iter++) {
		BeaconInfo* bi = iter->second;
		timersub(&now, &(bi->last_beacon_), &diff);
		u_int64_t millis_diff = (diff.tv_sec * (u_int64_t)1000) +
		                        (diff.tv_usec / (u_int64_t)1000);
		double threshold = ((double)bi->last_period_ * 1000 * beacon_threshold_);
		if (millis_diff > threshold)
		{
			log_debug("Missed beacon for %s", iter->first.c_str());
			handle_missing_beacon(bi);
			beacon_info_map_.erase(iter);
			delete bi;
		}
		else
		{
			// keep track of the smallest period between now and the
		    // next beacon expiration
		    u_int thresh_remain = threshold - millis_diff;
			if (thresh_remain < min_interval)
			{
				min_interval = thresh_remain;
			}
		}
	}
	return min_interval;
}

void IPNDDiscovery::handle_missing_beacon(BeaconInfo* bi)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(bi->cl_type.c_str());
    if (cl == NULL) {
        log_err("handle_missing_beacon: "
                "unknown convergence layer type '%s'", bi->cl_type.c_str());
        return;
    }

	// Look for match on convergence layer and remote EID
	LinkRef link("IPNDDiscovery::handle_missing_beacon");
	link = cm->find_link_to(cl, "", bi->remote_eid);

	if (link == NULL) {
	    log_err("handle_missing_beacon: "
	            "unknown link to '%s'", bi->remote_eid.c_str());
	    return;
	}

	ASSERT(link != NULL);

	if (link->isopen())
	{
		// request to set link unavailable
		BundleDaemon::post(
			new LinkStateChangeRequest(link, Link::CLOSED,
			                           ContactEvent::DISCOVERY)
		);
	}
}

} // namespace dtn
