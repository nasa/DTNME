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

#include <climits>
#include <third_party/oasys/util/HexDumpBuffer.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/OptParser.h>

#include "BundleCommand.h"
#include "CompletionNotifier.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "reg/RegistrationTable.h"
#include "reg/TclRegistration.h"

namespace dtn {

BundleCommand::BundleCommand()
    : TclCommand("bundle") 
{
    add_to_help("inject <src> <dst> <payload> <opt1=val1> .. <optN=valN>",
                "valid options:\n"
                "            custody_xfer\n"
                "            receive_rcpt\n"
                "            custody_rcpt\n"
                "            forward_rcpt\n"
                "            delivery_rcpt\n"
                "            deletion_rcpt\n"
                "            expiration=seconds\n"
                "            length=integer\n");
    add_to_help("stats", "get statistics on the bundles");
    add_to_help("daemon_stats", "daemon stats");
    add_to_help("dstats", "daemon stats");
    add_to_help("reset_stats", "reset currently maintained statistics");

    add_to_help("list", "list all of the bundles in the system\n"
                "valid options:\n"
                "   bundle list                       = list first 10 bundles (and last 1)\n"
                "   bundle list all                   = list all bundles\n"
                "   bundle list <count>               = list first <count> bundles (and last 1)\n"
                "   bundle list <start count> <count> = list count bundles after skipping <start count> bundles\n");


    add_to_help("ids", "list the ids of all bundles the system");
    add_to_help("info <id>", "get info on a specific bundle");
    add_to_help("dump <id>", "dump a specific bundle");
    add_to_help("dump_tcl <id>", "dump a bundle as a tcl list");
    add_to_help("dump_ascii <id>", "dump the bundle in ascii");
    add_to_help("expire <id>", "force a specific bundle to expire");
    add_to_help("cancel <id> [<link>]", "cancel a bundle being sent [on a specific link]");
    add_to_help("clear_fwdlog <id>", "clear the forwarding log for a bundle");
    add_to_help("daemon_idle_shutdown <secs>",
                "shut down the bundle daemon after an idle period");

    bind_var(new oasys::BoolOpt("use_age_block",
                                &BundleProtocol::params_.use_bundle_age_block_,
                                "Set creation time to zero and use the BPv7 Bundle Age block for new bundles "
                                "(default is false)"));
}

BundleCommand::InjectOpts::InjectOpts()
    : custody_xfer_(false),
      receive_rcpt_(false), 
      custody_rcpt_(false), 
      forward_rcpt_(false), 
      delivery_rcpt_(false), 
      deletion_rcpt_(false), 
      expiration_(60),  // bundle TTL
      length_(0),  // bundle payload length
      replyto_("")
{}
    
bool
BundleCommand::parse_inject_options(InjectOpts* options,
                                    int argc, const char** argv,
                                    const char** invalidp)
{
    // no options specified:
    if (argc < 6) {
        return true;
    }
    
    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("custody_xfer",  &options->custody_xfer_));
    p.addopt(new oasys::BoolOpt("receive_rcpt",  &options->receive_rcpt_));
    p.addopt(new oasys::BoolOpt("custody_rcpt",  &options->custody_rcpt_));
    p.addopt(new oasys::BoolOpt("forward_rcpt",  &options->forward_rcpt_));
    p.addopt(new oasys::BoolOpt("delivery_rcpt", &options->delivery_rcpt_));
    p.addopt(new oasys::BoolOpt("deletion_rcpt", &options->deletion_rcpt_));
    p.addopt(new oasys::UIntOpt("expiration",    &options->expiration_));
    p.addopt(new oasys::UIntOpt("length",        &options->length_));
    p.addopt(new oasys::StringOpt("replyto",     &options->replyto_));

    for (int i=5; i<argc; i++) {
        const char* option_name = argv[i];
        int len = strlen(option_name);

        if (! p.parse_opt(option_name, len)) {
            *invalidp = option_name;
            return false;
        }
    }
    return true;
}

