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

test::name storage
net::default_num_nodes 1

manifest::file apps/dtntest/dtntest dtntest

set storage_type berkeleydb
foreach {var val} $opt(opts) {
    if {$var == "-storage_type" || $var == "storage_type"} {
	set storage_type $val
    } else {
	testlog error "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config -storage_type $storage_type
dtn::config_topology_common false

proc rand_int {} {
    return [expr int(rand() * 1000)]
}

set source_eid1 dtn://host-0/storage-test-src-[rand_int]
set source_eid2 dtn://host-0/storage-test-src-[rand_int]
set dest_eid1   dtn://host-0/storage-test-dst-[rand_int]
set dest_eid2   dtn://host-0/storage-test-dst-[rand_int]
set reg_eid1    dtn://host-0/storage-test-reg-[rand_int]
set reg_eid2    dtn://host-0/storage-test-reg-[rand_int]

set payload    "storage test payload"

proc check {} {
    global source_eid1 source_eid2
    global dest_eid1 dest_eid2
    global reg_eid1 reg_eid2
    global payload

    dtn::wait_for_bundle_stat 0 2 pending

    dtn::check_bundle_data 0 bundleid-0 \
	    bundleid     0 \
	    source       $source_eid1 \
	    dest         $dest_eid1   \
	    expiration   30	      \
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check_bundle_data 0 bundleid-1 \
	    bundleid     1              \
	    source       $source_eid2   \
	    dest         $dest_eid2     \
	    expiration   10		\
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check test_reg_exists 0 10
    dtn::check test_reg_exists 0 11

    dtn::check_reg_data 0 10   \
	    regid          10        \
	    endpoint       $reg_eid1 \
	    expiration     10        \
	    failure_action 1         \
	    session_flags  0


    dtn::check_reg_data 0 11         \
	    regid          11        \
	    endpoint       $reg_eid2 \
	    expiration     30        \
	    failure_action 0         \
	    session_flags  0
}

test::script {
    testlog "starting dtnd..."
    dtn::run_dtnd 0
    
    testlog "waiting for dtnd"
    dtn::wait_for_dtnd 0

    testlog "setting up flamebox ignores"
    tell_dtnd 0 log /test always \
	    "flamebox-ignore ign scheduling IMMEDIATE expiration"

    testlog "injecting two bundles"
    tell_dtnd 0 bundle inject $source_eid1 $dest_eid1 $payload expiration=30
    tell_dtnd 0 bundle inject $source_eid2 $dest_eid2 $payload expiration=10

    testlog "running dtntest"
    dtn::run_dtntest 0
    dtn::wait_for_dtntest 0

    testlog "creating two registrations"
    set h [dtn::tell_dtntest 0 dtn_open]
    dtn::tell_dtntest 0 dtn_register $h endpoint=$reg_eid1 expiration=10 \
            failure_action=defer
    dtn::tell_dtntest 0 dtn_register $h endpoint=$reg_eid2 expiration=30 \
            failure_action=drop
    dtn::tell_dtntest 0 dtn_close $h
    
    testlog "checking the data before shutdown"
    check

    testlog "restarting dtnd without the tidy option"
    dtn::stop_dtnd 0
    after 2000
    dtn::run_dtnd 0 dtnd {}
    dtn::wait_for_dtnd 0

    testlog "checking the data after reloading the database"
    check

    testlog "shutting down dtnd"
    dtn::stop_dtnd 0

    testlog "waiting for expirations to elapse"
    after 10000

    testlog "restarting dtnd"
    dtn::run_dtnd 0 dtnd {}
    after 2000
    dtn::wait_for_dtnd 0

    testlog "checking that 1 bundle expired"
    dtn::check_bundle_stats 0 1 pending

    testlog "checking that 1 registration expired"
    dtn::check ! test_reg_exists 0 10
    dtn::check test_reg_exists 0 11

    testlog "waiting for the others to expire"
    after 20000

    testlog "checking they're expired"
    dtn::check_bundle_stats 0 0 pending
    dtn::check ! test_reg_exists 0 10
    dtn::check ! test_reg_exists 0 11

    testlog "restarting again"
    dtn::stop_dtnd 0
    after 5000
    dtn::run_dtnd 0 dtnd {}
    dtn::wait_for_dtnd 0

    testlog "making sure they're all really gone"
    dtn::check_bundle_stats 0 0 pending
    dtn::check ! test_reg_exists 0 10
    dtn::check ! test_reg_exists 0 11

    testlog "clearing flamebox ignores"
    tell_dtnd 0 log /test always \
	    "flamebox-ignore ign scheduling IMMEDIATE expiration"
}

test::exit_script {
    testlog "Shutting down dtnd and dtntest"
    dtn::stop_dtntest 0
    dtn::stop_dtnd 0
}
