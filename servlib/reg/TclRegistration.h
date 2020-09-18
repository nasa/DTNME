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

#ifndef _TCL_REGISTRATION_H_
#define _TCL_REGISTRATION_H_

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Thread.h>

#include "Registration.h"
#include "bundling/Bundle.h"

namespace dtn {

class BlockingBundleList;


/**
 * A simple utility class used mostly for testing registrations.
 *
 * When created, this sets up a new registration within the daemon,
 * and for any bundles that arrive, outputs logs of the bundle header
 * fields as well as the payload data (if ascii). The implementation
 * is structured as a thread that blocks (forever) waiting for bundles
 * to arrive on the registration's bundle list, then tcl the
 * bundles and looping again.
 */
class TclRegistration : public Registration {
public:
    TclRegistration(const EndpointIDPattern& endpoint,
                    Tcl_Interp* interp);
    int exec(int argc, const char** argv, Tcl_Interp* interp);

    /**
     * Return in the tcl result a Tcl_Channel to wrap the BundleList's
     * notifier pipe.
     */
    int get_list_channel(Tcl_Interp* interp);

    /**
     * Assuming the list channel has been notified, pops a bundle off
     * the list and then returns in the tcl result a list of the
     * relevant metadata and the payload data.
     */
    int get_bundle_data(Tcl_Interp* interp);

    /**
     * Parse the given bundle's internals into a new tcl list object
     * (or an error if parsing fails).
     */
    static int parse_bundle_data(Tcl_Interp* interp,
                                 const BundleRef& b,
                                 Tcl_Obj** result);

    /// virtual from Registration
    void deliver_bundle(Bundle* bundle);

protected:
    BlockingBundleList* bundle_list_;
    Tcl_Channel notifier_channel_;
};

} // namespace dtn

#endif /* _TCL_REGISTRATION_H_ */
