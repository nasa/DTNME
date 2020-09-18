#
#    Copyright 2007 Intel Corporation
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

test::name inflight-interrupt
net::num_nodes 2

set cl tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set cl $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config
dtn::config_interface $cl

# disable fragmentation
# XXX/demmer this only works for stream cl's
conf::add dtnd * "param set reactive_frag_enabled false"
conf::add dtnd * "link set_cl_defaults $cl reactive_frag_enabled=false"

dtn::config_linear_topology ALWAYSON $cl true \
	"test_write_delay=1000 sendbuf_len=1024"

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Waiting for link to open"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN

    set source dtn://host-0/test
    set dest   dtn://host-1/test
    
    dtn::tell_dtnd 1 tcl_registration $dest
    
    testlog "Sending bundle"
    dtn::tell_dtnd 0 sendbundle $source $dest length=5000
    
    testlog "Waiting for bundle to be in flight"
    dtn::wait_for_link_stats 0 $cl-link:0-1 {1 bundles_inflight}

    testlog "Closing the link"
    tell_dtnd 0 link close $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE
    
    testlog "Checking that bundle is still queued on the link"
    dtn::check_bundle_stats 0 {1 pending}
    dtn::check_bundle_stats 1 {0 received}
    dtn::check_link_stats 0 $cl-link:0-1 {1 bundles_queued 0 bundles_inflight}

    testlog "Reopening the link"
    tell_dtnd 0 link open $cl-link:0-1

    testlog "Waiting for it to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending 1 received 1 delivered}
    
    testlog "Checking the link stats"
    dtn::check_link_stats 0 $cl-link:0-1 {0 bundles_queued 0 bundles_inflight 1 bundles_transmitted}
    dtn::check_link_stats 0 $cl-link:0-1 {0 bytes_queued 0 bytes_inflight}
    
    testlog "Repeating the test with two bundles in flight"
    tell_dtnd 0 bundle reset_stats
    tell_dtnd 1 bundle reset_stats
    
    dtn::tell_dtnd 0 sendbundle $source $dest length=5000
    dtn::tell_dtnd 0 sendbundle $source $dest length=5000
    
    testlog "Waiting for first bundle to be in flight"
    dtn::wait_for_link_stats 0 $cl-link:0-1 {1 bundles_queued 1 bundles_inflight}

    testlog "Closing the link"
    tell_dtnd 0 link close $cl-link:0-1
    dtn::wait_for_link_state 0 $cl-link:0-1 UNAVAILABLE
    
    testlog "Checking that bundles are still queued on the link"
    dtn::check_bundle_stats 0 {2 pending}
    dtn::check_bundle_stats 1 {0 received}
    dtn::check_link_stats 0 $cl-link:0-1 {2 bundles_queued}

    testlog "Reopening the link"
    tell_dtnd 0 link open $cl-link:0-1

    testlog "Waiting for them to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending 2 received 2 delivered}
    
    testlog "Checking the link stats"
    dtn::check_link_stats 0 $cl-link:0-1 {0 bundles_queued 0 bundles_inflight 2 bundles_transmitted}
    dtn::check_link_stats 0 $cl-link:0-1 {0 bytes_queued 0 bytes_inflight}

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
