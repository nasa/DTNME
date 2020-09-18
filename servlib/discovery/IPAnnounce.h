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

#ifndef _IP_ANNOUNCE_H_
#define _IP_ANNOUNCE_H_

#include "Announce.h"
#include "IPDiscovery.h"
#include <oasys/io/UDPClient.h>

namespace dtn {

/**
 * Helper class that 1) formats outbound beacons to advertise this
 * CL instance via neighbor discovery, and 2) responds to inbound
 * advertisements by creating a new Contact
 */
class IPAnnounce : public Announce
{
public:
    /**
     * Serialize announcement out to buffer
     */
    size_t format_advertisement(u_char* buf, size_t len);

    /**
     * Export cl_addr to use in sending Announcement out on correct interface
     */
    const in_addr_t& cl_addr() const { return cl_addr_; }

protected:
    friend class Announce;

    typedef IPDiscovery::DiscoveryHeader DiscoveryHeader;

    /**
     * Constructor
     */
    IPAnnounce();

    /**
     * Deserialize parameters for configuration
     */
    bool configure(const std::string& name, ConvergenceLayer* cl, 
                   int argc, const char* argv[]);

    /**
     * next hop info for CL to be advertised
     */
    in_addr_t cl_addr_;
    u_int16_t cl_port_;
};

} // dtn
#endif // _IP_ANNOUNCE_H_
