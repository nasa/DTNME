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

#ifndef _DTPC_PAYLOAD_AGGREGATION_TIMER_H_
#define _DTPC_PAYLOAD_AGGREGATION_TIMER_H_

#include <oasys/thread/Timer.h>

namespace dtn {

/**
 * Payload Aggregation timer class.
 *
 * The timer is started at the beginning of an aggregation period
 * and is cancelled when the payload is sent.
 *
 */
class DtpcPayloadAggregationTimer : public oasys::Timer {
public:
    DtpcPayloadAggregationTimer(std::string key, u_int64_t seq_ctr);

    virtual ~DtpcPayloadAggregationTimer() {} 

    /// The key to find the Payload Aggregator
    std::string key_;

    /// sequence counter when the timer was started
    u_int64_t seq_ctr_;    

protected:
    virtual void timeout(const struct timeval& now);

};

} // namespace dtn

#endif /* _DTPC_PAYLOAD_AGGREGATION_TIMER_H_ */
