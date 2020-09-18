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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "StringScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::StringScheme* oasys::Singleton<dtn::StringScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
StringScheme::validate(const URI& uri, bool is_pattern)
{
    (void)is_pattern;
    
    if (uri.ssp() == "")
        return false;
    
    return true;
}

//----------------------------------------------------------------------
bool
StringScheme::match(const EndpointIDPattern& pattern,
                    const EndpointID& eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);
    return (pattern.str() == eid.str());
}

//----------------------------------------------------------------------
Scheme::singleton_info_t
StringScheme::is_singleton(const URI& uri)
{
    (void)uri;
    return EndpointID::SINGLETON;
}

} // namespace dtn
