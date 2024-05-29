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

#ifndef _IMC_SCHEME_H_
#define _IMC_SCHEME_H_

#include "EndpointID.h"
#include "Scheme.h"
#include <third_party/oasys/util/Singleton.h>

namespace dtn {

/**
 * This class implements the imc scheme, of the form:
 * <code>imc:[node].[service]</code>
 *
 * Where both node and service are pre-agreed identifiers.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class IMCScheme : public Scheme, public oasys::Singleton<IMCScheme> {
public:
    /// @{ virtual from Scheme
    virtual bool validate(const URI& uri, bool is_pattern, 
                          bool& node_wildcard, bool& service_wildcard) override;
    virtual bool match(const EndpointIDPattern& pattern,
                       const SPtr_EID& sptr_eid) override;
    virtual bool ipn_node_match(const SPtr_EID& sptr_eid1, const SPtr_EID& sptr_eid2) override;
    
    virtual eid_dest_type_t eid_dest_type(const URI& uri) override;
    virtual bool is_singleton(const URI& uri) override;
    /// @}

    /**
     * Try to parse the ssp into a group/service pair. 
     *
     * @return Whether the ssp is well-formed
     */
    static bool parse(const std::string& ssp,
                      size_t* node, size_t* service);

    /**
     * Try to parse the EID's ssp into a node/service pair.
     *
     * @return Whether the ssp is well-formed
     */
    static bool parse(const SPtr_EID& sptr_eid,
                      size_t* node, size_t* service);

private:
    /**
     * Fill in the given ssp string with a well-formed IMC eid using
     * the given node/service.
     */
    static void format(std::string* ssp, size_t node, size_t service);
    
    /**
     * Load the EndpointID object with a well-formed IMC eid using
     * the given node/service.
     */
    static void format(SPtr_EID& sptr_eid, size_t group, size_t service);
    
private:
    friend class oasys::Singleton<IMCScheme>;
    IMCScheme() {}
};
    
}

#endif /* _IMC_SCHEME_H_ */
