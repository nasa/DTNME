#ifndef _BLOCK_COMMAND_H_
#define _BLOCK_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * Parameter setting command
 */
class BlockCommand: public oasys::TclCommand {
public:
    BlockCommand();
};


} // namespace dtn

#endif /* _BLOCK_COMMAND_H_ */
