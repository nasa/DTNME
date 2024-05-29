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

#include <third_party/oasys/thread/SpinLock.h>

#include "ParamCommand.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundlePayload.h"
#include "bundling/CustodyTimer.h"
#include "naming/EndpointID.h"

namespace dtn {

ParamCommand::ParamCommand() 
    : TclCommand("param")
{
    static oasys::EnumOpt::Case IsSingletonCases[] = {
        {"unknown",   EndpointID::UNKNOWN},
        {"singleton", EndpointID::SINGLETON},
        {"multinode", EndpointID::MULTINODE},
        {0, 0}
    };
    
    bind_var(new oasys::BoolOpt("accept_custody",
                                &BundleDaemon::params_.accept_custody_,
                                "Accept BPv6 custody when requested "
                                "(default is true)"));
             
    bind_var(new oasys::BoolOpt("announce_ipn",
                                &BundleDaemon::params_.announce_ipn_,
                                "Send/announce the local IPN EID or the DTN EID "
                                "on TCP CL connection "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("api_send_bp7",
                                &BundleDaemon::params_.api_send_bp_version7_,
                                "configure API to send BPv7 (true) or BPv6 (false) bundles "
                                "(default is true)"));

    bind_var(new oasys::SizeOpt("api_deliver_max_memory_size",
                                &BundleDaemon::params_.api_deliver_max_memory_size_,
                                "bytes",
                                "max size of a payload to deliver via memory (range: 0 to 100M; "
                                "default: 1M)"));

    bind_var(new oasys::BoolOpt("bard_quota_strict",
                                &BundleDaemon::params_.bard_quota_strict_,
                                "Reject bundles that exceed quota even if CL is unreliable "
                                "(default is true)"));
    
    bind_var(new oasys::BoolOpt("clear_bundles_when_opp_link_unavailable",
                                &BundleDaemon::params_.clear_bundles_when_opp_link_unavailable_,
                                "Clear bundles from opportunistic link when it goes unavailable "
                                "(default is true)"));

    bind_var(new oasys::UIntOpt("custody_timer_min",
                                &CustodyTimerSpec::defaults_.min_,
                                "min",
                                "default value for custody timer min"));
    
    bind_var(new oasys::UIntOpt("custody_timer_lifetime_pct",
                                &CustodyTimerSpec::defaults_.lifetime_pct_,
                                "pct",
                                "default value for custody timer "
                                "lifetime percentage"));
    
    bind_var(new oasys::UIntOpt("custody_timer_max",
                                &CustodyTimerSpec::defaults_.max_,
                                "max",
                                "default value for custody timer max"));

    bind_var(new oasys::BoolOpt("early_deletion",
                                &BundleDaemon::params_.early_deletion_,
                                "Delete forwarded / delivered bundles "
                                "before they've expired "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("keep_expired_bundles",
                                &BundleDaemon::params_.keep_expired_bundles_,
                                "Delete forwarded / delivered bundles "
                                "before they've expired "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("glob_unknown_schemes",
                                &EndpointID::glob_unknown_schemes_,
                                "Whether unknown schemes use glob-based matching for "
                                "registrations and routes"));
    
    bind_var(new oasys::BoolOpt("injected_bundles_in_memory",
                                &BundleDaemon::params_.injected_bundles_in_memory_,
                                "Injected bundles are held in memory by default"
                                "(default is false)"));

    bind_var(new oasys::UInt64Opt("ipn_echo_service_number",
                                  &(BundleDaemon::params_.ipn_echo_service_number_), 
                                  "service", 
                                  "IPN service number on which to run an echo service if "
                                  "greater than 0. Admin service 0 will always echo if it "
                                  "receives a bundle with \"ping\" detected n the payload as a string."));

    bind_var(new oasys::UInt64Opt("ipn_echo_max_return_length",
                                  &(BundleDaemon::params_.ipn_echo_max_return_length_),
                                  "bytes", 
                                  "Max number of bytes the echo service will respond with to the source EID"));

    bind_var(new oasys::EnumOpt("eid_dest_type_default",
                                IsSingletonCases,
                                (int*)&EndpointID::unknown_eid_dest_type_default_,
                                "<unknown | singleton | multinode>",
                                "How to set the is_singleton bit for "
                                "unknown schemes"));
    
    bind_var(new oasys::UIntOpt("link_min_retry_interval",
                                &Link::default_params_.min_retry_interval_,
                                "seconds",
                                "Default minimum connection retry "
                                "interval for links"));

    bind_var(new oasys::UIntOpt("link_max_retry_interval",
                                &Link::default_params_.max_retry_interval_,
                                "seconds",
                                "Default maximum connection retry "
                                "interval for links"));

    bind_var(new oasys::BoolOpt("payload_test_no_remove",
                                &BundlePayload::test_no_remove_,
                                "Boolean to control not removing bundles "
                                "(for testing)."));

    bind_var(new oasys::BoolOpt("persistent_fwd_logs",
                                &BundleDaemon::params_.persistent_fwd_logs_,
                                "Maintain Forwarding Logs in database (default is false)"));

    bind_var(new oasys::BoolOpt("persistent_links",
                                &BundleDaemon::params_.persistent_links_,
                                "Maintain Links and their stats in database (default is false)"));

    bind_var(new oasys::BoolOpt("reactive_frag_enabled",
                                &BundleDaemon::params_.reactive_frag_enabled_,
                                "Is reactive fragmentation enabled "
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("recreate_links_on_restart",
                                &BundleDaemon::params_.recreate_links_on_restart_,
                                "Recreate non-opportunistic links added "
                                "during previous runs \nof the DTN daemon "
                                "when restarting "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("retry_reliable_unacked",
                                &BundleDaemon::params_.retry_reliable_unacked_,
                                "Retry unacked transmissions on reliable CLs "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("serialize_apireg_bundle_lists",
                                &BundleDaemon::params_.serialize_apireg_bundle_lists_,
                                "write API Registration bundle lists to the database"
                                " (default is false)"));

    bind_var(new oasys::BoolOpt("suppress_duplicates",
                                &BundleDaemon::params_.suppress_duplicates_,
                                "Do not route bundles that are a duplicate "
                                "of any currently pending bundle "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("test_permuted_delivery",
                                &BundleDaemon::params_.test_permuted_delivery_,
                                "Permute the order of bundles before "
                                "delivering to registrations"));

    // - can be renable with "param set warn_on_spinlock_contention true" command
    bind_var(new oasys::BoolOpt("warn_on_spinlock_contention",
                                &oasys::SpinLock::warn_on_contention_,
                                "enable/disable SpinLock contention warnings (default is false)"));

    bind_var(new oasys::Octal16Opt("file_permissions",
                                  &BundleDaemon::params_.file_permissions_,
                                  "mode",
                                  "best specified as an octal value as for chmod (default: 0775)\n"
                                  "   NOTE: only takes effect if specifed in startup config file\n"
                                  "         and owner will always have full access"));
    
    bind_var(new oasys::StringOpt("console_type",
                                  &BundleDaemon::params_.console_type_,
                                  "type",
                                  "set the type of console you want to use.\n"
                                  "Options include raw and websocket\n"));
    
    bind_var(new oasys::StringOpt("console_chain",
                                  &BundleDaemon::params_.console_chain_,
                                  "file path",
                                  "set the location of the certificate chain.\n"
                                  "No default options\n"));

    bind_var(new oasys::StringOpt("console_key",
                                  &BundleDaemon::params_.console_key_,
                                  "file path",
                                  "set the location of the private key.\n"
                                  "No default options\n"));

    bind_var(new oasys::StringOpt("console_dh",
                                  &BundleDaemon::params_.console_dh_,
                                  "file path",
                                  "set the location of the DH parameters.\n"
                                  "No default options\n"));

}
    
} // namespace dtn

