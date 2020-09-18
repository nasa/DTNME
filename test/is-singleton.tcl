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

test::name is-singleton
net::num_nodes 1
manifest::file apps/dtnsend/dtnsend dtnsend
dtn::config
dtn::config_topology_common false

test::script {
    testlog "Running dtnd"
    dtn::run_dtnd 0

    testlog "Waiting for dtnd to start up"
    dtn::wait_for_dtnd *

    testlog "Setting up flamebox ignores"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign1 client disconnected without calling dtn_close"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign2 bundle destination .* in unknown scheme and app did not assert.*"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign3 error sending bundle.*"
    
    testlog "Adding some registrations"
    dtn::tell_dtnd 0 tcl_registration dtn://host-0*/* 
    dtn::tell_dtnd 0 tcl_registration str:string
    dtn::tell_dtnd 0 tcl_registration str:string*
    dtn::tell_dtnd 0 tcl_registration unknown:something

    proc test {dest opts is_singleton} {
        testlog "Testing $dest with opts \"$opts\""

        set pid [dtn::run_app 0 dtnsend "-s dtn://host-0/source -t d -d $dest $opts"]
        run::wait_for_pid_exit 0 $pid
        
        if {$is_singleton == "fail"} {
            global net::host run::dirs
            set log [run::run_cmd $net::host(0) cat $run::dirs(0)/dtnsend.out]
            if {$log != "error sending bundle: -1 (invalid argument)"} {
                error "expected failure, output is \"$log\""
            }
            # clear the output file
            run::run_cmd $net::host(0) echo "" > $run::dirs(0)/dtnsend.out
            
        } else {
            dtn::wait_for_bundle_stats 0 {1 delivered}
            set guid [lindex [tell_dtnd 0 array names bundle_info] 0]
            dtn::check_bundle_data 0 $guid singleton_dest $is_singleton
        }
        
        dtn::tell_dtnd 0 bundle reset_stats
        dtn::tell_dtnd 0 array unset bundle_info
    }

    testlog ""
    testlog "Testing known schemes..."
    testlog ""
    test dtn://host-0/foo     "" 1
    test \"dtn://host-*/foo\" "" 0
    test dtn://host-0/foo     "-1" 1
    test dtn://host-0/foo     "-N" 0
    test \"dtn://host-*/foo\" "-1" 1
    test \"dtn://host-*/foo\" "-N" 0

    test str:string      "" 1
    test str:string*     "" 1
    test str:string      "-1" 1
    test str:string*     "-1" 1
    test str:string      "-N" 0
    test str:string*     "-N" 0


    testlog ""
    testlog "Testing unknown scheme..."
    testlog ""

    test unknown:something "" 0
    testlog "Setting is_singleton_default to singleton"
    tell_dtnd 0 param set is_singleton_default singleton
    test unknown:something "" 1
    
    testlog "Setting is_singleton_default to unknown"
    tell_dtnd 0 param set is_singleton_default unknown
    test unknown:something "" fail
    test unknown:something "-1" 1
    test unknown:something "-N" 0
    
    
    testlog "Test success!"
}

test::exit_script {
    testlog "Clearing flamebox ignores"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign1"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign2"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign3"
    
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
