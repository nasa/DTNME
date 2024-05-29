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
#include <third_party/oasys/serialize/TclListSerialize.h>
#include <third_party/oasys/thread/Notifier.h>
#include <third_party/oasys/util/StringBuffer.h>

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

    bind_var(new oasys::BoolOpt("suppress_duplicates",
                                &BundleDaemon::params_.dzdebug_reg_delivery_cache_enabled_,
                                "Suppress delivery of duplicate bundles to Registrations (default is true)"));
                                
}

RegistrationCommand::RegistrationCommand(const char* cmd_str)
    : TclCommand(cmd_str)
{
    add_to_help("add <logger | tcl> <endpoint> <args..>", "add a registration");
    add_to_help("tcl <reg id> <cmd <args ...>", "add a tcl registration");
    add_to_help("del <reg id>", "delete a registration");
    add_to_help("list", "list all of the registrations");
    add_to_help("dump_tcl <reg id>", "dump a tcl representation of the reg");

    bind_var(new oasys::BoolOpt("suppress_duplicates",
                                &BundleDaemon::params_.dzdebug_reg_delivery_cache_enabled_,
                                "Suppress delivery of duplicate bundles to Registrations (default is true)"));
                                
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
        
        SPtr_Registration sptr_reg = regtable->get(regid);
        if (!sptr_reg) {
            resultf("no registration exists with id %d", regid);
            return TCL_ERROR;
        }

        Tcl_Obj* result = Tcl_NewListObj(0, 0);
        oasys::TclListSerialize s(interp, result,
                                  oasys::Serialize::CONTEXT_LOCAL, 0);
        sptr_reg->serialize(&s);
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
        SPtr_EIDPattern sptr_eid = BD_MAKE_PATTERN(eid_str);
        
        if (!sptr_eid->valid()) {
            resultf("error in registration add %s %s: invalid endpoint id",
                    type, eid_str);
            return TCL_ERROR;
        }

        SPtr_Registration sptr_reg;
        if (strcmp(type, "logger") == 0) {
            sptr_reg = std::make_shared<LoggingRegistration>(sptr_eid);
            
        } else if (strcmp(type, "tcl") == 0) {
            sptr_reg = std::make_shared<TclRegistration>(sptr_eid, interp);
            
        } else {
            resultf("error in registration add %s %s: invalid type",
                    type, eid_str);
            return TCL_ERROR;
        }

        ASSERT(sptr_reg);

        RegistrationAddedEvent* event_to_post;
        event_to_post = new RegistrationAddedEvent(sptr_reg, EVENTSRC_ADMIN);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post_and_wait(sptr_event_to_post, CompletionNotifier::notifier());
        
        resultf("%d", sptr_reg->regid());
        return TCL_OK;
        
    } else if (strcmp(op, "del") == 0) {
        const RegistrationTable* regtable =
            BundleDaemon::instance()->reg_table();

        const char* regid_str = argv[2];
        int regid = atoi(regid_str);

        SPtr_Registration sptr_reg = regtable->get(regid);
        if (!sptr_reg) {
            resultf("no registration exists with id %d", regid);
            return TCL_ERROR;
        }

        RegistrationRemovedEvent* event_to_post;
        event_to_post = new RegistrationRemovedEvent(sptr_reg);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post_and_wait(sptr_event_to_post, CompletionNotifier::notifier());
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

        SPtr_Registration sptr_reg = regtable->get(regid);
        TclRegistration* tcl_reg = dynamic_cast<TclRegistration*>(sptr_reg.get());
        if (tcl_reg == NULL) {
            resultf("Error casting registration id %d as a TCLRegistration", regid);
            return TCL_ERROR;
        }

        if (!sptr_reg) {
            resultf("no matching registration for %d", regid);
            return TCL_ERROR;
        }
        
        return tcl_reg->exec(argc - 3, &argv[3], interp);
    }

    resultf("invalid registration subcommand '%s'", op);
    return TCL_ERROR;
}

} // namespace dtn
