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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef ACS_ENABLED

#include <oasys/util/StringBuffer.h>

#include "AcsCommand.h"

#include "bundling/BundleDaemonACS.h"


namespace dtn {

AcsCommand::AcsCommand()
    : TclCommand("acs")
{
    bind_var(new oasys::BoolOpt("enabled",
                                &BundleDaemonACS::params_.acs_enabled_,
                                "Enable Aggregate Custody Signal processing "
                                "(default is true)"));

    bind_var(new oasys::UIntOpt("delay",
                                &BundleDaemonACS::params_.acs_delay_,
                                "seconds",
                                "Number of seconds to accumulate bundles before issuing "
                                "an Aggregate Custody Signal"));

    bind_var(new oasys::UIntOpt("size",
                                &BundleDaemonACS::params_.acs_size_,
                                "bytes",
                                "Maximum size of an Aggregate Custody Signal payload "
                                "(0 = infinite)"));
    
    add_to_help("dump", 
                "Dump all Aggregate Custody Signal parameters");

    add_to_help("override <eid> <enabled> [<delay> <size>]", 
                "Set override Aggregate Custody Signal parameters for a custodian/destination EID");

    add_to_help("del <eid>", 
                "Delete override Aggregate Custody Signal parameters for a custodian/destination EID");

}

int
AcsCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a acs subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    
    if (strcmp(cmd, "override") == 0) {
        // acs acs_set <eid> <acs_enabled> [<acs_delay> <acs_size>] 
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, 6);
            return TCL_ERROR;
        }

        EndpointIDPattern pat(argv[2]);
        if (!pat.valid()) {
            resultf("invalid <eid> endpoint id pattern '%s'\n"
                    "usage: acs override <eid> <enabled> "
                    "[<delay> <size>]",
                    argv[2]);
            return TCL_ERROR;
        }

        // enable ACS for the destination?
        bool acs_enabled;
        const char* val = argv[3];
        int len = strlen(val);
        if ((strncasecmp(val, "t", len) == 0)    ||
            (strncasecmp(val, "true", len) == 0) ||
            (strncasecmp(val, "1", len) == 0))
        {
            acs_enabled = true;
        }
        else if ((strncasecmp(val, "f", len) == 0)     ||
                 (strncasecmp(val, "false", len) == 0) ||
                 (strncasecmp(val, "0", len) == 0))
        {
            acs_enabled = false;
        }
        else
        {
            resultf("invalid <enabled> value (true or false) '%s'\n"
                    "usage: acs override <eid> <enabled> "
                    "[<delay> <size>]",
                    argv[3]);
            return TCL_ERROR;
        }

        u_int acs_delay = 0;
        u_int acs_size = 0;

        // only check the other values if enabled
        if (acs_enabled) {
            if (argc != 6) {
                resultf("wrong number of arguments to 'acs override' - "
                        "all 4 required if enabled is true\n"
                        "usage: acs override <eid> <enabled> "
                        "[<delay> <size>]");
                return TCL_ERROR;
            }

            val = argv[4];
            len = strlen(val);
            char* endptr = 0;

            acs_delay = strtoul(val, &endptr, 0);
            if (endptr != (val + len)) {
                resultf("invalid <delay> value '%s'\n"
                        "usage: acs override <eid> <enabled> "
                        "[<delay> <size>]",
                        val);
                return TCL_ERROR;
            }

            val = argv[5];
            len = strlen(val);
            endptr = 0;

            acs_size = strtoul(val, &endptr, 0);
            if (endptr != (val + len)) {
                resultf("invalid <size> value '%s'\n"
                        "usage: acs override <eid> <enabled> "
                        "[<delay> <size>]",
                        val);
                return TCL_ERROR;
            }
        }

        // made it through all of the checks; apply the values 
        BundleDaemonACS::instance()->set_route_acs_params(pat, acs_enabled, acs_delay, acs_size);
        
        return TCL_OK;
    }

    else if (strcmp(cmd, "del") == 0) {
        // acs del <eid>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        EndpointIDPattern pat(argv[2]);
        if (!pat.valid()) {
            resultf("invalid <eid> endpoint id pattern '%s'\n"
                    "usage: acs del <eid>",
                    argv[2]);
            return TCL_ERROR;
        }

        // made it through all of the checks; delete the acs specific values
        if (-1 == BundleDaemonACS::instance()->delete_route_acs_params(pat)) {
            resultf("Warning - No ACS parameters found for %s", argv[2]);
        }
        
        return TCL_OK;
    }

    else if (strcmp(cmd, "dump") == 0) {
        // acs acs_dump
        if (argc != 2) {
            wrong_num_args(argc, argv, 2, 2, 2);
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;
        BundleDaemonACS::instance()->dump_acs_params(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    else {
        resultf("unimplemented acs subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

} // namespace dtn

#endif // ACS_ENABLED
