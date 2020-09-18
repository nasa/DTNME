/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _NODE_COMMAND_H_
#define _NODE_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

#include "cmd/BundleCommand.h"
#include "cmd/LinkCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

namespace dtnsim {

class Node;

/**
 * Class to control the node
 */
class NodeCommand : public oasys::TclCommand {
public:
    NodeCommand(Node* node);

    /// virtual from TclCommand
    virtual int exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp);

protected:
    Node* node_;
    dtn::BundleCommand bundle_cmd_;
    dtn::LinkCommand   link_cmd_;
    dtn::ParamCommand  param_cmd_;
    dtn::RouteCommand  route_cmd_;
    dtn::StorageCommand storage_cmd_;
};

} // namespace dtnsim

#endif /* _NODE_COMMAND_H_ */
