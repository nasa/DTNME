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
#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/util/Glob.h>

#include "DTNScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::DTNScheme* oasys::Singleton<dtn::DTNScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
DTNScheme::validate(const URI& uri, bool is_pattern, 
                    bool& node_wildcard, bool& service_wildcard)
{
    node_wildcard = false;
    service_wildcard = false;

    (void)is_pattern;

    if (!uri.valid()) {
        log_debug_p("/dtn/scheme/dtn", "DTNScheme::validate: invalid URI");
        return false;
    }

    // a valid dtn scheme uri must have a host component in the
    // authority unless it's the special "dtn:none" uri
    if (uri.host().length() == 0) {
        if (uri.ssp() != "none") {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::match(const EndpointIDPattern& pattern,
                 const SPtr_EID& sptr_eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);

    // we only match endpoint ids of the same scheme
    if (!sptr_eid->known_scheme() || (sptr_eid->scheme() != this)) {
        return false;
    }
    
    // if the ssp of either string is "none", then nothing should
    // match it (ever)
    if (pattern.ssp() == "none" || sptr_eid->ssp() == "none") {
        return false;
    }
    
    // check for a wildcard host specifier e.g dtn://*
    if (pattern.uri().host() == "*" && pattern.uri().path() == "")
    {
        return true;
    }
    
    // use glob rules to match the hostname part -- note that we don't
    // distinguish between which one has the glob patterns so we run
    // glob in both directions looking for a match
    if (! (oasys::Glob::fixed_glob(pattern.uri().host().c_str(),
                                   sptr_eid->uri().host().c_str()) ||
           oasys::Glob::fixed_glob(sptr_eid->uri().host().c_str(),
                                   pattern.uri().host().c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: uri hosts don't glob ('%s' != '%s')",
                    sptr_eid->uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().host().c_str(), sptr_eid->uri().host().c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern.uri().port_num() != sptr_eid->uri().port_num())
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: uri ports not equal (%d != %d)",
                    sptr_eid->uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().port_num(), sptr_eid->uri().port_num());
        return false;
    }

    // XXX/demmer I don't understand why this is needed...
    std::string pattern_path(pattern.uri().path());
    if ((pattern_path.length() >= 2) &&
        (pattern_path.substr((pattern_path.length() - 2), 2) == "/*")) {
        pattern_path.replace((pattern_path.length() - 2), 2, 1, '*');
    }

    // check for a glob match of the path strings
    if (! (oasys::Glob::fixed_glob(pattern_path.c_str(),
                                   sptr_eid->uri().path().c_str()) ||
           oasys::Glob::fixed_glob(sptr_eid->uri().path().c_str(),
                                   pattern_path.c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: paths don't glob ('%s' != '%s')",
                    sptr_eid->uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().path().c_str(), sptr_eid->uri().path().c_str());
        return false;
    }

    // XXX/demmer: maybe if the pattern has any query parameters, then
    // they should match or glob to the equivalent ones in the eid.
    // parameters present in the eid but not in the pattern should be
    // ignored
    
    // ignore the query parameter strings, so they still match the glob'd routes
    return true;
}

//----------------------------------------------------------------------
Scheme::eid_dest_type_t
DTNScheme::eid_dest_type(const URI& uri) 
{
    // BPv7 - multinode indicated by path starting with "/~"
    if (0 ==  strncmp(uri.path().c_str(), "/~", 2)) {
        log_debug_p("/dtn/scheme/dtn",
                    "URI path (%s) starts with a tilda so is BP7 MULTINODE",
                    uri.path().c_str());
        return EndpointID::MULTINODE;
    }

    // if there's a * in the hostname part of the URI, then it's not a
    // singleton endpoint
    if (uri.host().find('*') != std::string::npos) {
        log_debug_p("/dtn/scheme/dtn",
                    "URI host %s contains a wildcard, so is BP6 MULTINODE",
                    uri.host().c_str());
        return EndpointID::MULTINODE;
    }
    
    log_debug_p("/dtn/scheme/dtn",
                "URI host %s does not contain a BP6 wildcard or BP7 tilde, so is SINGLETON",
                uri.host().c_str());
    return EndpointID::SINGLETON;
}

//----------------------------------------------------------------------
bool
DTNScheme::is_singleton(const URI& uri)
{
    // BPv7 - multinode indicated by path starting with "/~"
    if (0 ==  strncmp(uri.path().c_str(), "/~", 2)) {
        log_debug_p("/dtn/scheme/dtn",
                    "URI path (%s) starts with a tilda so is BP7 MULTINODE",
                    uri.path().c_str());
        return false;
    }

    // if there's a * in the hostname part of the URI, then it's not a
    // singleton endpoint
    if (uri.host().find('*') != std::string::npos) {
        log_debug_p("/dtn/scheme/dtn",
                    "URI host %s contains a wildcard, so is BP6 MULTINODE",
                    uri.host().c_str());
        return false;
    }
    
    log_debug_p("/dtn/scheme/dtn",
                "URI host %s does not contain a BP6 wildcard or BP7 tilde, so is SINGLETON",
                uri.host().c_str());
    return true;
}

} // namespace dtn
