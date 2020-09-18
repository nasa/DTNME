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

test::name unknown-scheme
net::default_num_nodes 3

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

testlog "Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_linear_topology ALWAYSON $clayer true

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Adding routes for the unknown scheme"
    dtn::tell_dtnd 0 route add unknown:node-1 $clayer-link:0-1
    dtn::tell_dtnd 0 route add unknown:node-2 $clayer-link:0-1
    dtn::tell_dtnd 1 route add unknown:node-0 $clayer-link:1-0
    dtn::tell_dtnd 1 route add unknown:node-2 $clayer-link:1-2
    dtn::tell_dtnd 2 route add unknown:node-0 $clayer-link:2-1
    dtn::tell_dtnd 2 route add unknown:node-1 $clayer-link:2-1

    dtn::tell_dtnd 0 tcl_registration unknown:node-0
    dtn::tell_dtnd 1 tcl_registration unknown:node-1
    dtn::tell_dtnd 2 tcl_registration unknown:node-2

    testlog "Sending bundles without custody"
    foreach n {0 1 2} {
        set source unknown:node-0
        set dest   unknown:node-$n

        testlog "Sending bundle from $source to $dest"
        set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]
        
        testlog "Waiting for bundle arrival"
        dtn::wait_for_bundle $n "$source,$timestamp" 30

        testlog "Checking bundle data"
        dtn::check_bundle_data $n "$source,$timestamp" \
                is_admin 0 source $source dest $dest
    }

    testlog "Doing sanity check on stats"
    for {set i 0} {$i <= 2} {incr i} {
	dtn::wait_for_bundle_stats $i {0 pending}
	dtn::wait_for_bundle_stats $i {0 expired}
	dtn::wait_for_bundle_stats $i {1 delivered}
    }

    dtn::tell_dtnd * bundle reset_stats
    testlog "Sending bundles with custody"
    foreach n {0 1 2} {
        set source unknown:node-0
        set dest   unknown:node-$n

        testlog "Sending bundle from $source to $dest"
        set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest custody]
        
        testlog "Waiting for bundle arrival"
        dtn::wait_for_bundle $n "$source,$timestamp" 30

        testlog "Checking bundle data"
        dtn::check_bundle_data $n "$source,$timestamp" \
                is_admin 0 source $source dest $dest
    }

    testlog "Doing sanity check on stats"
    for {set i 0} {$i <= 2} {incr i} {
	dtn::wait_for_bundle_stats $i {0 pending}
	dtn::wait_for_bundle_stats $i {0 expired}
    }

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
