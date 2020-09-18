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

test::name dtnsendrecv
net::default_num_nodes 2

manifest::file apps/dtnsend/dtnsend dtnsend
manifest::file apps/dtnrecv/dtnrecv dtnrecv

set testopt(cl)       tcp
run::parse_test_opts

dtn::config

testlog "Configuring $testopt(cl) interfaces / links"
dtn::config_interface $testopt(cl)
dtn::config_linear_topology ALWAYSON $testopt(cl) true

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last   [expr [net::num_nodes] - 1]

    set source dtn://host-0/test
    set dest   dtn://host-$last/test
    
    testlog "Sending a small memory bundle"
    dtn::run_app_and_wait 0     dtnsend "-s $source -d $dest -t m -p test_payload"
    dtn::run_app_and_wait $last dtnrecv "-n 1 $dest"

    testlog "Sending dtnsend executable in a bundle"
    set file [file join [dist::get_rundir $net::host(0) 0] dtnsend]
    dtn::run_app_and_wait 0     dtnsend "-s $source -d $dest -t f -p $file"
    dtn::run_app_and_wait $last dtnrecv "-q -n 1 $dest"

    testlog "Sending dtnd executable in a bundle"
    set file [file join [dist::get_rundir $net::host(0) 0] dtnd]
    dtn::run_app_and_wait 0     dtnsend "-s $source -d $dest -t f -p $file"
    dtn::run_app_and_wait $last dtnrecv "-q -n 1 $dest"

    testlog "Checking to see if any bundle payloads were leaked"
    set payload_files [tell_dtnd $last glob -nocomplain /tmp/bundlePayload*]
    if {$payload_files != ""} {
        puts "WARNING: payload files still exist on destination host: $payload_files"
    }

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
