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

#include <ctype.h>
#include <oasys/debug/Log.h>
#include <oasys/util/Glob.h>

#include "SessionScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::SessionScheme* oasys::Singleton<dtn::SessionScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
SessionScheme::validate(const URI& uri, bool is_pattern)
{
    (void)is_pattern;

    // the uri itself must be valid
    if (!uri.valid()) {
        log_debug_p("/dtn/scheme/session", "SessionScheme::validate: invalid URI");
        return false;
    }

    // for patterns, the sub-uri need not be valid (i.e. dtn-session:* is ok)
    if (is_pattern) {
        return true;
    }

    // otherwise, there must be a valid sub-uri
    URI sub_uri(uri.ssp());
    if (!sub_uri.valid()) {
        log_debug_p("/dtn/scheme/session", "SessionScheme::validate: invalid sub URI");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
SessionScheme::match(const EndpointIDPattern& pattern, const EndpointID& eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);

    // we only match endpoint ids of the same scheme
    if (!eid.known_scheme() || (eid.scheme() != this)) {
        return false;
    }

    // use globbing to match
    if (oasys::Glob::fixed_glob(pattern.uri().ssp().c_str(),
                                eid.uri().ssp().c_str()))
    {
        log_debug_p("/dtn/scheme/session", "match(%s, %s) success",
                    eid.uri().c_str(), pattern.uri().c_str());
        return true;
    }
    else
    {
        log_debug_p("/dtn/scheme/session", "match(%s, %s) failed",
                    eid.uri().c_str(), pattern.uri().c_str());
        return false;
    }
}

//----------------------------------------------------------------------
Scheme::singleton_info_t
SessionScheme::is_singleton(const URI& uri)
{
    (void)uri;
    // XXX/demmer is this right??
    return EndpointID::SINGLETON;
}

} // namespace dtn
