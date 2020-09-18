#
# Testing scripts etc used for running in the simulator
#

source "$base_test_dir/../test-utils/import.tcl"
set import::path [list \
	$base_test_dir/../test-utils \
	$base_test_dir/../oasys/tclcmd \
	$base_test_dir/test-utils \
	$base_test_dir/test/nets \
	]

import "tcl-utils.tcl" 
import "dtn-test-lib.tcl"

# Override the tell_dtnd proc
namespace eval dtn {
    proc tell_dtnd { id args } {
        if {$id == "*"} {
            foreach node [sim nodes] {
                tell_dtnd $node $args
            }
            return
        }
	if {[llength $args] == 1} {
	    set cmd [lindex $args 0]
	} else {
	    set cmd $args
	}
        eval [concat $id $cmd]
    }
}

# Override the net::nodelist proc
namespace eval net {
    proc nodelist {} {
        return [sim nodes]
    }
}

proc parse_opts {{defaults ""}} {
    global opt opts

    if {! [info exists opt(verbose)]} {
        set opt(verbose) false
    }

    if {! [info exists opt(runtill)]} {
        set opt(runtill) [expr 365 * 24 * 60 * 60]; # 1 year
    }

    foreach {var val} $defaults {
        set opt($var) $val
    }

    foreach o $opts {
        set L [split $o =]
        set var [lindex $L 0]
        set val 1
        if {[llength $L] == 2} {
            set val [lindex $L 1]
        }

        set opt($var) $val
    }

    sim set runtill $opt(runtill)
}

proc dputs {args} {
    global opt
    if {$opt(verbose)} {
        eval puts $args
    }
}

proc enable_progress_counter {} {
    global opt

    puts -nonewline stderr " 0% done"
    for {set i 0} {$i < 100} {incr i 5} {
        sim at [expr $opt(runtill) * $i / 100.0] \
                puts -nonewline stderr [format "\b\b\b\b\b\b\b\b%2d%% done" $i]
    }
}
