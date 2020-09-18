/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _ACS_COMMAND_H_
#define _ACS_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * The "acs" command.
 */
class AcsCommand : public oasys::TclCommand {
public:
    AcsCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
};

} // namespace dtn

#endif /* _ACS_COMMAND_H_ */
