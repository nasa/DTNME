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

test::name bluez_inq
net::default_num_nodes 2

dtn::config 

set inq "oasys/test/bluez-inq-test"
if [file exists $inq] {
manifest::file $inq inq
} else {
    puts "Failed to stat $inq"
    exit -1
}

test::script {
    
    # XXX/wilson fix this, don't assume hci0!!
    testlog "Testing whether each node is set discoverable by Bluetooth Inquiry"
    bluez::discoverable *

    foreach id [net::nodelist] {
        if {$bluez::iscan($id)==0} {
            testlog "Node $id is not set discoverable, quitting"
            exit
        } else {
            testlog "ISCAN is set on node $id"
            set other [expr 1 - $id]
            testlog "Attempting inquiry from node $other"
            set bdaddr [bluez::inquire $other]
            testlog "Discovered: [join $bdaddr]"
        }
    }

    testlog "Executing oasys::BluetoothInquiry unit test on node 0"
    dtn::run_app_and_wait 0 inq

    testlog "Executing oasys::BluetoothInquiry unit test on node 1"
    dtn::run_app_and_wait 1 inq

    testlog "Test success!"
}

test::exit_script {
    testlog "Cleaning up"
}
