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

#include <oasys/debug/Log.h>
#include <oasys/util/Glob.h>

#include "applib/dtn_types.h"
#include "EndpointID.h"
#include "Scheme.h"
#include "SchemeTable.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
EndpointID        GlobalEndpointIDs::null_eid_;
EndpointIDPattern GlobalEndpointIDs::wildcard_eid_;

EndpointID::singleton_info_t
  EndpointID::is_singleton_default_ = EndpointID::MULTINODE;
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
        valid_ = scheme_->validate(uri_, is_pattern_);
    }

    return valid_;
}

//----------------------------------------------------------------------
bool
EndpointID::append_service_tag(const char* tag)
{
    if (!scheme_)
        return false;

    bool ok = scheme_->append_service_tag(&uri_, tag);
    if (!ok)
        return false;

    // rebuild the string
    if (!uri_.valid()) {
        log_err_p("/dtn/naming/endpoint/",
                  "EndpointID::append_service_tag: "
                  "failed to format appended URI");
        
        // XXX/demmer this leaves the URI in a bogus state... that
        // doesn't seem good since this really shouldn't ever happen
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
EndpointID::append_service_wildcard()
{
    if (!scheme_)
        return false;

    bool ok = scheme_->append_service_wildcard(&uri_);
    if (!ok)
        return false;

    // rebuild the string
    if (!uri_.valid()) {
        log_err_p("/dtn/naming/endpoint/",
                  "EndpointID::append_service_wildcard: "
                  "failed to format appended URI");
        
        // XXX/demmer this leaves the URI in a bogus state... that
        // doesn't seem good since this really shouldn't ever happen
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
EndpointID::remove_service_tag()
{
    if (! scheme_)
        return false;

    bool ok = scheme_->remove_service_tag(&uri_);
    if (!ok)
        return false;

    // rebuild the string
    if (!uri_.valid()) {
        log_err_p("/dtn/naming/endpoint/",
                  "EndpointID::remove_service_tag: "
                  "failed to format reduced URI");
        
        // see note in append_service_wildcard() ... :(
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
EndpointID::singleton_info_t
EndpointID::is_singleton() const
{
    if (! known_scheme()) {
        singleton_info_t ret = is_singleton_default_;
        log_warn_p("/dtn/naming/endpoint/",
                   "returning is_singleton=%s for unknown scheme %s",
                   ret == SINGLETON ? "true" :
                   ret == MULTINODE ? "false" :
                   "unknown",
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
EndpointIDPattern::match(const EndpointID& eid) const
{
    // only match if we're valid
    if (!uri_.valid()) {
        log_warn_p("/dtn/naming/endpoint",
                   "match error: pattern '%s' not a valid uri",
                   uri_.c_str());
        return false;
    }
    
    // only match valid eids
    if (!eid.uri().valid()) {
        log_warn_p("/dtn/naming/endpoint",
                   "match error: eid '%s' not a valid uri",
                   eid.c_str());
        return false;
    }
    
    if (known_scheme()) {
        return scheme()->match(*this, eid);

    } else if (glob_unknown_schemes_) {
        return oasys::Glob::fixed_glob(uri_.c_str(), eid.c_str());
        
    } else {
        return (*this == eid);
    }

}


} // namespace dtn
