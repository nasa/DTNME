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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef __linux__

#include <oasys/io/IPSocket.h>
#include <oasys/io/NetUtils.h>

#include "EthernetScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::EthernetScheme* oasys::Singleton<dtn::EthernetScheme>::instance_ = 0;
}

namespace dtn {

/*
 * Parse out an ethernet address from the ssp.
 *
 * @return true if the string is a valid ethernet address, false if not.
 */
bool
EthernetScheme::parse(const std::string& ssp, eth_addr_t* addr)
{
    // XXX/jakob - for now, assume it's a correctly formatted ethernet URI
    // eth://00:01:02:03:04:05 or eth:00:01:02:03:04:05

    const char* str = ssp.c_str();
    if (str[0] == '/' && str[1] == '/') {
        str = str + 2;
    }
    
    // this is a nasty hack. grab the ethernet address out of there,
    // assuming everything is correct
    int r = sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
                   &addr->octet[0],
                   &addr->octet[1],
                   &addr->octet[2],
                   &addr->octet[3],
                   &addr->octet[4],
                   &addr->octet[5]);
    if (r != 6) {
        return false;
    }

    return true;
}

/**
 * Validate that the SSP in the given URI is legitimate for
 * this scheme. If the 'is_pattern' parameter is true, then
 * the ssp is being validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
EthernetScheme::validate(const URI& uri, bool is_pattern)
{
    (void)is_pattern;
    
    // make sure it's a valid url
    eth_addr_t addr;
    return parse(uri.ssp(), &addr);
}

/**
 * Match the given ssp with the given pattern.
 *
 * @return true if it matches
 */
bool
EthernetScheme::match(const EndpointIDPattern& pattern,
                      const EndpointID& eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);
    
    log_debug_p("/dtn/scheme/ethernet",
                "matching %s against %s.", pattern.ssp().c_str(), eid.ssp().c_str());

    size_t patternlen = pattern.ssp().length();
    
    if (pattern.ssp() == eid.ssp()) 
        return true;
    
    if (patternlen >= 1 && pattern.ssp()[patternlen-1] == '*') {
        patternlen--;
        
        if (pattern.ssp().substr(0, patternlen) ==
            eid.ssp().substr(0, patternlen))
        {
            return true;
        }
    }

    return false;
}

EndpointID::singleton_info_t
EthernetScheme::is_singleton(const URI& uri)
{
    (void)uri;
    return EndpointID::SINGLETON;
}

char* 
EthernetScheme::to_string(u_int8_t* addr, char* outstring)
{
    sprintf(outstring,"eth://%02X:%02X:%02X:%02X:%02X:%02X", 
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    return outstring;
}


} // namespace dtn

#endif /* __linux__ */
