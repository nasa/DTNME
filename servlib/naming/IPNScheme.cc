/*
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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
#include <stdlib.h>

#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>

#include "IPNScheme.h"
#include "EndpointID.h"

namespace oasys {
    template <> dtn::IPNScheme* oasys::Singleton<dtn::IPNScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
IPNScheme::parse(const std::string& ssp,
                 u_int64_t* node, u_int64_t* service)
{
    if (0 == ssp.compare("none")) {
        *node = 0;
        *service = 0;
        return true;
    }

    // otherwise there must be a node
    size_t dot = ssp.find('.');
    if (dot == 0 || dot == std::string::npos) {
        log_debug_p("/dtn/scheme/ipn",
                    "no node portion in ssp %s", ssp.c_str());
        return false;
    }   

    // parse the node
    char* end;
    *node = strtoull(ssp.c_str(), &end, 10);
    if (end != (ssp.c_str() + dot)) {
        log_debug_p("/dtn/scheme/ipn",
                    "invalid node portion '%.*s' in ssp %s",
                    static_cast<int>(dot), ssp.c_str(), ssp.c_str());
        return false;
    }   
    
    // parse the service
    *service = strtoull(ssp.c_str() + dot + 1, &end, 10);
    if (end == ssp.c_str() + dot + 1 ||
        end != (ssp.c_str() + ssp.length())) {
        log_debug_p("/dtn/scheme/ipn",
                    "invalid service portion (parse) '%s' in ssp %s",
                    ssp.c_str() + dot + 1, ssp.c_str());
        return false;
    }   
 
    return true;
}

//----------------------------------------------------------------------
void
IPNScheme::format(std::string* ssp, u_int64_t node, u_int64_t service)
{
    oasys::StaticStringBuffer<256> buf("%llu.%llu",
                                       U64FMT(node), U64FMT(service));
    ssp->assign(buf.c_str());
}

//----------------------------------------------------------------------
bool
IPNScheme::validate(const URI& uri, bool is_pattern)
{
    if (!uri.valid()) {
        log_debug_p("/dtn/scheme/ipn", "IPNScheme::validate: invalid URI");
        return false;
    }
    
    u_int64_t node, service;

    if (!is_pattern) {
        return parse(uri.ssp(), &node, &service);
    } else {
        std::string ssp = uri.ssp();

        if (0 == ssp.compare("*")) {
            return true;
        } else {
            // otherwise there must be a node
            size_t dot = ssp.find('.');
            if (dot == 0 || dot == std::string::npos) {
                log_debug_p("/dtn/scheme/ipn",
                            "no node portion in ssp %s", ssp.c_str());
                return false;
            }   

            char* end;
            node = strtoull(ssp.c_str(), &end, 10);
            if (end != (ssp.c_str() + dot)) {
                log_debug_p("/dtn/scheme/ipn",
                            "invalid node portion '%.*s' in ssp %s",
                            static_cast<int>(dot), ssp.c_str(), ssp.c_str());
                return false;
            }   

            // check for wildcard service
            if ((ssp.length() == dot + 2) && ssp[dot+1] == '*') {
                return true;
            }   
    
            // parse the service
            service = strtoull(ssp.c_str() + dot + 1, &end, 10);
            if (end == ssp.c_str() + dot + 1 ||
                end != (ssp.c_str() + ssp.length())) {
                log_debug_p("/dtn/scheme/ipn",
                            "invalid service portion (validate) '%s' in ssp %s",
                            ssp.c_str() + dot + 1, ssp.c_str());
                return false;
            }   

            return true;
        }
    }
}

//----------------------------------------------------------------------
bool
IPNScheme::match(const EndpointIDPattern& pattern, const EndpointID& eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);

    // we only match endpoint ids of the same scheme
    if (!eid.known_scheme() || (eid.scheme() != this)) {
        return false;
    }

    // is the eid a valid numeric pair?
    u_int64_t eid_node, eid_service;
    std::string eid_ssp = eid.ssp();
    if (! IPNScheme::parse(eid_ssp, &eid_node, &eid_service)) {
        return false;
    }

    // does the eid match the pattern?
    u_int64_t pat_node, pat_service;
    std::string pat_ssp = pattern.ssp();
    if (strstr(pat_ssp.c_str(), "*") != NULL) {
        if (0 == pat_ssp.compare("*")) {
            return true;
        } else {
            // pattern must be of the form "[node].*"
            char* end;
            pat_node = strtoull(pat_ssp.c_str(), &end, 10);
            if (strcmp(end, ".*")) {
                log_debug_p("/dtn/scheme/ipn",
                            "match: invalid node portion in ssp %s",
                            pat_ssp.c_str());
                return false;
            }   
            return (pat_node == eid_node);
        }
    } else if (! IPNScheme::parse(pat_ssp, &pat_node, &pat_service)) {
        // the pattern was not a valid numeric pair
        log_debug_p("/dtn/scheme/ipn", "IPNScheme::match: invalid pattern: %s",
                    pat_ssp.c_str());
        return false;
    }

    return ((pat_node == eid_node) && (pat_service == eid_service));
}

//----------------------------------------------------------------------
bool
IPNScheme::append_service_tag(URI* uri, const char* tag)
{
    // the uri must be of the form ipn:[node].0, so it replaces the .0
    // with the tag value
    u_int64_t node, service;
    if (! IPNScheme::parse(uri->ssp(), &node, &service)) {
        return false; // malformed
    }

    if (service != 0) {
        return false;
    }

    // check if the tag is just a number
    char* end = NULL;
    service = strtoull(tag, &end, 10);
    if (*end != '\0') {
        // XXX/dz I don't think it is a good idea to arbitrarily
        // assign a service to the ping tag so I'm not doing it :)

        // if it's not a number already, see if it's a "well-known" tag
        //if (!strcmp(tag, "ping")) {
        //    service = 999;
        //} else {
        //    return false;
        //}
        return false;
    }
    
    if (0 != service) {
        std::string ssp;
        format(&ssp, node, service);
        uri->set_ssp(ssp);
    }

    return true;
}

//----------------------------------------------------------------------
bool
IPNScheme::append_service_wildcard(URI* uri)
{
    // the uri must be of the form ipn:[node].0, so it replaces the .0
    // with the tag value
    u_int64_t node, service;
    if (! IPNScheme::parse(uri->ssp(), &node, &service)) {
        return false; // malformed
    }

    if (service != 0) {
        return false;
    }

    oasys::StaticStringBuffer<256> buf("%llu.*", U64FMT(node));
    uri->set_ssp(buf.c_str());
    return true;
}

//----------------------------------------------------------------------
Scheme::singleton_info_t
IPNScheme::is_singleton(const URI& uri)
{
    (void)uri;
    return EndpointID::SINGLETON;
}

} // namespace dtn