int
BundleCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "inject") == 0) {
        // bundle inject <source> <dest> <payload> <param1<=value1?>?> ... <paramN<=valueN?>?>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }
        
        bool eids_valid = true;
        Bundle* b = new Bundle();
        std::string src_eid = argv[2];
        b->set_source(src_eid);
        eids_valid &= b->source().valid();
        eids_valid &= b->mutable_replyto()->assign(argv[2]);
        eids_valid &= b->mutable_dest()->assign(argv[3]);
        b->mutable_custodian()->assign(EndpointID::NULL_EID());

        EndpointID::singleton_info_t info = b->dest().known_scheme() ?
                                            b->dest().is_singleton() :
                                            EndpointID::is_singleton_default_;
        switch (info) {
        case EndpointID::SINGLETON:
            b->set_singleton_dest(true);
            break;
        case EndpointID::MULTINODE:
            b->set_singleton_dest(false);
            break;
        case EndpointID::UNKNOWN:
            resultf("can't determine is_singleton for destination %s",
                    b->dest().c_str());
            delete b;
            return TCL_ERROR;
        }
        
        if (!eids_valid) {
            resultf("bad value for one or more EIDs");
            delete b;
            return TCL_ERROR;
        }
        
        int payload_len;
        Tcl_Obj** tcl_objs = const_cast<Tcl_Obj**>(reinterpret_cast<const Tcl_Obj**>(argv));
        u_char* payload_data = Tcl_GetByteArrayFromObj(tcl_objs[4], &payload_len);

        // now process any optional parameters:
        InjectOpts options;
        const char* invalid;
        if (!parse_inject_options(&options, argc, argv, &invalid)) {
            resultf("error parsing bundle inject options: invalid option '%s'",
                    invalid);
            delete b;
            return TCL_ERROR;
        }

        b->set_custody_requested(options.custody_xfer_);
        b->set_receive_rcpt(options.receive_rcpt_);
        b->set_custody_rcpt(options.custody_rcpt_);
        b->set_forward_rcpt(options.forward_rcpt_);
        b->set_delivery_rcpt(options.delivery_rcpt_);
        b->set_deletion_rcpt(options.deletion_rcpt_);
        b->set_expiration_secs(options.expiration_);
        
        // Bundles with a null source EID are not allowed to request reports or
        // custody transfer, and must not be fragmented.
        if (b->source() == EndpointID::NULL_EID()) {
            if ( b->custody_requested() ||
                 b->receipt_requested() ||
                 b->app_acked_rcpt() )
            {
                log_err("bundle with null source EID cannot request reports or "
                        "custody transfer");
                delete b;
                return TCL_ERROR;
            }
            
            b->set_do_not_fragment(true);
        }
        
        else {
            // The bundle's source EID must be either dtn:none or an EID 
            // registered at this node.
            const RegistrationTable* reg_table = 
                    BundleDaemon::instance()->reg_table();
            std::string base_reg_str = b->source().uri().scheme() + "://" + 
                                       b->source().uri().host();
            
            if (!reg_table->get(EndpointIDPattern(base_reg_str)) &&
                !reg_table->get(EndpointIDPattern(b->source())))
            {
                log_err("this node is not a member of the bundle's source EID (%s)",
                        b->source().str().c_str());
                delete b;
                return TCL_ERROR;
            }
        }

        if (options.length_ != 0) {
            // explicit length but some of the data may just be left
            // as garbage. 
            b->mutable_payload()->set_length(options.length_);
            if (payload_len != 0) {
                b->mutable_payload()->write_data(payload_data, payload_len, 0);
            }

            // make sure to write a byte at the end of the payload to
            // properly fool the BundlePayload into thinking that we
            // actually got all the data
            u_char byte = 0;
            b->mutable_payload()->write_data(&byte, options.length_ - 1, 1);
            
            payload_len = options.length_;
        } else {
            // use the object length
            b->mutable_payload()->set_data(payload_data, payload_len);
        }
        
        if (options.replyto_ != "") {
            b->mutable_replyto()->assign(options.replyto_.c_str());
        }

        oasys::StringBuffer error;
        if (!b->validate(&error)) {
            resultf("bundle validation failed: %s", error.data());
            return TCL_ERROR;
        }
        
        log_debug("inject %d byte bundle %s->%s", payload_len,
                  b->source().c_str(), b->dest().c_str());

        BundleDaemon::post(new BundleReceivedEvent(b, EVENTSRC_APP));

        // return the creation timestamp (can use with source EID to
        // create a globally unique bundle identifier
        resultf("%zu.%zu", b->creation_ts().secs_or_millisecs_, b->creation_ts().seqno_);
        return TCL_OK;
        
    } else if (!strcmp(cmd, "stats")) {
        oasys::StringBuffer buf;
        BundleDaemon::instance()->get_bundle_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;

    } else if ((!strcmp(cmd, "daemon_stats")) || (!strcmp(cmd, "dstats"))) {
        oasys::StringBuffer buf("Bundle Daemon Statistics: ");
        BundleDaemon::instance()->get_daemon_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    } else if (!strcmp(cmd, "daemon_status")) {
        BundleDaemon::post_and_wait(new StatusRequest(),
                                    CompletionNotifier::notifier());
        set_result("DTN daemon ok");
        return TCL_OK;
    } else if (!strcmp(cmd, "reset_stats")) {
        BundleDaemon::instance()->reset_stats();
        return TCL_OK;
        
    } else if (!strcmp(cmd, "list")) {
        if (argc > 4) {
            resultf("too many arguments - "
                    "see \"help bundle\" for parameters\n");
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;
        all_bundles_t* all_bundles = BundleDaemon::instance()->all_bundles();

        bool default_cmd = false;
        bool list_all = false;
        bool list_last_bundle = false;
        size_t start_count = 0;
        size_t count = 0;


        oasys::ScopeLock l(all_bundles->lock(), "BundleCommand::exec");

        if (all_bundles->empty()) {
            resultf("No bundles pending\n");
            return TCL_OK;
        }



        if (argc >= 3) {
            list_all = false;

            const char* val = argv[2];

            if ((argc == 3) && (0 == strcmp(val, "all"))) {
                list_all = true;
                buf.appendf("All Bundles (%zu): \n", all_bundles->size());
            } else {
                char* endptr = 0;

                int len = strlen(val);
                count = strtoul(val, &endptr, 0);
                if (endptr != (val + len)) {
                    resultf("invalid 3rd value '%s'\n"
                            "see \"help bundle\" for parameters\n",
                            val);
                    return TCL_ERROR;
                }

                if (argc == 4) {
                    start_count = count;

                    val = argv[3];

                    len = strlen(val);
                    count = strtoul(val, &endptr, 0);
                    if (endptr != (val + len)) {
                        resultf("invalid 4th value '%s'\n"
                                "see 'help bundle' for parameters\n",
                                val);
                        return TCL_ERROR;
                    }

                    buf.appendf("%zu Bundles (of %zu) starting with #%zu: \n", count, all_bundles->size(), start_count+1);
                } else {
                    list_last_bundle = true;
                    buf.appendf("First %zu Bundles (of %zu): \n", count, all_bundles->size());
                }
            }
        } else {
            // command = "bundle list" - deafult to 1st 10 bundles
            default_cmd = true;
            count = 10;
            list_last_bundle = true;
            buf.appendf("First %zu Bundles (of %zu): \n", count, all_bundles->size());
        }



        Bundle* b;
        all_bundles_t::iterator iter;
        pending_bundles_t* pending = BundleDaemon::instance()->pending_bundles();
        custody_bundles_t* custody = BundleDaemon::instance()->custody_bundles();

        size_t bundle_count = 0;
        size_t end_count = start_count + count - 1;

        for (iter = all_bundles->begin(); iter != all_bundles->end(); ++iter) {
            b = iter->second;  // for <map> lists

            //if ( b->bundleid() >= (unsigned) startbundleid && b->bundleid() <= (unsigned) endbundleid )
            if ( list_all || ((bundle_count >= start_count) && (bundle_count <= end_count)))
            {
              buf.appendf("\t%-3" PRIbid ": %s -> %s length %zu%s%s%s\n",
                  b->bundleid(),
                  b->source().c_str(),
                  b->dest().c_str(),
                  b->payload().length(),
                  pending->contains(b) ? "" : " (NOT PENDING)",
                  custody->contains(b) ? " (Custodian)" : "",
                  b->is_bpv6() ? " (BPv6)" : " (BPv7)"
                  );
            }

            if ( !list_all && (++bundle_count > end_count)) {
                break;
            }
        }

        if (list_last_bundle) {
          
            buf.appendf("\nLast bundle:\n");

            BundleRef bref = all_bundles->back();
            if (bref != NULL) {
                b = bref.object();

                buf.appendf("\t%-3" PRIbid ": %s -> %s length %zu%s%s%s\n",
                    b->bundleid(),
                    b->source().c_str(),
                    b->dest().c_str(),
                    b->payload().length(),
                    pending->contains(b) ? "" : " (NOT PENDING)",
                    custody->contains(b) ? " (Custodian)" : "",
                  b->is_bpv6() ? " (BPv6)" : " (BPv7)"
                    );
            }
        }


        buf.appendf("\nTotal bundles: %zu  pending: %zu  custody: %zu    (max bundles at one time was: %zu)\n\n", 
                    all_bundles->size(), pending->size(), custody->size(), all_bundles->max_size());

        if (default_cmd) {
            buf.appendf("(use command \"bundle list all\" if you really want to list all bundles)");
        }

        set_result(buf.c_str());

        return TCL_OK;
        
    } else if (!strcmp(cmd, "ids")) {
        all_bundles_t::iterator iter;
        all_bundles_t* all_bundles = BundleDaemon::instance()->all_bundles();
        
        oasys::ScopeLock l(all_bundles->lock(), "BundleCommand::exec");
    
        for (iter = all_bundles->begin(); iter != all_bundles->end(); ++iter) {
            append_resultf("%" PRIbid " ", (iter->second)->bundleid()); // for <map> lists
        }
        
        return TCL_OK;
        
    } else if (!strcmp(cmd, "info") ||
               !strcmp(cmd, "dump") ||
               !strcmp(cmd, "dump_tcl") ||
               !strcmp(cmd, "dump_ascii") ||
               !strcmp(cmd, "expire"))
    {
        // bundle [info|dump|dump_ascii|expire] <id>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        uint64_t bundleid;
        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        bundleid = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <bundleid> value '%s'\n", val);
            return TCL_ERROR;
        }

        all_bundles_t* all_bundles = BundleDaemon::instance()->all_bundles();
        
        BundleRef bundle = all_bundles->find(bundleid);

        if (bundle == NULL) {
            resultf("no bundle with id %" PRIbid, bundleid);
            return TCL_ERROR;
        }

        if (strcmp(cmd, "info") == 0) {
            oasys::StringBuffer buf;
            bundle->format_verbose(&buf);
            set_result(buf.c_str());

        } else if (strcmp(cmd, "dump_tcl") == 0) {
            Tcl_Obj* result = NULL;
            int ok =
                TclRegistration::parse_bundle_data(interp, bundle, &result);
            
            set_objresult(result);
            return ok;
            
        } else if (strcmp(cmd, "dump_ascii") == 0) {
            size_t payload_len = bundle->payload().length();
            oasys::StringBuffer buf(payload_len);
            const u_char* bp = 
                bundle->payload().read_data(0, payload_len, (u_char*)buf.data());

            size_t length = strlen((const char*)bp);
            buf.append((const char*)bp, length);
            set_result(buf.c_str());
            
        } else if (strcmp(cmd, "dump") == 0) {
            size_t len = bundle->payload().length();
            oasys::HexDumpBuffer buf(len);
            
            bundle->payload().read_data(0, len, (u_char*)buf.tail_buf(len));
            buf.incr_len(len);
            
            set_result(buf.hexify().c_str());
            
        } else if (strcmp(cmd, "expire") == 0) {
            BundleDaemon::instance()->post_at_head(
                new BundleExpiredEvent(bundle.object()));
            return TCL_OK;
        }
        
        return TCL_OK;

    } else if (!strcmp(cmd, "cancel")) {
        // bundle cancel <id> <link>
        if (argc != 3 && argc != 4) {
            wrong_num_args(argc, argv, 2, 3, 4);
            return TCL_ERROR;
        }

        uint64_t bundleid;
        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        bundleid = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <bundleid> value '%s'\n", val);
            return TCL_ERROR;
        }

        const char* name = "";
        if (argc == 4) {
            name = argv[3];
        }

        BundleRef bundle
            = BundleDaemon::instance()->all_bundles()->find(bundleid);
        if (bundle != NULL) {
            bundle = BundleDaemon::instance()->pending_bundles()->find(bundleid);
        }

        if (bundle == NULL) {
            resultf("no pending bundle with id %" PRIbid, bundleid);
            return TCL_ERROR;
        }

        BundleDaemon::instance()->post_at_head(
                new BundleCancelRequest(bundle, name));
        
        return TCL_OK;

    } else if (!strcmp(cmd, "clear_fwdlog")) {
        // bundle clear_fwdlog <id>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        uint64_t bundleid;
        const char* val = argv[4];
        size_t len = strlen(val);
        char* endptr = 0;

        bundleid = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <bundleid> value '%s'\n", val);
            return TCL_ERROR;
        }

        BundleRef bundle
            = BundleDaemon::instance()->all_bundles()->find(bundleid);
        if (bundle != NULL) {
            bundle = BundleDaemon::instance()->pending_bundles()->find(bundleid);
        }

        if (bundle == NULL) {
            resultf("no pending bundle with id %" PRIbid, bundleid);
            return TCL_ERROR;
        }

        bundle->fwdlog()->clear();
        
        return TCL_OK;

    } else if (!strcmp(cmd, "daemon_idle_shutdown")) {
        oasys::StringBuffer buf("Bundle Daemon Statistics: ");
        
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        int interval;
        const char* val = argv[2];
        size_t len = strlen(val);
        char* endptr = 0;

        interval = strtoul(val, &endptr, 0);
        if (endptr != (val + len)) {
            resultf("invalid <secs> value '%s'\n", val);
            return TCL_ERROR;
        }

        BundleDaemon::instance()->init_idle_shutdown(interval);
        return TCL_OK;
        
    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}


} // namespace dtn
