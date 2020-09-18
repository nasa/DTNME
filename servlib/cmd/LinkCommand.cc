/*
 *	Copyright 2004-2006 Intel Corporation
 * 
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 * 
 *		http://www.apache.org/licenses/LICENSE-2.0
 * 
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include <oasys/thread/Lock.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/storage/DurableStore.h>

#include "LinkCommand.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "contacts/Link.h"
#include "contacts/ContactManager.h"
#include "conv_layers/ConvergenceLayer.h"
#include "naming/Scheme.h"

namespace dtn {

LinkCommand::LinkCommand()
	: TclCommand("link")
{
	add_to_help("add <name> <next hop> <type> <conv layer type> <opt=val> <opt2=val2>...",
			"add links"
			"\n"
	"	valid options:\n"
	"		<name>\n" 
	"			A string to define link name\n"
	"		<next hop>\n"
	"			A string to define next hop name\n"
	"\t\tfor <conve(rgence) layer type>\t<next hop> is\n"
	"\t\t\tserial\t\t\tSerial port device name\n"
	"\t\t\tnull\t\t\t(any string)\n"
#if 0
	"\t\t\tfile\t\t\tdirectory name\n"
#endif
#ifdef __linux__
	"\t\t\teth\t\t\tEthernet HW Address (MAC address format)\n"
#endif
	"\t\t\ttcp or udp\t\tIP address:PORT or hostname:PORT\n"
#ifdef OASYS_BLUETOOTHENABLED
	"\t\t\tbt\t\t\tBluetooth Address (MAC address format)\n"
#endif
#ifdef LTP_ENABLED
	"\t\t\tltp\t\t\tIP address:PORT or hostname:PORT\n"
#endif
#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)
	"\t\t\textcl\t\t\t(Externally defined)\n"
#endif
#ifdef OASYS_AX25_ENABLED
	"\t\t\tax25cm\t\t\tLOCAL_CALL:REMOTE_CALL:AXPORT or REMOTE_CALL<,DIGIPEATER>:axport\n"
#endif
#ifdef NORM_ENABLED
	"\t\t\tnorm\t\t\tIP address:PORT or hostname:PORT\n"
#endif
	"		<type>\n"
	"			link type options\n" 
	"\t\t\tALLWAYSON\n"
	"\t\t\tONDEMAND\n"
	"\t\t\tSCHEDULED\n"
	"\t\t\t(OPPORTUNISTIC)\n"
	"		<opt> (all links):\n"
	"			mtu <number (MTU)>\n"
	"			min_retry_interval <number (seconds)>\n"
	"			max_retry_interval <number (seconds)>\n"
	"			idle_close_time <number (seconds)>\n"
	"			potential_downtime <number (seconds)>\n"
	"			prevhop_hdr <true or false>\n"
	"			cost <number (abstract cost)>\n"
	"			qlimit_bundles_high <number (bundles)>\n"
	"			qlimit_bytes_high <number (bytes)>\n"
	"			qlimit_bundles_low <number (bundles)>\n"
	"			qlimit_bytes_low <number (bytes)>\n"
	"			retry_interval <number (seconds)>\n");
	add_to_help("open <name>", "open the link"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("close <name>", "close the link"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("delete <name>", "delete the link"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("set_available <name> <true | false>", 
				"hacky way to make the link available"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("state <name>", "return the state of a link"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("stats <name>", "dump link statistics"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("names", "print a list of existing link names"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("dump <name>", "print the list of existing links or "
				"detailed info about a single link"
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n");
	add_to_help("reconfigure <name> <opt=val> <opt2=val2>...",
			"configure link options after initialization "
			"\n"
	"	valid options:\n"
	"		<name>\n"
	"			A link name string\n"
	"		<opt>:\n"
	"			mtu <number (MTU)>\n"
	"			min_retry_interval <number (seconds)>\n"
	"			max_retry_interval <number (seconds)>\n"
	"			idle_close_time <number (seconds)>\n"
	"			potential_downtime <number (seconds)>\n"
	"			prevhop_hdr <true or false>\n"
	"			cost <number (abstract cost)>\n"
	"			qlimit_bundles_high <number (bundles)>\n"
	"			qlimit_bytes_high <number (bytes)>\n"
	"			qlimit_bundles_low <number (bundles)>\n"
	"			qlimit_bytes_low <number (bytes)>\n"
	"			retry_interval <number (seconds)>\n");
	add_to_help("set_cl_defaults <conv layer> <opt=val> <opt2=val2>...",
				"configure convergence layer specific default options"
				"\n"
	"	valid options:\n"
	"		<conv layer type>\n"
	"			The convergence layer type\n"
	"\t\t\t    serial\n"
	"\t\t\t    null\n"
#if 0
	"\t\t\t    file\n"
#endif
#ifdef __linux__
	"\t\t\t    eth\n"
#endif
	"\t\t\t    tcp\n"
	"\t\t\t    udp\n"
#ifdef OASYS_BLUETOOTHENABLED
	"\t\t\t    bt\n"
#endif
#ifdef LTP_ENABLED
	"\t\t\t    ltp\n"
#endif

//XXX/dz TODO - LTPUDP_ENABLED configuration - note to Mark - only common options re listed in the help
	"\t\t\t    ltpudp\n"



#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)
	"\t\t\t    extcl\n"
#endif
#ifdef OASYS_AX25_ENABLED
	"\t\t\t    ax25cm\n"
#endif
#ifdef NORM_ENABLED
	"\t\t\t    norm\n"
#endif
	"		<opt>:\n"
	"			mtu <number (MTU)>\n"
	"			min_retry_interval <number (seconds)>\n"
	"			max_retry_interval <number (seconds)>\n"
	"			idle_close_time <number (seconds)>\n"
	"			potential_downtime <number (seconds)>\n"
	"			prevhop_hdr <true or false>\n"
	"			cost <number (abstract cost)>\n"
	"			qlimit_bundles_high <number (bundles)>\n"
	"			qlimit_bytes_high <number (bytes)>\n"
	"			qlimit_bundles_low <number (bundles)>\n"
	"			qlimit_bytes_low <number (bytes)>\n"
	"			retry_interval <number (seconds)>\n\n");
}
/*	"	**Note options for add, reconfigure and set_cl_defaults:\n\n"
	"	mtu is the MTU of the link, used to control\n "
	"	proactive fragmentation.\n\n"
	"	min_retry_interval is the minimum amount to \n"
	"	wait between attempts to re-open the link \n"
	"	in seconds). Default is set by the various\n "
	"	link types but can be overridden by \n"
	"	configuration parameters.\n\n"
	"	max_retry_interval is the Maximum amount to \n"
	"	wait between attempts to re-open the link (in\n "
	"	seconds). Default is set by the various Link \n"
	"	types but can be overridden by configuration \n"
	"	parameters.\n\n"
	"	idle_close_time is seconds of idle time \n"
	"	before the link is closed. Must be zero for \n"
	"	always on links (i.e. they are never closed). \n"
	"	Default is 30 seconds for on demand links, \n"
	"	zero for opportunistic links.\n\n"
	"	potential_downtime is a Conservative estimate \n"
	"	of the maximum amount of time that the link may \n"
	"	be down during 'normal' operation. Used by \n"
	"	routing algorithms to determine how long to leave \n"
	"	bundles queued on the down link before rerouting \n"
	"	them. Default is 30 seconds.\n\n"
	"	prevhop_hdr indicates whether or not to send \n"
	"	the previous hop header on this link. Default is \n"
	"	false.\n\n"
	"	cost is the abstract cost of the link, used \n"
	"	by routing algorithms. Default is 100.\n\n"
	"	qlimit_bundles_high is the high limits on the \n"
	"	number of bundles that should be queued on the \n"
	"	link. The high limits are used to indicate whether \n"
	"	or not more bundles can be queued onto the link to \n"
	"	effect backpressure from the convergence layers.\n\n"
	"	qlimit_bytes_high and qlimit_bytes_low is the \n"
	"	high or low limits on the number of bytes that \n"
	"	should be queued on the link. The high limits \n"
	"	are used to indicate whether or not more bundles \n"
	"	can be queued onto the link to effect backpressure \n"
	"	from the convergence layers. The low limits can \n"
	"	be used by the router to determine when to re-scan \n"
	"	the pending bundle lists\n\n"
	"	retry_interval is the seconds to wait between \n"
	"	attempts to re-open an unavailable link. Initially \n"
	"	set to min_retry_interval_, then doubles up to \n"
	"	max_retry_interval");
*/

