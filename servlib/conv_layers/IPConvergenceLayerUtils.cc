/*
 *    Copyright 2006 Intel Corporation
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

#include <oasys/io/NetUtils.h>
#include "IPConvergenceLayerUtils.h"

namespace dtn {

/**
 * Parse a next hop address specification of the form
 * HOST[:PORT?].
 *
 * @return true if the conversion was successful, false
 */
bool
IPConvergenceLayerUtils::parse_nexthop(const char* logpath, const char* nexthop,
                                       in_addr_t* addr, u_int16_t* port)
{
    const char* host;
    std::string tmp;
                        
    *addr = INADDR_NONE;
    *port = 0;

    // first see if there's a colon in the string -- if so, it should
    // separate the hostname and port, if not, then the whole string
    // should be a hostname.
    //
    // note that it is invalid to have more than one colon in the
    // string, as caught by the check after calling strtoul
    const char* colon = strchr(nexthop, ':');
    if (colon != NULL) {
        char* endstr;
        u_int32_t portval = strtoul(colon + 1, &endstr, 10);
        
        if (*endstr != '\0' || portval > 65535) {
            log_warn_p(logpath, "invalid port %s in next hop '%s'",
                       colon + 1, nexthop);
            return false;
        }

        *port = (u_int16_t)portval;
        
        tmp.assign(nexthop, colon - nexthop);
        host = tmp.c_str();
    } else {
        host = nexthop;
    }

    // now look up the hostname
    if (oasys::gethostbyname(host, addr) != 0) {
        log_warn_p(logpath, "invalid hostname '%s' in next hop %s",
                   host, nexthop);
        return false;
    }
    
    return true;
}
    

} // namespace dtn
