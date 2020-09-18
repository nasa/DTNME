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

test::name no-duplicate-send
net::num_nodes 3

dtn::config

set clayer tcp
set length 5000

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_linear_topology ALWAYSON $clayer true
conf::add dtnd * "param set early_deletion false"

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest
    
    testlog "Sending bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest length=$length]
    
    testlog "Waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 30

    testlog "Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    is_admin 0 source $source dest $dest
    
    testlog "Checking that bundle was received"
    dtn::wait_for_bundle_stat 0 1 received
    dtn::wait_for_bundle_stat 1 1 transmitted
    dtn::wait_for_bundle_stat 2 1 transmitted

    testlog "Checking that bundle still pending"
    dtn::wait_for_bundle_stat 0 1 pending
    dtn::wait_for_bundle_stat 1 1 pending
    dtn::wait_for_bundle_stat 2 1 pending

    testlog "Stopping and restarting link"
    dtn::tell_dtnd 2 link close tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE
    dtn::tell_dtnd 2 link open tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    testlog "Checking that bundle was not retransmitted"
    dtn::wait_for_bundle_stat 0 1 received
    dtn::wait_for_bundle_stat 1 1 transmitted
    dtn::wait_for_bundle_stat 2 1 transmitted
    
    dtn::wait_for_bundle_stat 0 1 pending
    dtn::wait_for_bundle_stat 1 1 pending
    dtn::wait_for_bundle_stat 2 1 pending
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
