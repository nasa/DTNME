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

#ifndef _TEST_COMMAND_H_
#define _TEST_COMMAND_H_

// XXX/demmer this really belongs in the daemon directory...

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * CommandModule for the "test" command.
 */
class TestCommand : public oasys::TclCommand {
public:
    TestCommand();

    /**
     * Binding function. Since the class is created before logging is
     * initialized, this can't be in the constructor.
     */
    void bind_vars();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    
    int id_;			///< sets the test node id
    std::string initscript_;	///< tcl script to run at init
    std::string argv_;		///< "list" of space-separated args
};

} // namespace dtn

#endif /* _TEST_COMMAND_H_ */
