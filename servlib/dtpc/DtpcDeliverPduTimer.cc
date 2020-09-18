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

#ifdef DTPC_ENABLED

#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"

#include "DtpcDaemon.h"
#include "DtpcDeliverPduTimer.h"

namespace dtn {

DtpcDeliverPduTimer::DtpcDeliverPduTimer(std::string key, u_int64_t seq_ctr)
    : key_(key),
      seq_ctr_(seq_ctr)
{
}

void
DtpcDeliverPduTimer::timeout(const struct timeval& now)
{
    (void)now;
    
    // post the expiration event
    log_debug_p("dtpc/timer/deliverpdu", "Deliver PDU timer expired: %s", 
                key_.c_str());
    DtpcDaemon::post_at_head(new DtpcDeliverPduTimerExpiredEvent(key_, seq_ctr_));

    // clean ourselves up
    delete this;
}

} // namespace dtn

#endif // DTPC_ENABLED
