/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _ENDPOINTIDOPT_H_
#define _ENDPOINTIDOPT_H_

#include <string>
#include <vector>

#include "dtn_types.h"
#include <oasys/util/Options.h>

namespace dtn {

/**
 * Extension class to the oasys Opt hierarchy that validates that the
 * option is a DTN endpoint identifier (i.e. a URI).
 */
class APIEndpointIDOpt : public oasys::Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    APIEndpointIDOpt(const char* opt, dtn_endpoint_id_t* valp,
                     const char* valdesc = "", const char* desc = "",
                     bool* setp = NULL);

    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc 	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    APIEndpointIDOpt(char shortopt, const char* longopt,
                     dtn_endpoint_id_t* valp,
                     const char* valdesc = "", const char* desc = "",
                     bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(oasys::StringBuffer* buf);
};

} // namespace dtn
     
#endif /* _ENDPOINTIDOPT_H_ */
