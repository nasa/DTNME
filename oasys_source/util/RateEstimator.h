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
 
class RateEstimator : public Timer {
public:
    RateEstimator(int *var, int interval, double weight = 0.125);
    double rate() { return rate_; }
    virtual void timeout(const struct timeval& now);
    
protected:
    int* var_;		///< variable being estimated
    double rate_;	///< the estimated rate
    int lastval_;	///< last sample value
    int interval_;	///< timer interval (ms)
    timeval lastts_;	///< last sample timestamp
    double weight_;	///< weighting factor for sample decay
};

} // namespace oasys

#endif /* _RATE_ESTIMATOR_H_ */
