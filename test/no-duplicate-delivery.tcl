#
#    Copyright 2007 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

test::name no-duplicate-delivery
net::num_nodes 2

set testopt(cl)       tcp
run::parse_test_opts

set cl $testopt(cl)

dtn::config
dtn::config_interface $cl

dtn::config_linear_topology ALWAYSON $cl true

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *

    testlog "waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    dtn://host-0/test
    set dest      dtn://host-1/test
    dtn::tell_dtnd 1 tcl_registration $dest

    testlog "waiting for links to establish"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-0 OPEN

    testlog "telling node 0 not to delete bundles"
    tell_dtnd 0 "param set early_deletion false"

    testlog "sending a bundle"
    set timestamp1 [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "checking that it was delivered but still pending"
    dtn::wait_for_bundle_stats 0 {1 pending 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 delivered}

    testlog "clearing the forwarding log and rerouting"
    tell_dtnd 0 bundle clear_fwdlog 0
    tell_dtnd 0 route recompute_routes

    dtn::wait_for_bundle_stats 0 {1 pending  2 transmitted}
    dtn::wait_for_bundle_stats 1 {2 received 1 delivered}
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
