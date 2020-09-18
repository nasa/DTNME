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

test::name dtn-cp
net::num_nodes 3

manifest::file apps/dtncp/dtncp   dtncp
manifest::file apps/dtncpd/dtncpd dtncpd

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

proc wait_for_file {id path {timeout 30}} {
    do_until "in wait for file $path at node $id" $timeout {
        if {[tell_dtnd $id file exists $path]} {
            return
        }
	    
        after 500
    }
}
    
test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set N [net::num_nodes]
    set last_node [expr $N - 1]
    set dest      dtn://host-0/

    testlog "Running dtncpd..."
    tell_dtnd $last_node file mkdir incoming
    set dtncpd_pid [dtn::run_app $last_node dtncpd incoming]

    testlog "Creating and copying an empty file"
    tell_dtnd 0 file mkdir test
    tell_dtnd 0 {set fd [open test/empty w]; close $fd}
    dtn::check expr [tell_dtnd 0 file size test/empty] == 0
    dtn::run_app_and_wait 0 dtncp "test/empty dtn://host-2"
    wait_for_file $last_node incoming/host-0/empty
    dtn::check expr [tell_dtnd $last_node file size incoming/host-0/empty] == 0

    set dtnd_size [tell_dtnd 0 file size dtnd]

    testlog "Copying the dtnd executable into dtnd.new ($dtnd_size bytes)"
    dtn::run_app_and_wait 0 dtncp "dtnd dtn://host-2 dtnd.new"

    testlog "Waiting for the file to be delivered"
    wait_for_file $last_node incoming/host-0/dtnd.new
    
    testlog "Checking that size and md5sum matches"
    dtn::check expr [tell_dtnd $last_node file size \
            incoming/host-0/dtnd.new] == $dtnd_size
    set cksum1 [lindex [tell_dtnd 0 exec md5sum dtnd] 0]
    set cksum2 [lindex [tell_dtnd $last_node exec md5sum incoming/host-0/dtnd.new] 0]
    if {$cksum1 != $cksum2} {
        error "md5sum mismatch: $cksum1 != $cksum2"
    }
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping dtncpd"
    run::kill_pid $last_node $dtncpd_pid TERM

    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
