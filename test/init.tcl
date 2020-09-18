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

test::name init
net::default_num_nodes 2

set topology    linear
set router      static
set link_type   alwayson
set clayer      tcp
set segmentlen  0
set prevhop_hdr 0

foreach {var val} $opt(opts) {
    if {0} {
    } elseif {$var == "-topology" || $var == "topology"} {
	set topology $val
    } elseif {$var == "-router" || $var == "router"} {
	set router $val
    } elseif {$var == "-linktype" || $var == "linktype"} {
	set link_type $val
    } elseif {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-segmentlen" || $var == "segmentlen"} {
        set segmentlen $val	
    } elseif {$var == "-prevhop_hdr" || $var == "prevhop_hdr"} {
        set prevhop_hdr 1
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Initializing with $router router"
dtn::config -router_type $router

testlog "Configuring $clayer interfaces / links"
dtn::config_interface $clayer

set linkopts ""
if {$segmentlen != 0} {
    append linkopts "segment_length=$segmentlen "
}

if {$prevhop_hdr} {
    append linkopts "prevhop_hdr=1 "
}

set withroutes false
if {$router == "static"} {
    set withroutes true
}

dtn::config_${topology}_topology $link_type $clayer $withroutes $linkopts

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Setting up registrations"
    foreach node [net::nodelist] {
        dtn::tell_dtnd $node tcl_registration dtn://$node/test
    }

    testlog "Test initialization complete"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
