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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdlib.h>

#include "Node.h"
#include "NodeCommand.h"
#include "SimConvergenceLayer.h"
#include "SimRegistration.h"
#include "Simulator.h"
#include "Topology.h"
#include "TrAgent.h"
#include "bundling/Bundle.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/EndpointID.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "reg/RegistrationTable.h"

using namespace dtn;

namespace dtnsim {

NodeCommand::NodeCommand(Node* node)
    : TclCommand(node->name()), node_(node),
      storage_cmd_(node->storage_config())
{
}

int
NodeCommand::exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp)
{
    int argc = objc;
    const char* argv[objc];
    for (int i = 0; i < objc; ++i) {
        argv[i] = Tcl_GetStringFromObj(objv[i], NULL);
    }
    
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        add_to_help("bundle", "XXX");
        add_to_help("param", "XXX");
        add_to_help("link", "XXX");
        add_to_help("route", "XXX");
        add_to_help("tragent", "XXX");
        add_to_help("registration", "XXX");
        return TCL_ERROR;
    }

    // for all commands and their side-effects, install the node as
    // the "singleton" BundleDaemon instance, and store the command
    // time in the node so any posted events happen in the future
    node_->set_active();

    const char* cmd = argv[1];
    const char* subcmd = NULL;
    if (objc >= 3) {
        subcmd = argv[2];
    }

    // first we need to handle the set command since it's normally
    // handled by the TclCommand base class
    if (subcmd != NULL && strcmp(subcmd, "set") == 0) {
        TclCommand* cmdobj = NULL;
        if      (strcmp(cmd, "bundle")  == 0) cmdobj = &bundle_cmd_;
        else if (strcmp(cmd, "link")    == 0) cmdobj = &link_cmd_;
        else if (strcmp(cmd, "param")   == 0) cmdobj = &param_cmd_;
        else if (strcmp(cmd, "route")   == 0) cmdobj = &route_cmd_;
        else if (strcmp(cmd, "storage") == 0) cmdobj = &storage_cmd_;
        else 
        {
            resultf("node: unsupported subcommand %s", cmd);
            return TCL_ERROR;
        }
        
        return cmdobj->cmd_set(objc - 1, objv + 1, interp);
    }
        
    if (strcmp(cmd, "bundle") == 0)
    {
        return bundle_cmd_.exec(objc - 1, objv + 1, interp);
    }
    else if (strcmp(cmd, "link") == 0)
    {
        return link_cmd_.exec(argc - 1, argv + 1, interp);
    }
    else if (strcmp(cmd, "param") == 0)
    {
        return param_cmd_.exec(argc - 1, argv + 1, interp);
    }
    else if (strcmp(cmd, "route") == 0)
    {
        return route_cmd_.exec(argc - 1, argv + 1, interp);
    }
    else if (strcmp(cmd, "storage") == 0)
    {
        if (subcmd != NULL && !strcmp(subcmd, "set")) {
            return storage_cmd_.cmd_set(objc - 1, objv + 1, interp);
        }
        return storage_cmd_.exec(argc - 1, argv + 1, interp);
    }
    else if (strcmp(cmd, "registration") == 0)
    {
        if (strcmp(subcmd, "add") == 0) {
            // <node> registration add <eid> [<expiration>]
            const char* eid_str = argv[3];
            EndpointIDPattern eid(eid_str);

            if (!eid.valid()) {
                resultf("error in node registration add %s: "
                        "invalid demux eid", eid_str);
                return TCL_ERROR;
            }

            u_int32_t expiration = 10;

            if (argc > 4) {
            	expiration = atoi(argv[4]);
            }

            Registration* r = new SimRegistration(node_, eid, expiration);
            RegistrationAddedEvent* e =
                new RegistrationAddedEvent(r, EVENTSRC_APP); // Was EVENTSRC_ADMIN

            node_->post_event(e);
            
            return TCL_OK;
        } else if (strcmp(subcmd, "list") == 0 || strcmp(subcmd, "dump") == 0) {
            oasys::StringBuffer buf;
            node_->reg_table()->dump(&buf);
            set_result(buf.c_str());
            return TCL_OK;
        
        } else if (strcmp(subcmd, "del") == 0) {
            const RegistrationTable* regtable = node_->reg_table();

            const char* regid_str = argv[2];
            int regid = atoi(regid_str);

            Registration* reg = regtable->get(regid);
            if (!reg) {
                resultf("no registration exists with id %d", regid);
                return TCL_ERROR;
            }

            node_->post_event(new RegistrationRemovedEvent(reg));
            return TCL_OK;

        }
        resultf("node registration: unsupported subcommand %s", subcmd);
        return TCL_ERROR;
    }
    else if (strcmp(cmd, "tragent") == 0)
    {
        // <node> tragent <src> <dst> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 3, 5, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* src = argv[2];
        const char* dst = argv[3];

        // see if src/dest are node names, in which case we use its
        // local eid as the source address
        EndpointID src_eid;
        Node* src_node = Topology::find_node(src);
        if (src_node) {
            src_eid.assign(src_node->local_eid());
        } else {
            src_eid.assign(src);
            if (!src_eid.valid()) {
                resultf("node tragent: invalid src eid %s", src);
                return TCL_ERROR;
            }
        }
        
        EndpointID dst_eid;
        Node* dst_node = Topology::find_node(dst);
        if (dst_node) {
            dst_eid.assign(dst_node->local_eid());
        } else {
            dst_eid.assign(dst);
            if (!dst_eid.valid()) {
                resultf("node tragent: invalid dst eid %s", dst);
                return TCL_ERROR;
            }
        }
        
        TrAgent* a = TrAgent::init(src_eid, dst_eid,
                                   argc - 4, argv + 4);
        if (!a) {
            resultf("error in tragent config");
            return TCL_ERROR;
        }
        
        return TCL_OK;

    } else {
        resultf("node: unsupported subcommand %s", cmd);
        return TCL_ERROR;
    }
}

} // namespace dtnsim
