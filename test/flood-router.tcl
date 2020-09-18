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

test::name flood-router
net::num_nodes 5

dtn::config --no_null_link

set cl   tcp
set linktype ALWAYSON

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set cl $val
    } elseif {$var == "-linktype" || $var == "linktype"} {
        set linktype $val	
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Configuring $cl interfaces / links"

set dtn::router_type "flood"

dtn::config_interface $cl
dtn::config_diamond_topology $linktype $cl false

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    if {$linktype == "ALWAYSON"} {
	testlog "Waiting for links to open"
	dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
	dtn::wait_for_link_state 0 $cl-link:0-2 OPEN
    }
    
    testlog "injecting a bundle"
    tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test \
	    length=4096 expiration=10

    testlog "waiting for it to be flooded around"
    dtn::wait_for_bundle_stats 0 {1 pending 2 transmitted}
    dtn::wait_for_bundle_stats 1 {1 pending 1 transmitted}
    dtn::wait_for_bundle_stats 2 {1 pending 1 transmitted}
    dtn::wait_for_bundle_stats 3 {1 pending 1 transmitted}
    
    testlog "checking that duplicates were detected properly"
    dtn::check_bundle_stats 0 {0 duplicate}
    if {[dtn::test_bundle_stats 1 {0 duplicate}]} {
        dtn::check_bundle_stats 2 {1 duplicate}
    } else {
        dtn::check_bundle_stats 1 {1 duplicate}
        dtn::check_bundle_stats 2 {0 duplicate}
    }
    dtn::check_bundle_stats 3 {1 duplicate}

    testlog "adding links from node 4 to/from nodes 2 and 3"
    tell_dtnd 4 link add $cl-link:4-2 \
	    "$net::host(2):[dtn::get_port $cl 2]" $linktype $cl
    if {! [dtn::is_bidirectional $cl]} {
	tell_dtnd 2 link add $cl-link:2-4 \
		"$net::host(4):[dtn::get_port $cl 4]" $linktype $cl
    }
    tell_dtnd 4 link add $cl-link:4-3 \
	    "$net::host(3):[dtn::get_port $cl 3]" $linktype $cl
    if {! [dtn::is_bidirectional $cl]} {
	tell_dtnd 3 link add $cl-link:3-4 \
		"$net::host(4):[dtn::get_port $cl 4]" $linktype $cl
    }
    
    testlog "checking the bundle is flooded to node 4 and back out"
    dtn::wait_for_bundle_stats 4 {1 pending 1 transmitted 1 duplicate}
    
    testlog "waiting for bundle to expire everywhere "
    dtn::wait_for_bundle_stats 0 {0 pending 1 expired}
    dtn::wait_for_bundle_stats 1 {0 pending 1 expired}
    dtn::wait_for_bundle_stats 2 {0 pending 1 expired}
    dtn::wait_for_bundle_stats 3 {0 pending 1 expired}
    dtn::wait_for_bundle_stats 4 {0 pending 1 expired}

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
