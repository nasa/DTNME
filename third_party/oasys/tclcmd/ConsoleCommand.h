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

#ifndef _OASYS_CONSOLE_COMMAND_H_
#define _OASYS_CONSOLE_COMMAND_H_

#include "TclCommand.h"

namespace oasys {

/**
 * Class to export various configuration variables related to the tcl
 * console.
 */
class ConsoleCommand : public TclCommand {
public:
    ConsoleCommand(const char* default_prompt);

    bool      stdio_; 	 ///< If true, spawn a stdin/stdout interpreter
    in_addr_t addr_;	 ///< Address to listen for the console
    u_int16_t port_;	 ///< Port to listen for the console
    std::string prompt_; ///< Prompt string
};


} // namespace oasys

#endif //_OASYS_CONSOLE_COMMAND_H_
