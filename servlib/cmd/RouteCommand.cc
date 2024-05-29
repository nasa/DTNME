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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include <sys/ioctl.h>
#include <net/if.h>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/serialize/XMLSerialize.h>

#include "RouteCommand.h"
#include "CompletionNotifier.h"

#include "contacts/Link.h"
#include "contacts/ContactManager.h"

#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"

#include "naming/IPNScheme.h"

#include "routing/BundleRouter.h"
#include "routing/RouteEntry.h"
#include "routing/ExternalRouter.h"
#include "routing/IMCRouter.h"

namespace dtn {

RouteCommand::RouteCommand()
    : TclCommand("route")
{
    bind_var(new oasys::StringOpt("type", &BundleRouter::config_.type_, 
                                  "type", "Which routing algorithm to use "
				"(default static).\n"
		"	valid options:\n"
		"			static\n"
		"			external"));
    bind_var(new oasys::BoolOpt("add_nexthop_routes",
                                &BundleRouter::config_.add_nexthop_routes_,
				"Whether or not to automatically add routes "
				"for next hop links (default is true)\n"
    		"	valid options:	true or false\n"));
    
    bind_var(new oasys::BoolOpt("static_router_prefer_always_on",
                                &BundleRouter::config_.static_router_prefer_always_on_,
				"Static Router only send to ALWAYSON and Open links "
				"if possible (default is true)\n"
    		"	valid options:	true or false\n"));
    
    bind_var(new oasys::BoolOpt("auto_deliver_bundles",
                                &BundleRouter::config_.auto_deliver_bundles_,
				"Whether or not local bundles should be delivered automatically"
                "upon receipt or the router will initiate it (default is true)\n"
    		"	valid options:	true or false\n"));
    
    add_to_help("add <dest> <link/endpoint> [opts]", "add a route");

    add_to_help("add_ipn_range <start_node#> <end_node#> <link/endpoint> [opts]", 
                "add routes for all ipn:<node#>.* in range (inclusive)");

    add_to_help("recompute_routes", "use after adding routes to force bundles" 
                " to be re-evaluated for routing purposes");
   
    add_to_help("del <dest>", "delete a route");
    
    add_to_help("dump", "dump all of the static routes");

    bind_var(new oasys::BoolOpt("open_discovered_links",
                                &BundleRouter::config_.open_discovered_links_,
				"Whether or not to automatically open "
				"discovered opportunistic links (default is true)\n"
    		"	valid options:  true or false\n"));
    
    bind_var(new oasys::IntOpt("default_priority",
                               &BundleRouter::config_.default_priority_,
				"priority",
				"Default priority for new routes "
				"(default 0)\n"
		"	valid options:	number\n"));

    bind_var(new oasys::IntOpt("max_route_to_chain",
                               &BundleRouter::config_.max_route_to_chain_,
				"length",
				"Maximum number of route_to links to follow "
				"(default 10)\n"
		"	valid options:  number\n"));

    bind_var(new oasys::UIntOpt("subscription_timeout",
                                &BundleRouter::config_.subscription_timeout_,
				"timeout",
				"Default timeout for upstream subscription "
				"(default 600)\n"
		"	valid options:  number\n"));
             
    add_to_help("local_eid", "view or set the Endpoint EID for the local node (any known scheme)");
    add_to_help("local_eid_ipn", "view or set an alternate IPN scheme Endpoint EID for the local node");

    bind_var(new oasys::UInt64Opt("local_ltp_engine_id",
                                &BundleDaemon::params_.ltp_engine_id_,
                                "ID number",
                                "Local LTP Engine ID. If not specified then the node number "
                                "of the local IPN EID will be used."));

    add_to_help("extrtr_interface", "External Router network interface (default is INADDR_LOOPBACK)");

    bind_var(new oasys::UInt16Opt("server_port",
				&ExternalRouter::server_port,
				"port",
				"UDP port for IPC with external router(s) "
				"(default 8001)\n"
		"	valid options:  number\n"));
    
    bind_var(new oasys::UInt16Opt("hello_interval",
				&ExternalRouter::hello_interval,
				"interval",
				"Seconds between hello messages with external router(s)"
				"(default 30)\n"
		"	valid options:  number\n"));

    add_to_help("imc_router_enabled [true|false]",
                "Get or set the IMC Router enabled settings");

    add_to_help("is_imc_router_node [true|false]",
                "Get or set whether this node is an IMC Router Node");

    add_to_help("imc_home_region [<region#>]",
                "Get or set the IMC home region for this node.\n"
                "*** Set local_eid_ipn prior to setting the imc_home_region ***");

    add_to_help("clear_imc_region_db [<id#>]",
                "Clear the IMC Region database [if id# does not match that of the last issued clear (0 = always)]");

    add_to_help("clear_imc_group_db [<id#>]",
                "Clear the IMC Group database [if id# does not match that of the last issued clear (0 = always)]");

    add_to_help("clear_imc_manual_join_db [<id#>]",
                "Clear the IMC Manual Join database [if id# does not match that of the last issued clear (0 = always)]");

    add_to_help("imc_region_add <region#> <router_nodes? true|false> <start_node#> [<end_node#>]", 
                "Add a node or inclusive range of nodes with specified router_node designation to a specified IMC Region");

    add_to_help("imc_region_del <region#> <start_node#> [<end_node#>]", 
                "Delete a node or inclusive range of nodes to a specified IMC Region");

    add_to_help("imc_group_add <group#> <start_node#> [<end_node#>]", 
                "Add a node or inclusive range of nodes to a specified IMC Group");

    add_to_help("imc_group_del <group#> <start_node#> [<end_node#>]", 
                "Delete a node or inclusive range of nodes to a specified IMC Group");

    add_to_help("imc_manual_join_add <group#> <service#>", 
                "Manually join/register to receive bundles to EID imc:<group#>.<service#> "
                "and hold them until delivered to application");

    add_to_help("imc_manual_join_del <group#> <service#>", 
                "Delete manual join/registration to receive bundles to EID imc:<group#>.<service#> ");

    add_to_help("imc_region_dump [<region#>]",
                "List all of the configured IMC Regions and their member nodes or those of a specified region#");

    add_to_help("imc_group_dump",
                "List all of the configured IMC Groups and their member nodes or those of a specified group#");

    add_to_help("imc_manual_join_dump",
                "List all of the manually joined/registered IMC destination EIDs");

    add_to_help("imc_issue_bp6_joins [true|false]",
                "Get or set whether this node initiates BPv6 join requests\n"
                "(default: false)");

    add_to_help("imc_issue_bp7_joins [true|false]",
                "Get or set whether this node initiates BPv7 join requests\n"
                "(default: true)");

    add_to_help("imc_sync_on_startup [true|false]",
                "Get or set whether this node initiates BPv7 synchronization between "
                "all IMC nodes in the region at startup\n"
                "(default: false)");

    add_to_help("imc_sync [<node_number>]",
                "manually initiate an IMC Sync with all nodes or a single node_number");

    add_to_help("imc_delete_unrouteable [true|false]",
                "Get or set whether to delete bundles with all dest nodes unrouteable\n"
                "(default: true)");

    add_to_help("imc_unreachable_is_unrouteable [true|false]",
                "Get or set whether to treat known but currently unreachable nodes as unrouteable\n"
                "(default: true)");

}

int
RouteCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a route subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    
    if (strcmp(cmd, "add") == 0) {
        return process_add(argc, argv);
    }

