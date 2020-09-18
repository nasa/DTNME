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

test::name reroute-topology
net::num_nodes 4

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
    set dest      dtn://host-3/test
    dtn::tell_dtnd 3 tcl_registration $dest

    set addr0 $net::internal_host(3):[dtn::get_port $cl 0]
    set addr1 $net::internal_host(3):[dtn::get_port $cl 1]
    set addr2 $net::internal_host(3):[dtn::get_port $cl 2]
    set addr3 $net::internal_host(3):[dtn::get_port $cl 3]
    
    testlog "waiting for links to establish"
    dtn::wait_for_link_state 0 null OPEN
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-0 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-2 OPEN
    dtn::wait_for_link_state 2 $cl-link:2-1 OPEN
    dtn::wait_for_link_state 2 $cl-link:2-3 OPEN
    dtn::wait_for_link_state 3 $cl-link:3-2 OPEN

    testlog "closing link from 2-3"
    tell_dtnd 2 link close $cl-link:2-3
    dtn::wait_for_link_state 2 $cl-link:2-3 UNAVAILABLE
    
    testlog "adding a second link from 2-3 and a route to it"
    tell_dtnd 2 link add alt-$cl-link:2-3 $addr3 OPPORTUNISTIC $cl remote_eid=dtn://host-3
    tell_dtnd 2 route add dtn://host-3/* alt-$cl-link:2-3

    testlog "sending a bundle"
    set timestamp1 [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "checking that it's deferred on both links"
    dtn::wait_for_bundle_stats 2 {1 pending}
    dtn::wait_for_link_stats 2 $cl-link:2-3 {1 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {1 bundles_deferred}

    testlog "opening one link, checking that it gets sent"
    tell_dtnd 2 link open alt-$cl-link:2-3
    dtn::wait_for_bundle_stats 2 {0 pending}
    dtn::wait_for_link_stats 2 $cl-link:2-3 {0 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {0 bundles_deferred 1 bundles_transmitted}

    testlog "waiting for delivery"
    dtn::wait_for_bundle_stats 3 {1 delivered}

    testlog "checking it was deleted everywhere"
    dtn::check_bundle_stats 0 {0 pending}
    dtn::check_bundle_stats 1 {0 pending}
    dtn::check_bundle_stats 2 {0 pending}
    dtn::check_bundle_stats 3 {0 pending}

    tell_dtnd * bundle reset_stats

    testlog "closing the link again"
    tell_dtnd 2 link close alt-$cl-link:2-3
    dtn::wait_for_link_state 2 $cl-link:2-3 UNAVAILABLE

    testlog "sending another bundle"
    set timestamp2 [dtn::tell_dtnd 0 sendbundle $source $dest expiration=5]

    testlog "checking that it's deferred on both links"
    dtn::wait_for_bundle_stats 2 {1 pending}
    dtn::wait_for_link_stats 2 $cl-link:2-3 {1 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {1 bundles_deferred}

    testlog "checking that only one copy exists"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending}
    dtn::wait_for_bundle_stats 3 {0 pending}

    testlog "waiting for it to expire"
    dtn::wait_for_bundle_stats 2 {1 expired}
    dtn::wait_for_bundle_stats 2 {0 pending}
    dtn::wait_for_link_stats 2 $cl-link:2-3 {0 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {0 bundles_deferred}

    tell_dtnd * bundle reset_stats
    
    testlog "sending another set of bundles, waiting for them to be deferred on both links"
    dtn::tell_dtnd 0 sendbundle $source $dest expiration=120
    dtn::tell_dtnd 0 sendbundle $source $dest expiration=120
    dtn::tell_dtnd 0 sendbundle $source $dest expiration=120
    dtn::tell_dtnd 0 sendbundle $source $dest expiration=120
    dtn::tell_dtnd 0 sendbundle $source $dest expiration=120
    
    dtn::wait_for_bundle_stats 2 {5 pending}
    dtn::wait_for_link_stats 2 $cl-link:2-3 {5 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {5 bundles_deferred}

    testlog "adding links between node 1 and node 3"
    tell_dtnd 1 link add $cl-link:1-3 $addr3 opportunistic tcp remote_eid=dtn://host-3
    tell_dtnd 3 link add $cl-link:3-1 $addr1 opportunistic tcp remote_eid=dtn://host-1
    tell_dtnd 1 link open $cl-link:1-3
    dtn::wait_for_link_state 1 $cl-link:1-3 OPEN
    dtn::wait_for_link_state 3 $cl-link:3-1 OPEN

    testlog "switching routes on node 1 to node 3 and vice versa"
    tell_dtnd 1 route del dtn://host-3/*
    tell_dtnd 1 route add dtn://host-3/* $cl-link:1-3

    tell_dtnd 3 route del dtn://host-1/*
    tell_dtnd 3 route add dtn://host-1/* $cl-link:3-1

    testlog "switching routes on node 2"
    tell_dtnd 2 route del dtn://host-3/*
    tell_dtnd 2 route add dtn://host-3/* $cl-link:2-1

    testlog "checking that bundles were properly sent from node 2"
    dtn::wait_for_link_stats 2 $cl-link:2-3 {0 bundles_deferred}
    dtn::wait_for_link_stats 2 alt-$cl-link:2-3 {0 bundles_deferred}

    testlog "checking that bundles were delivered"
    dtn::wait_for_bundle_stats 3 {5 delivered}

    testlog "making sure no bundles are pending"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending}
    dtn::wait_for_bundle_stats 2 {0 pending}
    dtn::wait_for_bundle_stats 3 {0 pending}
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
