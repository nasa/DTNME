/*
 *    Copyright 2007 Intel Corporation
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
#include <dtn-config.h>
#endif

#include "Subscriber.h"
#include "reg/Registration.h"

namespace dtn {

//----------------------------------------------------------------------
Subscriber::~Subscriber()
{
}

//----------------------------------------------------------------------
int
Subscriber::format(char* buf, size_t len) const
{
    if (is_null()) {
        return snprintf(buf, len, "(null_subscriber)");

    } else if (is_local()) {
        return snprintf(buf, len, "regid:%d", reg_->regid());
        
    } else {
        return snprintf(buf, len, "peer:%s", nexthop_.c_str());
    }
}

//----------------------------------------------------------------------
bool
Subscriber::operator==(const Subscriber& other) const
{
    return (reg_ == other.reg_) && (nexthop_ == other.nexthop_);
}

} // namespace dtn
