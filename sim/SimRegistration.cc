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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/io/FileIOClient.h>
#include <oasys/util/StringBuffer.h>

#include "Simulator.h"
#include "SimLog.h"
#include "SimRegistration.h"
#include "Topology.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "storage/GlobalStore.h"
#include "session/Session.h"

using namespace dtn;

namespace dtnsim {

SimRegistration::SimRegistration(Node* node, const EndpointID& endpoint, u_int32_t expiration)
    : APIRegistration(GlobalStore::instance()->next_regid(),
                   endpoint, DEFER, NONE, 0, expiration,
                   false, 0, ""), node_(node)
{
    logpathf("/reg/%s/%d", node->name(), regid_);

    log_debug("new sim registration");
}

void
SimRegistration::deliver_bundle(Bundle* bundle)
{
    size_t payload_len = bundle->payload().length();

    log_info("N[%s]: RCV id:%" PRIbid " %s -> %s size:%zu",
             node_->name(), bundle->bundleid(),
             bundle->source().c_str(), bundle->dest().c_str(),
             payload_len);

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

int
SimRegistration::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz,
                    "id %u: %s %s (%s%s %s %s) [expiration %d%s%s%s%s]",
                    regid(),
                    active() ? "active" : "passive",
                    endpoint().c_str(),
                    failure_action_toa(failure_action()),
                    failure_action() == Registration::EXEC ?
                      script().c_str() : "",
                    replay_action_toa(replay_action()),
                    delivery_acking_ ? "ACK_Bndl_Dlvry" : "Auto-ACK_Bndl_Dlvry",
                    expiration(),
                    session_flags() != 0 ? " session:" : "",
                    (session_flags() & Session::CUSTODY) ? " custody" : "",
                    (session_flags() & Session::PUBLISH) ? " publish" : "",
                    (session_flags() & Session::SUBSCRIBE) ? " subscribe" : ""
        );
}
} // namespace dtnsim

