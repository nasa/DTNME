#
#    Copyright 2005-2006 Intel Corporation
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

# Set the name of the test
test::name example

# Set the number of test nodes used
net::num_nodes 2

# To add additional files and directories to be created, include the
# following:
# manifest::file "test_utils/example_test.tcl" "f/o/o/foo"
# manifest::dir  "my-directory"

#
# Generate boilerplate configuration code for db, cl and topology
# for all nodes in the test network.
#
config_berkeleydb
config_interface tcp
config_linear_topology ONDEMAND

#
# Add to configuration file for node 0
#
conf::add dtnd 0 {
    puts ""
    puts "dtnd id 0 starting up..."
    puts ""

    storage set type berkeleydb
    storage set payloaddir bundles
    storage set dbname     DTN
    storage set dbdir      db

    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

#
# If running in debug mode, add these commands to be run in GDB on
# node 0
#
conf::add gdb 0 {
    break main
    break dtn::BundleDaemon::BundleDaemon
}

#
# Add to configuration file for node 0
#
conf::add dtnd 1 {
    puts ""
    puts "dtnd id 1 starting up..."
    puts ""

    
    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

#
# Setup local ports using a unique portbase for each node
#
for {set i 0} {$i < 2} {incr i} {
    conf::add dtnd $i "api set local_port $net::portbase($i)\n"
}

test::script {
    puts ""
    puts "Example test script"
    puts ""

    # run all dtn daemons
    #
    # Alternatively each daemon can be run seperately:
    #
    # dtn::run_dtnd 0
    # dtn::run_dtnd 1
    # ... etc
    dtn::run_dtnd *

    # wait for all the daemons to start up
    dtn::wait_for_dtnd *
    
    # send a command to execute on daemon 0
    dtn::tell_dtnd 0 {
	sendbundle [route local_eid] dtn://host-1/test
    }
}
