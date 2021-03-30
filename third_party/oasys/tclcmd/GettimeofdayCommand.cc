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

#include <sys/time.h>
#include "GettimeofdayCommand.h"

namespace oasys {

GettimeofdayCommand::GettimeofdayCommand()
    : TclCommand("gettimeofday")
{
    add_to_help("gettimeofday",
                "Print the result of gettimeofday() in secs.usecs format");
}

int
GettimeofdayCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    if (argc != 1) {
        wrong_num_args(argc, argv, 1, 1, 1);
        return TCL_ERROR;
    }

    struct timeval tv;
    ::gettimeofday(&tv, 0);
    resultf("%lu.%lu",
            (unsigned long)tv.tv_sec,
            (unsigned long)tv.tv_usec);
    return TCL_OK;
}

} // namespace oasys
