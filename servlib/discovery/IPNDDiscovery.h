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
 * IPNDDiscovery.h
 *
 * An implementation of DTN IP Neighbor Discovery as defined in
 * draft-irtf-dtnrg-ipnd-02.
 *
 * The structure of this code is based on the IPDiscovery.cc implementation for
 * IP-based neighbor discovery.  The IPND implementation maintains the same
 * basic configuration parameters, but formats the beacon packets according to
 * the IPND Internet Draft referenced above and adds detection and appropriate
 * handling of missing beacons.
 *
 * Initial implementation by Rick Altmann (raltmann at bbn dot com)
 * IPND-SD-TLV integration by Alex Gladd (agladd at bbn dot com)
 */

#ifndef _IPND_DISCOVERY_H_
#define _IPND_DISCOVERY_H_

#include <map>
#include <string>

#include <oasys/thread/Thread.h>
#include <oasys/thread/Notifier.h>
#include <oasys/io/NetUtils.h>
#include <oasys/io/UDPClient.h>

#include "discovery/Discovery.h"
#include "IPNDService.h"
#include "IPNDAnnouncement.h"

namespace dtn {

/**
 * IPNDDiscovery is the main thread in IP-based neighbor discovery,
 * configured via config file or command console to listen to a specified
 * UDP port for unicast, broadcast, or multicast beacons from advertising
 * neighbors.
 */
class IPNDDiscovery : public Discovery,
                    public oasys::Thread
{
public:

	class BeaconInfo {
	public:
		std::string cl_type;         ///< CL type of the beacon
		std::string cl_addr;         ///< address (and port) details
		EndpointID remote_eid;       ///< endpoint id that sent the beacon
		u_int64_t last_period_;      ///< period declared in the last beacon
		struct timeval last_beacon_; ///< time last beacon was received
	};

    /**
     * If no other options are set for destination, default to sending 
     * to the IPv4 broadcast address.
     */
    static const u_int32_t DEFAULT_DST_ADDR;

    /**
     * If no other options are set for source, use this as default
     * local address
     */
    static const u_int32_t DEFAULT_SRC_ADDR;

    /**
     * If no other options are set for multicast TTL, set to 1
     */
    static const u_int DEFAULT_MCAST_TTL;

    /**
     * Default beacon period in seconds
     */
    static const u_int32_t DEFAULT_BEACON_PERIOD;

    /**
     * Default missed beacon threshold (0 = no missed beacon tracking)
     */
    static const double DEFAULT_BEACON_THRESHOLD;

    /**
     * Enumerate which type of CL is advertised
     */
    typedef enum
    {
        UNDEFINED = 0,
        TCPCL     = 1, // TCP Convergence Layer
        UDPCL     = 2, // UDP Convergence Layer
    }
    cl_type_t;

    static const char* type_to_str(cl_type_t t)
    {
        switch(t) {
            case TCPCL: return "tcp";
            case UDPCL: return "udp";
            case UNDEFINED: return "unsupported";
            default: PANIC("Undefined cl_type_t %d",t);
        }
        NOTREACHED;
    }

    static cl_type_t str_to_type(const char* cltype)
    {
    	/**
    	 * "tcpcl" and "udpcl" are the identifiers used in IBR-DTN for the
    	 * TCP and UDP convergence layer adapters respectively
    	 */
        if (strncmp(cltype,"tcpcl",5) == 0 || 
            strncmp(cltype,"tcp",3) == 0 ||
            strncmp(cltype,"cl-tcp",6) == 0)
            return TCPCL;
        else
        if (strncmp(cltype,"udpcl",5) == 0 || 
            strncmp(cltype,"udp",3) == 0 ||
            strncmp(cltype,"cl-udp",6) == 0)
            return UDPCL;
        else
            return UNDEFINED;
    }

    /**
     * XXX (agladd): THIS VERSION OF ANNOUNCE IS NOT USED FOR IPND.
     *
     * ALWAYS RETURNS FALSE.
     *
     * @param name An arbitrary name to identify the service
     * @param argc The number of additional parameters
     * @param argv The array of additional parameters
     */
    bool announce(const char* name, int argc, const char* argv[]);

    /**
     * XXX (agladd): IPND uses a slightly different implementation of
     * announce, since we are supposed to handle any arbitrary "service"
     * advertisement, not just convergence layer advertisements.
     *
     * @param name An arbitrary name to identify the service
     * @param argc The number of additional parameters
     * @param argv The array of additional parameters
     */
    bool svc_announce(const char* name, int argc, const char* argv[]);

    /**
     * Close main socket, causing thread to exit
     */
    void shutdown() { shutdown_ = true; socket_.get_notifier()->notify(); }

    virtual ~IPNDDiscovery();

protected:
    friend class Discovery;

    IPNDDiscovery(const std::string& name);

    /**
     * Set internal state using parameter list; return true
     * on success, else false
     */
    bool configure(int argc, const char* argv[]);

    /**
     * virtual from oasys::Thread
     */
    void run();

    /**
     * Convenience method to pull the relevant items out of 
     * the inbound packet
     */
    bool process_advertisement(u_char* buf, size_t len,
    		                   in_addr_t remote_addr, u_int16_t remote_port,
                               std::string& errorStr);

    /**
     * Handler for a service that was received in an inbound advertisement
     */
    void handle_service(const IPNDAnnouncement &beacon,
            const IPNDService *svc);

    /**
     * Update internal beacon tracker for the specified Endpoint
     * and convergence layer details.
     */
    void update_beacon_tracker(const std::string& cl_type,
                               const std::string& cl_addr,
                               const EndpointID& remote_eid,
                               const u_int64_t new_period);

    /**
     * Check all neighbors for missing beacons.  Act upon any neighbors
     * that seem to be gone based on a "missing" beacon and clean up the
     * tracking information as necessary.
     */
    u_int check_beacon_trackers(void);

    /**
     * Perform actions necessary for a beacon that has been missed.
     * The associated link should be marked as unavailable.
     */
    void handle_missing_beacon(BeaconInfo* beacon_info);

    bool send_beacon(u_char* packet_buf, size_t packet_buf_len);

    /**
     * Virtual from Discovery
     */
    void handle_announce()
    {
        socket_.get_notifier()->notify();
    }

private:

    /**
     * Return the number of milliseconds remaining until the interval
     * expires, or 0 if it's already expired
     */
    u_int interval_remaining()
    {
        struct timeval now;
        ::gettimeofday(&now,0);
        u_int timediff = TIMEVAL_DIFF_MSEC(now, beacon_sent_);
        u_int64_t beacon_period_millis = beacon_period_ * 1000;

        if (timediff > (beacon_period_millis)) {
            return 0;
        } else {
            return (beacon_period_millis - timediff);
        }
    }

    /**
     * Shortcut def for beacon map
     */
    typedef std::map<std::string, BeaconInfo*> BeaconInfoMap;

    /**
     * Shortcut def for services_ map iterator
     */
    typedef std::map<std::string, IPNDService*>::const_iterator ServicesIter;

    volatile bool shutdown_; ///< signal to close down thread
    in_addr_t local_addr_;   ///< address for bind() to receive beacons
    u_int16_t port_;         ///< local and remote
    in_addr_t remote_addr_;  ///< whether unicast, multicast, or broadcast
    u_int mcast_ttl_;        ///< TTL hop count for multicast option
    oasys::UDPClient socket_; ///< the socket for beacons in- and out-bound
    bool persist_;           ///< whether to exit thread on send/recv failures

    u_int32_t beacon_period_; ///< Beacon period in seconds
    double beacon_threshold_;  ///< Expression of tolerance for missed beacons
    BeaconInfoMap beacon_info_map_; ///< map for tracking beacons
    u_int16_t beacon_sequence_num_; ///< last sequence number sent
    struct timeval beacon_sent_; ///< mark each time data is sent

    std::map<std::string, IPNDService*> services_; ///< container for service block entries
};

} // namespace dtn

#endif /* _IPND_DISCOVERY_H_ */
