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

#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "ExpirationTimer.h"

namespace dtn {

ExpirationTimer::ExpirationTimer(Bundle* bundle)
    : bundleref_(bundle, "expiration timer")
{
}

void
ExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;
    oasys::ScopeLock l(bundleref_->lock(), "ExpirationTimer::timeout");

    // null out the pointer to ourself in the bundle class
    bundleref_->set_expiration_timer(NULL);
    
    // post the expiration event
    //log_debug_p("/timer/expiration", "Bundle %" PRIbid " expired", bundleref_.object()->bundleid());
    BundleDaemon::post_at_head(new BundleExpiredEvent(bundleref_.object()));

    // clean ourselves up
    delete this;
}

} // namespace dtn
