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

test::name reroute-from-down-link
net::num_nodes 2

set testopt(cl)       tcp
set testopt(downtime) 2
run::parse_test_opts

set cl $testopt(cl)

dtn::config
dtn::config_interface $cl

dtn::config_linear_topology ALWAYSON $cl false

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *

    testlog "waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    dtn://host-0/test
    set dest      dtn://host-1/test

    dtn::tell_dtnd 1 tcl_registration $dest

    testlog "waiting for links to establish"
    dtn::wait_for_link_state 0 null OPEN
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "adding a higher priority route to the null link"
    dtn::tell_dtnd 0 route add $dest null route_priority=100

    testlog "reconfiguring null link to not send bundles"
    dtn::tell_dtnd 0 link reconfigure null \
            can_transmit=false potential_downtime=$testopt(downtime)

    testlog "sending a couple bundles"
    set timestamp1 [dtn::tell_dtnd 0 sendbundle $source $dest]
    set timestamp2 [dtn::tell_dtnd 0 sendbundle $source $dest]
    set timestamp3 [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "checking that bundles are queued on null link"
    dtn::wait_for_bundle_stats 0 {3 pending 0 transmitted}
    dtn::wait_for_link_stats 0 null {3 bundles_queued 0 bundles_transmitted}
    dtn::check_link_stats 0 $cl-link:0-1 {0 bundles_queued 0 bundles_transmitted}

    testlog "removing the route from the null link, checking bundles are still queued"
    dtn::tell_dtnd 0 route del $dest
    dtn::tell_dtnd 0 route add $dest $cl-link:0-1
    dtn::check_bundle_stats 0 {3 pending 0 transmitted}
    dtn::check_link_stats 0 null {3 bundles_queued 0 bundles_transmitted}
    dtn::check_link_stats 0 $cl-link:0-1 {0 bundles_queued 0 bundles_transmitted}

    testlog "closing null link, checking that bundles are still queued"
    dtn::tell_dtnd 0 link close null
    dtn::wait_for_link_state 0 null UNAVAILABLE
    dtn::wait_for_bundle_stats 0 {3 pending 0 transmitted}
    dtn::check_link_stats 0 null {3 bundles_queued}

    testlog "waiting for reroute timer to expire"
    after [expr $testopt(downtime) * 1000]

    testlog "checking that bundles were rerouted and sent"
    dtn::wait_for_link_stats 0 null         {0 bundles_queued 3 bundles_cancelled}
    dtn::wait_for_link_stats 0 $cl-link:0-1 {3 bundles_transmitted}

    testlog "checking bundle stats"
    dtn::wait_for_bundle_stats 0 {0 pending 3 transmitted}
    dtn::wait_for_bundle_stats 1 {0 pending 3 received 3 delivered}

    # XXX/demmer add a test in which the null link comes back
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
