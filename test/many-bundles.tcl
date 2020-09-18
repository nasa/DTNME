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

test::name many-bundles
net::num_nodes 4
manifest::file apps/dtnrecv/dtnrecv dtnrecv

set clayer       tcp
set count        5000
set length       10
set wait_time    15
set storage_type berkeleydb

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-count" || $var == "count"} {
        set count $val	
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } elseif {$var == "-wait_time" || $var == "wait_time"} {
        set wait_time $val
    } elseif {$var == "-storage_type" } {
	set storage_type $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Configuring $clayer interfaces / links"
dtn::config -storage_type $storage_type
dtn::config_interface $clayer
#dtn::config_linear_topology OPPORTUNISTIC $clayer true
dtn::config_linear_topology AlWAYSON $clayer true
test::script {
    set starttime [clock seconds]
    
    testlog "Running dtnd 0"
    dtn::run_dtnd 0
    dtn::wait_for_dtnd 0

    set source    dtn://host-0/test
    set dest      dtn://host-3/test
    
    testlog "Sending $count bundles of length $length"
    for {set i 0} {$i < $count} {incr i} {
	set timestamp($i) [dtn::tell_dtnd 0 sendbundle $source $dest\
		length=$length expiration=3600]
    }

    testlog "Checking that all the bundles are queued"
    dtn::wait_for_bundle_stats 0 "$count pending"

    testlog "Restarting dtnd 0"
    dtn::stop_dtnd 0
    after 5000
    dtn::run_dtnd 0 dtnd ""

    testlog "Checking that all bundles were re-read from the database"
    dtn::wait_for_dtnd 0
    dtn::wait_for_bundle_stats 0 "$count pending"

    testlog "Starting dtnd 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1

    testlog "Opening link to dtnd 1"
    dtn::tell_dtnd 0 link open $clayer-link:0-1

    testlog "Waiting for all bundles to flow (up to $wait_time minutes)"
    set timeout [expr $wait_time * 60 * 1000]
    
    dtn::wait_for_bundle_stats 0 "0 pending"       $timeout
    dtn::wait_for_bundle_stats 1 "$count pending"  $timeout

    testlog "Checking that the link stayed open"
    dtn::check_link_stats 0 $clayer-link:0-1 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-0 "1 contacts"

    testlog "Starting dtnd 2 and 3"
    dtn::run_dtnd 2
    dtn::run_dtnd 3
    dtn::wait_for_dtnd 2
    dtn::wait_for_dtnd 3

    testlog "Starting dtnrecv on node 3"
    set rcvpid [dtn::run_app 3 dtnrecv "-q -n $count $dest"]

    testlog "Opening links"
    dtn::tell_dtnd 2 link open $clayer-link:2-3
    dtn::tell_dtnd 1 link open $clayer-link:1-2

    testlog "Waiting for all bundles to be delivered"
    dtn::wait_for_bundle_stats 1 "0 pending"        $timeout
    dtn::wait_for_bundle_stats 2 "0 pending"        $timeout
    dtn::wait_for_bundle_stats 3 "$count delivered" $timeout

    testlog "Checking that all links stayed open"
    dtn::check_link_stats 0 $clayer-link:0-1 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-0 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-2 "1 contacts"
    dtn::check_link_stats 2 $clayer-link:2-1 "1 contacts"
    dtn::check_link_stats 2 $clayer-link:2-3 "1 contacts"
    dtn::check_link_stats 3 $clayer-link:3-2 "1 contacts"

    testlog "Waiting for receiver to complete"
    run::wait_for_pid_exit 3 $rcvpid

    testlog "Waiting for all daemon event queues to flush"
    dtn::wait_for_daemon_stat 0 0 pending_events
    dtn::wait_for_daemon_stat 1 0 pending_events
    dtn::wait_for_daemon_stat 2 0 pending_events
    dtn::wait_for_daemon_stat 3 0 pending_events

    testlog "Test success!"

    set elapsed [expr [clock seconds] - $starttime]
    testlog "TEST TIME: $elapsed seconds"

}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
