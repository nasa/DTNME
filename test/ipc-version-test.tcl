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

test::name ipc-version-test
net::num_nodes 1

manifest::file apps/dtntest/dtntest dtntest

dtn::config
dtn::config_topology_common false

set version 54321
foreach {var val} $opt(opts) {
    if {$var == "-version" || $var == "version"} {
        set version $val
    }
}

test::script {
    testlog "Running dtnd and dtntest"
    dtn::run_dtnd 0
    dtn::run_dtntest 0

    testlog "Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd 0
    dtn::wait_for_dtntest 0

    testlog "Doing a dtn_open with default version (should work)"
    set id [dtn::tell_dtntest 0 dtn_open]
    testlog "dtn_open succeeded"
    dtn::tell_dtntest 0 dtn_close $id
    testlog "dtn_close succeeded"

    testlog "Setting up flamebox-ignore"
    dtn::tell_dtnd 0 log /test always \
	"flamebox-ignore ign1 handshake .*s version \[0-9\]* != DTN_IPC_VERSION"
    
    testlog "Doing a dtn_open with version=$version (should fail)"
    if { [catch \
	  {set id [dtn::tell_dtntest 0 dtn_open version=$version]} \
	  errorstr] } {
	testlog "  and it did!"
    } else {
	dtn::tell_dtntest 0 dtn_close $id
	error "dtn_open did not fail as expected despite IPC version mismatch"
    }
    
    testlog "Test success!"
}

test::exit_script {
    testlog "clearing flamebox ignores"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign1"
    
    testlog "Stopping dtnd and dtntest"
    dtn::stop_dtnd 0
    dtn::stop_dtntest 0
}
