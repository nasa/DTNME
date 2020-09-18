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
test::name discovery

# XXX/wilson automate these and other args
# number of participating nodes
net::num_nodes 3
set cl tcp
set disc_interval 7

dtn::config --router_type static --no_null_link
dtn::config_topology_common true

testlog "Bringing up listening interfaces"
dtn::config_interface $cl


testlog "Configuring discovery components"
testlog "* Limit visibility between 0 and 2 so that 1 is the intermediary"
# Force 0 and 2 to unicast to 1, to reduce their visibility
# and simulate a linear path  0 --> 1 <-- 2
conf::add dtnd 0 "discovery add ipdisc0 ip port=6789 addr=$net::host(1)"
conf::add dtnd 2 "discovery add ipdisc0 ip port=6789 addr=$net::host(1)"
conf::add dtnd 1 "discovery add ipdisc0 ip port=6789"

testlog "* Add announce to advertise each node's $cl CL"
conf::add dtnd 0 "discovery announce tcp0 ipdisc0 tcp interval=$disc_interval \
        cl_port=[dtn::get_port tcp 0]"
conf::add dtnd 1 "discovery announce tcp0 ipdisc0 tcp interval=$disc_interval \
        cl_port=[dtn::get_port tcp 1]"
conf::add dtnd 2 "discovery announce tcp0 ipdisc0 tcp interval=$disc_interval \
        cl_port=[dtn::get_port tcp 2]"

if {$opt(net) == "localhost"} {
    testlog "Discovery test cannot be run on localhost net"
    exit 0
}

test::script {
    

    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Wait for routes to populate"
    dtn::wait_for_route 0 dtn://host-1/* opportunistic-0 {}
    dtn::wait_for_route 2 dtn://host-1/* opportunistic-0 {}
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
