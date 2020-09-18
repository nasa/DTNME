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

#ifndef _DTN_SCHEME_H_
#define _DTN_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * This class implements the one default scheme as specified in the
 * bundle protocol. SSPs for this scheme take the canonical forms:
 *
 * <code>
 * dtn://<router identifier>[/<application tag>]
 * dtn:none
 * </code>
 *
 * Where "router identifier" is a DNS-style "hostname" string, however
 * not necessarily a valid internet hostname, and "application tag" is
 * any string of URI-valid characters.
 *
 * The special endpoint identifier "dtn:none" is used to represent the
 * null endpoint.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class DTNScheme : public Scheme, public oasys::Singleton<DTNScheme> {
public:
    /**
     * Validate that the SSP in the given URI is legitimate for
     * this scheme. If the 'is_pattern' paraemeter is true, then
     * the ssp is being validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const URI& uri, bool is_pattern = false);

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);
    
    /**
     * Append the given service tag to the uri in a scheme-specific
     * manner.
     *
     * @return true if this scheme is capable of service tags and the
     * tag is a legal one, false otherwise.
     */
    virtual bool append_service_tag(URI* uri, const char* tag);

    /**
     * Append a wildcard tag to the uri in a scheme-specific
     * manner.
     *
     * @return true if this scheme is capable of wildcards and the
     * wildcard is appended, false otherwise.
     */
    virtual bool append_service_wildcard(URI* uri);

    /**
     * Reduce URI to node ID in a scheme specific manner. The default
     * scheme is not capable of this.
     *
     * @return true if this scheme is capable of this reduction and 
     * the reduction is successful, else false.
     */
    virtual bool remove_service_tag(URI* uri);

    /**
     * Check if the given URI is a singleton EID.
     */
    virtual singleton_info_t is_singleton(const URI& uri);
    
private:
    friend class oasys::Singleton<DTNScheme>;
    DTNScheme() {}
};
    
}

#endif /* _DTN_SCHEME_H_ */
