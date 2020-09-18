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

#ifndef _CUSTODYTIMER_H_
#define _CUSTODYTIMER_H_

#include <oasys/serialize/Serialize.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Time.h>
#include "bundling/BundleRef.h"
#include "contacts/Link.h"

namespace dtn {

class Bundle;

/**
 * Utility class to abstract out various parameters that can be used
 * to calculate custody retransmission timers. This means that future
 * extensions that take into account other parameters or factors can
 * simply extend this class and modify the calculate_timeout()
 * function to add new features.
 *
 * The current basic scheme calculates the timer as:
 * <code>
 * timer = min((min_ + (lifetime_pct_ * bundle->lifetime_ / 100)), max_)
 * </code>
 *
 * In other words, this class allows a retransmisison to be specified
 * according to a minimum timer (min_), a multiplying factor based on
 * the bundle's lifetime (lifetime_pct_), and a maximum bound (max_).
 * All values are in seconds.
 */
class CustodyTimerSpec : public oasys::SerializableObject {
public:
    /**
     * Custody timer defaults, values set in the static initializer.
     */
    static CustodyTimerSpec defaults_;
    
    /**
     * Constructor.
     */
    CustodyTimerSpec(u_int32_t min,
                     u_int32_t lifetime_pct,
                     u_int32_t max)
        : min_(min), lifetime_pct_(lifetime_pct), max_(max) {}

    /**
     * Default Constructor.
     */
    CustodyTimerSpec()
        : min_(defaults_.min_),
          lifetime_pct_(defaults_.lifetime_pct_),
          max_(defaults_.max_) {}

    /**
     * Calculate the appropriate timeout for the given bundle.
     */
    u_int32_t calculate_timeout(const Bundle* bundle) const;

    /**
     * Parse options to set the fields of the custody timer. Shifts
     * any non-matching options to the beginning of the vector by
     * using OptParser::parse_and_shift.
     *
     * @return the number of parsed options
     */
    int parse_options(int argc, const char* argv[],
                      const char** invalidp = NULL);

    void serialize(oasys::SerializeAction* a);

    u_int32_t min_;		///< min timer
    u_int32_t lifetime_pct_;	///< percentage of lifetime
    u_int32_t max_;		///< upper bound
};

/**
 * A custody transfer timer. Each bundle stores a vector of these
 * timers, as the bundle may be in flight on multiple links
 * concurrently, each having distinct retransmission timer parameters.
 * When a given timer fires, a retransmission is expected to be
 * initiated on only one link.
 * 
 * When a successful custody signal is received, the daemon will
 * cancel all timers and release custody, informing the routers as
 * such. If a failed custody signal is received or a timer fires, it
 * is up to the router to initiate a retransmission on one or more
 * links.
 */
class CustodyTimer : public oasys::Timer, public oasys::Logger {
public:
    /** Constructor */
    CustodyTimer(const oasys::Time& xmit_time,
                 const CustodyTimerSpec& spec,
                 Bundle* bundle, const LinkRef& link);

    /** Virtual timeout function */
    void timeout(const struct timeval& now);

    ///< The bundle for whom the the timer refers
    BundleRef bundle_;

    ///< The link that it was transmitted on
    LinkRef link_;
};

/**
 * Class for a vector of custody timers.
 */
class CustodyTimerVec : public std::vector<CustodyTimer*> {};

} // namespace dtn

#endif /* _CUSTODYTIMER_H_ */
