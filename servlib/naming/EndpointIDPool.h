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

#ifndef _ENDPOINT_ID_POOL_H_
#define _ENDPOINT_ID_POOL_H_

#include <map>
#include <memory>
#include <string>

#include <third_party/oasys/thread/SpinLock.h>

#include "EndpointID.h"

namespace dtn {

class EndpointIDPool {
public:
    /**
     * Default constructor
     */
    EndpointIDPool();

    /**
     * Destructor.
     */
    virtual ~EndpointIDPool() {}

    /**
     * Dump the list of EIDs being managed
     */
    void dump(oasys::StringBuffer* buf);

    /**
     * get the NULL EID (for setter puproses)
     */
    SPtr_EID make_eid_null();

    /**
     * get the NULL EID (for comparison purposes)
     */
    SPtr_EID null_eid()
    {
        return make_eid_null();
    }

    /**
     * get the IMC Group Petition EID (imc:0.0)
     */
    SPtr_EID make_eid_imc00();


    /**
     * make or get an EID from a string representation
     */
    SPtr_EID make_eid(const std::string& eid_str);
    SPtr_EID make_eid(const char* eid_cstr);

    /**
     * make or get a DTN scheme EID from the components
     */
    SPtr_EID make_eid_dtn(const std::string& host, const std::string& service);
    SPtr_EID make_eid_dtn(const std::string& ssp);

    /**
     * make or get an IPN scheme EID from the components
     */
    SPtr_EID make_eid_ipn(size_t node_num, size_t service_num);

    /**
     * make or get an IMC scheme EID from the components
     */
    SPtr_EID make_eid_imc(size_t group_num, size_t service_num);

    /**
     * make or get an EID providing the scheme and ssp
     */
    SPtr_EID make_eid_scheme(const char* scheme, const char* ssp);


    /**
     * Get the number of EndpointIDs in the pool
     */
    size_t size_eids();

    /**
     * Get the number of EndpointIDPatterns in the pool
     */
    size_t size_patterns();


    /**
     * make or get EID Patterns
     */
    SPtr_EIDPattern make_pattern(const std::string& pattern_str);
    SPtr_EIDPattern make_pattern(const char* pattern_cstr);
    SPtr_EIDPattern make_pattern_null();
    SPtr_EIDPattern make_pattern_wild();
    SPtr_EIDPattern append_pattern_wild(const SPtr_EID& sptr_eid);
    SPtr_EIDPattern append_pattern_wild_dtn(const std::string& authority);
    SPtr_EIDPattern append_pattern_wild_ipn(size_t node_num);
    SPtr_EIDPattern append_pattern_wild_imc(size_t node_num);

    SPtr_EIDPattern wild_pattern()
    {
        return make_pattern_wild();
    }


protected:

    oasys::SpinLock lock_;
     
    typedef std::map<std::string, SPtr_EID>           EIDPool_EID_Map;
    typedef EIDPool_EID_Map::iterator                 EIDPool_EID_Iter;

    EIDPool_EID_Map eid_pool_;   ///< Maintained list of EndpointID objects

    SPtr_EID sptr_eid_null_;     ///< quick access to the NULL EID and prevents purging
    SPtr_EID sptr_eid_imc00_;    ///< quick access to the NULL EID and prevents purging


    typedef std::map<std::string, SPtr_EIDPattern>    EIDPool_EIDPattern_Map;
    typedef EIDPool_EIDPattern_Map::iterator          EIDPool_EIDPattern_Iter;

    EIDPool_EIDPattern_Map eid_pattern_pool_;   ///< Maintained list of EndpointIDPattern objects

    SPtr_EIDPattern sptr_eidpattern_null_;      ///< quick access to the NULL EID Pattern (dtn:none) and prevents purging
    SPtr_EIDPattern sptr_eidpattern_wild_;      ///< quick access to the WILDCARD EID Pattern (*.*) and prevents purging
};

} // namespace dtn
#endif // _ENDPOINT_ID_POOL_H_
