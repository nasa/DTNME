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
#include "HelpCommand.h"
#include "../memory/Memory.h"
#include "../util/StringBuffer.h"
#include "../util/StringUtils.h"

namespace oasys {

HelpCommand::HelpCommand()
    : TclCommand("help")
{
    add_to_help("help <cmd>", "Print the help documentation for cmd");
}

int
HelpCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    const TclCommandList *cmdlist = NULL;
    TclCommandList::const_iterator iter;
    
    cmdlist = TclCommandInterp::instance()->commands();

    if (argc == 1) 
    {
        StringBuffer buf;
        int len = 0;

        buf.append("For help on a particular command, type \"help <cmd>\".\n");
        buf.append("The registered commands are: \n\t");
                   
        std::vector<std::string> cmd_names;
        for (iter = cmdlist->begin(); iter != cmdlist->end(); ++iter) {
            cmd_names.push_back((*iter)->name());
        }
        std::sort(cmd_names.begin(), cmd_names.end(), StringLessThan());

        for (std::vector<std::string>::iterator j =  cmd_names.begin();
             j != cmd_names.end(); ++j)
        {
            if (len > 60) {
                buf.appendf("\n\t");
                len = 0;
            }

            len += buf.appendf("%s ", j->c_str());
        }
        set_result(buf.c_str());
        return TCL_OK;
    } 
    else if (argc == 2) 
    {
        for (iter = cmdlist->begin(); iter != cmdlist->end(); iter++) {
            if (!strcmp((*iter)->name(), argv[1])) {
                const char *help = (*iter)->help_string();

                if (!help || (help && help[0] == '\0')) {
                    help = "(no help, sorry)";
                }

                if ((*iter)->hasBindings()) {
                    append_resultf("%s cmd_info\n\t%s", (*iter)->name(),
                                   "Lists settable parameters.\n\n");
                }

                append_result(help);
                
                return TCL_OK;
            }
        }

        resultf("no registered command '%s'", argv[1]);
        return TCL_ERROR;
        
    } else {
        wrong_num_args(argc, argv, 2, 3, 3);
        return TCL_ERROR;
    }
}

} // namespace oasys
