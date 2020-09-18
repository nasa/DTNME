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

# name of test
test::name bluez_links

# these options need to be automated, perhaps?
net::num_nodes 2
set cl bt

dtn::config --router_type static --no_null_link

testlog "Configuring Bluetooth interfaces / links"
dtn::config_interface $cl ""

testlog "Reading Bluetooth adapter addresses from each host"
bluez::getbdaddr *
dtn::config_linear_topology ONDEMAND $cl true

test::script {

    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source "[dtn::tell_dtnd 0 {route local_eid}]/test"
    set dest   "[dtn::tell_dtnd 1 {route local_eid}]/test"

    dtn::tell_dtnd 1 tcl_registration $dest

    testlog "sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest]

    # wait for stuff to happen
    after 2000

    testlog "waiting for arrival"
    dtn::wait_for_bundle 1 "$source,$timestamp" 9

    testlog "sanity checking stats"
    dtn::wait_for_bundle_stats 0 {1 received 1 transmitted} 5
    dtn::wait_for_bundle_stats 1 {1 received 0 transmitted} 5

    testlog "Test success!"
}

test::exit_script {
    testlog "stopping dtnds"
    dtn::stop_dtnd *
}
