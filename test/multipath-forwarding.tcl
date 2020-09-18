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

test::name multipath-forwarding
net::num_nodes 4

dtn::config

set clayer   tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_diamond_topology ALWAYSON $clayer true

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    dtn::tell_dtnd 3 tcl_registration dtn://host-3/test
    
    testlog "Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 0 tcp-link:0-2 OPEN

    testlog "adding write delay to node 0 links"
    tell_dtnd 0 link reconfigure tcp-link:0-1 \
	    test_write_delay=500 sendbuf_len=1024
    tell_dtnd 0 link reconfigure tcp-link:0-2 \
	    test_write_delay=500 sendbuf_len=1024

    testlog "*"
    testlog "* Test 1: equal size bundles, equal priority routes"
    testlog "*"

    testlog "sending 20 bundles"
    for {set i 0} {$i < 20} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test length=2048
    }

    testlog "waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 20 transmitted} 60
    
    testlog "checking they were load balanced"
    dtn::check_link_stats 0 tcp-link:0-1 10 bundles_transmitted
    dtn::check_link_stats 0 tcp-link:0-2 10 bundles_transmitted

    testlog "waiting for delivery"
    dtn::wait_for_bundle_stats 1 {0 pending 10 received 10 transmitted}
    dtn::wait_for_bundle_stats 2 {0 pending 10 received 10 transmitted}
    dtn::wait_for_bundle_stats 3 {0 pending 20 delivered 0 duplicate}
    
    dtn::tell_dtnd * bundle reset_stats
    
    testlog "*"
    testlog "* Test 2: equal size bundles, different priority routes"
    testlog "*"

    # XXX/demmer fixme
    if {1} {
        testlog "WARNING: priority-based forwarding no longer works"
    } else {
        
    testlog "swapping routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1 route_priority=100
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2 route_priority=0
    
    testlog "sending 20 bundles"
    for {set i 0} {$i < 20} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test length=2048
    }

    testlog "waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 20 transmitted} 60
    
    testlog "checking they all went on one link"
    dtn::wait_for_link_stats 0 tcp-link:0-1 {20 bundles_transmitted}
    dtn::wait_for_link_stats 0 tcp-link:0-2 {0  bundles_transmitted}

    testlog "waiting for delivery"
    dtn::wait_for_bundle_stats 1 {0 pending 20 received 20 transmitted}
    dtn::wait_for_bundle_stats 2 {0 pending 0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 pending 20 delivered 0 duplicate}

    testlog "swapping back routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2
    
    dtn::tell_dtnd * bundle reset_stats
    }
    
    testlog "*"
    testlog "* Test 3: copy routes"
    testlog "*"

    testlog "swapping routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1 route_priority=100 
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2 \
	    route_priority=0 action=copy

    testlog "sending 20 bundles (to different destination)"
    for {set i 0} {$i < 20} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test2 length=2048
    }

    testlog "waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 40 transmitted} 60
    
    testlog "checking they all went on both links"
    dtn::wait_for_link_stats 0 tcp-link:0-1 {20 bundles_transmitted}
    dtn::wait_for_link_stats 0 tcp-link:0-2 {20 bundles_transmitted}

    testlog "waiting for arrival"
    dtn::wait_for_bundle_stats 1 {0 pending 20 received 20 transmitted 0 duplicate}
    dtn::wait_for_bundle_stats 2 {0 pending 20 received 20 transmitted 0 duplicate}
    dtn::wait_for_bundle_stats 3 {20 pending 0 delivered 20 duplicate}

    testlog "adding registration, waiting for delivery"
    dtn::tell_dtnd 3 tcl_registration dtn://host-3/test2
    dtn::wait_for_bundle_stats 3 {0 pending 20 delivered 20 duplicate}
    
    dtn::tell_dtnd * bundle reset_stats
    
    testlog "*"
    testlog "* Test 4: wildcard destination"
    testlog "*"

    testlog "adding different priority routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1 route_priority=100 
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2 route_priority=0

    testlog "sending 20 bundles to dummy wildcard destination"
    for {set i 0} {$i < 20} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://*/nowhere length=2048
    }

    # XXX/demmer it would be better to have a registration that these
    # are going to, but the problem is that we don't keep track of all
    # duplicates in the core bundle daemon, so it's non-deterministic
    # how many will actually get delivered

    testlog "waiting for all to be transmitted on both links"
    dtn::wait_for_bundle_stats 0 {0 pending 0 delivered 40 transmitted} 60
    dtn::wait_for_link_stats 0 tcp-link:0-1 {20 bundles_transmitted}
    dtn::wait_for_link_stats 0 tcp-link:0-2 {20 bundles_transmitted}

    dtn::tell_dtnd * bundle reset_stats
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
