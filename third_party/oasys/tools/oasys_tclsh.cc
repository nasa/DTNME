/*
 *    Copyright 2007 Intel Corporation
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

#include "../debug/Log.h"
#include "../tclcmd/TclCommand.h"

using namespace oasys;

int
main(int argc, char* const argv[])
{
    Log::init();
    if (oasys::TclCommandInterp::init(argv[0], "/oasys_tclsh") != 0)
    {
        fprintf(stderr, "can't init TCL\n");
        exit(1);
    }

    if (argc == 1) {
        oasys::TclCommandInterp::instance()->command_loop("oasys-tclsh% ");
    } else {
        // handle files
    }
}
