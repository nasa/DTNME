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

test::name startstop
net::default_num_nodes 1

dtn::config

test::script {
    testlog "Startstop test script executing..."
    dtn::run_dtnd 0
    
    testlog "Waiting for dtnd"
    dtn::wait_for_dtnd 0
    set dtnpid [dtn::tell_dtnd 0 pid]

    testlog "Sending dtnd pid $dtnpid an abort signal"
    run::kill_pid 0 $dtnpid ABRT

    testlog "Test complete"
}
