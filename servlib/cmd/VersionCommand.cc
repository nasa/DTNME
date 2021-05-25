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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "VersionCommand.h"

namespace dtn {

VersionCommand::VersionCommand()
    : TclCommand("version")
{
    add_to_help("version", "display software version");
}

int
VersionCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    // should have no subcommands or options
    if (argc != 1) {
        wrong_num_args(argc, argv, 1, 1, 1);
        return TCL_ERROR;
    }

    resultf("DTNME %s\nwith %s", DTN_VERSION_STRING, OASYS_VERSION_STRING);

    return TCL_OK;
}


} // namespace dtn