int
LinkCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
	(void)interp;
	
	if (argc < 2) {
		resultf("need a link subcommand");
		return TCL_ERROR;
	}

	const char* cmd = argv[1];

	if (strcmp(cmd, "add") == 0) {
		// link add <name> <nexthop> <type> <clayer> <args>
		if (argc < 6) {
			wrong_num_args(argc, argv, 2, 6, INT_MAX);
			return TCL_ERROR;
		}
		
		const char* name = argv[2];
		const char* nexthop = argv[3];
		const char* type_str = argv[4];
		const char* cl_str = argv[5];

		// validate the link type, make sure there's no link of the
		// same name, and validate the convergence layer
		Link::link_type_t type = Link::str_to_link_type(type_str);
		if (type == Link::LINK_INVALID) {
			resultf("invalid link type %s", type_str);
			return TCL_ERROR;
		}

		ConvergenceLayer* cl = ConvergenceLayer::find_clayer(cl_str);
		if (!cl) {
			resultf("invalid convergence layer %s", cl_str);
			return TCL_ERROR;
		}

		// Create the link, parsing the cl-specific next hop string
		// and other arguments
		LinkRef link;
		const char* invalid_arg = "(unknown)";
		link = Link::create_link(name, type, cl, nexthop, argc - 6, &argv[6],
								 &invalid_arg);
		if (link == NULL) {
			resultf("invalid link option: %s", invalid_arg);
			return TCL_ERROR;
		}

		// Add the link to contact manager's table if it is not already
		// present. The contact manager will post a LinkCreatedEvent to
		// the daemon if the link is added successfully.
		if (!BundleDaemon::instance()->contactmgr()->add_new_link(link)) {
			// A link of that name already exists
			link->delete_link();
			resultf("problem adding link with name %s: problem is one of:\n%s,\n%s, or\n%s;\n%s",
					name,
					"link with this name already exists,",
					"link name was previously used with different nexthop, type or convergence layer",
					"same nexthop, type and convergence layer are or were used with a different name",
					"Please check your parameters or retry with a different name.");
			return TCL_ERROR;
		}

		return TCL_OK;

	} else if (strcmp(cmd, "reconfigure") == 0) {
		// link set_option <name> <opt=val> <opt2=val2>...
		if (argc < 4) {
			wrong_num_args(argc, argv, 2, 4, INT_MAX);
			return TCL_ERROR;
		}

		const char* name = argv[2];
		
		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		} 

		argc -= 3;
		argv += 3;

		const char* invalid;
		int count = link->parse_args(argc, argv, &invalid);
		if (count == -1) {
			resultf("invalid link option: %s", invalid);
			return TCL_ERROR;
		}
		argc -= count;
		
		if (!link->reconfigure_link(argc, argv)) {
			return TCL_ERROR;
		}
		
		return TCL_OK;
		
	} else if (strcmp(cmd, "open") == 0) {
		// link open <name>
		if (argc != 3) {
			wrong_num_args(argc, argv, 2, 3, 3);
			return TCL_ERROR;
		}

		const char* name = argv[2];
		
		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		}

		if (link->isopen()) {
			resultf("link %s already open", name);
			return TCL_OK;
		}

		// XXX/TODO should change all these to post_and_wait
		BundleDaemon::post(new LinkStateChangeRequest(link, Link::OPEN,
													  ContactEvent::USER));
		
	} else if (strcmp(cmd, "close") == 0) {
		// link close <name>
		if (argc != 3) {
			wrong_num_args(argc, argv, 2, 3, 3);
			return TCL_ERROR;
		}

		const char* name = argv[2];
		
		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		}

		if (! link->isopen() && ! link->isopening()) {
			resultf("link %s already closed", name);
			return TCL_OK;
		}

		BundleDaemon::instance()->post(
			new LinkStateChangeRequest(link, Link::CLOSED,
									   ContactEvent::USER));

		return TCL_OK;

	} else if (strcmp(cmd, "delete") == 0) {
		// link delete <name>
		if (argc != 3) {
			wrong_num_args(argc, argv, 2, 3, 3);
			return TCL_ERROR;
		}

		const char * name = argv[2];

		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		}

		BundleDaemon::instance()->post(new LinkDeleteRequest(link));
		return TCL_OK;

	} else if (strcmp(cmd, "set_available") == 0) {
		// link set_available <name> <true|false>
		if (argc != 4) {
			wrong_num_args(argc, argv, 2, 4, 4);
			return TCL_ERROR;
		}

		const char* name = argv[2];

		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		}

		int len = strlen(argv[3]);
		bool set_available;

		if (strncmp(argv[3], "1", len) == 0) {
			set_available = true;
		} else if (strncmp(argv[3], "0", len) == 0) {
			set_available = false;
		} else if (strncasecmp(argv[3], "true", len) == 0) {
			set_available = true;
		} else if (strncasecmp(argv[3], "false", len) == 0) {
			set_available = false;
		} else if (strncasecmp(argv[3], "on", len) == 0) {
			set_available = true;
		} else if (strncasecmp(argv[3], "off", len) == 0) {
			set_available = false;
		} else {
			resultf("error converting argument %s to boolean value", argv[3]);
			return TCL_ERROR;
		}

		if (set_available) {
			if (link->state() != Link::UNAVAILABLE) {
				resultf("link %s already in state %s",
						name, Link::state_to_str(link->state()));
				return TCL_OK;
			}

			BundleDaemon::post(
				new LinkStateChangeRequest(link, Link::AVAILABLE,
										   ContactEvent::USER));
			
			return TCL_OK;

		} else { // ! set_available
			if (link->state() != Link::AVAILABLE) {
				resultf("link %s can't be set unavailable in state %s",
						name, Link::state_to_str(link->state()));
				return TCL_OK;
			}
			
			BundleDaemon::post(
				new LinkStateChangeRequest(link, Link::UNAVAILABLE,
										   ContactEvent::USER));
	
			return TCL_OK;
		}
	}
	else if ((strcmp(cmd, "state") == 0) ||
			 (strcmp(cmd, "stats") == 0))
	{
		// link state <name>
		// link stats <name>
		if (argc != 3) {
			wrong_num_args(argc, argv, 2, 3, 3);
			return TCL_ERROR;
		}

		const char* name = argv[2];

		LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);
		if (link == NULL) {
			resultf("link %s doesn't exist", name);
			return TCL_ERROR;
		}

		if (strcmp(cmd, "state") == 0) {
			resultf("%s", Link::state_to_str(link->state()));
		} else {
			oasys::StringBuffer buf;
			link->dump_stats(&buf);
			set_result(buf.c_str());
		}
		return TCL_OK;
	}
	else if (strcmp(cmd, "names") == 0) 
	{
		// link names
		if (argc != 2) {
			wrong_num_args(argc, argv, 2, 2, 2);
			return TCL_ERROR;
		}

		ContactManager* cm = BundleDaemon::instance()->contactmgr();
		oasys::ScopeLock l(cm->lock(), "LinkCommand::exec");
			
		const LinkSet* links = cm->links();
		append_resultf("Active links:\n");
		for (LinkSet::const_iterator i = links->begin();
			 i != links->end(); ++i)
		{
			append_resultf("%s\n", (*i)->name());
		}
		links = cm->previous_links();
		append_resultf("\nPreviously active links:\n");
		for (LinkSet::const_iterator i = links->begin();
			 i != links->end(); ++i)
		{
			append_resultf("%s\n", (*i)->name());
		}
		return TCL_OK;
	}
	else if (strcmp(cmd, "dump") == 0) 
	{
		// link dump <name?>
		if (argc == 2) {
			ContactManager* cm = BundleDaemon::instance()->contactmgr();
			oasys::ScopeLock l(cm->lock(), "LinkCommand::exec");
			
			const LinkSet* links = cm->links();
			append_resultf("Active links:\n");
			for (LinkSet::const_iterator i = links->begin();
				 i != links->end(); ++i)
			{
				append_resultf("*%p\n", (*i).object());
			}
			links = cm->previous_links();
			append_resultf("\nPreviously active links:\n");
			for (LinkSet::const_iterator i = links->begin();
				 i != links->end(); ++i)
			{
				append_resultf("*%p\n", (*i).object());
			}
		} else if (argc == 3) {
			const char* name = argv[2];
			const char* prefix = "Current link:\n";
			
			LinkRef link =
				BundleDaemon::instance()->contactmgr()->find_link(name);
			if (link == NULL) {
				prefix = "Link from previous run:\n";
				link = BundleDaemon::instance()->contactmgr()->find_previous_link(name);
				if (link == NULL)
				{
					resultf("link %s not known.", name);
					return TCL_ERROR;
				}
			}

			oasys::StringBuffer buf;
			link->dump(&buf);
			//set_result(buf.c_str());
			resultf("%s%s", prefix, buf.c_str());
			return TCL_OK;
		} else {
			wrong_num_args(argc, argv, 2, 2, 3);
			return TCL_ERROR;
		}
	}
	else if (strcmp(cmd, "set_cl_defaults") == 0)
	{
		// link set_cl_defaults <cl> <opt=val> <opt2=val2> ...
		if (argc < 4) {
			wrong_num_args(argc, argv, 2, 4, INT_MAX);
			return TCL_ERROR;
		}

		ConvergenceLayer* cl = ConvergenceLayer::find_clayer(argv[2]);
		if (cl == NULL) {
			resultf("unknown convergence layer %s", argv[2]);
			return TCL_ERROR;
		}

		const char* invalid;
		if (!cl->set_link_defaults(argc - 3, &argv[3], &invalid)) {
			resultf("invalid link option: %s", invalid);
			return TCL_ERROR;
		}
		
		return TCL_OK;
	}
	else
	{
		resultf("unimplemented link subcommand %s", cmd);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

} // namespace dtn
