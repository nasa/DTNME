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

#ifndef _DTPC_RECEIVER_H_
#define _DTPC_RECEIVER_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/compat/inttypes.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/SpinLock.h>

#include "reg/APIRegistration.h"

namespace dtn {

/**
 * Class that registers to receive bundles and feeds them into the Daemon.
 */
class DtpcReceiver : public oasys::Thread,
                     public oasys::Logger
{
public:
    /**
     * Constructor.
     */
    DtpcReceiver();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~DtpcReceiver();

    /**
     * Virtual initialization function, possibly overridden by a 
     * future simulator
     */
    virtual void do_init();

    /**
     * Format the given StringBuffer with the current 
     * statistics.
     */
    virtual void get_stats(oasys::StringBuffer* buf);

    /**
     * Reset all internal stats.
     */
    virtual void reset_stats();

    /**
     * Wait for a bundle to arrive
     */
    virtual int wait_for_notify(const char*       operation,
                                u_int32_t         dtn_timeout,
                                APIRegistration** recv_ready_reg);

    /**
     * Process a received bundle
     */
    virtual void receive_bundle(APIRegistration* reg);

protected:

    /**
     * Main thread function that dispatches events.
     */
    virtual void run();

    /// Statistics structure definition
    struct Stats {
        u_int32_t delivered_bundles_;
        u_int32_t transmitted_bundles_;
        u_int32_t expired_bundles_;
        u_int32_t deleted_bundles_;
        u_int32_t injected_bundles_;
        u_int32_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    /// List of DTPC Registrations (dtn and ipn schemes)
    APIRegistrationList* bindings_;

    /// Notifier used to wait for BundleDaemon response
    oasys::Notifier notifier_;
};

} // namespace dtn

#endif /* _DTPC_RECEIVER_H_ */
