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

test::name version-mismatch
net::num_nodes 2

set cl tcp

for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]
    if {$var == "-cl" || $var == "cl"} {
	set val [lindex $opt(opts) [incr i]]
	set cl $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config
dtn::config_interface $cl
dtn::config_linear_topology OPPORTUNISTIC $cl true

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Setting up flamebox ignore"
    tell_dtnd 1 log /test always \
	    "flamebox-ignore ign1 remote sent version .*, expected version 99"

    testlog "Changing the CL version on one of the nodes"
    tell_dtnd 1 link reconfigure $cl-link:1-0 cl_version=99

    testlog "Opening the link"
    tell_dtnd 1 link open $cl-link:1-0

    testlog "Checking that the links are still closed"
    dtn::wait_for_link_state 0 tcp-link:0-1 UNAVAILABLE
    dtn::wait_for_link_state 1 tcp-link:1-0 UNAVAILABLE
    
    testlog "Test success!"
}

test::exit_script {
    tell_dtnd 1 log /test always \
	    "flamebox-ignore-cancel ign1"
    
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
