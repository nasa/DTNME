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

#ifndef _DTPC_RETRANSMIT_TIMER_H_
#define _DTPC_RETRANSMIT_TIMER_H_

#include <oasys/thread/Timer.h>

namespace dtn {

/**
 * Payload Aggregation timer class.
 *
 * The timer is started when at the beginning of an aggregation period
 * and is cancelled when the payload is sent.
 *
 */
class DtpcRetransmitTimer : public oasys::Timer {
public:
    DtpcRetransmitTimer(std::string key);

    virtual ~DtpcRetransmitTimer() {} 

    /// The key to find the Payload Aggregator
    std::string key_;
protected:
    virtual void timeout(const struct timeval& now);

};

} // namespace dtn

#endif /* _DTPC_RETRANSMIT_TIMER_H_ */
