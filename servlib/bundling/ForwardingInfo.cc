/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "ForwardingInfo.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

void 
ForwardingInfo::serialize(oasys::SerializeAction *a)
{
    a->process("state", &state_);
    a->process("action", &action_);
    a->process("link_name", &link_name_);
    a->process("regid", &regid_);
    a->process("remote_eid", &remote_eid_);
    a->process("timestamp_sec", &timestamp_.sec_);
    a->process("timestamp_usec", &timestamp_.usec_);
    if(a->action_code() == oasys::Serialize::UNMARSHAL) {
        if(state_ == QUEUED && !BundleDaemon::instance()->contactmgr()->has_link(link_name_.c_str())) {
            state_ = TRANSMIT_FAILED;
            timestamp_.get_time();
        }
    }
}

} // namespace dtn
