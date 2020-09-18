#
#    Copyright 2008 Intel Corporation
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

test::name sequence-id
net::num_nodes 3

manifest::file apps/dtntest/dtntest dtntest

set testopt(cl)       tcp
run::parse_test_opts

set cl $testopt(cl)

dtn::config -storage_type filesysdb
dtn::config_interface $cl

dtn::config_linear_topology ALWAYSON $cl true

test::script {
    testlog "running dtnd and dtntest"
    dtn::run_dtnd *
    dtn::run_dtntest *

    testlog "waiting for dtnds and dtntest to start up"
    dtn::wait_for_dtnd *
    dtn::wait_for_dtntest *

    set h0 [tell_dtntest 0 dtn_open]
    set h1 [tell_dtntest 1 dtn_open]
    set h2 [tell_dtntest 2 dtn_open]

    set eid0    dtn://host-0/test
    set eid1    dtn://host-1/test
    set eid2    dtn://host-2/test

    set seqid00 <($eid0\ 0)>
    set seqid01 <($eid0\ 1)>
    set seqid02 <($eid0\ 2)>
    set seqid03 <($eid0\ 3)>

    set seqid10 <($eid1\ 0)>
    set seqid11 <($eid1\ 1)>
    set seqid12 <($eid1\ 2)>
   
    testlog "creating registrations"
    tell_dtntest 0 dtn_register $h0 endpoint=$eid0 expiration=0
    tell_dtntest 1 dtn_register $h1 endpoint=$eid1 expiration=0
    tell_dtntest 2 dtn_register $h2 endpoint=$eid2 expiration=0

    testlog "waiting for links to establish"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-0 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-2 OPEN
    dtn::wait_for_link_state 2 $cl-link:2-1 OPEN
    
    testlog "sending a bundle with sequence id and no session group"
    set id [dtn::tell_dtntest 0 dtn_send $h0 source=$eid0 dest=$eid2 \
                                             payload_data=this_is_a_test \
                                             sequence_id=$seqid00]

    testlog "waiting for arrival"
    dtn::wait_for_bundle_stats 0 {0 pending 1 deleted}
    dtn::wait_for_bundle_stats 1 {0 pending 1 deleted}
    dtn::wait_for_bundle_stats 2 {1 pending 0 deleted}
    array set bundle_data [dtn::tell_dtntest 2 dtn_recv $h2 payload_mem=true timeout=0]
    dtn::wait_for_bundle_stats 2 {0 pending 1 delivered 1 deleted}

    testlog "checking that sequence id was conveyed properly"
    dtn::check string equal $bundle_data(sequence_id) $seqid00
    dtn::check string equal $bundle_data(obsoletes_id) {}

    testlog "closing link from 1-2"
    dtn::tell_dtnd 1 link close $cl-link:1-2
    dtn::wait_for_link_state 1 $cl-link:1-2 UNAVAILABLE
    dtn::wait_for_link_state 2 $cl-link:2-1 UNAVAILABLE

    testlog "sending another bundle, waiting for it to be queued at node 1"
    set id [dtn::tell_dtntest 0 dtn_send $h0 source=$eid0 dest=$eid2 \
                                             payload_data=this_is_another_test \
                                             sequence_id=$seqid01]
    
    dtn::wait_for_bundle_stats 0 {0 pending 2 deleted}
    dtn::wait_for_bundle_stats 1 {1 pending 1 deleted}
    dtn::wait_for_bundle_stats 2 {0 pending 1 deleted}

    testlog "sending a third bundle, checking that it obsoletes the other one"
    set id [dtn::tell_dtntest 0 dtn_send $h0 source=$eid0 dest=$eid2 \
                                             payload_data=this_is_the_third_test \
                                             sequence_id=$seqid02 \
                                             obsoletes_id=$seqid01]
    
    dtn::wait_for_bundle_stats 0 {0 pending 3 deleted}
    dtn::wait_for_bundle_stats 1 {1 pending 2 deleted}
    dtn::wait_for_bundle_stats 2 {0 pending 1 deleted}

    testlog "sending a bundle that doesn't obsolete the other"
    set id [dtn::tell_dtntest 0 dtn_send $h0 source=$eid0 dest=$eid2 \
                                             payload_data=this_is_the_fourth_test \
                                             sequence_id=$seqid11 \
                                             obsoletes_id=$seqid10]
    
    dtn::wait_for_bundle_stats 0 {0 pending 4 deleted}
    dtn::wait_for_bundle_stats 1 {2 pending 2 deleted}
    dtn::wait_for_bundle_stats 2 {0 pending 1 deleted}

    testlog "sending a bundle that's obsolete when it arrives"
    set id [dtn::tell_dtntest 0 dtn_send $h0 source=$eid0 dest=$eid2 \
                                             payload_data=this_is_the_fifth_test \
                                             sequence_id=$seqid10]
    
    dtn::wait_for_bundle_stats 0 {0 pending 5 deleted}
    dtn::wait_for_bundle_stats 1 {2 pending 3 deleted}
    dtn::wait_for_bundle_stats 2 {0 pending 1 deleted}

    testlog "opening the link again, waiting for both bundles to be delivered"
    dtn::tell_dtnd 1 link open $cl-link:1-2
    
    array set bundle_data [dtn::tell_dtntest 2 dtn_recv $h2 payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid02
    dtn::check string equal $bundle_data(obsoletes_id) $seqid01

    array set bundle_data [dtn::tell_dtntest 2 dtn_recv $h2 payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid11
    dtn::check string equal $bundle_data(obsoletes_id) $seqid10

    dtn::wait_for_bundle_stats 0 {0 pending 5 deleted}
    dtn::wait_for_bundle_stats 1 {0 pending 5 deleted}
    dtn::wait_for_bundle_stats 2 {0 pending 3 delivered 3 deleted}
}

test::exit_script {
    testlog "stopping all dtnds and dtntest"
    dtn::stop_dtnd *
    dtn::stop_dtntest *
}
