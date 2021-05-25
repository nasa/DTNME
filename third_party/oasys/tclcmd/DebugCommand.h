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

#ifndef _OASYS_DEBUG_COMMAND_H_
#define _OASYS_DEBUG_COMMAND_H_

#include "TclCommand.h"

namespace oasys {

/**
 * Class to export the debugging system to tcl scripts.
 */
class DebugCommand : public TclCommand {
public:
    DebugCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
};


} // namespace oasys

#endif //_OASYS_DEBUG_COMMAND_H_
