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

#include <climits>
#include <oasys/serialize/TclListSerialize.h>
#include <oasys/thread/Notifier.h>
#include <oasys/util/StringBuffer.h>

#include "RegistrationCommand.h"
#include "CompletionNotifier.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "reg/LoggingRegistration.h"
#include "reg/RegistrationTable.h"
#include "reg/TclRegistration.h"

namespace dtn {

RegistrationCommand::RegistrationCommand()
    : TclCommand("registration")
{
    add_to_help("add <logger | tcl> <endpoint> <args..>", "add a registration");
    add_to_help("tcl <reg id> <cmd <args ...>", "add a tcl registration");
    add_to_help("del <reg id>", "delete a registration");
    add_to_help("list", "list all of the registrations");
    add_to_help("dump_tcl <reg id>", "dump a tcl representation of the reg");
}

int
RegistrationCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }
    const char* op = argv[1];

    if (strcmp(op, "list") == 0 || strcmp(op, "dump") == 0) {
        oasys::StringBuffer buf;
        BundleDaemon::instance()->reg_table()->dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;
        
    } else if (strcmp(op, "dump_tcl") == 0) {
        if (argc < 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }
        
        const RegistrationTable* regtable =
            BundleDaemon::instance()->reg_table();
        
        const char* regid_str = argv[2];
        int regid = atoi(regid_str);
        
        Registration* reg = regtable->get(regid);
        if (!reg) {
            resultf("no registration exists with id %d", regid);
            return TCL_ERROR;
        }

        Tcl_Obj* result = Tcl_NewListObj(0, 0);
        oasys::TclListSerialize s(interp, result,
                                  oasys::Serialize::CONTEXT_LOCAL, 0);
        reg->serialize(&s);
        set_objresult(result);
        return TCL_OK;
        
    } else if (strcmp(op, "add") == 0) {
        // registration add <logger|tcl> <demux> <args...>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        const char* type = argv[2];
        const char* eid_str = argv[3];
        EndpointIDPattern eid(eid_str);
        
        if (!eid.valid()) {
            resultf("error in registration add %s %s: invalid endpoint id",
                    type, eid_str);
            return TCL_ERROR;
        }

        Registration* reg = NULL;
        if (strcmp(type, "logger") == 0) {
            reg = new LoggingRegistration(eid);
            
        } else if (strcmp(type, "tcl") == 0) {
            reg = new TclRegistration(eid, interp);
            
        } else {
            resultf("error in registration add %s %s: invalid type",
                    type, eid_str);
            return TCL_ERROR;
        }

        ASSERT(reg);

        BundleDaemon::post_and_wait(
            new RegistrationAddedEvent(reg, EVENTSRC_ADMIN),
            CompletionNotifier::notifier());
        
        resultf("%d", reg->regid());
        return TCL_OK;
        
    } else if (strcmp(op, "del") == 0) {
        const RegistrationTable* regtable =
            BundleDaemon::instance()->reg_table();

        const char* regid_str = argv[2];
        int regid = atoi(regid_str);

        Registration* reg = regtable->get(regid);
        if (!reg) {
            resultf("no registration exists with id %d", regid);
            return TCL_ERROR;
        }

        BundleDaemon::post_and_wait(new RegistrationRemovedEvent(reg),
                                    CompletionNotifier::notifier());
        return TCL_OK;

    } else if (strcmp(op, "tcl") == 0) {
        // registration tcl <regid> <cmd> <args...>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }

        const char* regid_str = argv[2];
        int regid = atoi(regid_str);

        const RegistrationTable* regtable =
            BundleDaemon::instance()->reg_table();

        TclRegistration* reg = (TclRegistration*)regtable->get(regid);

        if (!reg) {
            resultf("no matching registration for %d", regid);
            return TCL_ERROR;
        }
        
        return reg->exec(argc - 3, &argv[3], interp);
    }

    resultf("invalid registration subcommand '%s'", op);
    return TCL_ERROR;
}

} // namespace dtn
