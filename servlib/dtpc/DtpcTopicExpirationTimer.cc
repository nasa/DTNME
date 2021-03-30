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
#include "DtpcTopicExpirationTimer.h"

namespace dtn {

DtpcTopicExpirationTimer::DtpcTopicExpirationTimer(u_int32_t topic_id)
    : topic_id_(topic_id)
{
}

void
DtpcTopicExpirationTimer::set_sptr(SPtr_DtpcTopicExpirationTimer& sptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    sptr_ = sptr;
}

void
DtpcTopicExpirationTimer::start(int seconds)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_ != nullptr) {
        schedule_in(seconds, sptr_);
    } else {
        log_err_p("dtpc/topic/expiration", 
                  "Attempt to start DtpcTopicExpirationTimer(%" PRIu32 ") without a SPtr_Timer defined", 
                  topic_id_);
    }
}

bool
DtpcTopicExpirationTimer::cancel()
{
    bool result = false;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_ != nullptr) {
        result = oasys::SharedTimer::cancel(sptr_);

        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    }

    return result;
}

void
DtpcTopicExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;
    
    if (cancelled() || sptr_ == nullptr) {
        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    } else {
        // post the expiration event
        log_debug_p("dtpc/topic/expiration", "TopicExpiration timer expired: %" PRIu32, 
                   topic_id_); 
        DtpcDaemon::post_at_head(new DtpcTopicExpirationCheckEvent(topic_id_));

        // clear the internal reference so this obejct will be deleted
        sptr_ = nullptr;
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
