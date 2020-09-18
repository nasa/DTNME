/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifndef _SECURITY_COMMAND_H_
#define _SECURITY_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

#ifdef BSP_ENABLED

namespace dtn {

class SecurityCommand : public oasys::TclCommand {

public:

    SecurityCommand();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
private:
    bool is_a_bab(int num);
    bool is_a_pib(int num);
    bool is_a_pcb(int num);
    bool is_a_esb(int num);
    bool is_a_eib(int num);
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _SECURITY_COMMAND_H_ */
