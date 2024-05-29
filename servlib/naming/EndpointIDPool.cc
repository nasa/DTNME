/*
 *    Copyright 2022 United States Government as represented by NASA
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

#include <third_party/oasys/util/StringBuffer.h>

#include "EndpointIDPool.h"
#include "WildcardScheme.h"

namespace dtn {

//----------------------------------------------------------------------
EndpointIDPool::EndpointIDPool()
{
    // create the common, well known EIDs
    sptr_eid_null_  = make_eid("dtn:none");
    sptr_eid_imc00_ = make_eid("imc:0.0");

    sptr_eidpattern_null_ = make_pattern("dtn:none");
    sptr_eidpattern_wild_ = make_pattern("*:*");
}


//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid(const std::string& eid_str)
{
    SPtr_EID sptr_eid;

    oasys::ScopeLock scoplok(&lock_, __func__);

    EIDPool_EID_Iter iter = eid_pool_.find(eid_str);

    if (iter == eid_pool_.end()) {
        sptr_eid = std::make_shared<EndpointID>(eid_str);

        // adding to the list even if not valid 
        // in case a slew of these are in the pipeline
        eid_pool_[eid_str] = sptr_eid;
    } else {
        sptr_eid = iter->second;
    }

    return sptr_eid;
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid(const char* eid_cstr)
{
    std::string eid_str = eid_cstr;

    return make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_dtn(const std::string& host, const std::string& service)
{
    std::string eid_str = "dtn://" + host + "/" + service;

    return make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_dtn(const std::string& ssp)
{
    std::string eid_str = "dtn:" + ssp;

    return make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_ipn(size_t node_num, size_t service_num)
{
    std::string eid_str = "ipn:" + std::to_string(node_num) + "." + 
                                   std::to_string(service_num);

    return make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_imc(size_t group_num, size_t service_num)
{
    std::string eid_str = "imc:" + std::to_string(group_num) + "." + 
                                   std::to_string(service_num);

    return make_eid(eid_str);
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_scheme(const char* scheme, const char* ssp)
{
    std::string eid_str(scheme);
    eid_str.append(":");
    eid_str.append(ssp);

    return make_eid(eid_str);
}


//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_null()
{
    return sptr_eid_null_;
}

//----------------------------------------------------------------------
SPtr_EID
EndpointIDPool::make_eid_imc00()
{
    return sptr_eid_imc00_;
}

//----------------------------------------------------------------------
size_t
EndpointIDPool::size_eids()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    return eid_pool_.size();
}

//----------------------------------------------------------------------
size_t
EndpointIDPool::size_patterns()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    return eid_pattern_pool_.size();
}

//----------------------------------------------------------------------
void
EndpointIDPool::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    buf->appendf("\nEndpointID Patterns Pool (%zu): \n", eid_pattern_pool_.size());

    EIDPool_EIDPattern_Iter pattern_iter = eid_pattern_pool_.begin();

    while  (pattern_iter != eid_pattern_pool_.end()) {
        buf->appendf("    %s\n", pattern_iter->first.c_str());

        ++pattern_iter;
    }


    buf->appendf("\nEndpointID Pool (%zu): \n", eid_pool_.size());

    EIDPool_EID_Iter eid_iter = eid_pool_.begin();

    while  (eid_iter != eid_pool_.end()) {
        buf->appendf("    %s\n", eid_iter->first.c_str());

        ++eid_iter;
    }

    buf->append("\n");
}


//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::make_pattern(const std::string& pattern_str)
{
    SPtr_EIDPattern sptr_pattern;

    oasys::ScopeLock scoplok(&lock_, __func__);

    EIDPool_EIDPattern_Iter iter = eid_pattern_pool_.find(pattern_str);

    if (iter == eid_pattern_pool_.end()) {
        sptr_pattern = std::make_shared<EndpointIDPattern>(pattern_str);

        // adding to the list even if not valid 
        // in case a slew of these are in the pipeline
        eid_pattern_pool_[pattern_str] = sptr_pattern;
    } else {
        sptr_pattern = iter->second;
    }

    return sptr_pattern;
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::make_pattern(const char* pattern_cstr)
{
    std::string pattern_str = pattern_cstr;

    return make_pattern(pattern_str);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::make_pattern_wild()
{
    return sptr_eidpattern_wild_;
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::append_pattern_wild(const SPtr_EID& sptr_eid)
{

    if (sptr_eid->is_dtn_scheme()) {
        return append_pattern_wild_dtn(sptr_eid->uri().authority());
    } else if (sptr_eid->is_ipn_scheme()) {
        return append_pattern_wild_ipn(sptr_eid->node_num());
    } else if (sptr_eid->is_imc_scheme()) {
        return append_pattern_wild_imc(sptr_eid->node_num());
    }

    // unknown scheme - return null pattern
    return sptr_eidpattern_null_;
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::append_pattern_wild_dtn(const std::string& authority)
{
    std::string pattern_str = "dtn://" + authority + "/*";

    return make_pattern(pattern_str);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::append_pattern_wild_ipn(size_t node_num)
{
    std::string pattern_str = "ipn:" + std::to_string(node_num) + ".*";

    return make_pattern(pattern_str);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::append_pattern_wild_imc(size_t node_num)
{
    std::string pattern_str = "imc:" + std::to_string(node_num) + ".*";

    return make_pattern(pattern_str);
}

//----------------------------------------------------------------------
SPtr_EIDPattern
EndpointIDPool::make_pattern_null()
{
    return sptr_eidpattern_null_;
}

} // namespace dtn
