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

#include "WildcardScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::WildcardScheme* oasys::Singleton<dtn::WildcardScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
WildcardScheme::validate(const URI& uri, bool is_pattern)
{
    // the wildcard scheme can only be used for patterns and must be
    // exactly the string "*"
    if ((is_pattern == false) || (uri.ssp() != "*")) {
        return false;
    }

    return true;
}
    
//----------------------------------------------------------------------
bool
WildcardScheme::match(const EndpointIDPattern& pattern,
                      const EndpointID& eid)
{
    (void)eid;
    ASSERT(pattern.scheme() == this); // sanity

    // XXX/demmer why was this here:
    
    // the only thing we don't match is the special null endpoint
//     if (eid.str() == "dtn:none") {
//         return false;
//     }

    return true;
}

//----------------------------------------------------------------------
Scheme::singleton_info_t
WildcardScheme::is_singleton(const URI& uri)
{
    (void)uri;
    return EndpointID::MULTINODE;
}

} // namespace dtn

