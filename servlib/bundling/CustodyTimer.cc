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

#include <algorithm>
#include <oasys/util/OptParser.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "CustodyTimer.h"

namespace dtn {

/**
 * Default custody timer specification:
 *
 * min: 30 minutes
 * lifetime percent: 25%
 * max: unlimited
 */
CustodyTimerSpec CustodyTimerSpec::defaults_(30 * 60, 25, 0);

//----------------------------------------------------------------------
u_int32_t
CustodyTimerSpec::calculate_timeout(const Bundle* bundle) const
{
    u_int32_t timeout = (u_int32_t)((double)lifetime_pct_ * bundle->expiration() / 100.0);

    if (min_ != 0) {
        timeout = std::max(timeout, min_);
    }
    if (max_ != 0) {
        timeout = std::min(timeout, max_);
    }
    
    log_debug_p("/dtn/bundle/custody_timer", "calculate_timeout: "
                "min %u, lifetime_pct %u, expiration %" PRIu64 ", max %u: timeout %u",
                min_, lifetime_pct_, bundle->expiration(), max_, timeout);
    return timeout;
}

//----------------------------------------------------------------------
int
CustodyTimerSpec::parse_options(int argc, const char* argv[],
                                const char** invalidp)
{
    oasys::OptParser p;
    p.addopt(new oasys::UIntOpt("custody_timer_min", &min_));
    p.addopt(new oasys::UIntOpt("custody_timer_lifetime_pct", &lifetime_pct_));
    p.addopt(new oasys::UIntOpt("custody_timer_max", &max_));
    return p.parse_and_shift(argc, argv, invalidp);
}

//----------------------------------------------------------------------
void
CustodyTimerSpec::serialize(oasys::SerializeAction* a)
{
    a->process("min", &min_);
    a->process("lifetime_pct", &lifetime_pct_);
    a->process("max", &max_);
}

//----------------------------------------------------------------------
CustodyTimer::CustodyTimer(const oasys::Time& xmit_time,
                           const CustodyTimerSpec& spec,
                           Bundle* bundle, const LinkRef& link)
    : Logger("CustodyTimer", "/dtn/bundle/custody_timer"),
      bundle_(bundle, "CustodyTimer"), link_(link.object(), "CustodyTimer")
{
    oasys::Time time(xmit_time);
    u_int32_t delay = spec.calculate_timeout(bundle);
    time.sec_ += delay;

    log_info("scheduling timer: xmit_time %u.%u delay %u secs "
             "(in %u msecs) for *%p",
             xmit_time.sec_, xmit_time.usec_, delay,
             (time - oasys::Time::now()).in_milliseconds(), bundle);

    // XXX/demmer the Timer interface should be changed to use oasys::Time
    struct timeval tv;
    tv.tv_sec  = time.sec_;
    tv.tv_usec = time.usec_;
    schedule_at(&tv);
}

//----------------------------------------------------------------------
void
CustodyTimer::timeout(const struct timeval& now)
{
    (void)now;
    log_info("CustodyTimer::timeout");
    if (NULL != bundle_.object()) {
        BundleDaemon::post(new CustodyTimeoutEvent(bundle_.object(), link_));
    }
}

} // namespace dtn
