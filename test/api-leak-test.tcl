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

test::name api-leak-test
net::num_nodes 1

manifest::file apps/dtntest/dtntest dtntest
manifest::file Rules.make test-payload.dat

dtn::config
dtn::config_topology_common false

set count 10000
foreach {var val} $opt(opts) {
    if {$var == "-count" || $var == "count"} {
        set count $val
    }
}

test::script {
    testlog "Running dtnd and dtntest"
    dtn::run_dtnd 0
    dtn::run_dtntest 0

    testlog "Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd 0
    dtn::wait_for_dtntest 0

    testlog "Creating / removing $count dtn handles"
    for {set i 0} {$i < $count} {incr i} {
	set id [dtn::tell_dtntest 0 dtn_open]
	dtn::tell_dtntest 0 dtn_close $id
    }

    testlog "Creating one more handle"
    set h [dtn::tell_dtntest 0 dtn_open]

    testlog "Creating / removing $count registrations and bindings"
    for {set i 0} {$i < $count} {incr i} {
	set regid [dtn::tell_dtntest 0 dtn_register $h \
		endpoint=dtn://test expiration=100 init_passive=true]
	dtn::tell_dtntest 0 dtn_bind $h $regid
	dtn::tell_dtntest 0 dtn_unbind $h $regid
	dtn::tell_dtntest 0 dtn_unregister $h $regid
    }

    testlog "Creating / removing a registration with an eval script"
    set regid [dtn::tell_dtntest 0 dtn_register $h \
	    endpoint=dtn://dest expiration=100 script="/tmp/foobar"]
    dtn::tell_dtntest 0 dtn_unregister $h $regid
    dtn::tell_dtntest 0 dtn_unbind $h $regid
    dtn::tell_dtntest 0 dtn_unregister $h $regid
    
    testlog "Creating one more registration for source and dest"
    set regid0 [dtn::tell_dtntest 0 dtn_register $h \
	    endpoint=dtn://source expiration=100]
    set regid1 [dtn::tell_dtntest 0 dtn_register $h \
	    endpoint=dtn://dest expiration=100]
    
    testlog "Sending / receiving $count bundles from memory"
    for {set i 0} {$i < $count} {incr i} {
	dtn::tell_dtntest 0 dtn_send $h source=dtn://source dest=dtn://dest \
		expiration=10 payload_data=this_is_some_test_payload_data
	dtn::tell_dtntest 0 dtn_recv $h payload_mem=true timeout=10000
    }
    
    testlog "Sending / receiving $count bundles from files"
    for {set i 0} {$i < $count} {incr i} {
	dtn::tell_dtntest 0 dtn_send $h source=dtn://source dest=dtn://dest \
		expiration=10 payload_file=test-payload.dat
	dtn::tell_dtntest 0 dtn_recv $h payload_file=true timeout=10000
    }
    
    testlog "Checking that all bundles were delivered"
    dtn::wait_for_bundle_stat 0 0 pending
    dtn::wait_for_bundle_stat 0 [expr $count * 2] delivered
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping dtnd and dtntest"
    dtn::tell_dtntest 0 dtn_close $h
    dtn::stop_dtntest 0
    dtn::stop_dtnd 0
}
