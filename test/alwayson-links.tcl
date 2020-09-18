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

test::name tcp-alwayson-links
net::num_nodes 3

set cl tcp
set link_opts ""

for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]
    if {$var == "-cl" || $var == "cl"} {
	set val [lindex $opt(opts) [incr i]]
	set cl $val
    } elseif {$var == "fast" || $var == "-fast_retries"} {
	append link_opts " min_retry_interval=1 max_retry_interval=10 data_timeout=1000"
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config
dtn::config_interface $cl

dtn::config_linear_topology ALWAYSON $cl true $link_opts

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *

    testlog "waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    dtn://host-0/test
    set dest      dtn://host-2/test

    dtn::tell_dtnd 2 tcl_registration $dest

    testlog "checking that link is open"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5
    
    testlog "killing daemon 1"
    dtn::stop_dtnd 1
    
    testlog "checking that link is unavailable"
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "waiting for a few retry timers"
    after 10000
    
    testlog "checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE
    
    testlog "restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    testlog "checking that link is OPEN"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5
    
    testlog "waiting for the idle timer, making sure the link is open"
    after 10000
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "forcibly closing the link"
    dtn::tell_dtnd 0 link close $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "sending another bundle, checking that the link stays closed"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]
    after 2000
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "forcibly re-opening the link"
    dtn::tell_dtnd 0 link open $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5

    testlog "test success!"
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
