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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "IPNScheme.h"
#include "EndpointID.h"

#include "bundling/BundleDaemon.h"


namespace oasys {
    template <> dtn::IPNScheme* oasys::Singleton<dtn::IPNScheme>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
bool
IPNScheme::parse(const std::string& ssp,
                 size_t* node, size_t* service)
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
bool
IPNScheme::parse(const SPtr_EID& sptr_eid,
                 size_t* node, size_t* service)
{
    bool result = false;
    if (sptr_eid->valid() && sptr_eid->is_ipn_scheme()) {
        *node = sptr_eid->node_num();
        *service = sptr_eid->service_num();
        result = true;
    } else {
        *node = 0;
        *service = 0;
    }

    return result;
}

//----------------------------------------------------------------------
void
IPNScheme::format(std::string* ssp, size_t node, size_t service)
{
    oasys::StaticStringBuffer<256> buf("%zu.%zu", node, service);
    ssp->assign(buf.c_str());
}

//----------------------------------------------------------------------
void
IPNScheme::format(SPtr_EID& sptr_eid, size_t node, size_t service)
{
    sptr_eid = BD_MAKE_EID_IPN(node, service);
}
    
//----------------------------------------------------------------------
bool
IPNScheme::validate(const URI& uri, bool is_pattern, 
                    bool& node_wildcard, bool& service_wildcard)
{
    node_wildcard = false;
    service_wildcard = false;

    if (!uri.valid()) {
        log_debug_p("/dtn/scheme/ipn", "IPNScheme::validate: invalid URI");
        return false;
    }
    
    size_t node, service;

    if (!is_pattern) {
        return parse(uri.ssp(), &node, &service);
    } else {
        std::string ssp = uri.ssp();

        if ((0 == ssp.compare("*")) || (0 == ssp.compare("*.*"))) {
            node_wildcard = true;
            service_wildcard = true;
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
                service_wildcard = true;
                return true;
            } else {
                // parse the service
                service = strtoull(ssp.c_str() + dot + 1, &end, 10);
                if (end == ssp.c_str() + dot + 1 ||
                    end != (ssp.c_str() + ssp.length())) {
                    log_debug_p("/dtn/scheme/ipn",
                                "invalid service portion (validate) '%s' in ssp %s",
                                ssp.c_str() + dot + 1, ssp.c_str());
                    return false;
                }   
            }

            return true;
        }
    }
}

//----------------------------------------------------------------------
bool
IPNScheme::match(const EndpointIDPattern& pattern, const SPtr_EID& sptr_eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);

    // we only match endpoint ids of the same scheme
    if (!sptr_eid->known_scheme() || (sptr_eid->scheme() != this)) {
        return false;
    }

    if (pattern.is_node_wildcard()) {
        return true;
    } else if (pattern.node_num() != sptr_eid->node_num()) {
        return false;
    } else if (pattern.is_service_wildcard()) {
        return true;
    }

    // node matches so last thing to check is the service numbers
    return pattern.service_num() == sptr_eid->service_num();
}

//----------------------------------------------------------------------
bool
IPNScheme::ipn_node_match(const SPtr_EID& sptr_eid1, const SPtr_EID& sptr_eid2)
{
    if ((sptr_eid1->scheme() != this) || (sptr_eid2->scheme() != this)) {
        return false;
    }

    return (sptr_eid1->node_num() == sptr_eid2->node_num());
}

//----------------------------------------------------------------------
Scheme::eid_dest_type_t
IPNScheme::eid_dest_type(const URI& uri) 
{
    (void)uri;
    return EndpointID::SINGLETON;
}

//----------------------------------------------------------------------
bool
IPNScheme::is_singleton(const URI& uri)
{
    (void)uri;
    return true;
}

} // namespace dtn
