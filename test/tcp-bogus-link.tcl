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

manifest::file apps/dtntest/dtntest dtntest

test::name tcp-bogus-link
net::num_nodes 1

dtn::config
dtn::config_interface tcp
dtn::config_topology_common false

test::script {
    testlog "Running dtnd"
    dtn::run_dtnd 0

    testlog "Waiting for dtnd to start up"
    dtn::wait_for_dtnd 0

    testlog "Adding a bogus link"
    dtn::tell_dtnd 0 link add bogus 1.2.3.4:9999 ALWAYSON tcp

    testlog "Checking link is OPENING or UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus {OPENING UNAVAILABLE}

    testlog "Closing bogus link"
    dtn::tell_dtnd 0 link close bogus

    testlog "Checking link is UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus UNAVAILABLE

    testlog "Reopening link"
    dtn::tell_dtnd 0 link open bogus
    
    testlog "Checking link is OPENING or UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus {OPENING UNAVAILABLE}
    
    testlog "Closing bogus link again"
    dtn::tell_dtnd 0 link close bogus

    testlog "Checking link is UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus UNAVAILABLE

    testlog "Running dtntest"
    dtn::run_dtntest 0

    testlog "Bumping down the link data timeout"
    dtn::tell_dtnd 0 link set_cl_defaults tcp data_timeout=2000 

    testlog "Connecting with a bogus socket"
    set sock [dtn::tell_dtntest 0 socket $net::host(0) [dtn::get_port tcp 0]]

    testlog "Waiting for data timeout"
    after 5000

    testlog "Closing the socket"
    dtn::tell_dtntest 0 close $sock

    testlog "Running a bogus server socket"
    dtn::tell_dtntest 0 proc connected {args} {}
    set sock [dtn::tell_dtntest 0 socket -server connected \
            -myaddr $net::host(0) [dtn::get_port misc 0]]

    testlog "Creating a link to the new socket"
    dtn::tell_dtnd 0 link add l-test $net::host(0):[dtn::get_port misc 0] ALWAYSON tcp
    
    testlog "Checking that link is in state OPENING"
    dtn::wait_for_link_state 0 l-test OPENING
    
    testlog "Waiting for contact header timeout"
    after 5000
    
    testlog "Checking that link is in state UNAVAILABLE"
    dtn::check_link_state 0 l-test UNAVAILABLE
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *

    testlog "Stopping dtntest"
    dtn::stop_dtntest *
}
