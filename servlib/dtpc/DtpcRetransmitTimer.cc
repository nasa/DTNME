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

#include "bundling/BundleEvent.h"

#include "DtpcDaemon.h"
#include "DtpcRetransmitTimer.h"

namespace dtn {

DtpcRetransmitTimer::DtpcRetransmitTimer(std::string key)
    : key_(key)
{
}

void
DtpcRetransmitTimer::set_sptr(SPtr_DtpcRetransmitTimer sptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    sptr_ = sptr;
}

void
DtpcRetransmitTimer::start(int seconds)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_ != nullptr) {
        schedule_in(seconds, sptr_);
    } else {
        log_err_p("dtpc/timer/retran", 
                  "Attempt to start DtpcRetransmitTimer(%s) without a SPtr_Timer defined", 
                  key_.c_str());
    }
}

bool
DtpcRetransmitTimer::cancel()
{
    bool result = false;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_ != nullptr) {
        result = oasys::SharedTimer::cancel_timer(sptr_);

        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    }

    return result;
}

void
DtpcRetransmitTimer::timeout(const struct timeval& now)
{
    (void)now;
    
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (cancelled() || sptr_ == nullptr) {
        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    } else {
        // post the expiration event
        log_debug_p("dtpc/timer/retran", "PDU Retransmit timer expired: %s", 
                    key_.c_str());

        DtpcRetransmitTimerExpiredEvent* event = new DtpcRetransmitTimerExpiredEvent(key_);
        SPtr_BundleEvent sptr_event(event);
        DtpcDaemon::post_at_head(sptr_event);

        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
