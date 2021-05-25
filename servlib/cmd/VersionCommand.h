
#ifndef _VERSION_COMMAND_H_
#define _VERSION_COMMAND_H_

#include <third_party/oasys/oasys-version.h>
#include <third_party/oasys/tclcmd/TclCommand.h>

#include "dtn-version.h"

namespace dtn {

/**
 * CommandModule for the "version" command.
 */
class VersionCommand : public oasys::TclCommand {
public:
    VersionCommand();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

};

} // namespace dtn

#endif /* _VERSION_COMMAND_H_ */
