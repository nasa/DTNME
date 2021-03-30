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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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
#include "ExpirationTimer.h"

namespace dtn {

ExpirationTimer::ExpirationTimer(Bundle* bundle)
    : bref_(bundle, "expiration timer")
{
}

ExpirationTimer::~ExpirationTimer()
{
}


void
ExpirationTimer::start(int64_t expiration_ms, SPtr_ExpirationTimer& sptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    sptr_ = sptr;

    if (sptr_ != nullptr) {
        oasys::SharedTimer::schedule_in(expiration_ms, sptr_);
    }
}

bool
ExpirationTimer::cancel()
{
    bool result = false;

    oasys::ScopeLock scoplok(&lock_, __func__);

    bref_.release();

    if (sptr_ != nullptr) {
        result = oasys::SharedTimer::cancel(sptr_);

        // clear self reference
        sptr_ = nullptr;
    }

    return result;
}

void
ExpirationTimer::timeout(const struct timeval& now)
{
    (void) now;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (!cancelled() && (bref_ != nullptr)) {
        // null out the pointer to ourself in the bundle class
        bref_->clear_expiration_timer();
    
        // post the expiration event
        BundleDaemon::post_at_head(new BundleExpiredEvent(bref_.object()));
    }

    bref_.release();

    // clear self reference in case  it has not been done yet
    sptr_ = nullptr;
}

} // namespace dtn
