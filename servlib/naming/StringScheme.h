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

#ifndef _STRING_SCHEME_H_
#define _STRING_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {

class StringScheme : public Scheme, public oasys::Singleton<StringScheme> {
public:
    /**
     * Validate that the SSP in the given URI is legitimate for
     * this scheme. If * the 'is_pattern' paraemeter is true, then
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
     * Check if the given URI is a singleton EID.
     */
    virtual singleton_info_t is_singleton(const URI& uri);

private:
    friend class oasys::Singleton<StringScheme>;
    StringScheme() {}
};

} // namespace dtn

#endif /* _STRING_SCHEME_H_ */
