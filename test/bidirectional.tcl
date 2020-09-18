#
#    Copyright 2005-2006 Intel Corporation
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

test::name bidirectional
net::num_nodes 3

set cl    tcp
set count 200
set storage_type filesysdb

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set cl $val
    } elseif {$var == "-count" || $var == "count"} {
	set count $val
    } elseif {$var == "-storage_type" } {
	set storage_type $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config --no_null_link --storage_type $storage_type

dtn::config_interface $cl
dtn::config_app_manifest 1
dtn::config_linear_topology ALWAYSON $cl true

conf::add dtnd * "param set early_deletion false"

# XXX/demmer do this test with bigger bundles...

test::script {
    set N [net::num_nodes]
    set last [expr $N - 1]

    set eid1 dtn://host-0/test
    set eid2 dtn://host-$last/test

    set sleep 100

    # ----------------------------------------------------------------------
    testlog " "
    testlog " Test phase 1: continuous connectivity"
    testlog " "

    testlog " Running dtnds"
    dtn::run_dtnd *

    testlog " Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog " Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "-q -n $count $eid1"]
    set rcvpid2 [dtn::run_app $last dtnrecv "-q -n $count $eid2"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    testlog " Waiting for senders / receivers to complete (up to 5 mins)"
    run::wait_for_pid_exit 0     $sndpid1 [expr 5 * 60]
    run::wait_for_pid_exit $last $sndpid2 [expr 5 * 60]
    run::wait_for_pid_exit 0     $rcvpid1 [expr 5 * 60]
    run::wait_for_pid_exit $last $rcvpid2 [expr 5 * 60]

    testlog " Waiting for all events to be processed"
    dtn::wait_for_daemon_stats 0 {0 pending_events}
    dtn::wait_for_daemon_stats 1 {0 pending_events}
    dtn::wait_for_daemon_stats 2 {0 pending_events}
    
    foreach node [list 0 $last] {
	testlog " Checking bundle stats on node $node"
	dtn::check_bundle_stats $node \
		$count "delivered" \
		[expr $count * 2] "received"
    }

    for {set i 0} {$i < $N} {incr i} {
	testlog "Node $i: [dtn::tell_dtnd $i bundle stats]"
    }

    testlog " Checking link stats"
    dtn::check_link_stats 0 $cl-link:0-1 1 contacts $count bundles_transmitted
    dtn::check_link_stats 1 $cl-link:1-0 1 contacts

    testlog " Stopping dtnds"
    dtn::stop_dtnd *

    # ----------------------------------------------------------------------
    testlog " "
    testlog " Test phase 2: interrupted links"
    testlog " "

    testlog " Running dtnds"
    dtn::run_dtnd *

    testlog " Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    testlog " Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "-q -n $count $eid1"]
    set rcvpid2 [dtn::run_app $last dtnrecv "-q -n $count $eid2"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    for {set i 0} {$i < 100} {incr i} {
	after [expr int(10000 * rand())]

	testlog " Closing links to/from node 1 ([test::elapsed] seconds elapsed)"
	tell_dtnd 0 link close $cl-link:0-1
	tell_dtnd 1 link close $cl-link:1-2

        dtn::dump_stats
        
	after [expr int(5000 * rand())]
	testlog " Opening links to/from node 1 ([test::elapsed] seconds elapsed)"
	tell_dtnd 0 link open $cl-link:0-1
	tell_dtnd 1 link open $cl-link:1-2

        # if it finishes early, break out of the loop
        if {[dtn::get_bundle_stats 0 delivered] == $count} { break }
    }

    testlog " Done will link closing loop, stats:"
    for {set i 0} {$i < $N} {incr i} {
	testlog "Node $i: [dtn::tell_dtnd $i bundle stats]"
    }

    testlog " Waiting for senders / receivers to complete (up to 5 mins)"
    run::wait_for_pid_exit 0     $sndpid1 [expr 5 * 60]
    run::wait_for_pid_exit $last $sndpid2 [expr 5 * 60]
    run::wait_for_pid_exit 0     $rcvpid1 [expr 5 * 60]
    run::wait_for_pid_exit $last $rcvpid2 [expr 5 * 60]
     
    foreach node [list 0 $last] {
	testlog " Checking bundle stats on node $node"
	dtn::check_bundle_stats $node \
		[expr $count * 2] "pending" \
		$count "transmitted" \
		$count "delivered"
    }

    for {set i 0} {$i < $N} {incr i} {
	testlog "Node $i: [dtn::tell_dtnd $i bundle stats]"
    }

    testlog " Stopping dtnds"
    dtn::stop_dtnd *

    # ----------------------------------------------------------------------

    testlog " "
    testlog " Test phase 3: killing node"
    testlog " "

    testlog " Running dtnds"
    for {set i 0} {$i < [net::num_nodes]} {incr i} {
	set pids($i) [dtn::run_dtnd $i]
    }

    testlog " Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    testlog " Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "-q $eid1"]
    set rcvpid2 [dtn::run_app $last dtnrecv "-q $eid2"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    for {set i 0} {$i < 20} {incr i} {

	if {[dtn::test_bundle_stats 0 [list $count "delivered"]] && \
		[dtn::test_bundle_stats 1 [list $count "delivered"]]} {
	    break; # all done
	}

	set sleep [expr int(5000 * rand())]
	testlog " Sleeping for $sleep msecs"
	after $sleep
	
	testlog " Killing node 1"
	run::kill_pid 1 $pids(1) KILL	
	tell::wait_for_close $net::host(1) [dtn::get_port console 1]

	set sleep [expr int(5000 * rand())]
	testlog " Sleeping for $sleep msecs"
	after $sleep

	testlog " Starting node 1"
	set pids(1) [dtn::run_dtnd 1 dtnd ""]
	dtn::wait_for_dtnd 1
    }

    testlog " Done with killing loop.. stats:"
    for {set i 0} {$i < $N} {incr i} {
	testlog "Node $i: [tell_dtnd $i bundle stats]"
    }

    testlog " Waiting for senders to complete"
    run::wait_for_pid_exit 0     $sndpid1
    run::wait_for_pid_exit $last $sndpid2

    testlog " Killing receivers"
    run::kill_pid 0     $rcvpid1 TERM
    run::kill_pid $last $rcvpid2 TERM

    # all we can really check for is that they all got transmitted,
    # since some might end up getting lost when the node gets killed,
    # but after the ack was sent
    foreach node [list 0 $last] {
	testlog " Checking bundle stats on node $node"
	dtn::check_bundle_stats $node $count "transmitted"
    }

    testlog " Test success!"
}

test::exit_script {
    testlog " Stopping all dtnds"
    dtn::stop_dtnd *

}
