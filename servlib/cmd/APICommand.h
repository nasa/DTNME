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

#ifndef _API_COMMAND_H_
#define _API_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

#include "applib/APIServer.h"

namespace dtn {

/**
 * API options command
 */
class APICommand : public oasys::TclCommand {
public:
    APICommand(APIServer* server);
};


} // namespace dtn

#endif /* _API_COMMAND_H_ */
