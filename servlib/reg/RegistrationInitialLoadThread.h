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

#ifndef _REGISTRATION_INITIAL_LOAD_H_
#define _REGISTRATION_INITIAL_LOAD_H_

#include "bundling/BundleDaemon.h"

#include <oasys/thread/Thread.h>


namespace dtn {

class Registration;

/**
 * Class/thread that runs through the pending bundles and delivers the
 * appriate ones to a newly added Registration. This thread offloads
 * the work to a background so that the main BundleDaemon processing 
 * can continue.
 */
class RegistrationInitialLoadThread : public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    RegistrationInitialLoadThread(Registration* registration);

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~RegistrationInitialLoadThread();

protected:
    /**
     * Main thread function that dispatches events.
     */
    virtual void run();

#ifdef PENDING_BUNDLES_IS_MAP
    /**
     * Find bundles to deliver to this registration (map)
     */
    virtual void scan_pending_bundles_map();
#else
    /**
     * Find bundles to deliver to this registration (list)
     */
    virtual void scan_pending_bundles_list();
#endif

    /// The Registration being loaded
    Registration* reg_;
};

} // namespace dtn
#endif // _REGISTRATION_INITIAL_LOAD_H_
