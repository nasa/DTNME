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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "SMTPUtils.h"
#include "debug/Log.h"
#include "util/Regex.h"

namespace oasys {

//----------------------------------------------------------------------
bool
SMTPUtils::extract_address(const std::string& str, std::string* address)
{
    // try a simple regex for now
    Regex pat("([A-Za-z0-9_]+@[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+)", REG_EXTENDED);

    int err = pat.match(str.c_str(), 0);
    if (err != 0) {
        log_debug_p("/oasys/smtp/utils",
                    "extract_address %s failed: %s",
                    str.c_str(), pat.regerror_str(err).c_str());
        return false;
    }

    ASSERT(pat.num_matches() >= 1);

    address->assign(str.substr(pat.get_match(0).rm_so,
                               pat.get_match(0).rm_eo - pat.get_match(0).rm_so));

    log_debug_p("/oasys/smtp/utils",
                "extract_address %s -> %s", str.c_str(), address->c_str());
    return true;
}

} // namespace oasys
