#
#    Copyright 2007 Intel Corporation
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

test::name api-poll
net::num_nodes 1

manifest::file apps/dtntest/dtntest dtntest

dtn::config
dtn::config_topology_common false

test::script {
    testlog "Running dtnd and dtntest"
    dtn::run_dtnd 0
    dtn::run_dtntest 0

    testlog "Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd 0
    dtn::wait_for_dtntest 0

    testlog "Creating two handles"
    set h1 [dtn::tell_dtntest 0 dtn_open]
    set h2 [dtn::tell_dtntest 0 dtn_open]

    testlog "Creating registrations for source and dest"
    set regid0 [dtn::tell_dtntest 0 dtn_register $h1 endpoint=dtn://host-0/src expiration=30]
    set regid1 [dtn::tell_dtntest 0 dtn_register $h2 endpoint=dtn://host-0/dst expiration=30]

    testlog "Setting up a callback handler"
    set chan [dtn::tell_dtntest 0 dtn_poll_channel $h2]
    dtn::tell_dtntest 0 { proc arrived {h} { global bundles; \
            lappend bundles [dtn_recv $h payload_mem=true timeout=0]; } }

    proc send_recv {dest {do_poll 1}} {
        global chan h1 h2

        if {$do_poll} {
            dtn::tell_dtntest 0 dtn_begin_poll $h2 10000
            dtn::tell_dtntest 0 fileevent $chan readable [list arrived $h2]
        }
        
        dtn::tell_dtntest 0 "dtn_send $h1 source=dtn://host-0/src $dest\
                expiration=10 payload_data=test_payload"

        # when polling, we first wait for delivery and then check the
        # bundles var, otherwise we need to call recv to get the
        # contents, then it will be delivered
        if {$do_poll} {
            dtn::wait_for_bundle_stat 0 1 delivered
            puts [dtn::tell_dtntest 0 set bundles]
            dtn::tell_dtntest 0 set bundles {}
            dtn::tell_dtntest 0 fileevent $chan readable ""
        } else {
            puts [dtn::tell_dtntest 0 dtn_recv $h2 payload_mem=true timeout=10000]
            dtn::wait_for_bundle_stat 0 1 delivered
        }

        dtn::wait_for_bundle_stat 0 0 pending
        dtn::tell_dtnd 0 bundle reset_stats
    }

    testlog "Sending and polling for a couple bundles"
    for {set i 0} {$i < 5} {incr i} {
        send_recv dest=dtn://host-0/dst
        after 1000
    }

    testlog "Testing poll timeout"
    dtn::tell_dtntest 0 "dtn_begin_poll $h2 1000"
    after 2000
    catch {dtn::tell_dtntest 0 dtn_recv $h2 payload_mem=true timeout=0} err
    if {$err != "error: error in dtn_recv: operation timed out"} {
        error "unexpected error from dtn_recv: $err"
    }

    testlog "Testing a normal delivery"
    send_recv dest=dtn://host-0/dst 0

    testlog "Testing a poll delivery"
    send_recv dest=dtn://host-0/dst

    testlog "Testing poll cancel"
    dtn::tell_dtntest 0 "dtn_begin_poll $h2 10000"
    dtn::tell_dtntest 0 fileevent $chan readable \
            "error: unexpected notification on idle channel"
    after 2000
    dtn::tell_dtntest 0 fileevent $chan readable ""
    dtn::tell_dtntest 0 "dtn_cancel_poll $h2"

    testlog "Testing a normal delivery"
    send_recv dest=dtn://host-0/dst 0

    testlog "Testing a poll delivery"
    send_recv dest=dtn://host-0/dst

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping dtnd and dtntest"
    dtn::tell_dtntest 0 dtn_close $h1
    dtn::tell_dtntest 0 dtn_close $h2
    dtn::stop_dtntest 0
    dtn::stop_dtnd 0
}
