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

#ifndef _ACS_EXPIRATION_TIMER_H_
#define _ACS_EXPIRATION_TIMER_H_

#ifdef ACS_ENABLED

#include <oasys/thread/Timer.h>

namespace dtn {

/**
 * Aggregate Custody Signal expiration timer class.
 *
 * The timer is started when the pending ACS is first initiated. When
 * the timer is triggered the ACS is sent or it can be cancelled if
 * the ACS is sent early due to size considerations.
 *
 */
class AcsExpirationTimer : public oasys::Timer {
public:
    AcsExpirationTimer(std::string key, uint32_t pacs_id);

    virtual ~AcsExpirationTimer() {} 

    /// The key to access the pending acs in the map
    std::string acs_key_;

    /// ID of the Pending ACS this timer refers to
    uint32_t pacs_id_;
    
protected:
    void timeout(const struct timeval& now);

};

} // namespace dtn

#endif // ACS_ENABLED

#endif /* _ACS_EXPIRATION_TIMER_H_ */
