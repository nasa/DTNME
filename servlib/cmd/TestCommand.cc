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

#include "TestCommand.h"

namespace dtn {

TestCommand::TestCommand()
    : TclCommand("test"), id_(0)
{
    add_to_help("segfault", "Generate a segfault.");
    add_to_help("panic", "Trigger a panic.");
    add_to_help("assert", "Trigger a false assert.");
}

void
TestCommand::bind_vars()
{
    bind_var(new oasys::IntOpt("id", &id_, "id",
                               "The test id. (Default is 0.)"));
    bind_var(new oasys::StringOpt("initscript", &initscript_, "script",
                                  "The script to start."));
    bind_var(new oasys::StringOpt("argv", &argv_, "args",
                                  "A string to pass as the argument to the script."));
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a test subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    if (!strcmp(cmd, "segfault"))
    {
        int* x = NULL;
        (void)*x;
        NOTREACHED;
    }
    else if (!strcmp(cmd, "panic"))
    {
        PANIC("panic");
    }
    else if (!strcmp(cmd, "assert"))
    {
        ASSERT(0);
    }

    return TCL_ERROR;
}

} // namespace dtn
