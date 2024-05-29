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

#ifndef _DTPC_DELIVER_PDU_TIMER_H_
#define _DTPC_DELIVER_PDU_TIMER_H_

#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/thread/Timer.h>

namespace dtn {

/**
 * Deliver Data PDU timer class.
 *
 * This timer signals that it is time to give up waiting 
 * for missed PDUs and deliver the next PDU
 *
 */


class DtpcDeliverPduTimer;
typedef std::shared_ptr<DtpcDeliverPduTimer> SPtr_DtpcDeliverPduTimer;


class DtpcDeliverPduTimer : public oasys::SharedTimer 
{
public:
    DtpcDeliverPduTimer(std::string key, u_int64_t seq_ctr);

    virtual ~DtpcDeliverPduTimer() {} 

    virtual void start(int seconds);

    virtual bool cancel();

    void set_sptr(SPtr_DtpcDeliverPduTimer sptr);

public:
    /// The key to find the DataPduCollector
    std::string key_;

    /// The sequence counter of the PDU to be delivered at expiration
    u_int64_t seq_ctr_;

protected:
    virtual void timeout(const struct timeval& now);

protected:

    oasys::SPtr_Timer sptr_;

    oasys::SpinLock lock_;
};

} // namespace dtn

#endif /* _DTPC_DELIVER_PDU_TIMER_H_ */
