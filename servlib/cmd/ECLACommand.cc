/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)

#include <oasys/io/NetUtils.h>

#include "ECLACommand.h"
#include "conv_layers/ConvergenceLayer.h"
#include "conv_layers/ExternalConvergenceLayer.h"

namespace dtn {

static ExternalConvergenceLayer* ecl = NULL;

ECLACommand::ECLACommand() 
    : TclCommand("ecla")
{
    bind_var( new oasys::StringOpt("schema",
              &ExternalConvergenceLayer::schema_,
              "ECLA schema location",
              "The location of the ECLA schema file.") );
    
    bind_var( new oasys::InAddrOpt("server_addr",
              &ExternalConvergenceLayer::server_addr_,
              "ECLA server address",
              "The address to listen for external CLAs on") );
    
    bind_var( new oasys::UInt16Opt("server_port",
              &ExternalConvergenceLayer::server_port_,
              "ECLA server port",
              "The port to listen for external CLAs on") );
    
    bind_var( new oasys::BoolOpt("create_discovered_links",
              &ExternalConvergenceLayer::create_discovered_links_,
              "Whether external CLAs should create discovered links") );
    
    bind_var( new oasys::BoolOpt("discovered_prev_hop_header",
              &ExternalConvergenceLayer::discovered_prev_hop_header_,
              "Whether discovered links get prev-hop headers") );
}

int
ECLACommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("Need an ecla subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "start") == 0) {
        if (ecl) {
            resultf("ExternalConvergenceLayer already started");
            return TCL_ERROR;
        }
        
        ecl = new ExternalConvergenceLayer();
        ecl->start();        
        ConvergenceLayer::add_clayer(ecl);
    }
    
    else {
        resultf("Unimplemented ecla subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}
    
} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_CL_ENABLED
