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

#ifndef _IPN_SCHEME_H_
#define _IPN_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * This class implements the ipn scheme, of the form:
 * <code>ipn:[node].[service]</code>
 *
 * Where both node and service are pre-agreed identifiers.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class IPNScheme : public Scheme, public oasys::Singleton<IPNScheme> {
public:
    /// @{ virtual from Scheme
    virtual bool validate(const URI& uri, bool is_pattern = false);
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);
    virtual bool append_service_tag(URI* uri, const char* tag);
    virtual bool append_service_wildcard(URI* uri);
    virtual singleton_info_t is_singleton(const URI& uri);
    /// @}

    /**
     * Try to parse the ssp into a node/service pair. 
     * NOTE: The NULL_EID (dtn:none : ssp="none") is valid and 
     *       returns zeros for the node and service values.
     *
     * XXX/dz: This implementation only supports parsing an endpoint 
     * and not a pattern. Another implementation used 2^64-1 as a 
     * wildcard value but that is usurping a valid value.
     *
     * @return Whether the ssp is well-formed
     */
    static bool parse(const std::string& ssp,
                      u_int64_t* node, u_int64_t* service);

    /**
     * Try to parse the EID's ssp into a node/service pair.
     *
     * @return Whether the ssp is well-formed
     */
    static bool parse(const EndpointID& eid,
                      u_int64_t* node, u_int64_t* service)
    {
        if (eid.scheme() != IPNScheme::instance() && eid != EndpointID::NULL_EID())
        {
            return false;
        }
        return parse(eid.ssp(), node, service);
    }

    /**
     * Fill in the given ssp string with a well-formed IPN eid using
     * the given node/service.
     */
    static void format(std::string* ssp, u_int64_t node, u_int64_t service);
    
    /**
     * Fill in the given eid string with a well-formed IPN eid using
     * the given node/service.
     */
    static void format(EndpointID* eid, u_int64_t node, u_int64_t service)
    {
        if ( 0 == node && 0 == service ) {
            eid->assign(EndpointID::NULL_EID());
        } else {
            std::string ssp;
            format(&ssp, node, service);
            eid->assign("ipn", ssp);
        }
    }
    
private:
    friend class oasys::Singleton<IPNScheme>;
    IPNScheme() {}
};
    
}

#endif /* _IPN_SCHEME_H_ */
