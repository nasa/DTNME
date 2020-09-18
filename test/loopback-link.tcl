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

test::name loopback-link
net::num_nodes 1

dtn::config

set clayer tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config_topology_common false

testlog "Configuring $clayer interface"
dtn::config_interface $clayer

test::script {
    testlog "Running dtnd"
    dtn::run_dtnd 0

    testlog "Waiting for dtnd to start up"
    dtn::wait_for_dtnd *
    
    testlog "Setting up flamebox-ignore"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign1 scheduling IMMEDIATE expiration"
    
    testlog "Adding a link that points back to itself and a default route"
    eval dtn::tell_dtnd 0 link add loopback-link \
	    "$net::host(0):[dtn::get_port $clayer 0]" ALWAYSON $clayer

    dtn::tell_dtnd 0 route add *:* loopback-link action=copy
    
    testlog "Adding a registration"
    dtn::tell_dtnd 0 tcl_registration dtn://host-0/test
    
    testlog "Sending a bundle"
    dtn::tell_dtnd 0 sendbundle dtn://host-0/source dtn://host-0/test \
	    expiration=1

    testlog "Waiting for it to loop"
    after 5000
    
    testlog "Checking stats"
    dtn::check_bundle_stats 0 0 pending 1 transmitted 2 received 1 delivered
    dtn::check_link_stats 0 loopback-link 0 bundles_queued 1 bundles_transmitted
    
    testlog "Test success!"
}

test::exit_script {
    testlog "clearing flamebox ignores"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign1"
    
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
