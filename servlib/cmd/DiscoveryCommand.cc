/*
 *    Copyright 2006 Baylor University
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
#include "DiscoveryCommand.h"
#include "discovery/Discovery.h"
#include "discovery/DiscoveryTable.h"
#ifdef BBN_IPND_ENABLED
#include "discovery/IPNDDiscovery.h"
#endif
#include "conv_layers/ConvergenceLayer.h"
#include <oasys/util/StringBuffer.h>

namespace dtn {

DiscoveryCommand::DiscoveryCommand()
    : TclCommand("discovery")
{
    add_to_help("add <discovery_name> <cl_type> [ options ]",
                "add a discovery agent\n"
                "    valid options:\n"
                "        <discovery_name>\n"
                "            A string to define agent name\n"
                "        <cl_type>\n"
#ifdef BBN_IPND_ENABLED
                "            The CLA type [bt, ip, ipnd, bonjour]\n"
#else
                "            The CLA type [bt, ip, bonjour]\n"
#endif
                "        [ CLA specific options ]\n"
                "            <port=port>\n"
                "            <continue_on_error=true or false>\n"
                "            <addr=A.B.C.D>\n"
                "            <local_addr=A.B.C.D>\n"
                "            <multicast_ttl=TTL number>\n"
                "            <unicast=true or false>\n"
#ifdef BBN_IPND_ENABLED
                "        [ BBN-IPND specific options (CLA type \"ipnd\")]\n"
                "            <beacon_period=beacon period in seconds>\n"
                "            <beacon_threshold=tracking accuracy (0 to disable)\n"
#endif
                );

    add_to_help("del <discovery_name>",
                "remove discovery agent\n"
                "    valid options:\n"
                "        <discovery_name>\n"
                "            A string defining the agent to delete\n");

    add_to_help("announce <cl_name> <discovery_name> <cl_type> "
                "<interval=val> [ options ]",
                "announce the address of a local interface (convergence "
                "layer)\n"
                "    valid options:\n"
                "        <cl_name>\n"
                "            The CLA name\n"
                "        <discovery_name>\n"
                "            An agent name string\n"
                "        <cl_type>\n"
                "            The CLA type [bt, ip, bonjour]\n"
                "        <interval=val>\n"
                "            Periodic announcement interval\n"
                "        [ CLA specific options ]\n"
                "            <cl_addr=A.B.C.D>\n"
                "            <cl_port=port number>\n");

#ifdef BBN_IPND_ENABLED
    add_to_help("announce-svc <svc_name> <discovery_name> <svc_type> [ options ]",
                "Announce a service in the IPND service block (only for CLA type "
                "\"ipnd\")\n"
                "    Required options:\n"
                "        <svc_name>\n"
                "            Arbitrary name for the service\n"
                "        <discovery_name>\n"
                "            The discovery agent in which to add the service\n"
                "        <svc_type>\n"
                "            The identifier of the service type to add\n"
                "    Available service type identifiers (with options):\n"
                "        cla-tcp-v4 [TCP convergence layer with IPv4 address]\n"
                "            <addr=inet4-addr>\n"
                "                IPv4 address of the convergence layer\n"
                "            <port=portnum>\n"
                "                TCP port of the convergence layer\n"
                "        cla-udp-v4 [UDP convergence layer with IPv4 address]\n"
                "            <addr=inet6-addr>\n"
                "                IPv4 address of the convergence layer\n"
                "            <port=portnum>\n"
                "                TCP port of the convergence layer\n");
#endif

    add_to_help("remove <cl_name>",
                "remove announcement for local interface\n"
                "    valid options:\n"
                "        <cl_name>\n"
                "            The CLA name\n");

    add_to_help("list", "list all discovery agents and announcements\n");
}

int
DiscoveryCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    if (strncasecmp("list",argv[1],4) == 0)
    {
        if (argc > 2)
        {
            wrong_num_args(argc, argv, 1, 2, 2);
        }

        oasys::StringBuffer buf;
        DiscoveryTable::instance()->dump(&buf);
        set_result(buf.c_str());

        return TCL_OK;
    }
    else
    // discovery add <name> <address family> <port>
    // [<local_addr> <addr> <multicast_ttl>  <channel>]
    if (strncasecmp("add",argv[1],3) == 0)
    {
        if (argc < 4)
        {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* afname = argv[3];

        const char* err = "(unknown error)";
        if (! DiscoveryTable::instance()->add(name, afname,
                                              argc - 4, argv + 4,
                                              &err))
        {
            resultf("error adding agent %s: %s", name, err);
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    else
    // discovery del <name>
    if (strncasecmp("del",argv[1],3) == 0)
    {
        if (argc != 3)
        {
            wrong_num_args(argc,argv,2,3,3);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        
        if (! DiscoveryTable::instance()->del(name))
        {
            resultf("error removing agent %s", name);
            return TCL_ERROR;
        }

        return TCL_OK;
    }
#ifdef BBN_IPND_ENABLED
    // discovery announce-svc ... (BBN IPND only)
    else if (strncasecmp("announce-svc", argv[1], 12) == 0) {
        if (argc < 5)
        {
            wrong_num_args(argc,argv,2,5,INT_MAX);
            return TCL_ERROR;
        }

        const char *sname = argv[2];
        const char *dname = argv[3];

        DiscoveryList::iterator iter;
        if (! DiscoveryTable::instance()->find(dname,&iter))
        {
            resultf("error adding announce %s to %s: "
                    "no such discovery agent",
                    sname, dname);
            return TCL_ERROR;
        }

        IPNDDiscovery *ipnd = dynamic_cast<IPNDDiscovery*>(*iter);
        if (ipnd == 0)
        {
            // cast failure
            resultf("discovery agent %s is not an implementation of IPND "
                    "Discovery", dname);
            return TCL_ERROR;
        }
        else if (! ipnd->svc_announce(sname, argc - 4, argv + 4))
        {
            resultf("error adding service announcement %s to %s", sname, dname);
            return TCL_ERROR;
        }
        else
        {
            return TCL_OK;
        }
    }
#endif
    // discovery announce <name> <discovery name> <cl type> <interval>
    //                    [<cl_addr> <cl_port>]
    else
    if (strncasecmp("announce",argv[1],8) == 0)
    {
        if (argc < 6)
        {
            wrong_num_args(argc,argv,2,6,INT_MAX);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* dname = argv[3];

        DiscoveryList::iterator iter;
        if (! DiscoveryTable::instance()->find(dname,&iter))
        {
            resultf("error adding announce %s to %s: "
                    "no such discovery agent",
                    name,dname);
            return TCL_ERROR;
        }

        Discovery* disc = *iter;
        if (! disc->announce(name,argc - 4,argv + 4))
        {
            resultf("error adding announce %s to %s",name,dname);
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    else
    // discovery remove <name> <discovery name>
    if (strncasecmp("remove",argv[1],6) == 0)
    {
        if (argc != 4)
        {
            wrong_num_args(argc,argv,2,4,4);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* dname = argv[3];

        DiscoveryList::iterator iter;
        if (! DiscoveryTable::instance()->find(dname,&iter))
        {
            resultf("error removing announce %s from %s: no such discovery agent",
                    name,dname);
            return TCL_ERROR;
        }

        Discovery* disc = *iter;
        if (! disc->remove(name))
        {
            resultf("error removing announce %s from %s: no such announce",
                    name,dname);
            return TCL_ERROR;
        }
        return TCL_OK;
    }

    resultf("invalid discovery command: %s",argv[1]);
    return TCL_ERROR;
}

} // namespace dtn
