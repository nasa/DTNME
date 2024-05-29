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

#ifndef _SCHEME_H_
#define _SCHEME_H_

#include <string>

#include <third_party/oasys/util/URI.h>
#include "EndpointID.h"

namespace dtn {

typedef oasys::URI URI;

class EndpointID;
class EndpointIDPattern;

/**
 * The base class for various endpoint id schemes. The class provides
 * two pure virtual methods -- validate() and match() -- that are
 * overridden by the various scheme implementations.
 */
class Scheme {
public:
    /**
     * Destructor -- should be called only at shutdown time.
     */
    virtual ~Scheme();

    /**
     * Validate that the SSP within the given URI is legitimate for
     * this scheme. If the 'is_pattern' paraemeter is true, then the
     * ssp is being validated as an EndpointIDPattern.
     *
     * also returns whether IPN or IMC patterns have 
     * wildcards for the node/group or service numbers
     *
     * @return true if valid
     */
    virtual bool validate(const URI& uri, bool is_pattern, 
                          bool& node_wildcard, bool& service_wildcard) = 0;

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const SPtr_EID& sptr_eid) = 0;
    
    /**
     * Whether the IPN Scheme Node Numbers match. 
     * Default is to assume this is not the IPN scheme so return false.
     */
    virtual bool ipn_node_match(const SPtr_EID& sptr_eid1, const SPtr_EID& sptr_eid2)
    {
        (void) sptr_eid1;
        (void) sptr_eid2;
        return false;
    }
    
    /**
     * Copy of the EndpointID's type structure for determining if the
     * endpoint is, is not, or may be a singleton.
     */
    typedef EndpointID::eid_dest_type_t eid_dest_type_t;
    
    /**
     * Get the destination type of the given URI`
     */
    virtual eid_dest_type_t eid_dest_type(const URI& uri) = 0;

    /**
     * Check if the scheme type is a singleton endpoint id.
     */
    virtual bool is_singleton(const URI& uri) = 0;

};
    
}

#endif /* _SCHEME_H_ */
