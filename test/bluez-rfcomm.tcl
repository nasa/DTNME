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

test::name bluez_rfcomm
net::default_num_nodes 2

dtn::config 

# client must have arg "remote=$bdaddr"
set client "oasys/test/rfcomm-client-test"
if [file exists $client] {
manifest::file $client client
} else {
    puts "Failed to stat $client"
    exit -1
}
# server needs no args
set server "oasys/test/rfcomm-server-test"
if [file exists $server] {
manifest::file $server server
} else {
    puts "Failed to stat $server"
    exit -1
}

test::script {

    testlog "Reading Bluetooth adapter addresses from each node"
    bluez::getbdaddr *
    
    testlog "Starting RFCOMM test server on node 0"
    set srvpid [dtn::run_app 0 server]

    testlog "Starting RFCOMM test client on node 1"
    dtn::run_app_and_wait 1 client "remote=$bluez::btaddr(0)"

    testlog "Waiting for RFCOMM server to complete"
    run::wait_for_pid_exit 0 $srvpid

    testlog "Test success!"
}

test::exit_script {
    testlog "Cleaning up"
}
