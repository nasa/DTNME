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

#ifndef _ETH_DISCOVERY_H_
#define _ETH_DISCOVERY_H_

#include <oasys/thread/Thread.h>

#include "Discovery.h"

namespace dtn {

class EthDiscovery : public Discovery,
                     public oasys::Thread
{
public:

    void shutdown() { shutdown_ = true; }

    virtual ~EthDiscovery() {}

protected:
    friend class Discovery;
    friend class EthAnnounce;

    /**
     * Ethernet beacon header structure.
     * Version and type fields are for compatibility with EthCLHeader
     */
    struct EthBeaconHeader {
        u_int8_t version;               ///< ethcl protocol version
        u_int8_t type;                  ///<
        u_int8_t interval;              ///< interval in 100ms units
        u_int16_t eid_len;              ///<
        char sender_eid[0];
    } __attribute__((packed));


    static const u_int8_t  ETHCL_VERSION = 0x01;
    static const u_int16_t ETHERTYPE_DTN_BEACON = 0xd711;
    static const u_int8_t  ETHCL_BEACON = 0x01;

    EthDiscovery(const std::string& name);
    bool configure(int argc, const char* argv[]);
    void run();
    bool parse_advertisement(u_char* bp, size_t len, u_char* remote_addr,
                             std::string& nexthop, EndpointID& remote_eid);
    void handle_neighbor_discovered(const std::string& cl_type,
                                    const std::string& cl_addr,
                                    const EndpointID& remote_eid);

    volatile bool shutdown_;
    std::string iface_;
    int sock_;
};

} // dtn

#endif
