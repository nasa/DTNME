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
#  include <oasys-config.h>
#endif
#include "DebugCommand.h"
#include "../memory/Memory.h"

namespace oasys {

DebugCommand::DebugCommand()
    : TclCommand("debug")
{
#ifdef OASYS_DEBUG_MEMORY_ENABLED
    add_to_help("dump_memory", "Dump memory usage");
    add_to_help("dump_memory_diffs", "Dump memory diff of usage");
#endif    
}

int
DebugCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a subcommand");
        return TCL_ERROR;
    }
    const char* cmd = argv[1];

#ifdef OASYS_DEBUG_MEMORY_ENABLED
    // debug dump_memory
    if (!strcmp(cmd, "dump_memory")) {
        DbgMemInfo::debug_dump();
        return TCL_OK;
    } else if (!strcmp(cmd, "dump_memory_diffs")) {
        DbgMemInfo::debug_dump(true);
        return TCL_OK;
    }
#endif // OASYS_DEBUG_MEMORY_ENABLED
    
    resultf("unimplemented debug subcommand: %s", cmd);
    return TCL_ERROR;
}

} // namespace oasys
