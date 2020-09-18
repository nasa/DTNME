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

test::name dtn-perf
#net::num_nodes 3

manifest::file apps/dtnperf/dtnperf-server dtnperf-server
manifest::file apps/dtnperf/dtnperf-client dtnperf-client

set perftime 60
set delivery_opts ""
set storage_type berkeleydb
set clayer tcp

set mode "memory"

for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]
    if {$var == "-perftime" } {
	set perftime [lindex $opt(opts) [incr i]]

    } elseif {$var == "-forwarding_rcpts" } {
	append delivery_opts "-F "

    } elseif {$var == "-receive_rcpts" } {
	append delivery_opts "-R "

    } elseif {$var == "-payload_len"} {
	set len [lindex $opt(opts) [incr i]]
	append delivery_opts "-p $len "

    } elseif {$var == "-file_payload"} {
	set mode "file"
	
    } elseif {$var == "-storage_type" } {
	set storage_type [lindex $opt(opts) [incr i]]

    } elseif {$var == "-cl" || $var == "cl"} {
        set clayer [lindex $opt(opts) [incr i]]

    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config -storage_type $storage_type
dtn::config_interface $clayer
dtn::config_linear_topology ALWAYSON $clayer true

if {$mode == "memory"} {
    append delivery_opts "-m "
} else {
    append delivery_opts "-f dtnperf.snd "
}

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set N [net::num_nodes]
    set last_node [expr $N - 1]

    set dest      dtn://host-${last_node}

    set server_rundir [dist::get_rundir $net::host($last_node) $last_node]
    set server_opts "-v -a 100 -d $server_rundir "
    if {$mode == "memory"} {
	append server_opts "-m"
    }
    set server_pid [dtn::run_app $last_node dtnperf-server $server_opts]
    after 1000

    set client_rundir [dist::get_rundir $net::host(0) 0]
    regsub {dtnperf.snd} $delivery_opts "$client_rundir/dtnperf.snd" delivery_opts
    testlog "Running dtnperf-client for $perftime seconds"
    set client_pid [dtn::run_app 0 dtnperf-client \
			"-t $perftime $delivery_opts -d $dest" ]

    # XXX might want to try running dtnperf-client when sending to a
    # non-existent endpoint too, such as:
    # "-t $perftime -m -d # $dest/foo" ]

    for {set i 0} {$i < $perftime} {incr i} {
	for {set id 0} {$id <= $last_node} {incr id} {
	    testlog "Node $id: [dtn::tell_dtnd $id bundle stats]"
	}
	testlog ""
	after 1000
    }

    testlog "waiting for dtnperf-client to exit"
    run::wait_for_pid_exit 0 $client_pid

    testlog "Final stats:"
    for {set id 0} {$id <= $last_node} {incr id} {
	testlog "$id: [dtn::tell_dtnd $id bundle stats]"
    }
    testlog ""

    testlog "Test success!"
}

test::exit_script {
    if {$server_pid == ""} {
        testlog "ERROR: server_pid not set"
    } else {
        testlog "Stopping dtnperf-server (node $last_node server_pid $server_pid test pid [pid])"
        run::kill_pid $last_node $server_pid 1
        run::wait_for_pid_exit $last_node $server_pid
    }
    
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
