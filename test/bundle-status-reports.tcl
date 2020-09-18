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

test::name bundle-status-reports
net::default_num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

test::script {

    proc test_sr {sr_option sr_field node_list} {

	global last_node
	global source
	global sr_dest
	global dest
	global dest_node
	
	testlog "Sending bundle to $dest with $sr_option set"

	# 3 second expiration for deleted SR's to be generated in time:
	set timestamp [dtn::tell_dtnd $last_node \
			   sendbundle $source $dest replyto=$sr_dest \
			   expiration=3 $sr_option]

	# If we selected deletion receipt we assume the bundle
	# won't ever arrive at the destination
	if {$sr_option != "deletion_rcpt"} {
	    testlog "Waiting for bundle arrival at $dest"
	    dtn::wait_for_bundle 0 "$source,$timestamp" 5
	}
    
	set sr_guid "$source,$timestamp,$dest_node"

	testlog "Testing for $sr_option SR(s):"
	foreach node_name $node_list {
	    set sr_guid "$source,$timestamp,$node_name"

	    testlog "  Waiting for SR arrival from $node_name to $sr_dest"
	    dtn::wait_for_sr $last_node $sr_guid 5
	    
	    testlog "  Verifying received SR has field \"$sr_field\""
	    dtn::check_sr_fields $last_node $sr_guid $sr_field

	}
    }

    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    testlog "Waiting for all links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-2 OPEN
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    # global constants
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest_node dtn://host-0
    set dest      $dest_node/test
    set sr_dest   $source/sr
    set all_nodes [list]
    for {set i 0} {$i <= $last_node} {incr i} {
	lappend all_nodes "dtn://host-$i"
    }

    # report bundle arrivals at the bundle delivery points
    dtn::tell_dtnd $last_node tcl_registration $sr_dest
    dtn::tell_dtnd 0 tcl_registration $dest

    # test the SR's
    test_sr delivery_rcpt sr_delivered_time $dest_node
    test_sr forward_rcpt  sr_forwarded_time [lrange $all_nodes 1 end]
    test_sr receive_rcpt  sr_received_time  [lrange $all_nodes 0 [expr $last_node - 1]]
    
    # need to cut the link to create a deletion SR:
    dtn::tell_dtnd 1 link close tcp-link:1-0
    after 1000
    test_sr deletion_rcpt sr_deleted_time dtn://host-1

    # XXX/todo: add Custody and App-Acknowledgement SR tests once the
    # ability to generate those types of SRs has been implemented

    testlog "Success!"
    
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
