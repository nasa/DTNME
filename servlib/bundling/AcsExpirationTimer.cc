/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifdef ACS_ENABLED

#include "AcsExpirationTimer.h"
#include "BundleDaemonACS.h"
#include "BundleEvent.h"

namespace dtn {

AcsExpirationTimer::AcsExpirationTimer(std::string key, uint32_t pacs_id)
    : acs_key_(key), 
      pacs_id_(pacs_id) 
{}

void
AcsExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;

    // post the expiration event
    BundleDaemonACS::post_at_head(new AcsExpiredEvent(acs_key_, pacs_id_));

    // clean ourselves up
    delete this;
}

} // namespace dtn

#endif // ACS_ENABLED
