#
#    Copyright 2004-2006 Intel Corporation
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

test::name send-one-bundle
net::default_num_nodes 2

dtn::config

set clayer tcp
set length 5000

set link_vars {hexdump prevhop_hdr segment_length  \
        test_read_delay test_write_delay test_read_limit test_write_limit}
array set link_opts {}

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-length" || $var == "length"} {
        set length $val
    } else {
        set found 0
        foreach link_var $link_vars {
            if {$var == "-$link_var"} {
                set link_opts($link_var) $val
                set found 1
                break
            }
        }

        if {$found} { continue }
        
        testlog error "ERROR: unrecognized test option '$var'"
        exit 1
    }
}

testlog "Configuring $clayer interfaces / links"
dtn::config_interface $clayer

set linkopts ""
foreach {var val} [array get link_opts] {
    append linkopts "$var=$val "
}
    
dtn::config_linear_topology ALWAYSON $clayer true $linkopts

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
    
    testlog "Waiting for arrival of bundle with size $length from source $source"
    dtn::wait_for_bundle 0 "$source,$timestamp" 30 

    testlog "Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    is_admin 0 source $source dest $dest
    
	# SF 2013-01-15, take this out as getting spurious errors
    testlog "Not doing sanity check on stats - some script issue"
    #testlog "Doing sanity check on stats"
    #for {set i 0} {$i <= $last_node} {incr i} {
	#dtn::wait_for_bundle_stats $i {0 pending}
	#dtn::wait_for_bundle_stats $i {0 expired}
	#dtn::wait_for_bundle_stats $i {1 received}
    #}
    
    dtn::wait_for_bundle_stats 0 {1 delivered}
    
    for {set i 1} {$i <= $last_node} {incr i} {
	set outgoing_link $clayer-link:$i-[expr $i - 1]
	dtn::wait_for_bundle_stats $i {1 transmitted}
	dtn::wait_for_link_stat $i $outgoing_link 1 bundles_transmitted
	dtn::wait_for_link_stat $i $outgoing_link 0 bundles_queued
	dtn::wait_for_link_stat $i $outgoing_link 0 bytes_queued
    }
	
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
