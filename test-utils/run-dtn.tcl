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



#
# Before the import, snarf out the base test dir option so we can
# properly set up the import path
#
set base_test_dir [pwd]
if {[llength $argv] >= 2} {
    for {set i 0} {$i < [llength $argv]} {incr i} {
	if {[lindex $argv $i] == "--base-test-dir"} {
	    set base_test_dir [file normalize [pwd]/[lindex $argv [expr $i + 1]]]
	}
    }
}

source "$base_test_dir/../test-utils/import.tcl"

set import::path [list \
	$base_test_dir/../test-utils \
	$base_test_dir/../oasys/tclcmd \
	$base_test_dir/oasys/tclcmd \
	$base_test_dir/test-utils \
	$base_test_dir/test/nets \
	]

import "test-lib.tcl" 
import "dtn-test-lib.tcl"

if {[llength $argv] < 1} {
    puts "run-dtn.tcl <init_options> <test script> <options>..."
    puts ""
    puts "Required:"
    puts "    <test script> Test script to run"
    puts ""
    run::usage

    if {[info commands real_exit] != ""} {
	real_exit
    } else {
	exit
    }
}

# no buffering for stdout
fconfigure stdout -buffering none

# default args
set defaults [list -net localhost --gdb-match dtnd*]
set argv [concat $defaults $argv]
run::init $argv

test::error_script { dtn::dump_stats; dtn::dump_routes }
test::run_script
if {!$opt(daemon)} {
    command_loop "dtntest% "
}
test::run_exit_script
run::wait_for_programs
run::collect_logs
run::cleanup
test::run_cleanup_script
