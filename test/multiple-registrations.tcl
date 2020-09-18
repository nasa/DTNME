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

test::name multiple-registrations
net::num_nodes 2

manifest::file apps/dtntest/dtntest dtntest
manifest::file Rules.make test-payload.dat

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

test::script {
    testlog "Running dtnd and dtntest"
    dtn::run_dtnd *
    dtn::run_dtntest *

    testlog "Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd *
    dtn::wait_for_dtntest *

    testlog "Creating a handle on each node"
    set h0 [dtn::tell_dtntest 0 dtn_open]
    set h1 [dtn::tell_dtntest 1 dtn_open]

    testlog "Creating two passive registrations on node 0"
    set reg1 [dtn::tell_dtntest 0 dtn_register $h0 \
            endpoint=dtn://host-0/test1 expiration=3600 init_passive=true \
            failure_action=defer]
    set reg2 [dtn::tell_dtntest 0 dtn_register $h0 \
            endpoint=dtn://host-0/test2 expiration=3600 init_passive=true \
            failure_action=defer]

    testlog "Binding to the two registrations"
    dtn::tell_dtntest 0 dtn_bind $h0 $reg1
    dtn::tell_dtntest 0 dtn_bind $h0 $reg2

    testlog "Sending a bundle to each registration"
    dtn::tell_dtntest 1 dtn_send $h1 \
            source=dtn://host-1/test \
            dest=dtn://host-0/test1 \
            payload_data="test"

    dtn::tell_dtntest 1 dtn_send $h1 \
            source=dtn://host-1/test \
            dest=dtn://host-0/test2 \
            payload_data="test"

    testlog "Waiting for the bundles to arrive"
    dtn::wait_for_bundle_stats 0 "2 received"
    
    testlog "Receiving both bundles"
    set b1_spec [dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=5000]
    set b2_spec [dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=5000]

    testlog "Trying to receive something else, checking that it times out"
    catch "dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=2000" recv_err
    dtn::check_equal $recv_err "error: error in dtn_recv: operation timed out"

    testlog "Unbinding from one of the registrations"
    dtn::tell_dtntest 0 dtn_unbind $h0 $reg1

    testlog "Sending another bundle to both registrations"
    dtn::tell_dtntest 1 dtn_send $h1 \
            source=dtn://host-1/test \
            dest=dtn://host-0/test1 \
            payload_data="test"

    dtn::tell_dtntest 1 dtn_send $h1 \
            source=dtn://host-1/test \
            dest=dtn://host-0/test2 \
            payload_data="test"
    
    testlog "Waiting for the bundles to arrive"
    dtn::wait_for_bundle_stats 0 "4 received"

    testlog "Receiving a bundle"
    set spec [dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=5000]

    testlog "Checking that the right one was delivered"
    foreach {var val} $spec {set bundle_data($var) $val}
    dtn::check_equal $bundle_data(dest) dtn://host-0/test2

    testlog "Checking that dtn_recv correctly times out"
    catch "dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=2000" recv_err

    testlog "Re-binding to the right registration"
    dtn::tell_dtntest 0 dtn_bind $h0 $reg1

    testlog "Checking that the bundle is now received properly"
    set spec [dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=5000]
    foreach {var val} $spec {set bundle_data($var) $val}
    dtn::check_equal $bundle_data(dest) dtn://host-0/test1

    testlog "Checking that many registrations works too"
    for {set i 2} {$i < 100} {incr i} {
        dtn::tell_dtntest 0 \
                dtn_register $h0 endpoint=dtn://host-0/test$i expiration=3600
    }

    for {set i 0} {$i < 100} {incr i} {
        dtn::tell_dtntest 1 dtn_send $h1 \
                source=dtn://host-1/test \
                dest=dtn://host-0/test$i \
                payload_data="test"
    }

    for {set i 0} {$i < 100} {incr i} {
        dtn::tell_dtntest 0 dtn_recv $h0 payload_mem=true timeout=30000
    }

    testlog "Checking final bundle stats"
    dtn::wait_for_bundle_stats 0 "104 delivered"
}

test::exit_script {
    testlog "Stopping dtnd and dtntest"
    dtn::tell_dtntest 0 dtn_close $h0
    dtn::tell_dtntest 1 dtn_close $h1
    dtn::stop_dtntest *
    dtn::stop_dtnd *
}
