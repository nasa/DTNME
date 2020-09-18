/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *	http://www.apache.org/licenses/LICENSE-2.0
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
#include "InterfaceCommand.h"
#include "contacts/InterfaceTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include <oasys/util/StringBuffer.h>

namespace dtn {

InterfaceCommand::InterfaceCommand()
    : TclCommand("interface") 
{
    add_to_help("add <name> <conv layer> [arg1=val1 arg=val2 argN=valN...]",
		"add an interface\n"
	"	valid options:\n"
	"	       <name>\n"
	"			An interface name (string)\n"
	"	       <conv layer>\n"
	"			The Convergence Layer Adapter (CLA)\n"
	"				    udp\n"
	"				    tcp\n"
	"				    eth\n"
        "                                       <name>:\n"
        "					Ethernet interface name (string://eth0)\n"
	"				    bt\n"
        "					<arg>:\n"
	"					Bluetooth RFCOMM channel number (number)\n"
	"				    serial\n"
	"				    null\n"
	"				    file\n"
	"				    extcl\n"
	"	       <arg>:\n"
	"			mtu <number (MTU)>\n"
	"			min_retry_interval <number (seconds)>\n"
	"			max_retry_interval <number (seconds)>\n"
	"			idle_close_time <number (seconds)>\n"
	"			potential_downtime <number (seconds)>\n"
	"			prevhop_hdr <true or false>\n"
	"			cost <number (abstract cost)>\n"
	"			qlimit_enabled <true or false (default is false)>\n"
	"			qlimit_bundles_high <number (bundles)>\n"
	"			qlimit_bytes_high <number (bytes)>\n"
	"			qlimit_bundles_low <number (bundles)>\n"
	"			qlimit_bytes_low <number (bytes)>\n"
	"			retry_interval <number (seconds)>\n");
    add_to_help("del <name>", "delete an interface"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			An interface name (string)\n");
    add_to_help("list", "list all of the interfaces");
}

int
InterfaceCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    // interface list
    if (strcasecmp("list", argv[1]) == 0) {
	// XXX/bowei -- seems to like to core
	if (argc > 2) {
	    wrong_num_args(argc, argv, 1, 2, 2);
	}

	oasys::StringBuffer buf;
	InterfaceTable::instance()->list(&buf);
	set_result(buf.c_str());

	return TCL_OK;
    }
    
    // interface add <name> <conv_layer> <args>
    else if (strcasecmp(argv[1], "add") == 0) {
	if (argc < 4) {
	    wrong_num_args(argc, argv, 1, 4, INT_MAX);
	    return TCL_ERROR;
	}
	
	const char* name    = argv[2];
	const char* proto   = argv[3];

	ConvergenceLayer* cl = ConvergenceLayer::find_clayer(proto);
	if (!cl) {
	    resultf("can't find convergence layer for %s", proto);
	    return TCL_ERROR;
	}

	// XXX/demmer return error string from here
	if (! InterfaceTable::instance()->add(name, cl, proto,
						argc - 4, argv + 4)) {
	    resultf("error adding interface %s", name);
	    return TCL_ERROR;
	}
	return TCL_OK;
    }

    // interface del <name>
    else if (strcasecmp(argv[1], "del") == 0) {
	if (argc != 3) {
	    wrong_num_args(argc, argv, 2, 4, 4);
	    return TCL_ERROR;
	}

	const char* name = argv[2];
	
	if (! InterfaceTable::instance()->del(name)) {
	    resultf("error removing interface %s", name);
	    return TCL_ERROR;
	}

	return TCL_OK;
    }
    
    resultf("invalid interface subcommand %s", argv[1]);
    return TCL_ERROR;
}

} // namespace dtn
