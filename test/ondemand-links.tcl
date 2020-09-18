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

test::name ondemand-links
net::num_nodes 3

set cl tcp
set link_opts "idle_close_time=5"

for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]
    if {$var == "-cl" || $var == "cl"} {
	set val [lindex $opt(opts) [incr i]]
	set cl $val
    } elseif {$var == "fast" || $var == "-fast_retries"} {
	append link_opts " min_retry_interval=1 max_retry_interval=10 data_timeout=1000"
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config
dtn::config_interface $cl

dtn::config_linear_topology ONDEMAND $cl true $link_opts

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *

    testlog "waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    dtn://host-0/test
    set dest      dtn://host-2/test 

    dtn::tell_dtnd 2 tcl_registration $dest

    testlog "checking that link is available "
    dtn::wait_for_link_state 0 $cl-link:0-1 AVAILABLE
    
    testlog "sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5
    
    testlog "sanity checking stats"
    dtn::wait_for_bundle_stats 0 {1 received 1 transmitted} 5
    dtn::wait_for_bundle_stats 1 {1 received 1 transmitted} 5
    dtn::wait_for_bundle_stats 2 {1 received 0 transmitted} 5

    testlog "checking that link is open"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    testlog "killing daemon 1"
    dtn::stop_dtnd 1
    
    testlog "checking that link is unavailable"
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "sending another bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]

    testlog "waiting for a few retry timers"
    after 10000
    
    testlog "checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE
    
    testlog "restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    testlog "checking that link is AVAILABLE or OPEN"
    dtn::wait_for_link_state 0 $cl-link:0-1 {AVAILABLE OPEN}

    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5
    
    testlog "sanity checking stats"
    dtn::wait_for_bundle_stats 0 {2 received 2 transmitted} 5
    dtn::wait_for_bundle_stats 1 {1 received 1 transmitted} 5
    dtn::wait_for_bundle_stats 2 {2 received 0 transmitted} 5

    testlog "waiting for the idle timer to close the link"
    dtn::wait_for_link_state 0 $cl-link:0-1 AVAILABLE

    testlog "forcibly setting the link to unavailable"
    dtn::tell_dtnd 0 link set_available $cl-link:0-1 false
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "sending another bundle, checking that the link stays closed"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE

    testlog "resetting the link to available, checking that it opens"
    dtn::tell_dtnd 0 link set_available $cl-link:0-1 true
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    
    testlog "waiting for bundle arrival"
    dtn::wait_for_bundle 2 "$source,$timestamp" 5

    testlog "forcibly closing the link"
    dtn::tell_dtnd 0 link close $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE
    
    testlog "resetting the link to available, checking that it's not yet open"
    dtn::tell_dtnd 0 link set_available $cl-link:0-1 true
    dtn::wait_for_link_state 0 $cl-link:0-1 AVAILABLE
    
    testlog "forcibly opening the link"
    dtn::tell_dtnd 0 link open $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    
    testlog "test success!"
}

test::exit_script {
    testlog "stopping all dtnds"
    dtn::stop_dtnd *
}
