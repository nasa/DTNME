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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/StringBuffer.h>
#include "APIEndpointIDOpt.h"
#include "dtn_api.h"

namespace dtn {

APIEndpointIDOpt::APIEndpointIDOpt(const char* opt, dtn_endpoint_id_t* valp,
                                   const char* valdesc, const char* desc,
                                   bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

APIEndpointIDOpt::APIEndpointIDOpt(char shortopt, const char* longopt,
                                   dtn_endpoint_id_t* valp,
                                   const char* valdesc, const char* desc,
                                   bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
APIEndpointIDOpt::set(const char* val, size_t len)
{
    char buf[DTN_MAX_ENDPOINT_ID];
    if (len > (DTN_MAX_ENDPOINT_ID - 1)) {
        return -1;
    }
    
    memcpy(buf, val, len);
    buf[len] = '\0';

    int err = dtn_parse_eid_string(((dtn_endpoint_id_t*)valp_), buf);
    if (err != 0) {
        return -1;
    }

    if (setp_)
        *setp_ = true;
    
    return 0;
}

void
APIEndpointIDOpt::get(oasys::StringBuffer* buf)
{
    buf->append(((dtn_endpoint_id_t*)valp_)->uri);
}

} // namespace dtn
