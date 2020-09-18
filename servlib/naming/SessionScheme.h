/*
 *    Copyright 2008 Intel Corporation
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

#ifndef _SESSION_SCHEME_H_
#define _SESSION_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * This class implements a scheme to match dtn-session: URIs.
 *
 * The SSP must itself be another URI, e.g.:
 *
 *   dtn-session:http://foo/bar
 *
 * If it's an endpoint id pattern, then as long as the characters are
 * valid, any string can follow, and globbing rules are used to match.
 */
class SessionScheme : public Scheme, public oasys::Singleton<SessionScheme> {
public:
    /// @{ Virtual from Scheme
    bool validate(const URI& uri, bool is_pattern = false);
    bool match(const EndpointIDPattern& pattern, const EndpointID& eid);
    singleton_info_t is_singleton(const URI& uri);
    /// @}
    
private:
    friend class oasys::Singleton<SessionScheme>;
    SessionScheme() {}
};
    
}

#endif /* _SESSION_SCHEME_H_ */
