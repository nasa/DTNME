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

#ifndef _BUNDLE_COMMAND_H_
#define _BUNDLE_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * Debug command for hand manipulation of bundles.
 */
class BundleCommand : public oasys::TclCommand {
public:
    BundleCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp);

private:

    /**
     * "bundle inject" command parameters/options
     */
    class InjectOpts {
    public:
        InjectOpts();
        
        bool custody_xfer_; 	///< Custody xfer requested
        bool receive_rcpt_;	///< Hop by hop reception receipt
        bool custody_rcpt_;	///< Custody xfer reporting
        bool forward_rcpt_;	///< Hop by hop forwarding reporting
        bool delivery_rcpt_;	///< End-to-end delivery reporting
        bool deletion_rcpt_;	///< Bundle deletion reporting
        u_int expiration_;      ///< Bundle TTL
        u_int length_;          ///< Bundle payload length
        std::string replyto_;   ///< Bundle Reply-To EID
    };

    /**
     * Parse the "bundle inject" command line options
     */
    bool parse_inject_options(InjectOpts* options, int objc, Tcl_Obj** objv,
                              const char** invalidp);
    
};

} // namespace dtn

#endif /* _BUNDLE_COMMAND_H_ */
