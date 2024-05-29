/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _WILDCARD_SCHEME_H_
#define _WILDCARD_SCHEME_H_

#include <third_party/oasys/util/Singleton.h>

#include "Scheme.h"

namespace dtn {

class WildcardScheme : public Scheme, public oasys::Singleton<WildcardScheme> {
public:
    /// @{ virtual from Scheme
    virtual bool validate(const URI& uri, bool is_pattern, 
                          bool& node_wildcard, bool& service_wildcard) override;
    virtual bool match(const EndpointIDPattern& pattern,
                       const SPtr_EID& sptr_eid) override;
    virtual eid_dest_type_t eid_dest_type(const URI& uri) override;
    virtual bool is_singleton(const URI& uri) override;
    /// @}

private:
    friend class oasys::Singleton<WildcardScheme>;
    WildcardScheme() {}
};

} // namespace dtn

#endif /* _WILDCARD_SCHEME_H_ */
