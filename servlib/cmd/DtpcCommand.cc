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

#ifdef DTPC_ENABLED

#include <inttypes.h>

#include "DtpcCommand.h"
#include "dtpc/DtpcDaemon.h"
#include "dtpc/DtpcProfileTable.h"
#include "dtpc/DtpcTopicTable.h"
#include <oasys/util/StringBuffer.h>

namespace dtn {

DtpcCommand::DtpcCommand()
    : TclCommand("dtpc") 
{
    add_to_help("local_eid <eid>", "view or set the EID for the DTPC application");

    add_to_help("add_profile <profile id> [arg1=val1 arg2=val2 argN=valN...]",
		"add a transmission profile\n"
	"	valid options:\n"
	"	       <profile id>\n"
	"			The Profile ID number\n"
	"	       <args>:\n"
	"			retran=<number>          - retransmission limit (default=0)\n"
	"			agg_size=<number>        - aggregation size limit in bytes (default=1000)\n"
	"			agg_time=<number>        - aggregation time limit in seconds (default=60)\n"
	"			custody=[true|false]     - request custody transfer (default=false)\n"
	"			lifetime=<number>        - lifetime seconds for bundles (default=864000)\n"
	"			replyto=<EndpointID>     - ReplyTo EID for bundles (default=dtn:none)\n"
	"			priority=[b|n|e|r]       - priority for bundles [bulk,normal,expedited or reserved] (default=bulk)\n"
	"			ecos=<number>            - extended class of service value (default=0)\n"
	"			rpt_rcpt=[true|false]    - request bundle reception report (default=false)\n"
	"			rpt_acpt=[true|false]    - request custody acceptance report (default=false)\n"
	"			rpt_fwrd=[true|false]    - request bundle forward report (default=false)\n"
	"			rpt_dlvr=[true|false]    - request bundle delivery report (default=false)\n"
	"			rpt_dele=[true|false]    - request bundle deletion report (default=false)\n"
    );

    add_to_help("del_profile <profile id>", 
                "delete a transmission profile\n"
	"	valid options:\n"
	"		<profile id>\n"
	"			The Profile ID number\n");

    add_to_help("list_profiles", "list all of the transmission profiles");

    add_to_help("add_topic <topic id> [<\"quoted description\">]",
		"add a DTPC topic\n"
	"	valid options:\n"
	"	       <topic id>\n"
	"			The Topic ID number\n"
	"	       <\"quoted description\">\n"
	"			optional description - enclose in quotes if it has embedded spaces\n"
    );

    add_to_help("del_topic <topic id>", 
                "delete a DTPC topic\n"
	"	valid options:\n"
	"		<topic id>\n"
	"			The Topic ID number\n");

    add_to_help("list_topics", "list all of the DTPC Topics");

    bind_var(new oasys::BoolOpt("require_predefined_topics",
                                &DtpcDaemon::params_.require_predefined_topics_,
                                "Require predefined topics or allow ad hoc topics - "
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("restrict_send",
                                &DtpcDaemon::params_.restrict_send_to_registered_client_,
                                "Restrict send of a topic to the registered client - "
                                "(default is false)"));

    bind_var(new oasys::UInt64Opt("ipn_receive_service_number",
                                  &DtpcDaemon::params_.ipn_receive_service_number,
                                  "svc num",
                                  "IPN DTPC receive service number - "
                                  "(default is 129)"));

    bind_var(new oasys::UInt64Opt("ipn_transmit_service_number",
                                  &DtpcDaemon::params_.ipn_transmit_service_number,
                                  "svc num",
                                  "IPN DTPC transmit service number - "
                                  "(default is 128)"));

}

int
DtpcCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    // dtpc list_profile
    if (strncasecmp("list_profiles", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 6) {
	if (argc > 2) {
	    wrong_num_args(argc, argv, 1, 2, 2);
	    return TCL_ERROR;
	}

	oasys::StringBuffer buf;
	DtpcProfileTable::instance()->list(&buf);
	set_result(buf.c_str());

	return TCL_OK;
    }
    
    // dtpc add_profile <profile id> ...
    else if (strncasecmp("add_profile", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 5) {
	if (argc < 3) {
	    wrong_num_args(argc, argv, 1, 3, 2147483647);
	    return TCL_ERROR;
	}
	
        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        u_int32_t profile_id = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <profile id> value '%s'\n"
                    "usage: dtpc add_profile <profile id> [args] ",
                    val);
            return TCL_ERROR;
        }

	oasys::StringBuffer errmsg;

	// return error string from here
	if (! DtpcProfileTable::instance()->add(&errmsg, profile_id, argc - 3, argv + 3)) {
	    resultf("\nerror adding transmission profile %"PRIu32" - %s\n", profile_id, errmsg.c_str());
	    return TCL_ERROR;
	}
	return TCL_OK;
    }

    // dtpc del_profile <profile id>
    else if (strncasecmp("del_profile", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 5) {
	if (argc != 3) {
	    wrong_num_args(argc, argv, 2, 3, 3);
	    return TCL_ERROR;
	}

        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        u_int32_t profile_id = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <profile id> value '%s'\n"
                    "usage: dtpc add_profile <profile id> [args] ",
                    val);
            return TCL_ERROR;
        }

	if (! DtpcProfileTable::instance()->del(profile_id)) {
	    resultf("error removing transmission profile %"PRIu32, profile_id);
	    return TCL_ERROR;
	}

	return TCL_OK;
    }

