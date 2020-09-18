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

#ifndef _IP_DISCOVERY_H_
#define _IP_DISCOVERY_H_

#include <oasys/thread/Thread.h>
#include <oasys/thread/Notifier.h>
#include <oasys/io/NetUtils.h>
#include <oasys/io/UDPClient.h>

#include "discovery/Discovery.h"

namespace dtn {

/**
 * IPDiscovery is the main thread in IP-based neighbor discovery,
 * configured via config file or command console to listen to a specified
 * UDP port for unicast, broadcast, or multicast beacons from advertising
 * neighbors.
 */
class IPDiscovery : public Discovery,
                    public oasys::Thread
{
public:

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
     * On-the-wire (radio, whatever) representation of
     * IP address family's advertisement beacon
     */
    struct DiscoveryHeader
    {
        u_int8_t cl_type;         // Type of CL offered
        u_int8_t interval;        // 100ms units
        u_int16_t length;         // total length of packet
        u_int32_t inet_addr;      // IPv4 address of CL
        u_int16_t inet_port;      // IPv4 port of CL
        u_int16_t name_len;       // length of EID
        char sender_name[0];      // DTN URI of beacon sender
    }
    __attribute__((packed));

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
            case UNDEFINED:
            default: PANIC("Undefined cl_type_t %d",t);
        }
        NOTREACHED;
    }

    static cl_type_t str_to_type(const char* cltype)
    {
        if (strncmp(cltype,"tcp",3) == 0)
            return TCPCL;
        else
        if (strncmp(cltype,"udp",3) == 0)
            return UDPCL;
        else
            PANIC("Unsupported CL type %s",cltype);
        NOTREACHED;
    }

    /**
     * Close main socket, causing thread to exit
     */
    void shutdown() { shutdown_ = true; socket_.get_notifier()->notify(); }

    virtual ~IPDiscovery() {}

protected:
    friend class Discovery;

    IPDiscovery(const std::string& name);

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
    bool parse_advertisement(u_char* buf, size_t len,
                             in_addr_t remote_addr,
                             u_int8_t& cl_type,
                             std::string& nexthop,
                             EndpointID& remote_eid);

    /**
     * Virtual from Discovery
     */
    void handle_announce()
    {
        socket_.get_notifier()->notify();
    }

    volatile bool shutdown_; ///< signal to close down thread
    in_addr_t local_addr_;   ///< address for bind() to receive beacons
    u_int16_t port_;         ///< local and remote 
    in_addr_t remote_addr_;  ///< whether unicast, multicast, or broadcast 
    u_int mcast_ttl_;        ///< TTL hop count for multicast option
    oasys::UDPClient socket_; ///< the socket for beacons in- and out-bound
    bool persist_;           ///< whether to exit thread on send/recv failures
};

} // namespace dtn

#endif /* _IP_DISCOVERY_H_ */
