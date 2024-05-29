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

#include <ctype.h>

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/util/Glob.h>

#include "applib/dtn_types.h"
#include "EndpointID.h"
#include "DTNScheme.h"
#include "IMCScheme.h"
#include "IPNScheme.h"
#include "Scheme.h"
#include "SchemeTable.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------

EndpointID::eid_dest_type_t
  EndpointID::unknown_eid_dest_type_default_ = EndpointID::MULTINODE;

bool EndpointID::glob_unknown_schemes_ = true;

//----------------------------------------------------------------------
bool
EndpointID::validate()
{
    static const char* log = "/dtn/naming/endpoint/";
    (void)log;
    
    scheme_ = NULL;
    valid_ = false;

    if (!uri_.valid()) {
        log_debug_p(log, "EndpointID::validate: invalid URI");
        return false;
    }
    
    if (scheme_str().length() > MAX_EID_PART_LENGTH) {
        log_err_p(log, "scheme name is too large (>%zu)", MAX_EID_PART_LENGTH);
        valid_ = false;
        return false;
    }
    
    if (ssp().length() > MAX_EID_PART_LENGTH) {
        log_err_p(log, "ssp is too large (>%zu)", MAX_EID_PART_LENGTH);
        valid_ = false;
        return false;
    }

    valid_ = true;

    if ((scheme_ = SchemeTable::instance()->lookup(uri_.scheme())) != NULL) {
        valid_ = scheme_->validate(uri_, is_pattern_, is_node_wildcard_, is_service_wildcard_);
    }

    if (valid_) {
        set_node_num();
    } else {
        node_num_ = 0;
        service_num_ = 0;
        is_node_wildcard_ = false;
        is_service_wildcard_ = false;
    }

    return valid_;
}

//----------------------------------------------------------------------
EndpointID::eid_dest_type_t
EndpointID::eid_dest_type() const
{
    if (! known_scheme()) {
        eid_dest_type_t ret = unknown_eid_dest_type_default_;
        log_warn_p("/dtn/naming/endpoint/",
                   "returning default eid_dest_type=%s for unknown scheme %s",
                   ret == SINGLETON ? "SINGLETON" :
                   ret == MULTINODE ? "MULTINODE" :
                   "unknown",
                   scheme_str().c_str());
        return ret;
    }
    return scheme_->eid_dest_type(uri_);
}

//----------------------------------------------------------------------
bool
EndpointID::is_singleton() const
{
    if (! known_scheme()) {
        bool ret = (unknown_eid_dest_type_default_ == SINGLETON);
        log_warn_p("/dtn/naming/endpoint/",
                   "returning is_singleton=%s for unknown scheme %s",
                   ret ? "true" : "false",
                   scheme_str().c_str());
        return ret;
    }
    return scheme_->is_singleton(uri_);
}

//----------------------------------------------------------------------
bool
EndpointID::assign(const dtn_endpoint_id_t* eid)
{
    uri_.assign(std::string(eid->uri));
    node_num_ = 0;
    service_num_ = 0;
    return validate();
}
    
//----------------------------------------------------------------------
void
EndpointID::copyto(dtn_endpoint_id_t* eid) const
{
    ASSERT(uri_.uri().length() <= DTN_MAX_ENDPOINT_ID + 1);
    strcpy(eid->uri, uri_.uri().c_str());
}

//----------------------------------------------------------------------
void
EndpointID::serialize(oasys::SerializeAction* a)
{
    a->process("uri", &uri_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        validate();
    }
}

//----------------------------------------------------------------------
bool
EndpointID::subsume_ipn(const EndpointID& other) const
{
    bool result = false;

    if (is_ipn_scheme() && other.is_ipn_scheme()) {
        result = node_num_ == other.node_num();
    }

    return result;
}

//----------------------------------------------------------------------
bool
EndpointID::subsume_ipn(const SPtr_EID& other) const
{
    bool result = false;

    if (is_ipn_scheme() && other->is_ipn_scheme()) {
        result = node_num_ == other->node_num();
    }

    return result;
}

//----------------------------------------------------------------------
void
EndpointID::set_node_num()
{
    node_num_ = 0;
    service_num_ = 0;
    is_null_eid_ = false;

    if (scheme_ == IPNScheme::instance()) {
        IPNScheme::parse(uri_.ssp(), &node_num_, &service_num_);
    } else if (scheme_ == IMCScheme::instance()) {
        IMCScheme::parse(uri_.ssp(), &node_num_, &service_num_);
    } else if (scheme_ == DTNScheme::instance()) {
        is_null_eid_ = (uri_.ssp() == "none");
    }

}

//----------------------------------------------------------------------
bool
EndpointID::is_dtn_scheme() const
{
    return (scheme_ == DTNScheme::instance());
}

//----------------------------------------------------------------------
bool
EndpointID::is_ipn_scheme() const
{
    return (scheme_ == IPNScheme::instance());
}

//----------------------------------------------------------------------
bool
EndpointID::is_imc_scheme() const
{
    return (scheme_ == IMCScheme::instance());
}

//----------------------------------------------------------------------
bool
EndpointID::is_cbhe_compat() const
{
    return ((scheme_ == IPNScheme::instance()) || 
            (scheme_ == IMCScheme::instance()) ||
            is_null_eid_);
}

//----------------------------------------------------------------------
size_t
EndpointID::node_num() const
{
    return node_num_;
}

//----------------------------------------------------------------------
size_t
EndpointID::service_num() const
{
    return service_num_;
}

//----------------------------------------------------------------------
bool
EndpointIDPattern::match(const SPtr_EID& sptr_eid) const
{
    if (!valid_) {
        log_crit_p("/dtn/naming/endpointpattern",
                   "match error: pattern '%s' is not valid",
                   c_str());
        return false;
    }

    if (!sptr_eid->valid()) {
        log_crit_p("/dtn/naming/endpointpattern",
                   "match error: EID to match '%s' is not valid",
                   sptr_eid->c_str());
        return false;
    }

    if (known_scheme()) {
        return scheme()->match(*this, sptr_eid);
    }

    // only match valid eids
    if (!sptr_eid->uri().valid()) {
        log_warn_p("/dtn/naming/endpoint",
                   "match error: eid '%s' not a valid uri",
                   sptr_eid->c_str());
        return false;
    
    } else if (glob_unknown_schemes_) {
        return oasys::Glob::fixed_glob(uri_.c_str(), sptr_eid->c_str());
        
    } else {
        return (*this == *sptr_eid);
    }

}

//----------------------------------------------------------------------
bool
EndpointIDPattern::match_ipn(size_t test_node_num) const
{
    bool result = false;

    if (!valid_) {
        log_crit_p("/dtn/naming/endpointpattern",
                   "match error: pattern '%s' is not valid",
                   c_str());
    } else if (scheme() == IPNScheme::instance()) {
        result = is_node_wildcard_ || (node_num_ == test_node_num);
    }

    return result;
}


} // namespace dtn