    else if (strcmp(cmd, "add_ipn_range") == 0) {
        return process_add_ipn_range(argc, argv);
    }

    else if (strcmp(cmd, "del") == 0) {
        return process_del(argc, argv);
    }

    else if (strcmp(cmd, "dump") == 0) {
        oasys::StringBuffer buf;
        BundleDaemon::instance()->get_routing_state(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    else if (strcmp(cmd, "dump_tcl") == 0) {
        // XXX/demmer this could be done better
        oasys::StringBuffer buf;
        BundleDaemon::instance()->router()->tcl_dump_state(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    else if (strcmp(cmd, "recompute_routes") == 0) {
        if (BundleDaemon::instance()->started()) {
            oasys::Time t = oasys::Time::now();
            BundleDaemon::instance()->router()->recompute_routes();
            resultf("Elapsed millisecs to recompute: %" PRIu64, t.elapsed_ms());
        } else {
            RouteRecomputeEvent* event_to_post;
            event_to_post = new RouteRecomputeEvent();
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);
            resultf("Recompute event posted to BundleDaemon");
        }
        return TCL_OK;
    }

    else if (strcmp(cmd, "local_eid") == 0) {
        return process_local_eid(argc, argv);
    }

    else if (strcmp(cmd, "local_eid_ipn") == 0) {
        return process_local_eid_ipn(argc, argv);
    }

    else if (strcmp(cmd, "extrtr_multicast") == 0) {
        resultf("The parameter \"extrtr_multicast\" is no longer used");
        return TCL_OK;
    }
    else if (strcmp(cmd, "schema") == 0) {
        resultf("The parameter \"schema\" is no longer used");
        return TCL_OK;
    }
    else if (strcmp(cmd, "xml_server_validation") == 0) {
        resultf("The parameter \"xml_server_validation\" is no longer used");
        return TCL_OK;
    }
    else if (strcmp(cmd, "xml_client_validation") == 0) {
        resultf("The parameter \"xml_client_validation\" is no longer used");
        return TCL_OK;
    }
    else if (strcmp(cmd, "extrtr_use_tcp") == 0) {
        resultf("The parameter \"extrtr_use_tcp\" is no longer used as the external router interface is now always TCP");
        return TCL_OK;
    }
    else if (strcmp(cmd, "extrtr_interface") == 0) {
        return process_extrtr_interface(argc, argv);
    }

    else if (strcmp(cmd, "imc_router_enabled") == 0) {
        return process_imc_router_enabled(argc, argv);
    }
    else if (strcmp(cmd, "is_imc_router_node") == 0) {
        return process_is_imc_router_node(argc, argv);
    }
    else if (strcmp(cmd, "imc_home_region") == 0) {
        return process_imc_home_region(argc, argv);
    }
    else if (strcmp(cmd, "clear_imc_region_db") == 0) {
        return process_clear_imc_region_db(argc, argv);
    }
    else if (strcmp(cmd, "clear_imc_group_db") == 0) {
        return process_clear_imc_group_db(argc, argv);
    }
    else if (strcmp(cmd, "clear_imc_manual_join_db") == 0) {
        return process_clear_imc_manual_join_db(argc, argv);
    }
    else if (strcmp(cmd, "imc_region_add") == 0) {
        return process_imc_region_add(argc, argv);
    }
    else if (strcmp(cmd, "imc_region_del") == 0) {
        return process_imc_region_del(argc, argv);
    }
    else if (strcmp(cmd, "imc_group_add") == 0) {
        return process_imc_group_add(argc, argv);
    }
    else if (strcmp(cmd, "imc_group_del") == 0) {
        return process_imc_group_del(argc, argv);
    }
    else if (strcmp(cmd, "imc_manual_join_add") == 0) {
        return process_imc_manual_join_add(argc, argv);
    }
    else if (strcmp(cmd, "imc_manual_join_del") == 0) {
        return process_imc_manual_join_del(argc, argv);
    }

    else if (strcmp(cmd, "imc_region_dump") == 0) {
        return process_imc_region_dump(argc, argv);
    }
    else if (strcmp(cmd, "imc_group_dump") == 0) {
        return process_imc_group_dump(argc, argv);
    }
    else if (strcmp(cmd, "imc_manual_join_dump") == 0) {
        return process_imc_manual_join_dump(argc, argv);
    }
    else if (strcmp(cmd, "imc_sync_on_startup") == 0) {
        return process_imc_sync_on_startup(argc, argv);
    }
    else if (strcmp(cmd, "imc_sync") == 0) {
        return process_imc_sync(argc, argv);
    }
    else if (strcmp(cmd, "imc_issue_bp6_joins") == 0) {
        return process_imc_issue_bp6_joins(argc, argv);
    }
    else if (strcmp(cmd, "imc_issue_bp7_joins") == 0) {
        return process_imc_issue_bp7_joins(argc, argv);
    }
    else if (strcmp(cmd, "imc_delete_unrouteable") == 0) {
        return process_imc_delete_unrouteable(argc, argv);
    }
    else if (strcmp(cmd, "imc_unreachable_is_unrouteable") == 0) {
        return process_imc_unreachable_is_unrouteable(argc, argv);
    }

    else {
        resultf("unimplemented route subcommand %s", cmd);
        //return TCL_ERROR;
        return TCL_OK;
    }
    
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_add(int argc, const char** argv)
{
    // route add <dest> <link/endpoint> <args>
    if (argc < 4) {
        wrong_num_args(argc, argv, 2, 4, INT_MAX);
        return TCL_ERROR;
    }

    const char* dest_str = argv[2];

    SPtr_EIDPattern sptr_dest = BD_MAKE_PATTERN(dest_str);
    if (!sptr_dest->valid()) {
        resultf("invalid destination eid %s", dest_str);
        return TCL_ERROR;
    }

    const char* next_hop = argv[3];

    RouteEntry* entry;
    LinkRef link;

    link = BundleDaemon::instance()->contactmgr()->find_link(next_hop);
    if (link != nullptr) {
        entry = new RouteEntry(sptr_dest, link);
    } else {
        SPtr_EIDPattern sptr_route_to = BD_MAKE_PATTERN(next_hop);
        if (sptr_route_to->valid()) {
            entry = new RouteEntry(sptr_dest, sptr_route_to);
        } else {
            resultf("next hop %s is not a valid link or endpoint id",
                    next_hop);
            return TCL_ERROR;
        }
    }

    // skip over the consumed arguments and parse optional ones.
    // any invalid options are shifted into argv[0]
    argc -= 4;
    argv += 4;
    if (argc != 0 && (entry->parse_options(argc, argv) != argc))
    {
        resultf("invalid argument '%s'", argv[0]);
        delete entry;
        return TCL_ERROR;
    }
    
    // post the event -- if the daemon has been started, we wait
    // for the event to be consumed, otherwise we just return
    // immediately. this allows the command to have the
    // appropriate semantics both in the static config file and in
    // an interactive mode
    
    if (BundleDaemon::instance()->started()) {
        RouteAddEvent* event_to_post;
        event_to_post = new RouteAddEvent(entry);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post_and_wait(sptr_event_to_post, CompletionNotifier::notifier());
    } else {
        RouteAddEvent* event_to_post;
        event_to_post = new RouteAddEvent(entry);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    }

    return TCL_OK;
}


//----------------------------------------------------------------------
int
RouteCommand::process_add_ipn_range(int argc, const char** argv)
{
    // route add_ipn_range <start_node#> <end_node#> <link/endpoint> <args>
    if (argc < 5) {
        wrong_num_args(argc, argv, 2, 5, INT_MAX);
        return TCL_ERROR;
    }

    const char* start_node_str = argv[2];
    const char* end_node_str = argv[3];


    char* endptr = nullptr;

    // get the start node number
    int len = strlen(start_node_str);
    size_t start_node_num = strtoull(start_node_str, &endptr, 0);

    if (endptr != (start_node_str + len)) {
        resultf("invalid start_node# value '%s'\n",
                start_node_str);
        return TCL_ERROR;
    }


    // get the end node number
    endptr = nullptr;
    len = strlen(end_node_str);
    size_t end_node_num = strtoull(end_node_str, &endptr, 0);

    if (endptr != (end_node_str + len)) {
        resultf("invalid end_node# value '%s'\n",
                end_node_str);
        return TCL_ERROR;
    }

    if (end_node_num < start_node_num) {
        resultf("invalid - end_node# is less than the start_node#\n");
        return TCL_ERROR;
    }


    const char* next_hop = argv[4];

    RouteEntry* entry = nullptr;
    LinkRef link;
    SPtr_EIDPattern sptr_route_to = BD_MAKE_PATTERN_NULL();

    // validate the next hop
    link = BundleDaemon::instance()->contactmgr()->find_link(next_hop);

    if (link == nullptr) {
        sptr_route_to = BD_MAKE_PATTERN(next_hop);
        if (! sptr_route_to->valid()) {
            resultf("next hop %s is not a valid link or endpoint id",
                    next_hop);
            return TCL_ERROR;
        }
    }

    // skip over the consumed arguments and parse optional ones.
    // any invalid options are shifted into argv[0]
    argc -= 5;
    argv += 5;

    // generate the route entries
    char dest_str[64];
    for (size_t node_num=start_node_num; node_num<=end_node_num; ++node_num) {

        // generate the destination endpoint
        snprintf(dest_str, sizeof(dest_str), "ipn:%zu.*", node_num);
        SPtr_EIDPattern sptr_dest = BD_MAKE_PATTERN(dest_str);
        if (!sptr_dest->valid()) {
            resultf("invalid destination eid %s", dest_str);
            return TCL_ERROR;
        }

        if (link != nullptr) {
            entry = new RouteEntry(sptr_dest, link);
        } else {
            entry = new RouteEntry(sptr_dest, sptr_route_to);
        }

        if (argc != 0 && (entry->parse_options(argc, argv) != argc))
        {
            resultf("invalid argument '%s'", argv[0]);
            delete entry;
            return TCL_ERROR;
        }
    
        // post the event -- if the daemon has been started, we wait
        // for the event to be consumed, otherwise we just return
        // immediately. this allows the command to have the
        // appropriate semantics both in the static config file and in
        // an interactive mode
    
        if (BundleDaemon::instance()->started()) {
              RouteAddEvent* event_to_post;
              event_to_post = new RouteAddEvent(entry);
              SPtr_BundleEvent sptr_event_to_post(event_to_post);
              BundleDaemon::post_and_wait(sptr_event_to_post, CompletionNotifier::notifier());
          } else {
              RouteAddEvent* event_to_post;
              event_to_post = new RouteAddEvent(entry);
              SPtr_BundleEvent sptr_event_to_post(event_to_post);
              BundleDaemon::post(sptr_event_to_post);
          }
    }

    resultf("%zu routes have been posted for processing", 
            (end_node_num - start_node_num + 1));
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_del(int argc, const char** argv)
{
    // route del <dest>
    if (argc != 3) {
        wrong_num_args(argc, argv, 2, 3, 3);
        return TCL_ERROR;
    }

    SPtr_EIDPattern sptr_pat = BD_MAKE_PATTERN(argv[2]);
    if (!sptr_pat->valid()) {
        resultf("invalid endpoint id pattern '%s'", argv[2]);
        return TCL_ERROR;
    }

    if (BundleDaemon::instance()->started()) {
        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
        RouteDelEvent* event_to_post;
        event_to_post = new RouteDelEvent(sptr_pat);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post_and_wait(sptr_event_to_post, CompletionNotifier::notifier());
    } else {
        RouteDelEvent* event_to_post;
        event_to_post = new RouteDelEvent(sptr_pat);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    }
    
    return TCL_OK;
}


//----------------------------------------------------------------------
int
RouteCommand::process_local_eid(int argc, const char** argv)
{
    if (argc == 2) {
        // route local_eid
        set_result(BundleDaemon::instance()->local_eid()->c_str());
        return TCL_OK;
        
    } else if (argc == 3) {
        // route local_eid <eid?>
        BundleDaemon::instance()->set_local_eid(argv[2]);
        if (! BundleDaemon::instance()->local_eid()->valid()) {
            resultf("invalid eid '%s'", argv[2]);
            return TCL_ERROR;
        }
        if (! BundleDaemon::instance()->local_eid()->known_scheme()) {
            resultf("local eid '%s' has unknown scheme", argv[2]);
            return TCL_ERROR;
        }

        resultf("Local EID set to %s", argv[2]);
        return TCL_OK;
    } else {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }
}

//----------------------------------------------------------------------
int
RouteCommand::process_local_eid_ipn(int argc, const char** argv)
{
    if (argc == 2) {
        // route local_eid_ipn
        set_result(BundleDaemon::instance()->local_eid_ipn()->c_str());
        return TCL_OK;

    } else if (argc == 3) {
        // route local_eid_ipn <eid?>
        SPtr_EID sptr_eid = BD_MAKE_EID(argv[2]);
        if (!sptr_eid->valid()) {
            resultf("invalid ipn eid '%s'", argv[2]);
            return TCL_ERROR;
        }
        if (!sptr_eid->is_ipn_scheme()) {
            resultf("invalid ipn eid '%s'", argv[2]);
            return TCL_ERROR;
        }
        if (sptr_eid->service_num() != 0 ) {
            resultf("invalid - local_ipn_eid must end with '.0'");
            return TCL_ERROR;
        }
        BundleDaemon::instance()->set_local_eid_ipn(argv[2]);

        resultf("Local IPN EID set to %s", argv[2]);
        return TCL_OK;
    } else {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }
}

//----------------------------------------------------------------------
int
RouteCommand::process_extrtr_interface(int argc, const char** argv)
{
    if (argc == 2) {
        // route extrtr_interface_
        set_result(intoa(ExternalRouter::network_interface_));
        ExternalRouter::network_interface_configured_ = true;
        return TCL_OK;
    } else if (argc == 3) {

        std::string val = argv[2];
        if (0 == val.compare("any")) {
            ExternalRouter::network_interface_ = htonl(INADDR_ANY);
        } else if (0 == val.compare("lo")) {
            ExternalRouter::network_interface_ = htonl(INADDR_LOOPBACK);
        } else if (0 == val.compare("localhost")) {
            ExternalRouter::network_interface_ = htonl(INADDR_LOOPBACK);
        } else if (val.find("eth") != std::string::npos) {
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, val.c_str(), IFNAMSIZ-1);
            if (ioctl(fd, SIOCGIFADDR, &ifr)) {
                resultf("Error in ioctl for interface name: %s", val.c_str());
                return TCL_ERROR;
            } else {
                struct sockaddr_in *sin = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
                ExternalRouter::network_interface_ = sin->sin_addr.s_addr;
            }
        } else {
           // Assume this is the IP address of one of our interfaces?
            if (0 != oasys::gethostbyname(val.c_str(), &ExternalRouter::network_interface_)) {
                resultf("Error in gethostbyname for %s", val.c_str());
                return TCL_ERROR;
            }
        }

        ExternalRouter::network_interface_configured_ = true;
        return TCL_OK;

    } else {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_router_enabled(int argc, const char** argv)
{
    //add_to_help("imc_router_enabled [true|false]",
    //            "Get or set the IMC Router enabled settings");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (BundleDaemon::instance()->local_ipn_node_num() == 0) {
        resultf("error - local_eid_ipn must be set before enabling the IMC Router");
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.imc_router_enabled_ = enabled;
    }

    resultf("IMC Router enabled: %s",  IMCRouter::imc_config_.imc_router_enabled_ ? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_is_imc_router_node(int argc, const char** argv)
{
    //add_to_help("is_imc_router_node [true|false]",
    //            "Get or set whether this node is an IMC Router Node");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (BundleDaemon::instance()->local_ipn_node_num() == 0) {
        resultf("error - local_eid_ipn must be set before setting the IMC Router Node designation");
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.set_imc_router_node_designation(enabled);
    }

    resultf("IMC Router Node designation: %s",  IMCRouter::imc_config_.imc_router_node_designation()? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_home_region(int argc, const char** argv)
{
    //add_to_help("imc_home_region [<region#>]",
    //            "Get or set the IMC home region for this node");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (BundleDaemon::instance()->local_ipn_node_num() == 0) {
        resultf("error - local_eid_ipn must be set before setting the imc_home_region");
        return TCL_ERROR;
    }

    size_t region_num = 0;

    if (argc == 3) {
        const char* region_str = argv[2];
        char* endptr = nullptr;

        int len = strlen(region_str);
        region_num = strtoull(region_str, &endptr, 0);

        if (endptr != (region_str + len)) {
            resultf("invalid region# value '%s'\n",
                    region_str);
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;
        IMCRouter::imc_config_.set_home_region(region_num, buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    region_num = IMCRouter::imc_config_.home_region();

    resultf("IMC Home Region = %zu", region_num);

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_clear_imc_region_db(int argc, const char** argv)
{
    //add_to_help("clear_imc_region_db [<id#>]",
    //            "Clear the IMC Region database [if id# does not match that of the last issued clear (0 = always)]");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    size_t id_num = 0;

    if (argc == 3) {
        const char* id_str = argv[2];
        char* endptr = nullptr;

        int len = strlen(id_str);
        id_num = strtoull(id_str, &endptr, 0);

        if (endptr != (id_str + len)) {
            resultf("invalid id# value '%s'\n",
                    id_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.clear_imc_region_db(id_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_clear_imc_group_db(int argc, const char** argv)
{
    //add_to_help("clear_imc_group_db [<id#>]",
    //            "Clear the IMC Group database [if id# does not match that of the last issued clear (0 = always)]");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    const char* id_str = argv[2];
    char* endptr = nullptr;

    size_t id_num = 0;

    if (argc == 3) {
        int len = strlen(id_str);
        id_num = strtoull(id_str, &endptr, 0);

        if (endptr != (id_str + len)) {
            resultf("invalid id# value '%s'\n",
                    id_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.clear_imc_group_db(id_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_clear_imc_manual_join_db(int argc, const char** argv)
{
    //add_to_help("clear_imc_manual_join_db [<id#>]",
    //            "Clear the IMC Manual Join database [if id# does not match that of the last issued clear (0 = always)]");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    const char* id_str = argv[2];
    char* endptr = nullptr;

    size_t id_num = 0;

    if (argc == 3) {
        int len = strlen(id_str);
        id_num = strtoull(id_str, &endptr, 0);

        if (endptr != (id_str + len)) {
            resultf("invalid id# value '%s'\n",
                    id_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.clear_imc_manual_join_db(id_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}


//----------------------------------------------------------------------
int
RouteCommand::process_imc_region_add(int argc, const char** argv)
{
    //add_to_help("imc_region_add <region#> <router_nodes? true|false> <start_node#> [<end_node#>]", 
    //            "Add a node or inclusive range of nodes with specified router_node designation to a specified IMC Region");

    if ((argc < 5) || (argc > 6)) {
        wrong_num_args(argc, argv, 2, 5, 6);
        return TCL_ERROR;
    }

    const char* region_str = argv[2];
    const char* router_node_str = argv[3];
    const char* start_node_str = argv[4];
    char* endptr = nullptr;

    size_t region_num = 0;
    size_t start_node_num = 0;
    size_t end_node_num = 0;

    int len = strlen(region_str);
    region_num = strtoull(region_str, &endptr, 0);

    if (endptr != (region_str + len)) {
        resultf("invalid region# value '%s'\n",
                region_str);
        return TCL_ERROR;
    }

    bool is_router_node = false;
    if (strcmp(router_node_str, "true") == 0) {
        is_router_node = true;
    }  else if (strcmp(router_node_str, "false") != 0) {
        resultf("invalid router node desgination  value '%s'  -- must be 'true' or 'false'\n",
                router_node_str);
        return TCL_ERROR;
    }


    len = strlen(start_node_str);
    start_node_num = strtoull(start_node_str, &endptr, 0);

    if (endptr != (start_node_str + len)) {
        resultf("invalid start_node# value '%s'\n",
                start_node_str);
        return TCL_ERROR;
    }

    end_node_num = start_node_num;
    
    if (argc == 6) {
        const char* end_node_str = argv[5];

        len = strlen(end_node_str);
        end_node_num = strtoull(end_node_str, &endptr, 0);

        if (endptr != (end_node_str + len)) {
            resultf("invalid end_node# value '%s'\n",
                    end_node_str);
            return TCL_ERROR;
        }

        if (start_node_num > end_node_num) {
            resultf("invalid range of node number. start_node# (%s) must be greater then end_node (%s) \n",
                    start_node_str, end_node_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.add_node_range_to_region(region_num, is_router_node, start_node_num, end_node_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_region_del(int argc, const char** argv)
{
    //add_to_help("imc_region_del <region#> <start_node#> [<end_node#]>", 
    //            "Delete a node or inclusive range of nodes to a specified IMC Region");

    if ((argc < 4) || (argc > 5)) {
        wrong_num_args(argc, argv, 2, 4, 5);
        return TCL_ERROR;
    }

    const char* region_str = argv[2];
    const char* start_node_str = argv[3];
    const char* end_node_str = argv[4];
    char* endptr = nullptr;

    size_t region_num = 0;
    size_t start_node_num = 0;
    size_t end_node_num = 0;

    int len = strlen(region_str);
    region_num = strtoull(region_str, &endptr, 0);

    if (endptr != (region_str + len)) {
        resultf("invalid region# value '%s'\n",
                region_str);
        return TCL_ERROR;
    }


    len = strlen(start_node_str);
    start_node_num = strtoull(start_node_str, &endptr, 0);

    if (endptr != (start_node_str + len)) {
        resultf("invalid start_node# value '%s'\n",
                start_node_str);
        return TCL_ERROR;
    }

    end_node_num = start_node_num;
    
    if (argc == 5) {
        len = strlen(end_node_str);
        end_node_num = strtoull(end_node_str, &endptr, 0);

        if (endptr != (end_node_str + len)) {
            resultf("invalid end_node# value '%s'\n",
                    end_node_str);
            return TCL_ERROR;
        }

        if (start_node_num > end_node_num) {
            resultf("invalid range of node number. start_node# (%s) must be greater then end_node (%s) \n",
                    start_node_str, end_node_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.del_node_range_from_region(region_num, start_node_num, end_node_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_group_add(int argc, const char** argv)
{
    //add_to_help("imc_group_add <group#> <start_node#> [<end_node#]>", 
    //            "add a node or inclusive range of nodes to a specified IMC Region");

    if ((argc < 4) || (argc > 5)) {
        wrong_num_args(argc, argv, 2, 4, 5);
        return TCL_ERROR;
    }

    const char* group_str = argv[2];
    const char* start_node_str = argv[3];
    const char* end_node_str = argv[4];
    char* endptr = nullptr;

    size_t group_num = 0;
    size_t start_node_num = 0;
    size_t end_node_num = 0;

    int len = strlen(group_str);
    group_num = strtoull(group_str, &endptr, 0);

    if (endptr != (group_str + len)) {
        resultf("invalid group# value '%s'\n",
                group_str);
        return TCL_ERROR;
    }


    len = strlen(start_node_str);
    start_node_num = strtoull(start_node_str, &endptr, 0);

    if (endptr != (start_node_str + len)) {
        resultf("invalid start_node# value '%s'\n",
                start_node_str);
        return TCL_ERROR;
    }

    end_node_num = start_node_num;
    
    if (argc == 5) {
        len = strlen(end_node_str);
        end_node_num = strtoull(end_node_str, &endptr, 0);

        if (endptr != (end_node_str + len)) {
            resultf("invalid end_node# value '%s'\n",
                    end_node_str);
            return TCL_ERROR;
        }

        if (start_node_num > end_node_num) {
            resultf("invalid range of node number. start_node# (%s) must be greater then end_node (%s) \n",
                    start_node_str, end_node_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.add_node_range_to_group(group_num, start_node_num, end_node_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_group_del(int argc, const char** argv)
{
    //add_to_help("imc_group_del <group#> <start_node#> [<end_node#]>", 
    //            "Delete a node or inclusive range of nodes to a specified IMC Region");

    if ((argc < 4) || (argc > 5)) {
        wrong_num_args(argc, argv, 2, 4, 5);
        return TCL_ERROR;
    }

    const char* group_str = argv[2];
    const char* start_node_str = argv[3];
    const char* end_node_str = argv[4];
    char* endptr = nullptr;

    size_t group_num = 0;
    size_t start_node_num = 0;
    size_t end_node_num = 0;

    int len = strlen(group_str);
    group_num = strtoull(group_str, &endptr, 0);

    if (endptr != (group_str + len)) {
        resultf("invalid group# value '%s'\n",
                group_str);
        return TCL_ERROR;
    }


    len = strlen(start_node_str);
    start_node_num = strtoull(start_node_str, &endptr, 0);

    if (endptr != (start_node_str + len)) {
        resultf("invalid start_node# value '%s'\n",
                start_node_str);
        return TCL_ERROR;
    }

    end_node_num = start_node_num;
    
    if (argc == 5) {
        len = strlen(end_node_str);
        end_node_num = strtoull(end_node_str, &endptr, 0);

        if (endptr != (end_node_str + len)) {
            resultf("invalid end_node# value '%s'\n",
                    end_node_str);
            return TCL_ERROR;
        }

        if (start_node_num > end_node_num) {
            resultf("invalid range of node number. start_node# (%s) must be greater then end_node (%s) \n",
                    start_node_str, end_node_str);
            return TCL_ERROR;
        }
    }

    std::string result_str;
    IMCRouter::imc_config_.del_node_range_from_group(group_num, start_node_num, end_node_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_manual_join_add(int argc, const char** argv)
{
    //add_to_help("imc_manual_join_add <group#> <service#>", 
    //            "Manually join/register to receive bundles to EID imc:<group#>.<service#> "
    //            "and hold them until delivered to application");

    if (argc != 4) {
        wrong_num_args(argc, argv, 2, 4, 4);
        return TCL_ERROR;
    }

    const char* group_str = argv[2];
    const char* service_str = argv[3];
    char* endptr = nullptr;

    size_t group_num = 0;
    size_t service_num = 0;

    int len = strlen(group_str);
    group_num = strtoull(group_str, &endptr, 0);

    if (endptr != (group_str + len)) {
        resultf("invalid group# value '%s'\n",
                group_str);
        return TCL_ERROR;
    }


    len = strlen(service_str);
    service_num = strtoull(service_str, &endptr, 0);

    if (endptr != (service_str + len)) {
        resultf("invalid service# value '%s'\n",
                service_str);
        return TCL_ERROR;
    }

    std::string result_str;
    IMCRouter::imc_config_.add_manual_join(group_num, service_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_manual_join_del(int argc, const char** argv)
{
    //add_to_help("imc_manual_join_del <group#> <service#>", 
    //            "Delete manual join/registration to receive bundles to EID imc:<group#>.<service#> ");

    if (argc != 4) {
        wrong_num_args(argc, argv, 2, 4, 4);
        return TCL_ERROR;
    }

    const char* group_str = argv[2];
    const char* service_str = argv[3];
    char* endptr = nullptr;

    size_t group_num = 0;
    size_t service_num = 0;

    int len = strlen(group_str);
    group_num = strtoull(group_str, &endptr, 0);

    if (endptr != (group_str + len)) {
        resultf("invalid group# value '%s'\n",
                group_str);
        return TCL_ERROR;
    }


    len = strlen(service_str);
    service_num = strtoull(service_str, &endptr, 0);

    if (endptr != (service_str + len)) {
        resultf("invalid service# value '%s'\n",
                service_str);
        return TCL_ERROR;
    }

    std::string result_str;
    IMCRouter::imc_config_.del_manual_join(group_num, service_num, result_str);

    resultf("%s", result_str.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_region_dump(int argc, const char** argv)
{
    //add_to_help("imc_region_dump [<region#>]",
    //            "List all of the configured IMC Regions and their member nodes or those of a specified region#");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    const char* region_str = argv[2];
    char* endptr = nullptr;

    size_t region_num = 0;

    if (argc == 3) {
        int len = strlen(region_str);
        region_num = strtoull(region_str, &endptr, 0);

        if (endptr != (region_str + len)) {
            resultf("invalid region# value '%s'\n",
                    region_str);
            return TCL_ERROR;
        }
    }

    oasys::StringBuffer buf;

    IMCRouter::imc_config_.imc_region_dump(region_num, buf);

    set_result(buf.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_group_dump(int argc, const char** argv)
{
    //add_to_help("imc_group_dump [<group#>]",
    //            "List all of the configured IMC Groups and their member nodes or those of a specified group#");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    const char* group_str = argv[2];
    char* endptr = nullptr;

    size_t group_num = 0;

    if (argc == 3) {
        int len = strlen(group_str);
        group_num = strtoull(group_str, &endptr, 0);

        if (endptr != (group_str + len)) {
            resultf("invalid group# value '%s'\n",
                    group_str);
            return TCL_ERROR;
        }
    }

    oasys::StringBuffer buf;

    IMCRouter::imc_config_.imc_group_dump(group_num, buf);

    set_result(buf.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_manual_join_dump(int argc, const char** argv)
{
    //add_to_help("imc_manual_join_dump",
    //            "List all of the manually joined/registered IMC destination EIDs");

    if (argc != 2) {
        wrong_num_args(argc, argv, 2, 2, 2);
        return TCL_ERROR;
    }

    oasys::StringBuffer buf;

    IMCRouter::imc_config_.imc_manual_join_dump(buf);

    set_result(buf.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_issue_bp6_joins(int argc, const char** argv)
{
    //add_to_help("imc_issue_bp6_joins [true|false]",
    //            "Get or set whether this node initiates BPv6 join requests\n"
    //            "(default: false)");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.imc_issue_bp6_joins_ = enabled;
    }

    resultf("IMC issue BPv6 joins: %s",  IMCRouter::imc_config_.imc_issue_bp6_joins_ ? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_issue_bp7_joins(int argc, const char** argv)
{
    //add_to_help("imc_issue_bp7_joins [true|false]",
    //            "Get or set whether this node initiates BPv7 join requests\n"
    //            "(default: true)");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.imc_issue_bp7_joins_ = enabled;
    }

    resultf("IMC issue BPv7 joins: %s",  IMCRouter::imc_config_.imc_issue_bp7_joins_ ? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_sync_on_startup(int argc, const char** argv)
{
    //add_to_help("imc_sync_on_startup [true|false]",
    //            "Get or set whether this node initiates synchronization between all IMC nodes in the region at startup");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.imc_sync_on_startup_ = enabled;
    }

    resultf("IMC Sync on startup: %s",  IMCRouter::imc_config_.imc_sync_on_startup_ ? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_sync(int argc, const char** argv)
{
    //add_to_help("imc_sync [<node_number>]",
    //            "manually initiate an IMC Sync with all nodes or a single nude_number");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (!IMCRouter::imc_config_.imc_router_enabled_) {
        resultf("IMC Router is not enabled so cannot issue IMC Sync");
        return TCL_ERROR;
    } else if (!IMCRouter::imc_config_.imc_router_active_) {
        resultf("IMC Router is not active yet so cannot issue IMC Sync");
        return TCL_ERROR;
    } else if (IMCRouter::imc_config_.imc_do_manual_sync_) {
        resultf("IMC Sync is already pending");
        return TCL_ERROR;
    } else {
        size_t node_num = 0;
        if (argc == 3) {
            const char* node_num_str = argv[2];
            char* endptr = nullptr;

            // get the node number
            int len = strlen(node_num_str);
            node_num = strtoull(node_num_str, &endptr, 0);

            if (endptr != (node_num_str + len)) {
                resultf("invalid node number '%s'\n",
                        node_num_str);
                return TCL_ERROR;
            }
        }

        IMCRouter::imc_config_.imc_manual_sync_node_num_ = node_num;
        IMCRouter::imc_config_.imc_do_manual_sync_ = true;

        if (node_num > 0) {
            resultf("IMC Sync initiated to node number %zu", node_num);
        } else {
            resultf("IMC Sync initiated to all nodes");
        }

    }

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_delete_unrouteable(int argc, const char** argv)
{
    //add_to_help("imc_delete_unrouteable [true|false]",
    //            "Get or set whether to delete bundles with all dest nodes currently unrouteable\n"
    //            "(default: true)");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.delete_unrouteable_bundles_ = enabled;
    }

    resultf("IMC Router - delete unrouteable bundles: %s",  IMCRouter::imc_config_.delete_unrouteable_bundles_ ? "true": "false");

    return TCL_OK;
}

//----------------------------------------------------------------------
int
RouteCommand::process_imc_unreachable_is_unrouteable(int argc, const char** argv)
{
    //add_to_help("imc_unreachable_is_unrouteable [true|false]",
    //            "Get or set whether to treat known but currently unreachable nodes as unrouteable\n"
    //            "(default: true)");

    if (argc > 3) {
        wrong_num_args(argc, argv, 2, 2, 3);
        return TCL_ERROR;
    }

    if (argc == 3) {
        const char* bool_str = argv[2];

        bool enabled = (bool_str[0] == 't') || (bool_str[0] == 'T') || (bool_str[0] == '1');

        IMCRouter::imc_config_.treat_unreachable_nodes_as_unrouteable_= enabled;
    }

    resultf("IMC Router - treat known but currently unreachable nodes as unrouteable: %s",  
            IMCRouter::imc_config_.treat_unreachable_nodes_as_unrouteable_ ? "true": "false");

    return TCL_OK;
}

} // namespace dtn
