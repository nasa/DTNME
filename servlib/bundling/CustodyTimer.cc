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

#include <algorithm>
#include <third_party/oasys/util/OptParser.h>

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
    u_int32_t timeout = (u_int32_t)((double)lifetime_pct_ * bundle->expiration_secs() / 100.0);

    if (min_ != 0) {
        timeout = std::max(timeout, min_);
    }
    if (max_ != 0) {
        timeout = std::min(timeout, max_);
    }
    
    log_debug_p("/dtn/bundle/custody_timer", "calculate_timeout: "
                "min %u, lifetime_pct %u, expiration %" PRIu64 ", max %u: timeout %u",
                min_, lifetime_pct_, bundle->expiration_secs(), max_, timeout);
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
CustodyTimer::CustodyTimer(Bundle* bundle, const LinkRef& link)
    : Logger("CustodyTimer", "/dtn/bundle/custody_timer"),
      bref_(bundle, "CustodyTimer"), lref_(link.object(), "CustodyTimer")
{
    if (lref_ != nullptr) {
        link_name_ = lref_->name();
    }
}


//----------------------------------------------------------------------
void
CustodyTimer::start(const oasys::Time& xmit_time,
                    const CustodyTimerSpec& spec,
                    SPtr_CustodyTimer sptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // save an internal reference so we can later cancel
    sptr_ = sptr;

    oasys::Time time(xmit_time);
    u_int32_t delay = spec.calculate_timeout(bref_.object());
    time.sec_ += delay;

    log_debug("scheduling timer: xmit_time %" PRIu64 ".%" PRIu64 " delay %u secs "
             "(in %" PRIu64 " msecs) for *%p",
             xmit_time.sec_, xmit_time.usec_, delay,
             (time - oasys::Time::now()).in_milliseconds(), bref_.object());

    // XXX/demmer the Timer interface should be changed to use oasys::Time
    struct timeval tv;
    tv.tv_sec  = time.sec_;
    tv.tv_usec = time.usec_;

    oasys::SharedTimer::schedule_at(&tv, sptr_);
}

//----------------------------------------------------------------------
void
CustodyTimer::cancel()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // release the bundle and link
    bref_.release();
    lref_.release();

    if (sptr_ != nullptr) {
        oasys::SharedTimer::cancel_timer(sptr_);
        sptr_ = nullptr;
    }
}

//----------------------------------------------------------------------
void
CustodyTimer::timeout(const struct timeval& now)
{
    (void) now;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (bref_ != nullptr) {
        BundleDaemon::post(new CustodyTimeoutEvent(bref_.object(), lref_));
    }

    // release the bundle and link
    bref_.release();
    lref_.release();

    sptr_ = nullptr;
}

} // namespace dtn
