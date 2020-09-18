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
#  include <dtn-config.h>
#endif

#include "APICommand.h"
#include "applib/APIServer.h"

namespace dtn {

APICommand::APICommand(APIServer* server)
    : TclCommand("api")
{
    bind_var(new oasys::BoolOpt("enabled", server->enabled_ptr(),
                                "Whether or not to enable the api server"));
    
    bind_var(new oasys::InAddrOpt("local_addr",  server->local_addr_ptr(),
                                  "addr",
                                  "The IP address on which the "
                                  "API Server will listen. "
                                  "Default is localhost."));
    
    bind_var(new oasys::UInt16Opt("local_port", server->local_port_ptr(),
                                  "port",
                                  "The TCP port on which the "
                                  "API Server will listen. "
                                  "Default is 5010."));
}

} // namespace dtn
