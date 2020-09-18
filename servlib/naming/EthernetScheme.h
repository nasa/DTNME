/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _ETHERNET_SCHEME_H_
#define _ETHERNET_SCHEME_H_

#ifdef __linux__

#include "Scheme.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>
#include <oasys/util/Singleton.h>

namespace dtn {

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif
    
typedef struct ether_addr {
    u_char octet[ETHER_ADDR_LEN];
} eth_addr_t;    

/**
 * Endpoint ID scheme to represent ethernet-style addresses.
 *
 * e.g:  eth://00:01:02:03:04:05
 */
class EthernetScheme : public Scheme, public oasys::Singleton<EthernetScheme> {
public:
    /**
     * Validate that the SSP within the given URI is legitimate for
     * this scheme. If the 'is_pattern' paraemeter is true, then the
     * ssp is being validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const URI& uri, bool is_pattern = false);

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);
    
    /**
     * Check if the given URI is a singleton endpoint id.
     */
    virtual singleton_info_t is_singleton(const URI& uri);
    
    /*
     * Parse out an ethernet address from the ssp.
     *
     * @return true if the extraction was a success, false if
     * malformed
     */
    static bool parse(const std::string& ssp, eth_addr_t* addr);

    /**
     * Given an ethernet address, write out a string representation.
     * addr needs to point to a six-byte buffer that contains the address.
     * outstring needs to point to a buffer of length at least 23 chars. 
     * eth://00:00:00:00:00:00
     * Returns outstring. 
     */
    static char* to_string(u_int8_t* addr, char* outstring);

private:
    friend class oasys::Singleton<EthernetScheme>;
    EthernetScheme() {}
};
    
} // namespace dtn


#endif /* __linux__ */

#endif /* _ETHERNET_SCHEME_H_ */
