#!/usr/bin/tclsh

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



# usage: test-utils/run-tcl-tests.tcl <test group> <run-dtn.tcl options?>
if {[llength $argv] < 1} {
    puts "test-utils/run-tcl-tests.tcl <test group> options..."
    puts ""
    puts "Required:"
    puts "    <test group> Test group to run"
    puts ""
    exit
}

# the basic test group
set tests(basic) {
    "alwayson-links.tcl"	""
    "api-poll.tcl"		""
    "bundle-status-reports.tcl"	""
    "custody-transfer.tcl"      ""
    "discovery.tcl"             ""
    "dtnsendrecv.tcl"           ""
    "dtn-cp.tcl"		""
    "dtn-ping.tcl"		""
    "dtn-tunnel.tcl"		""
    "dtnsendrecv.tcl"           ""
    "dtnsim.tcl"		"-c two-node.conf"
    "dtnsim.tcl"		"-c bandwidth-test.conf"
    "dtnsim.tcl"		"-c storage-limit-test.conf"
    "dtnsim.tcl"		"-c conn-test.conf"
    "extension-block.tcl"       ""
    "flood-router.tcl"		""
    "inflight-expiration.tcl"	""
    "inflight-interrupt.tcl"	""
    "is-singleton.tcl"		""
    "ipc-version-test.tcl"	""
    "loopback-link.tcl"		""
    "multipath-forwarding.tcl"	""
    "multiple-registrations.tcl" ""
    "ondemand-links.tcl"	""
    "no-duplicate-send.tcl"	""
    "reactive-fragmentation.tcl" ""
    "reroute-from-down-link.tcl" ""
    "sequence-id.tcl"		""
    "send-one-bundle.tcl"	"-cl tcp"
    "send-one-bundle.tcl"	"-cl tcp -length 0"
    "send-one-bundle.tcl"	"-cl tcp -length 5 -segment_length 1"
    "send-one-bundle.tcl"	"-cl tcp -test_read_limit 1"
    "send-one-bundle.tcl"	"-cl udp -length 0"
    "send-one-bundle.tcl"	"-cl udp"
    "storage.tcl"		""
    "storage.tcl"		"-storage_type filesysdb"
    "tcp-bogus-link.tcl"	""
    "unknown-scheme.tcl"	""
    "version-mismatch.tcl"	""
}


# BlueZ utils test group
set tests(bluez) {
    "bluez-rfcomm.tcl"   ""
    "bluez-sdp.tcl"    ""
    "bluez-inq.tcl"    ""
    "bluez-links.tcl"    ""
}

# the stress test group
set tests(stress) {
    "bidirectional.tcl"         ""
    "many-bundles.tcl"		""
    "many-bundles.tcl"		"-storage_type berkeleydb-no-txn"
    "many-bundles.tcl"		"-storage_type filesysdb"
    "dtn-perf.tcl"		""
    "dtn-perf.tcl"		"-payload_len 50B"
    "dtn-perf.tcl"		"-payload_len 5MB  -file_payload"
    "dtn-perf.tcl"		"-payload_len 50MB -file_payload"
    "dtn-perf.tcl"		"-storage_type filesysdb"
    "dtn-perf.tcl"		"-payload_len 50B -storage_type filesysdb"
    "dtn-perf.tcl"		"-payload_len 5MB  -file_payload -storage_type filesysdb"
    "dtn-perf.tcl"		"-payload_len 50MB -file_payload -storage_type filesysdb"
}

# LTP CL utils test group
set tests(ltp) {
    "dtn-ping.tcl"		"-cl ltp"
    "dtn-ping.tcl"              "-n 1 -cl ltp -s ltp.sched"
    "send-one-bundle.tcl"       "-cl ltp -length 0"
    "send-one-bundle.tcl"       "-cl ltp -length 10"
    "send-one-bundle.tcl"       "-cl ltp -length 100"
    "send-one-bundle.tcl"       "-cl ltp -length 1000"
    "dtn-perf.tcl"              "-cl ltp -payload_len 1024B -perftime 30 -storage_type filesysdb"
    "dtn-perf.tcl"              "-cl ltp -payload_len 10240B -perftime 30 -storage_type filesysdb"
    "dtn-perf.tcl"              "-cl ltp -payload_len 1MB -perftime 30 -storage_type filesysdb"
    "custody-transfer.tcl"      "-cl ltp"
}

# LTP CL utils test group
# Setup a test environment showing some bundles of various sizes (1k, 10k, 100K) 
# being sent in both directions using the TCP-CL and the LTP-CL (red and green mode) 
# with no added delay and no imposed errors
set tests(ltp_WP4200) {
    "dtn-ping.tcl"              "-n 1 -e 30 -cl tcp -t 999"
    "send-one-bundle.tcl"       "-cl tcp -length 1000"
    "send-one-bundle.tcl"       "-cl tcp -length 10000"
    "send-one-bundle.tcl"       "-cl tcp -length 100000"
    "dtn-ping.tcl"              "-n 1 -e 30 -cl ltp -t 999"
    "send-one-bundle.tcl"       "-cl ltp -length 1000"
    "send-one-bundle.tcl"       "-cl ltp -length 10000"
    "send-one-bundle.tcl"       "-cl ltp -length 100000"
}

# check out send-one-bundle.tcl - getting an error when run from above
set tests(tsob) {
    "send-one-bundle.tcl"       "-cl tcp -length 1000"
}


set tests(other) {
    api-leak-test.tcl"		""
}


# check test group
set group [lindex $argv 0]
set common_options [lrange $argv 1 end]

if {![info exists tests($group)]} {
    puts "Error: test group \"$group\" not defined"
    exit -1
}

# print a "Table of Contents" first before running the tests
puts "***"
puts "*** The following tests will be run:"
puts "***"
foreach {test testopts} $tests($group) {
    puts "***   $test $testopts"
}
puts "***\n"

# run the tests
foreach {test testopts} $tests($group) {
    puts "***"
    puts "*** Running Test: $test $testopts"
    puts "***"

    if {$testopts != ""} {
	set options "$common_options --opts \"$testopts\""
    } else {
	set options $common_options
    }
    
    if [catch {
	eval exec [file dirname $argv0]/run-dtn.tcl test/$test -d $options \
		< /dev/null >&@ stdout
    } err ] {
	puts "unexpected error: $err"
    }

    # give the last test an extra couple seconds to quit
   after 500
}