    // dtpc list_topic
    else if (strncasecmp("list_topics", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 6) {
	if (argc > 2) {
	    wrong_num_args(argc, argv, 1, 2, 2);
	    return TCL_ERROR;
	}

	oasys::StringBuffer buf;
	DtpcTopicTable::instance()->list(&buf);
	set_result(buf.c_str());

	return TCL_OK;
    }
    
    // dtpc add_topic <topic id> ...
    else if (strncasecmp("add_topic", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 5) {
	if (argc < 3) {
	    wrong_num_args(argc, argv, 1, 3, 4);
	    return TCL_ERROR;
	}
	
        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        u_int32_t topic_id = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <topic id> value '%s'\n"
                    "usage: dtpc add_topic <topic id> [args] ",
                    val);
            return TCL_ERROR;
        }

        const char* description = NULL;
        if (4 == argc) {
            description = argv[3];
        }
	oasys::StringBuffer errmsg;

	// return error string from here
	if (! DtpcTopicTable::instance()->add(&errmsg, topic_id, true, description)) {
	    resultf("\nerror adding DTPC topic %"PRIu32" - %s\n", topic_id, errmsg.c_str());
	    return TCL_ERROR;
	}
	return TCL_OK;
    }

    // dtpc del_topic <topic id>
    else if (strncasecmp("del_topic", argv[1], strlen(argv[1])) == 0 && strlen(argv[1]) >= 5) {
	if (argc != 3) {
	    wrong_num_args(argc, argv, 2, 3, 3);
	    return TCL_ERROR;
	}

        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        u_int32_t topic_id = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <topic id> value '%s'\n"
                    "usage: dtpc add_topic <topic id> [args] ",
                    val);
            return TCL_ERROR;
        }

	if (! DtpcTopicTable::instance()->del(topic_id)) {
	    resultf("error removing DTPC topic %"PRIu32, topic_id);
	    return TCL_ERROR;
	}

	return TCL_OK;
    }

    else if (strcmp(argv[1], "local_eid") == 0) {
        if (argc == 2) {
            // dtpc local_eid
            set_result(DtpcDaemon::instance()->local_eid().c_str());
            return TCL_OK;
            
        } else if (argc == 3) {
            // dtpc local_eid <eid?>
            DtpcDaemon::instance()->set_local_eid(argv[2]);
            if (! DtpcDaemon::instance()->local_eid().valid()) {
                resultf("invalid eid '%s'", argv[2]);
                return TCL_ERROR;
            }
            if (! DtpcDaemon::instance()->local_eid().known_scheme()) {
                resultf("local eid '%s' has unknown scheme", argv[2]);
                return TCL_ERROR;
            }
        } else {
            wrong_num_args(argc, argv, 2, 2, 3);
            return TCL_ERROR;
        }

	return TCL_OK;
    }
    
    resultf("invalid dtpc subcommand %s", argv[1]);
    return TCL_ERROR;
}

} // namespace dtn

#endif // DTPC_ENABLED
