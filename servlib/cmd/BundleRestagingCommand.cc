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

#ifdef BARD_ENABLED


#include <inttypes.h>

#include <third_party/oasys/util/StringBuffer.h>

#include "BundleRestagingCommand.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleArchitecturalRestagingDaemon.h"
#include "bundling/BARDNodeStorageUsage.h"

namespace dtn {

//----------------------------------------------------------------------
BundleRestagingCommand::BundleRestagingCommand()
    : TclCommand("bard") 
{
    add_to_help("add_quota <quota type> <naming scheme> <node number/name> <internal bundles> <internal bytes>\n"
                "                 [<restage link name> <auto reload> <external bundles> <external bytes>]", 
                "add or update a node quota entry; where:\n"
                "        <quota type>        - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>     - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>  - 'ipn' and 'imc' require a number and 'dtn' requires a name\n"
                "        <internal bundles>  - max number of bundles allowed in internal storage (0=no max)\n"
                "        <internal bytes>    - max number of payload bytes allowed in internal storage (0=no max)\n\n"
                "    (if no other parameters are provided then bundle refusal will be attempted)\n\n"
                "        <restage link name> - name of the restage CL to use for offloading to external storage\n"
                "        <auto reload>       - whether to reload bundles when usage drops to 20% ('true' or 'false')\n"
                "        <external bundles>  - max number of bundles allowed in external storage (0=no max)\n"
                "        <external bytes>    - max number of payload bytes allowed in external storage (0=no max)\n\n"
                "    (max number values may be specified exactly or with a magnitude character of K, M, G or T;\n"
                "     where K = x1000, M = x1000000, etc.   example: 12G = 12000000000)");

    add_to_help("del_quota <quota type> <naming scheme> <node number/name>",
                "delete a node quota entry; where:\n"
                "        <quota type>        - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>     - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>  - 'ipn' and 'imc' require a number and 'dtn' requires a name");

    add_to_help("unlimited_quota <quota type> <naming scheme> <node number/name>",
                "clear limits of a node quota entry and leave it in the database; where:\n"
                "        <quota type>           - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>        - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>     - 'ipn' and 'imc' require a number and 'dtn' requires a name\n"
                "   (forces override of a quota in a startup config file upon restart)");

    add_to_help("force_restage <quota type> <naming scheme> <node number/name>",
                "force over-quota bundles of specified type to be restaged if quota is configured for restaging; where:\n"
                "        <quota type>        - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>     - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>  - 'ipn' and 'imc' require a number and 'dtn' requires a name");


    add_to_help("quotas [exact]", "list all quota definitions using magnitude values or exact values if specified");

    add_to_help("usage [exact]", "list usage data for all nodes with and without quotas using magnitude values or exact values if specified");

    add_to_help("dump", "list all usage and reserved data for all nodes with and without quota definitions");

    add_to_help("rescan", "rescan external storage (best done while DTN daemon is mostly idle)");

    add_to_help("del_all_restaged_bundles",
                "delete all restaged bundles from all restage locations\n");

    add_to_help("del_restaged_bundles <quota type> <naming scheme> <node number/name>",
                "delete restaged bundles for a specific quota type; where:\n"
                "        <quota type>           - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>        - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>     - 'ipn' and 'imc' require a number and 'dtn' requires a name\n"
                );

    add_to_help("reload <quota type> <naming scheme> <node number/name> [<new expiration secs>] [<new dest EID>]",
                "attempt to reload restaged bundles for a specific quota type; where:\n"
                "        <quota type>           - 'dst' = destination node or 'src' = source node\n"
                "        <naming scheme>        - 'ipn', 'imc' or 'dtn'\n"
                "        <node number/name>     - 'ipn' and 'imc' require a number and 'dtn' requires a name\n"
                "   optional:\n"
                "        <new expiration secs>  - adjust expiration if needed to provide a minimum time to live\n"
                "        <new dest EID>         - changes the bundle destination EID\n"
                );

    add_to_help("reload_all [<new expiration secs>]",
                "attempt to reload all restaged bundles; where:\n"
                "        <new expiration secs>  - adjust expiration if needed to provide a minimum time to live\n"
                );



}

//----------------------------------------------------------------------
int
BundleRestagingCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    if (argc < 2) {
        resultf("need a quota subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    
    if (0 == strcmp(cmd, "quotas")) {
        return process_quotas(argc, argv);

    } else if (0 == strcmp(cmd, "usage")) {
        return process_usage(argc, argv);

    } else if (0 == strcmp(cmd, "dump")) {
        return process_dump(argc, argv);

    } else if (0 == strcmp(cmd, "add_quota")) {
        return process_add_quota(argc, argv);

    } else if (0 == strcmp(cmd, "del_quota")) {
        return process_delete_quota(argc, argv);

    } else if (0 == strcmp(cmd, "unlimited_quota")) {
        return process_unlimited_quota(argc, argv);

    } else if (0 == strcmp(cmd, "force_restage")) {
        return process_force_restage(argc, argv);

    } else if (0 == strcmp(cmd, "rescan")) {
        return process_rescan(argc, argv);

    } else if (0 == strcmp(cmd, "del_restaged_bundles")) {
        return process_del_restaged_bundles(argc, argv);

    } else if (0 == strcmp(cmd, "del_all_restaged_bundles")) {
        return process_del_all_restaged_bundles(argc, argv);

    } else if (0 == strcmp(cmd, "reload")) {
        return process_reload(argc, argv);

    } else if (0 == strcmp(cmd, "reload_all")) {
        return process_reload_all(argc, argv);

    }


    resultf("invalid quota subcommand %s", argv[1]);
    return TCL_ERROR;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_quotas(int argc, const char** argv)
{
    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    bool exact = false;
    if (argc > 2) {
        exact = (strcmp(argv[2], "exact") == 0);
    }

    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    oasys::StringBuffer buf;

    if (exact) {
        sptr_bard->bardcmd_quotas_exact(buf);
    } else {
        sptr_bard->bardcmd_quotas(buf);
    }

    set_result(buf.c_str());

    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_usage(int argc, const char** argv)
{
    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    bool exact = false;
    if (argc > 2) {
        exact = (strcmp(argv[2], "exact") == 0);
    }

    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    oasys::StringBuffer buf;

    if (exact) {
        sptr_bard->bardcmd_usage_exact(buf);
    } else {
        sptr_bard->bardcmd_usage(buf);
    }

    set_result(buf.c_str());

    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_dump(int argc, const char** argv)
{
    if (argc > 2) {
        wrong_num_args(argc, argv, 2, 2, 2);
        return TCL_ERROR;
    }

    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    oasys::StringBuffer buf;

    sptr_bard->bardcmd_dump(buf);
    
    set_result(buf.c_str());

    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_add_quota(int argc, const char** argv)
{
    if ((argc != 7) && (argc != 11)) {
        resultf("wrong number of arguments to 'quota add' expected 7 or 11, got %d", argc);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;
    size_t internal_bundles = 0;
    size_t internal_bytes = 0;
    // optional
    bool refuse_bundle = true;
    bool auto_reload = false;
    std::string restage_link_name;
    size_t external_bundles = 0;
    size_t external_bytes = 0;


    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    // internal bundles
    ++idx;
    if (!parse_number_value(argv[idx], internal_bundles)) {
        resultf("invalid internal bundles: %s", argv[idx]);
        return TCL_ERROR;
    }

    // internal bytes
    ++idx;
    if (!parse_number_value(argv[idx], internal_bytes)) {
        resultf("invalid internal bytes: %s", argv[idx]);
        return TCL_ERROR;
    }

    if (argc == 11) {
        // restaging instead of refusing if we get to here
        refuse_bundle = false;

        // restage link name
        ++idx;
        restage_link_name = argv[idx];

        // TODO: validate the link name??

        // auto reload
        ++idx;
        if (!parse_bool(argv[idx], auto_reload)) {
            resultf("invalid auto reload: %s (must be true or false)", argv[idx]);
            return TCL_ERROR;
        }

        
        // external bundles
        ++idx;
        if (!parse_number_value(argv[idx], external_bundles)) {
            resultf("invalid internal bundles: %s", argv[idx]);
            return TCL_ERROR;
        }


        // external bytes
        ++idx;
        if (!parse_number_value(argv[idx], external_bytes)) {
            resultf("invalid internal bytes: %s", argv[idx]);
            return TCL_ERROR;
        }
    }

    // add the quota in the BundleArchitecturalRestagingDaemon
    std::string warning_msg;  // if new quota is less than current bundles in local storage
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_add_quota(quota_type, scheme, nodename,
                             internal_bundles, internal_bytes, refuse_bundle,
                             restage_link_name, auto_reload, 
                             external_bundles, external_bytes,
                             warning_msg)) {
        resultf("error adding quota - check log file for reason");
        return TCL_ERROR;
    }

    if (warning_msg.length() > 0) {
        resultf("%s", warning_msg.c_str());
    } else {
        resultf("success");
    }

    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_delete_quota(int argc, const char** argv)
{
    if (argc != 5) {
        wrong_num_args(argc, argv, 2, 5, 5);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;

    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    // delete the quota in the BundleArchitecturalRestagingDaemon
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_delete_quota(quota_type, scheme, nodename)) {
        resultf("error deleting quota - check log file for reason");
        return TCL_ERROR;
    }

    resultf("success");
    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_unlimited_quota(int argc, const char** argv)
{
    // add_to_help("unlimited <quota type> <naming scheme> <node number/name>",
    if (argc != 5) {
        wrong_num_args(argc, argv, 2, 5, 5);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;

    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // valid input was a good node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    // add/update the quota in the BundleArchitecturalRestagingDaemon
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_unlimited_quota(quota_type, scheme, nodename)) {
        resultf("error setting unlimited quota - check log file for reason");
        return TCL_ERROR;
    }

    resultf("success");
    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_force_restage(int argc, const char** argv)
{
    if (argc != 5) {
        wrong_num_args(argc, argv, 2, 5, 5);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;

    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    // inform the BundleArchitecturalRestagingDaemon
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_force_restage(quota_type, scheme, nodename)) {
        resultf("specified bundles not configured for restaging");
        return TCL_ERROR;
    }

    resultf("success - restage initiated");
    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_rescan(int argc, const char** argv)
{
    if (argc != 2) {
        wrong_num_args(argc, argv, 2, 2, 2);
        return TCL_ERROR;
    }

    // request rescan from the BARD
    std::string result_str;
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_rescan(result_str)) {
        resultf("%s", result_str.c_str());
        return TCL_ERROR;
    }

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
BundleRestagingCommand::process_del_restaged_bundles(int argc, const char** argv)
{
   //  add_to_help("del_restaged_bundles <quota type> <naming scheme> <node number/name>",

    if ((argc < 5) || (argc > 7)) {
        wrong_num_args(argc, argv, 2, 5, 5);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;

    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    // issue the request to the BARD
    std::string result_str;
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_del_restaged_bundles(quota_type, scheme, nodename, result_str)) {
        resultf("%s", result_str.c_str());
        return TCL_ERROR;
    }

    resultf("%s", result_str.c_str());
    return TCL_OK;
}


//----------------------------------------------------------------------
int
BundleRestagingCommand::process_del_all_restaged_bundles(int argc, const char** argv)
{
   //  add_to_help("del_all_restaged_bundles",

    if (argc != 2) {
        wrong_num_args(argc, argv, 2, 2, 2);
        return TCL_ERROR;
    }

    // issue the request to the BARD
    std::string result_str;
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_del_all_restaged_bundles(result_str)) {
        resultf("%s", result_str.c_str());
        return TCL_ERROR;
    }

    resultf("%s", result_str.c_str());
    return TCL_OK;
}


//----------------------------------------------------------------------
int
BundleRestagingCommand::process_reload(int argc, const char** argv)
{
   //  add_to_help("reload <quota type> <naming scheme> <node number/name> [<new expiration secs> <new dest EID>]",

    if ((argc < 5) || (argc > 7)) {
        wrong_num_args(argc, argv, 2, 5, 7);
        return TCL_ERROR;
    }

    bard_quota_type_t quota_type = BARD_QUOTA_TYPE_UNDEFINED;
    bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    size_t node_number = 0;
    std::string nodename;
    // optional
    size_t new_expiration = 0;
    std::string new_dest_eid;

    // parameters start at index nummber 2
    int idx = 2;

    // quota type (src/dst)
    if (!parse_quota_type(argv[idx], quota_type)) {
        return TCL_ERROR;
    }

    // naming scheme (ipn, dtn or imc)
    ++idx;
    if (!parse_naming_scheme(argv[idx], scheme)) {
        return TCL_ERROR;
    }

    // node number or node name
    ++idx;
    nodename = argv[idx]; // keep node number as both a string and a numeric value
    if ((scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (scheme == BARD_QUOTA_NAMING_SCHEME_IMC)) {
        // node number
        if (!parse_node_number(argv[idx], node_number)) {
            return TCL_ERROR;
        }
    }

    std::string tmp_str;
    size_t tmp_num;
    bool have_new_exp = false;
    bool have_new_dest = false;

    while (++idx < argc) {
        if ((*argv[idx] == 'i') || (*argv[idx] == 'd')) {
            EndpointID eid;
            bool is_valid = eid.assign(argv[idx]);

            if (!is_valid) {
                resultf("invalid new destination EID: %s", argv[idx]);
                return TCL_ERROR;
            }

            if (have_new_dest) {
                resultf("error - only one new destination EID may be specified");
                return TCL_ERROR;
            }

            new_dest_eid = argv[idx];
            have_new_dest = true;

        } else {
            if (!parse_number_value(argv[idx], tmp_num)) {
                resultf("invalid new expiration seconds: %s", argv[idx]);
                return TCL_ERROR;
            }

            if (have_new_exp) {
                resultf("error - only one new expiration seconds may be specified");
                return TCL_ERROR;
            }

            new_expiration = tmp_num;
            have_new_exp = true;
        }
    }

    // request reload from the BARD
    std::string result_str;
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_reload(quota_type, scheme, nodename, new_expiration, new_dest_eid, result_str)) {
        resultf("%s", result_str.c_str());
        return TCL_ERROR;
    }

    resultf("%s", result_str.c_str());
    return TCL_OK;
}


//----------------------------------------------------------------------
int
BundleRestagingCommand::process_reload_all(int argc, const char** argv)
{
   //  add_to_help("reload_all [<new expiration secs>]",

    if ((argc < 2) || (argc > 3)) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    // optional
    size_t new_expiration = 0;

    // parameters start at index nummber 2
    int idx = 2;

    if (argc > 2) {
        if (!parse_number_value(argv[idx], new_expiration)) {
            resultf("invalid new expiration seconds: %s", argv[idx]);
            return TCL_ERROR;
        }
    }

    // request reload from the BARD
    std::string result_str;
    SPtr_BundleArchitecturalRestagingDaemon sptr_bard = BundleDaemon::instance()->bundle_restaging_daemon();

    if (!sptr_bard->bardcmd_reload_all(new_expiration, result_str)) {
        resultf("%s", result_str.c_str());
        return TCL_ERROR;
    }

    resultf("%s", result_str.c_str());
    return TCL_OK;
}


//----------------------------------------------------------------------
bool
BundleRestagingCommand::parse_node_number(const char* val_str, size_t& retval)
{
    retval = 0;

    if (val_str == nullptr) {
        resultf("invalid node number: null");
        return false;
    }

    uint64_t bigval = 0;   // in case size_t is 32 bits
    char* endp = nullptr;

    bigval = strtoull(val_str, &endp, 0);

    if (*endp != '\0') {
        // error - value had non-numeric characters
        resultf("invalid node number: %s", val_str);
        return false;
    }

    retval = bigval;
    return (bigval == retval);  // assume a val too big for 32 bits would return false - not verified yet
}

//----------------------------------------------------------------------
bool
BundleRestagingCommand::parse_number_value(const char* val_str, size_t& retval)
{
    retval = 0;

    if (val_str == nullptr) {
        return false;
    }

    uint64_t bigval = 0;   // in case size_t is 32 bits
    char* endp = nullptr;

    bigval = strtoull(val_str, &endp, 0);

    // if a magnitude character was provided then multiple as specified
    if (*endp != '\0') {
        switch (*endp) {
            case 'B':
            case 'b':
                break;

            case 'K':
            case 'k':
                bigval *= 1000;
                break;

            case 'M':
            case 'm':
                bigval *= 1000000;
                break;

            case 'G':
            case 'g':
                bigval *= 1000000000;
                break;

            case 'T':
            case 't':
                bigval *= 1000000000000;
                break;

            default:
                return false;
        }
    }

    retval = bigval;
    return (bigval == retval);  // assume a val too big for 32 bits would return false - not verified yet
}


//----------------------------------------------------------------------
bool
BundleRestagingCommand::parse_bool(const char* val_str, bool& retval)
{
    retval = false;

    if (val_str == nullptr) {
        return false;
    }

    if ((val_str[0] == 't') || (val_str[0] == 'T') || (val_str[0] == '1')) {
        retval = true;
    } else if ((val_str[0] == 'f') || (val_str[0] == 'F') || (val_str[0] == '0')) {
        //retval = false;
    } else {
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
BundleRestagingCommand::parse_quota_type(const char* val_str, bard_quota_type_t& retval)
{
    // valid quota type strings are "src" and "dst" 

    retval = BARD_QUOTA_TYPE_DST;

    if (val_str == nullptr) {
        resultf("invalid quota type: null  (valid options: 'src' or 'dst')");
        return false;
    }

    bool result = true;

    if (0 == strcmp(val_str, "src")) {
        retval = BARD_QUOTA_TYPE_SRC;
    } else if (0 != strcmp(val_str, "dst")) {
        resultf("invalid quota type: %s  (valid options: 'src' or 'dst')", val_str);
        result = false;
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleRestagingCommand::parse_naming_scheme(const char* val_str, bard_quota_naming_schemes_t& retval)
{
    // valid naming scheme strings are "ipn", "dtn" and "imc"

    retval = BARD_QUOTA_NAMING_SCHEME_IPN;

    if (val_str == nullptr) {
        resultf("invalid naming scheme: null  (valid options: 'ipn', 'dtn' or 'imc')");
        return false;
    }

    bool result = true;

    if (0 == strcmp(val_str, "dtn")) {
        retval = BARD_QUOTA_NAMING_SCHEME_DTN;
    } else if (0 == strcmp(val_str, "imc")) {
        retval = BARD_QUOTA_NAMING_SCHEME_IMC;
    } else if (0 != strcmp(val_str, "ipn")) {
        resultf("invalid quota type: %s  (valid options: 'ipn', 'dtn' or 'imc')", val_str);
        result = false;
    }

    return result;
}

} // namespace dtn

#endif  // BARD_ENABLED
