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


#ifndef _RATE_ESTIMATOR_H_
#define _RATE_ESTIMATOR_H_

#include "../thread/Timer.h"

namespace oasys {

/**
 * Simple rate estimation class that does a weighted filter of
 * samples.
 */
 
class RateEstimator;
typedef std::shared_ptr<RateEstimator> SPtr_RateEstimator;

class RateEstimator : public SharedTimer {
public:
    //RateEstimator(int *var, int interval, double weight = 0.125);
    RateEstimator(int *var, int interval, double weight, bool force_update);
    double rate() { return rate_; }
    virtual void timeout(const struct timeval& now);

    // Schedule initial timeout and pass in shread_ptr for future reschedules
    void start(SPtr_RateEstimator& sptr);
    void reschedule();

protected:
    int* var_;		///< variable being estimated
    double rate_;	///< the estimated rate
    int lastval_;	///< last sample value
    int interval_;	///< timer interval (ms)
    timeval lastts_;	///< last sample timestamp
    double weight_;	///< weighting factor for sample decay
    SPtr_Timer sptr_; ///< shared_ptr to self to allow internal rescheduling
};

} // namespace oasys

#endif /* _RATE_ESTIMATOR_H_ */
