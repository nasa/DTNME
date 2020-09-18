/*
 *    Copyright 2011 Trinity College Dublin
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

#ifdef BPQ_ENABLED

#include "BPQCommand.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

BPQCommand::BPQCommand()
	: TclCommand("bpq")
{
	add_to_help("enabled", "enable BPQ cache");
	add_to_help("cache_size <size>", "set BPQ cache size");
	add_to_help("list", "list all keys in cache");
	add_to_help("lru", "ordered list of keys in LRU list");
}

int
BPQCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
	(void)interp;
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }
    const char* op = argv[1];

    if (strncmp(op, "enable", strlen("enable")) == 0) {

		BundleDaemon::instance()->bpq_cache()->cache_enabled_ = true;
		return TCL_OK;

    } else if(strncmp(op, "disable", strlen("disable")) == 0) {

    	BundleDaemon::instance()->bpq_cache()->cache_enabled_ = false;
		return TCL_OK;

    } else if(strncmp(op, "cache_size", strlen("cache_size")) == 0) {
        if (argc < 3) {
            wrong_num_args(argc, argv, 2, 3, INT_MAX);
            return TCL_ERROR;
        }

        const char* size_str = argv[2];
		u_int size = atoi(size_str);

		BundleDaemon::instance()->bpq_cache()->max_cache_size_ = size;
		return TCL_OK;
    } else if(strncmp(op, "list", strlen("list")) == 0) {
    	oasys::StringBuffer buf;

    	BundleDaemon::instance()->bpq_cache()->get_keys(&buf);

    	set_result(buf.c_str());

    	return TCL_OK;
    } else if(strncmp(op, "lru", strlen("lru")) == 0) {
    	oasys::StringBuffer buf;

    	BundleDaemon::instance()->bpq_cache()->get_lru_list(&buf);

    	set_result(buf.c_str());

    	return TCL_OK;
    }

    resultf("invalid bpq subcommand '%s'", op);
    return TCL_ERROR;
}

} // namespace dtn

#endif /* BPQ_ENABLED */
