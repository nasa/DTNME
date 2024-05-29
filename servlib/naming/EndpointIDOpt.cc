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

#include <third_party/oasys/util/StringBuffer.h>
#include "EndpointIDOpt.h"
#include "EndpointID.h"

#include "bundling/BundleDaemon.h"

namespace dtn {

EndpointIDOpt::EndpointIDOpt(const char* opt, SPtr_EID* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

EndpointIDOpt::EndpointIDOpt(char shortopt, const char* longopt,
                             SPtr_EID* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
EndpointIDOpt::set(const char* val, size_t len)
{
    std::string s(val, len);

    (*(SPtr_EID*)valp_) = BD_MAKE_EID(s);

    if (! ((*(SPtr_EID*)valp_)->valid())) {
        return -1;
    }
    
    if (setp_) {
        *setp_ = true;
    }
    
    return 0;
}

void
EndpointIDOpt::get(oasys::StringBuffer* buf)
{
    buf->append((*(SPtr_EID*)valp_)->c_str());
}









EIDPatternOpt::EIDPatternOpt(const char* opt, SPtr_EIDPattern* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

EIDPatternOpt::EIDPatternOpt(char shortopt, const char* longopt,
                             SPtr_EIDPattern* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
EIDPatternOpt::set(const char* val, size_t len)
{
    std::string s(val, len);

    (*(SPtr_EIDPattern*)valp_) = BD_MAKE_PATTERN(s);

    if (! ((*(SPtr_EIDPattern*)valp_)->valid())) {
        return -1;
    }
    
    if (setp_) {
        *setp_ = true;
    }
    
    return 0;
}

void
EIDPatternOpt::get(oasys::StringBuffer* buf)
{
    buf->append((*(SPtr_EIDPattern*)valp_)->c_str());
}

} // namespace dtn
