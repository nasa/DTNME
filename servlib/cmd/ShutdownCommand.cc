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

#include "ShutdownCommand.h"
#include "CompletionNotifier.h"
#include "DTNServer.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

ShutdownCommand::ShutdownCommand(DTNServer* dtnserver, const char* cmd)
    : TclCommand(cmd), dtnserver_(dtnserver)
{
    add_to_help(cmd, "shutdown the daemon");
}

void
ShutdownCommand::call_exit(void* clientData)
{
    (void)clientData;
    oasys::TclCommandInterp::instance()->exit_event_loop();
}

int
ShutdownCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    // should have no subcommands or options
    if (argc != 1) {
        wrong_num_args(argc, argv, 1, 1, 1);
        return TCL_ERROR;
    }

    // to make it possible to both return from the shutdown command
    // and still cleanly return from the tcl command, we exit from the
    // event loop in the background
    Tcl_CreateTimerHandler(0, ShutdownCommand::call_exit,
                           (void*)dtnserver_);

    return TCL_OK;
}


} // namespace dtn
