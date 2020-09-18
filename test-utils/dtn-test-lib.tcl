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

import "dtn-config.tcl"
import "dtn-topology.tcl"

# procedure naming conventions:
#
# test_*     - returns true/false for a given check
# check_*    - errors out if the check fails
# wait_for_* - errors out if the check never succeeds within a timeout

proc tell_dtnd {id args} {
    return [eval dtn::tell_dtnd $id $args]
}

proc tell_dtntest {id args} {
    return [eval dtn::tell_dtntest $id $args]
}

namespace eval dtn {
    proc run_dtnd { id {exec_file "dtnd"} {other_opts "-t"} } {
	global opt net::portbase test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_dtnd $id $exec_file $other_opts]
	    }
	    return $pids
	}
	
	set exec_opts "-i $id -c $test::testname.conf --seed $opt(seed)"

	append exec_opts " $other_opts"

	return [run::run $id dtnd $exec_opts $test::testname.conf \
		    [conf::get dtnd $id] "" $exec_file]
	
    }

    proc run_dtntest { id {other_opts ""}} {
	global opt net::listen_addr net::portbase test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_dtntest $id $other_opts]
	    }
	    return $pids
	}

	set confname "$test::testname.dtntest.conf"
	set exec_opts "-c $confname"
	append exec_opts " $other_opts"
        
	lappend exec_env DTNAPI_ADDR $net::listen_addr($id)
	lappend exec_env DTNAPI_PORT [dtn::get_port api $id]

	return [run::run $id "dtntest" $exec_opts \
		$confname [conf::get dtntest $id] $exec_env]
	
    }

    proc stop_dtnd {id} {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		stop_dtnd $id
	    }
	    return
	}

	if [catch {
	    tell_dtnd $id shutdown
	    after 500
	} err] {
	    puts "ERROR: error in shutdown of dtnd id $id: $err"
	}
	catch {
	    tell::wait_for_close $net::host($id) [dtn::get_port console $id]
	}
    }
    
    proc stop_dtntest {id} {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		stop_dtntest $id
	    }
	    return
	}

	if [catch {
	    tell_dtntest $id shutdown
	} err] {
	    puts "ERROR: error in shutdown of dtnd id $id"
	}
	catch {
	    tell::close_socket $net::host($id) [dtn::get_port dtntest $id]
	}
    }

    proc app_env {id} {
        global net::listen_addr
        
        set exec_env {}
        
	lappend exec_env DTNAPI_ADDR $net::listen_addr($id)
	lappend exec_env DTNAPI_PORT [dtn::get_port api $id]

        return $exec_env
    }

    proc run_app { id app_name {exec_args ""} {exec_name ""}} {
	global opt net::listen_addr net::portbase test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_app $id $app_name $exec_args $exec_name]
	    }
	    return $pids
	}

        set exec_env [app_env $id]
	
	return [run::run $id "$app_name" $exec_args \
		    $test::testname-$app_name.conf \
		    [conf::get $app_name $id] $exec_env $exec_name]
    }

    proc run_app_and_wait { id app_name {exec_args ""} } {
        set pid [run_app $id $app_name $exec_args]
        run::wait_for_pid_exit $id $pid
    }

    proc wait_for_dtnd {id} {
	global net::host
	
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		wait_for_dtnd $id
	    }
	}

	tell::wait $net::host($id) [dtn::get_port console $id]
    }
    
    proc wait_for_dtntest {id} {
	global net::host
	
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		wait_for_dtnd $id
	    }
	}

	tell::wait $net::host($id) [dtn::get_port dtntest $id]
    }
    
    proc tell_dtnd { id args } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		eval tell_dtnd $id $args
	    }
	    return
	}
	return [eval "tell::tell $net::host($id) \
		[dtn::get_port console $id] $args"]
    }

    proc tell_dtntest { id args } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		eval tell_dtntest $id $args
	    }
	    return
	}
	return [eval "tell::tell $net::host($id) \
		[dtn::get_port dtntest $id] $args"]
    }
    
    # generic checker function
    proc check {args} {
	set orig_args $args
	
	set expected 1
	if {[lindex $args 0] == "!"} {
	    set expected 0
	    set args [lrange $args 1 end]
	}

	set result [eval $args]
 	if {$result != $expected} {
	    error "check '$orig_args' failed"
	}
    }
    
    # generic checker function
    proc check_equal {result expected} {
 	if {$result != $expected} {
	    error "check result '$result' != expected '$expected'"
	}
    }
    
    # dtn bundle data functions

    proc check_bundle_arrived {id bundle_guid} {
	if {![dtn::tell_dtnd $id "info exists bundle_info($bundle_guid)"]} {
	    error "check for bundle arrival failed: \
	        node $id bundle $bundle_guid"
	}
    }

    proc wait_for_bundle {id bundle_guid {timeout 30}} {
	do_until "wait_for_bundle $bundle_guid" $timeout {
	    if {![catch {check_bundle_arrived $id $bundle_guid}]} {
		break
	    }
	    after 500
	}
    }

    # this gets bundle data either through the tcl registration (in
    # which case it's in a global array bundle_info and bundle_guid is
    # a source,timestamp pair, or by calling the bundle dump_tcl
    # command if it's in the pending list, in in which case bundle_guid
    # should be of the form "bundleid-XXX"
    
    proc get_bundle_data {id bundle_guid} {
	if [regexp -- {bundleid-([0-9]+)} $bundle_guid match bundle_id] {
	    return [dtn::tell_dtnd $id "bundle dump_tcl $bundle_id"]
	} else {
	    return [dtn::tell_dtnd $id "set bundle_info($bundle_guid)"]
	}
    }

    proc check_bundle_data {id bundle_guid {args}} {
	array set bundle_data [get_bundle_data $id $bundle_guid]
	foreach {var val} $args {
	    if {$bundle_data($var) != $val} {
		error "check_bundle_data: bundle $bundle_guid \
			$var $bundle_data($var) != expected $val"
	    }
	}
    }

    # dtn status report bundle data functions

    proc check_sr_arrived {id sr_guid} {
	if {![dtn::tell_dtnd $id "info exists bundle_sr_info($sr_guid)"]} {
	    error "check for SR arrival failed: node $id SR $sr_guid"
	}
    }

    proc wait_for_sr {id sr_guid {timeout 30}} {
	do_until "wait_for_sr $sr_guid" $timeout {
	    if {![catch {check_sr_arrived $id $sr_guid}]} {
		break
	    }
	    after 500
	}
    }

    proc get_sr_data {id sr_guid} {
	return [dtn::tell_dtnd $id "set bundle_sr_info($sr_guid)"]
    }

    proc check_sr_data {id sr_guid {args}} {
	array set sr_data [get_sr_data $id $sr_guid]
	foreach {var val} $args {
	    if {$sr_data($var) != $val} {
		error "check_sr_data: SR $sr_guid \
			$var $sr_data($var) != expected $val"
	    }
	}
    }

    proc check_sr_fields {id sr_guid {args}} {
	array set sr_data [get_sr_data $id $sr_guid]
	foreach field $args {
	    if {![info exists sr_data($field)]} {
		error "check_sr_fields: SR \"$sr_guid\" field $field not found"
	    }
	}
    }

    # registration functions
    proc test_reg_exists {id regid} {
	if [catch {
	    tell_dtnd $id registration dump_tcl $regid
	} err] {
	    return 0
	}
	return 1
    }
    
    proc check_reg_data {id regid {args}} {
	array set reg_data [tell_dtnd $id registration dump_tcl $regid]
	foreach {var val} $args {
	    if {$reg_data($var) != $val} {
		error "check_reg_data: registration $regid \
			$var $reg_data($var) != expected $val"
	    }
	}
    }

    # dtnd "bundle stats" functions

    proc get_bundle_stat {id name} {
        if {$id == "*"} {
            set ret ""
            foreach id [net::nodelist] {
                append ret "$id: [get_bundle_stat $id $name]\n"
            }
            return $ret
        }

        if {$name == "all"} {
            return [dtn::tell_dtnd $id "bundle stats"]
        }
        
        set stats [regsub -all -- {--} [dtn::tell_dtnd $id "bundle stats"] ""]
	foreach {val stat_type} $stats {
	    if {$stat_type == $name} {
		return $val
	    }
	}
	error "unknown stat $name"
    }

    proc get_bundle_stats {id name} {
        return [get_bundle_stat $id $name]
    }
    
    proc check_bundle_stats {id args} {
        if {[llength $args] == 1} {
            set args [lindex $args 0]
        }
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		error "node $id stat check for $stat_type failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_bundle_stats {id args} {
        if {[llength $args] == 1} {
            set args [lindex $args 0]
        }
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_bundle_stat {id val stat_type {timeout 30}} {
	do_until "in wait for node $id's stat $stat_type = $val" $timeout {
	    if {[test_bundle_stats $id $val $stat_type]} {
		break
	    }
	    after 500
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_bundle_stats {id stat_list {timeout 30}} {
	foreach {val stat_type} $stat_list {
	    do_until "in wait for node $id's stat $stat_type = $val" $timeout {
		if {[test_bundle_stats $id $val $stat_type]} {
		    break
		}
		after 500
	    }
	}
    }

    # daemon stat functions
    proc get_daemon_stats {id} {
        if {$id == "*"} {
            set ret ""
            foreach id [net::nodelist] {
                append ret "$id: [get_daemon_stats $id]\n"
            }
            return $ret
        }

        return [tell_dtnd $id bundle daemon_stats]
    }

    proc check_daemon_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle daemon_stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		error "node $id stat check for $stat_type failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_daemon_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle daemon_stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_daemon_stat {id val stat_type {timeout 30}} {
	do_until "in wait for node $id's daemon stat $stat_type = $val" $timeout {
	    if {[test_daemon_stats $id $val $stat_type]} {
		break
	    }
	    after 500
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_daemon_stats {id stat_list {timeout 30}} {
	foreach {val stat_type} $stat_list {
	    do_until "in wait for node $id's daemon stat $stat_type = $val" $timeout {
		if {[test_daemon_stats $id $val $stat_type]} {
		    break
		}
		after 500
	    }
	}
    }

    # dtnd "link state" functions

    proc check_link_state { id link state } {
	set result [tell_dtnd $id "link state $link"]

	if {$result != $state} {
	    error "ERROR: check_link_state: \
		id $id expected state $state, got $result"
	}
    }

    proc wait_for_link_state { id link states {timeout 30} } {
	do_until "waiting for link state $states" $timeout {
	    foreach state $states {
		if {![catch {check_link_state $id $link $state}]} {
		    return
		}
	    }
	    after 500
	}
    }


    # utility function to wait until no bundles are queued for
    # transmission or in flight
    proc wait_for_state_on_all_links {id state {timeout 30}} {
        do_until "wait_for_state_on_all_links $id $state" $timeout {
            if {$id == "*"} {
                set ids [net::nodelist]
            } else {
                set ids $id
            }

            # to make sure that all links meet the criteria, we use
            # wait_for_link_state so that any error includes the
            # link that failed to match
            if [catch {
                foreach id $ids {
                    foreach l [tell_dtnd $id link names] {
                        wait_for_link_state $id $l $state 1
                    }
                }
            } err] {
                if {[do_timeout_remaining] == 0} {
                    error $err
                } else {
                    continue
                }
            }
            
            break
        }
    }

    # dtnd "link stats" functions
    
    proc get_link_stat {id link {name "all"} } {
        if {$id == "*"} {
            set ret ""
            foreach id [net::nodelist] {
                append ret [get_link_stat $id $link $name]
            }
            return $ret
        }
        
	if {$link == "*"} {
	    set links [dtn::tell_dtnd $id link dump]
	    foreach line [split $links "\n"] {
		set link [lindex [split $line] 0]
                if {$link == ""} {
                    continue
                }
		append ret "$id $link: [get_link_stat $id $link $name]\n"
	    }

            return $ret
	}

	if {$name == "all"} {
	    return [dtn::tell_dtnd $id "link stats $link"]
	}

        set stats [regsub -all -- {--} [dtn::tell_dtnd $id "link stats $link"] ""]
	foreach {val stat_type} $stats {
	    if {$stat_type == $name} {
		return $val
	    }
	}
	error "unknown stat $name"
    }
    
    proc check_link_stats {id link args} {
        if {[llength $args] == 1} {
            set args [lindex $args 0]
        }
        set stats [dtn::tell_dtnd $id "link stats $link"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		error "node $id link $link stat check for ${stat_type} failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_link_stats {id link args} {
        if {[llength $args] == 1} {
            set args [lindex $args 0]
        }
        set stats [dtn::tell_dtnd $id "link stats $link"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_link_stat {id link val stat_type {timeout 30}} {
	do_until "wait for node $id's link $link stat $stat_type = $val" \
		$timeout {
	    if {[test_link_stats $id $link $val $stat_type]} {
		break
	    }
	    after 500
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_link_stats {id link stat_list {timeout 30}} {
        do_until "wait for node $id's link $link stats $stat_list" $timeout {
            set ok 1
            foreach {val stat_type} $stat_list {
		if {! [test_link_stats $id $link $val $stat_type]} {
		    set ok false
                    break
		}
	    }
            
            if {$ok} {
                return
            }
            
            after 500
	}
    }

    # utility function to wait until no bundles are queued for
    # transmission or in flight
    proc wait_for_stats_on_all_links {id stat_list {timeout 30}} {
        do_until "wait_for_stats_on_all_links $id $stat_list" $timeout {
            if {$id == "*"} {
                set ids [net::nodelist]
            } else {
                set ids $id
            }

            # to make sure that all links meet the criteria, we use
            # wait_for_link_stats so that any error includes the
            # specific link/stat pair that failed to match
            if [catch {
                foreach id $ids {
                    foreach l [tell_dtnd $id link names] {
                        wait_for_link_stats $id $l $stat_list 1
                    }
                }
            } err] {
                if {[do_timeout_remaining] == 0} {
                    error $err
                } else {
                    continue
                }
            }
            
            break
        }
    }

    # route state functions
    proc test_route {id dest link params} {
        set routes [tell_dtnd $id route dump_tcl]
        foreach route $routes {
            set d [lindex $route 0]
            set l [lindex $route 1]
            if {$dest == $d && $link == $l} {
                # XXX/demmer check args
                return true;
            }
        }
        return false
    }

    proc check_route {id dest link params} {
        if {![test_route $id $dest $link $params]} {
            error "ERROR: check_route: expected route on $id \
                    to $dest ($link $params)"
        }
    }

    proc test_no_route {id dest} {
        set routes [tell_dtnd $id route dump_tcl]
        foreach route $routes {
            set d [lindex $route 0]
            set l [lindex $route 1]
            if {$dest == $d} {
                return false
            }
        }
        return true
    }

    proc check_no_route {id dest} {
        if {![test_no_route $id $dest]} {
            error "ERROR: check_route: expected no route on $id to $dest"
        }
    }

    proc wait_for_route {id dest link params {timeout 30}} {
        do_until "wait for node $id's route to $dest: link $link $params" \
                $timeout {
            if {[test_route $id $dest $link $params ]} {
                break
            }
            after 500
        }
    }

    proc dump_stats {} {
        global errorInfo
        puts "============================================================"
        foreach id [net::nodelist] {
            puts "Statistics for dtnd $id:"
            if [catch {
                puts [tell_dtnd $id bundle stats]
                foreach l [tell_dtnd $id link names] {
                    puts "Link stats for $l: [tell_dtnd $id link stats $l]"
                }
            } err] {
                puts "$err\n$errorInfo"
            }
            puts "============================================================"
        }
    }

    proc dump_routes {} {
        global errorInfo
        puts "============================================================"
        foreach id [net::nodelist] {
            puts "Route table for dtnd $id:"
            if [catch {puts [tell_dtnd $id route dump]} err] {
                puts "$err\n$errorInfo"
            }
            puts "============================================================"
        }
    }
}
