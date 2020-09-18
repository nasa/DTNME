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

test::name extension_block
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

manifest::file apps/dtntest/dtntest dtntest

set block_content "some_long_text_thats_more_than_64_bytes_long_and_therefore_would_trigger_a_buffer_overflow_error_if_the_code_didnt_correctly_handle_the_fact_that_extension_blocks_might_be_more_than_64_bytes_long"

test::script {
    testlog "running dtnds"
    dtn::run_dtnd *

    testlog "running dtntest"
    dtn::run_dtntest 0

    testlog "waiting for dtnds and dtntest to start up"
    dtn::wait_for_dtnd *
    dtn::wait_for_dtntest 0

    set src_eid dtn://host-0
    set mid_eid dtn://host-1
    set dst_eid dtn://host-2
    set src dtn://host-0/test
    set mid dtn://host-1/test
    set dst dtn://host-2/test

    dtn::tell_dtnd 0 tcl_registration $src
    dtn::tell_dtnd 1 tcl_registration $mid
    dtn::tell_dtnd 2 tcl_registration $dst

    set sr_dst dtn://host-0/sr
    dtn::tell_dtnd 0 tcl_registration $sr_dst

    testlog "dtn_open"
    set handle [dtn::tell_dtntest 0 dtn_open]

    
    testlog "*"
    testlog "Test 1: Forward Unprocessed Block"

    ## Node 1 sends a bundle that includes an extension block to node 3
    ## via the API. The intermediary node 2 classifies the block as unknown,
    ## and because the block contains no set flags, marks the block's
    ## "forwarded unprocessed" flag prior to forwarding the bundle (block
    ## included) to node 3.

    testlog "sending bundle"
    set id [dtn::tell_dtntest 0 dtn_send $handle source=$src dest=$dst        \
                                                 replyto=$sr_dst              \
                                                 payload_data=this_is_a_test! \
                                                 block_type=16 block_flags=0  \
                                                 block_content=$block_content]

    testlog "waiting to receive bundle at destination"
    dtn::wait_for_bundle 2 $id
    dtn::check_bundle_data 2 $id recv_blocks 3 block,0x10,flags 0x20

    testlog "checking that each node received/transmitted bundle"
    dtn::wait_for_bundle_stats 0 {1 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 transmitted}
    dtn::wait_for_bundle_stats 2 {1 received 0 transmitted}

    testlog "*"
    testlog "Test 2: Report Unknown Block"

    ## Node 1 sends a bundle that includes an extension block to node 3
    ## via the API. The intermediary node 2 classifies the block as unknown,
    ## and because the block contains the REPORT_ONERROR block flag, sends
    ## a status report to the bundle's reply-to endpoint (i.e., node1).
    ## Node 2 then marks the block's "forwarded unprocessed" flag prior
    ## to forwarding the bundle (block included) to node 3, which also
    ## sends a status report to the bundle's reply-to endpoint (.e., node 1).

    testlog "sending bundle"
    set id [dtn::tell_dtntest 0 dtn_send $handle source=$src dest=$dst        \
                                                 replyto=$sr_dst              \
                                                 payload_data=this_is_a_test! \
                                                 block_type=16 block_flags=2  \
                                                 block_content=$block_content]

    testlog "waiting to receive bundle at destination"
    dtn::wait_for_bundle 2 $id
    dtn::check_bundle_data 2 $id recv_blocks 3
    dtn::check_bundle_data 2 $id recv_blocks 3 block,0x10,flags 0x22

    testlog "waiting for status reports"
    set sr_guid "$id,$mid_eid"
    dtn::wait_for_sr 0 $sr_guid
    dtn::check_sr_fields 0 $sr_guid sr_received_time
    set sr_guid "$id,$dst_eid"
    dtn::wait_for_sr 0 $sr_guid
    dtn::check_sr_fields 0 $sr_guid sr_received_time

    testlog "checking that each node received/transmitted bundle"
    dtn::wait_for_bundle_stats 0 {4 received 2 transmitted}
    dtn::wait_for_bundle_stats 1 {3 received 4 transmitted}
    dtn::wait_for_bundle_stats 2 {2 received 1 transmitted}

    testlog "*"
    testlog "Test 3: Delete Bundle"

    ## Node 1 sends a bundle that includes an extension block to node 3
    ## via the API. The intermediary node 2 classifies the block as unknown,
    ## and because the block contains the DELETE_BUNDLE_ONERROR block flag,
    ## deletes the bundle. Node 2 also sends a status report that notifies
    ## the bundle's reply-to endpoint (i.e. node 1) of the bundle deletion.
    ## because the bundle includes a request for such notifications.

    testlog "sending bundle"
    set id [dtn::tell_dtntest 0 dtn_send $handle source=$src dest=$dst        \
                                                 replyto=$sr_dst              \
                                                 payload_data=this_is_a_test! \
                                                 deletion_rcpt=true           \
                                                 block_type=16 block_flags=4  \
                                                 block_content=$block_content]

    testlog "waiting for status reports"
    set sr_guid "$id,$mid_eid"
    dtn::wait_for_sr 0 $sr_guid
    dtn::check_sr_fields 0 $sr_guid sr_deleted_time

    testlog "checking that each node received/transmitted bundle"
    dtn::wait_for_bundle_stats 0 {6 received 3 transmitted}
    dtn::wait_for_bundle_stats 1 {4 received 5 transmitted}
    dtn::wait_for_bundle_stats 2 {2 received 1 transmitted}

    testlog "*"
    testlog "Test 4: Discard Block"

    ## Node 1 sends a bundle that includes an extension block to node 3
    ## via the API. The intermediary node 2 classifies the block as unknown,
    ## and because the block contains the DISCARD_BLOCK_ONERROR block flag,
    ## forwards the bundle to node 3 after removing the block from the bundle.

    testlog "sending bundle"
    set id [dtn::tell_dtntest 0 dtn_send $handle source=$src dest=$dst        \
                                                 replyto=$sr_dst              \
                                                 payload_data=this_is_a_test! \
                                                 block_type=16 block_flags=16 \
                                                 block_content=$block_content]

    testlog "waiting to receive bundle at destination"
    dtn::wait_for_bundle 2 $id
    dtn::check_bundle_data 2 $id recv_blocks 2

    testlog "checking that each node received/transmitted bundle"
    dtn::wait_for_bundle_stats 0 {7 received 4 transmitted}
    dtn::wait_for_bundle_stats 1 {5 received 6 transmitted}
    dtn::wait_for_bundle_stats 2 {3 received 1 transmitted}


    testlog "*"
    testlog "test success!"
    testlog "*"

    testlog "dtn_close"
    dtn::tell_dtntest 0 dtn_close $handle
}

test::exit_script {
    testlog "stopping all dtnds and dtntest"
    dtn::stop_dtntest 0
    dtn::stop_dtnd *
}
