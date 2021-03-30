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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <malloc.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "BPCommand.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

BPCommand::BPCommand()
    : TclCommand("bp") 
{
    bind_var(new oasys::BoolOpt("status_rpts_enabled",
                                &BundleProtocol::params_.status_rpts_enabled_,
                                "Whether or not to generate Bundle Status Reports "
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("use_age_block",
                                &BundleProtocol::params_.use_bundle_age_block_,
                                "Use the BPv7 Bundle Age block for locally sourced bundles "
                                "(default is false)"));

    bind_var(new oasys::UInt8Opt("default_hop_limit",
                                &BundleProtocol::params_.default_hop_limit_,
                                "hop limit",
                                "Use the BPv7 Hop Count block for locally sourced bundles if limit > 0 "
                                "(default is 0)"));

    add_to_help("malloc_trim", "release heap space back to OS");
}


int
BPCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void) interp;

    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, 2);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "malloc_trim") == 0) {
        malloc_trim(0);

        resultf("malloc_trim() has been called");
        return TCL_OK;
        
    } else {
        resultf("unknown bp subcommand %s", cmd);
        return TCL_ERROR;
    }
}

} // namespace dtn
