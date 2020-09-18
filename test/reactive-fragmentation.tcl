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

test::name reactive-fragmentation
net::num_nodes 2

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true \
	"test_write_delay=1000 segment_length=512"

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    dtn::tell_dtnd 1 tcl_registration dtn://host-1/test

    testlog "Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN

    testlog "Sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle \
	    dtn://host-0/test dtn://host-1/test length=8096 expiration=3600]

    for {set i 0} {$i < 5} {incr i} {
	set wait [expr 2000 + int(2500 * rand())]
	testlog "Waiting [expr $wait / 1000] seconds"
	after $wait
	
	testlog "Interrupting the link"
	dtn::tell_dtnd 0 link close tcp-link:0-1 
	dtn::wait_for_link_state 0 tcp-link:0-1 UNAVAILABLE
	dtn::tell_dtnd 0 link open tcp-link:0-1
	dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    }

    testlog "Waiting for the bundle to arrive and to be reassembled"
    dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp" 30

    testlog "Checking that no bundles are pending"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending 1 delivered}

    testlog "Checking that it really did fragment"
    set stats [dtn::tell_dtnd 1 bundle stats]
    regexp {(\d+) received} $stats match received
    if {$received == 1} {
	error "only one bundle received"
    }
    testlog "$received fragments received"

    testlog "Resetting stats"
    tell_dtnd 0 bundle reset_stats
    tell_dtnd 1 bundle reset_stats

    testlog "Disabling reactive fragmentation on node 1"
    tell_dtnd 0 link reconfigure tcp-link:0-1 reactive_frag_enabled=0

    testlog "Sending another bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle \
	    dtn://host-0/test dtn://host-1/test length=8096 expiration=3600]

    for {set i 0} {$i < 5} {incr i} {
	set wait [expr 2000 + int(2500 * rand())]
	testlog "Waiting [expr $wait / 1000] seconds"
	after $wait
	
	testlog "Interrupting the link"
	dtn::tell_dtnd 0 link close tcp-link:0-1 
	dtn::wait_for_link_state 0 tcp-link:0-1 UNAVAILABLE
	dtn::tell_dtnd 0 link open tcp-link:0-1
	dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    }

    testlog "Waiting for the bundle to arrive and to be reassembled"
    dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp" 30

    testlog "Checking that no bundles are pending"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending 1 delivered}

    testlog "Checking that it really did fragment"
    set stats [dtn::tell_dtnd 1 bundle stats]
    regexp {(\d+) received} $stats match received
    if {$received == 1} {
	error "only one bundle received"
    }
    testlog "$received fragments received"

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
