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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "OndemandLink.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
OndemandLink::OndemandLink(std::string name,
                           ConvergenceLayer* cl,
                           const char* nexthop)
    : Link(name, ONDEMAND, cl, nexthop)
{
    set_state(AVAILABLE);

    // override the default for the idle close time
    params_.idle_close_time_ = 30;
}

//----------------------------------------------------------------------
void
OndemandLink::set_initial_state()
{
    BundleDaemon::post(
        new LinkAvailableEvent(LinkRef(this, "OndemandLink"),
                               ContactEvent::NO_INFO));
}


} // namespace dtn
