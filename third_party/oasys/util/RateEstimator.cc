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
#  include <oasys-config.h>
#endif

#include "RateEstimator.h"

namespace oasys {

RateEstimator::RateEstimator(int *var, int interval, double weight, bool force_update)
{
    (void) force_update;

    var_ = var;
    interval_ = interval;
    weight_ = weight;
    lastval_ = 0;
    rate_ = 0.0;
    lastts_.tv_sec = 0;
    lastts_.tv_usec = 0;
}

void
RateEstimator::start(SPtr_RateEstimator sptr)
{
    sptr_ = sptr;
    reschedule();
}


void
RateEstimator::reschedule()
{
    if (sptr_ != nullptr) {
        SharedTimer::schedule_in(interval_, sptr_);
    }
}



void
RateEstimator::timeout(const struct timeval& now)
{
    if (lastts_.tv_sec == 0 && lastts_.tv_usec == 0) {
        // first time through
        rate_    = 0.0;
 done:
        lastval_ = *var_;
        lastts_  = now;
        reschedule();
        return;
    }
    
    double dv = (double)(*var_ - lastval_);
    double dt = TIMEVAL_DIFF_DOUBLE(now, lastts_);
    
    double newrate = dv/dt;
    double oldrate = rate_;
    double delta   = newrate - oldrate;

    rate_ += delta * weight_;

    goto done;
}

} // namespace oasys
