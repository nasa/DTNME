#
#    Copyright 2007 Baylor University
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

test::name bluez_sdp
net::default_num_nodes 2
# sdp-reg-test's default is 30
set reg_sleep_seconds 30

dtn::config 

# query must have arg "remote=$bdaddr"
set query "oasys/test/sdp-query-test"
if [file exists $query] {
manifest::file $query query
} else {
    puts "Failed to stat $query"
    exit -1
}
# reg needs no args
set reg "oasys/test/sdp-reg-test"
if [file exists $reg] {
manifest::file $reg reg
} else {
    puts "Failed to stat $reg"
    exit -1
}

test::script {
    
    testlog "Reading Bluetooth adapter addresses from each node"
    bluez::getbdaddr *

    testlog "Execute SDP registration test on node 0"
    set regpid [dtn::run_app 0 reg sleep=$reg_sleep_seconds]

    testlog "Query SDP from node 1 (using BlueZ sdptool)"
    set i 0
    foreach svc [bluez::sdpquery 1 0] {
        testlog "Found registration for \"$svc\""
        incr i
    }
    testlog "Found $i registrations using sdptool"

    testlog "Execute SDP query test on node 1"
    dtn::run_app_and_wait 1 query "remote=$bluez::btaddr(0)"

    testlog "Waiting for SDP registration to expire"
    # wait_for_pid_exit's default timeout is 30 
    run::wait_for_pid_exit 0 $regpid

    testlog "Test success!"
}

test::exit_script {
    testlog "Cleaning up"
}
