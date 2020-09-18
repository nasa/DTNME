#
#    Copyright 2006 Intel Corporation
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

test::name custody-transfer
net::num_nodes 4
set clayer tcp
foreach {var val} $opt(opts) {
    if {$var == "-cl" } {
        set clayer $val
    }
}


dtn::config

dtn::config_interface $clayer
dtn::config_linear_topology OPPORTUNISTIC $clayer true

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *
    
    testlog "waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set N [net::num_nodes]
    set src dtn://host-0/test
    set dst dtn://host-[expr $N - 1]/test
    set src_route dtn://host-0/*
    set dst_route dtn://host-[expr $N - 1]/*
    
    for {set i 0} {$i < $N} {incr i} {
	dtn::tell_dtnd $i tcl_registration dtn://host-$i/test
    }

    testlog "*"
    testlog "* Test 1: basic custody transfer"
    testlog "*"
    
    testlog "sending bundle with custody transfer"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]

    testlog "checking that node 0 got the bundle"
    dtn::wait_for_bundle_stats 0 {1 received 0 transmitted}
    dtn::wait_for_bundle_stats 1 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}

    testlog "checking that node 0 took custody"
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "opening link from 0-1"
    dtn::tell_dtnd 0 link open $clayer-link:0-1
    dtn::wait_for_link_state 0 $clayer-link:0-1 OPEN

    testlog "checking that custody was transferred"
    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "open the rest of the links, check delivery"
    dtn::tell_dtnd 1 link open $clayer-link:1-2
    dtn::tell_dtnd 2 link open $clayer-link:2-3

    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "*"
    testlog "* Test 2: custody timer retransmission"
    testlog "*"

    dtn::tell_dtnd * bundle reset_stats

    set custody_timer_opts "custody_timer_min=5 custody_timer_lifetime_pct=0"

    testlog "removing route from node 1"
    dtn::tell_dtnd 1 route del $dst_route

    testlog "sending another bundle, checking that node 1 has custody"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]
    
    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "adding route to null on node 1, checking transmitted"
    eval dtn::tell_dtnd 1 route add $dst_route null $custody_timer_opts

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 2 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "waiting for a couple retransmissions"

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 4 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "switching route back, waiting for retransmission and delivery"
    dtn::tell_dtnd 1 route del $dst_route
    eval dtn::tell_dtnd 1 route add $dst_route $clayer-link:1-2 $custody_timer_opts

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {2 received 5 transmitted}
    dtn::wait_for_bundle_stats 2 {2 received 2 transmitted}
    dtn::wait_for_bundle_stats 3 {1 received 1 transmitted}
    
    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "*"
    testlog "* Test 3: duplicate detection"
    testlog "*"

    dtn::tell_dtnd * bundle reset_stats

    set custody_timer_opts "custody_timer_min=5 custody_timer_lifetime_pct=0"

    testlog "speeding up custody timer for route on node 0"
    dtn::tell_dtnd 0 route del $dst_route
    eval dtn::tell_dtnd 0 route add $dst_route $clayer-link:0-1 $custody_timer_opts
    
    testlog "removing destination route for node 1"
    dtn::tell_dtnd 1 route del $dst_route
    
    testlog "switching reverse route for node 1 to null"
    dtn::tell_dtnd 1 route del $src_route
    dtn::tell_dtnd 1 route add $src_route null

    testlog "sending another bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]

    testlog "checking that node 1 and node 0 both have custody"
    dtn::wait_for_bundle_stats 0 {1 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "waiting for a retransmission, making sure duplicate is deleted"
    dtn::wait_for_bundle_stats 0 {1 received 2 transmitted}
    dtn::wait_for_bundle_stats 1 {2 received 2 generated 2 transmitted}
    
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}

    testlog "flipping back the route from 1 to 0"
    dtn::tell_dtnd 1 route del $src_route
    dtn::tell_dtnd 1 route add $src_route $clayer-link:1-0

    testlog "waiting for another retransmission, checking custody transferred"
    dtn::wait_for_bundle_stats 0 {2 received 3 transmitted}
    dtn::wait_for_bundle_stats 1 {3 received 3 generated 3 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}

    testlog "adding route back to 1 and waiting for delivery"
    dtn::tell_dtnd 1 route del $dst_route
    dtn::tell_dtnd 1 route add $dst_route $clayer-link:1-2

    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    testlog "*"
    testlog "* Test 4: bundle expiration with custody"
    testlog "*"
    dtn::tell_dtnd * bundle reset_stats

    testlog "removing route for node 1"
    dtn::tell_dtnd 1 route del $dst_route

    testlog "sending bundle, checking custody transfer"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody expiration=10]
    
    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody 1 delivered}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody 0 delivered}

    testlog "waiting for bundle to expire"
    dtn::wait_for_bundle_stats 1 {1 expired}

    testlog "checking that custody was cleaned up"
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}

    testlog "checking that expiration status report was delivered"
    dtn::wait_for_bundle_stats 0 {2 delivered}

    testlog "re-adding route"
    dtn::tell_dtnd 1 route add $dst_route $clayer-link:1-2

    testlog "*"
    testlog "* Test 5: delivery before taking custody"
    testlog "*"

    dtn::tell_dtnd * bundle reset_stats

    testlog "disabling custody acceptance on node 0"
    dtn::tell_dtnd 0 param set accept_custody 0

    testlog "sending a bundle to a nonexistant registration"
    set dst2 "dtn://host-0/test2"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst2 custody expiration=10]

    testlog "checking that it is pending with no custodian"
    dtn::wait_for_bundle_stats 0 {1 received 1 pending 0 custody}

    testlog "adding a registration, checking delivery"
    dtn::tell_dtnd 0 tcl_registration $dst2
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody 1 delivered}
    
    # XXX/TODO: add support / test cases for:
    #
    # restart posts a new custody timer
    # noroute timer
    # no contact timer
    # retransmission over an ONDEMAND link that's gone idle
    # multiple routes, ensure retransmission only on one
    # race between bundle delete and custody timer cancelling
    
    testlog "test success!"
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
