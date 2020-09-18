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

#include "IdleTclExit.h"
#include "TclCommand.h"

namespace oasys {

//----------------------------------------------------------------------
IdleTclExit::IdleTclExit(u_int interval)
    : Logger("IdleTclExit", "/command/idle_exit"),
      notifier_("/command/idle_exit"),
      interval_(interval)
{
    int fd = notifier_.read_fd();

    TclCommandInterp* interp = TclCommandInterp::instance();

    intptr_t fd_as_ptr_size = (intptr_t) fd;
    Tcl_Channel chan =
        interp->register_file_channel((ClientData)fd_as_ptr_size, TCL_READABLE);

    StringBuffer cmd("fileevent %s readable exit_event_loop",
                     Tcl_GetChannelName(chan));
    int ok = interp->exec_command(cmd.c_str());
    if (ok != 0) {
        log_err("error setting up file event");
    }
    reschedule();
}

//----------------------------------------------------------------------
void
IdleTclExit::reschedule()
{
    schedule_in(interval_ * 1000);
}

//----------------------------------------------------------------------
void
IdleTclExit::timeout(const struct timeval& now)
{
    if (is_idle(now)) {
        log_notice("idle timer triggered shutdown time");
        notifier_.notify();
    } else {
        log_debug("idle time not reached");
        reschedule();
    }
}

} // namespace oasys

