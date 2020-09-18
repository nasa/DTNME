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

#include "oasys/thread/SpinLock.h"

#include "ParamCommand.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundlePayload.h"
#include "bundling/CustodyTimer.h"
#include "naming/EndpointID.h"

namespace dtn {

ParamCommand::ParamCommand() 
    : TclCommand("param")
{
    bind_var(new oasys::BoolOpt("payload_test_no_remove",
                                &BundlePayload::test_no_remove_,
                                "Boolean to control not removing bundles "
                                "(for testing)."));

    bind_var(new oasys::BoolOpt("early_deletion",
                                &BundleDaemon::params_.early_deletion_,
                                "Delete forwarded / delivered bundles "
                                "before they've expired "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("suppress_duplicates",
                                &BundleDaemon::params_.suppress_duplicates_,
                                "Do not route bundles that are a duplicate "
                                "of any currently pending bundle "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("accept_custody",
                                &BundleDaemon::params_.accept_custody_,
                                "Accept custody when requested "
                                "(default is true)"));
             
    bind_var(new oasys::BoolOpt("reactive_frag_enabled",
                                &BundleDaemon::params_.reactive_frag_enabled_,
                                "Is reactive fragmentation enabled "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("retry_reliable_unacked",
                                &BundleDaemon::params_.retry_reliable_unacked_,
                                "Retry unacked transmissions on reliable CLs "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("test_permuted_delivery",
                                &BundleDaemon::params_.test_permuted_delivery_,
                                "Permute the order of bundles before "
                                "delivering to registrations"));

    bind_var(new oasys::BoolOpt("injected_bundles_in_memory",
                                &BundleDaemon::params_.injected_bundles_in_memory_,
                                "Injected bundles are held in memory by default"
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("recreate_links_on_restart",
                                &BundleDaemon::params_.recreate_links_on_restart_,
                                "Recreate non-opportunistic links added "
                                "during previous runs \nof the DTN daemon "
                                "when restarting "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("persistent_links",
                                &BundleDaemon::params_.persistent_links_,
                                "Maintain Links and their stats in database (default is true)"));

    bind_var(new oasys::BoolOpt("persistent_fwd_logs",
                                &BundleDaemon::params_.persistent_fwd_logs_,
                                "Maintain Forwarding Logs in database (default is false)"));

    bind_var(new oasys::BoolOpt("clear_bundles_when_opp_link_unavailable",
                                &BundleDaemon::params_.clear_bundles_when_opp_link_unavailable_,
                                "Clear bundles when opportunistic link goes unavailable (default is true)"));

    static oasys::EnumOpt::Case IsSingletonCases[] = {
        {"unknown",   EndpointID::UNKNOWN},
        {"singleton", EndpointID::SINGLETON},
        {"multinode", EndpointID::MULTINODE},
        {0, 0}
    };
    
    bind_var(new oasys::EnumOpt("is_singleton_default",
                                IsSingletonCases,
                                (int*)&EndpointID::is_singleton_default_,
                                "<unknown|singleton|multinode>",
                                "How to set the is_singleton bit for "
                                "unknown schemes"));
    
    bind_var(new oasys::BoolOpt("glob_unknown_schemes",
                                &EndpointID::glob_unknown_schemes_,
                                "Whether unknown schemes use glob-based matching for "
                                "registrations and routes"));
    
    bind_var(new oasys::UIntOpt("link_min_retry_interval",
                                &Link::default_params_.min_retry_interval_,
                                "interval",
                                "Default minimum connection retry "
                                "interval for links"));

    bind_var(new oasys::UIntOpt("link_max_retry_interval",
                                &Link::default_params_.max_retry_interval_,
                                "interval",
                                "Default maximum connection retry "
                                "interval for links"));

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

    bind_var(new oasys::BoolOpt("announce_ipn",
                                &BundleDaemon::params_.announce_ipn_,
                                "announce eid_ipn or announce eid "
                                "on connection "
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("serialize_apireg_bundle_lists",
                                &BundleDaemon::params_.serialize_apireg_bundle_lists_,
                                "write API Registration bundle lists to the database"
                                " (default is false)"));

    bind_var(new oasys::UInt64Opt("ipn_echo_service_number",
                                  &(BundleDaemon::params_.ipn_echo_service_number_), 
                                  "service", " should be greater than 0."));

    bind_var(new oasys::UInt64Opt("ipn_echo_max_return_length",
                                  &(BundleDaemon::params_.ipn_echo_max_return_length_),
                                  "service", " should be greater than 0."));

    // - can be renable with "param set warn_on_spinlock_contention true" command
    bind_var(new oasys::BoolOpt("warn_on_spinlock_contention",
                                &oasys::SpinLock::warn_on_contention_,
                                "enable/disable SpinLock contention warnings (default is false)"));

}
    
} // namespace dtn
