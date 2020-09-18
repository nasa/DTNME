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

#ifndef _SHUTDOWN_COMMAND_H_
#define _SHUTDOWN_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

class DTNServer;

/**
 * CommandModule for the "shutdown" command.
 */
class ShutdownCommand : public oasys::TclCommand {
public:
    ShutdownCommand(DTNServer* server, const char* cmd = "shutdown");

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

protected:
    static void call_exit(void* clientData);

    DTNServer* dtnserver_;
};

} // namespace dtn

#endif /* _SHUTDOWN_COMMAND_H_ */
