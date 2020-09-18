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
# Basic configuration proc for test dtnd's
#

namespace eval dtn {

proc config {args} {
    conf::add dtnd * {source "dtnd-test-utils.tcl"}

    # defaults
    set router_type static
    set storage_type berkeleydb
    set null_link true
    set copy_executables true
    
    set i 0
    while {$i < [llength $args]} {
	set arg [lindex $args $i]
	switch -- $arg {
	    -router_type  -
	    --router_type {
		set router_type [lindex $args [incr i]]
	    }
	    -storage_type  -
	    --storage_type {
		set storage_type [lindex $args [incr i]]
	    }
	    -no_null_link -
	    --no_null_link {
		set null_link false
	    }
            -no_copy_executables {
                set copy_executables false
            }

	    default {
		error "unknown dtn::config argument '$arg'"
	    }
	}

	incr i
    }
    
    dtn::standard_manifest $copy_executables
    dtn::config_console
    dtn::config_api_server
    dtn::config_storage $storage_type
#    if {$null_link} {
#	dtn::config_null_link
#    }

    set dtn::router_type $router_type
    dtn::config_dtntest
}

#
# Standard manifest
#
proc standard_manifest {copy_executables} {
    global opt
    if {$copy_executables} {
        manifest::file daemon/dtnd dtnd
    }
    manifest::file [file join $opt(base_test_dir) \
	    test-utils/dtnd-test-utils.tcl] dtnd-test-utils.tcl
    manifest::dir  bundles
    manifest::dir  db
}

proc config_app_manifest {copy_executables {apps "dtnsend dtnrecv" } } {
    global opt
    if {$copy_executables} {
        foreach app $apps {
            manifest::file apps/$app/$app $app
        }
    }
}

#
# Utility proc to get the adjusted port number for various things
#
proc get_port {what id} {
    global net::portbase net::used_ports
    set dtn_portbase $net::portbase($id)
    
    switch -- $what {
	console { set port [expr $dtn_portbase + 0] }
	api	{ set port [expr $dtn_portbase + 1] }
	tcp	{ set port [expr $dtn_portbase + 2] }
	udp	{ set port [expr $dtn_portbase + 2] }
        ltp     { set port [expr $dtn_portbase + 2] }
	dtntest	{ set port [expr $dtn_portbase + 3] }
	misc	{ set port [expr $dtn_portbase + 4] }
	default { return -1 }
    }
    lappend net::used_ports($id) $port
    return $port
}

#
# Utility proc to determine whether or not the given convergence layer
# is bidirectional
#
proc is_bidirectional {cl} {
    switch -- $cl {
	tcp { return 1 }
	bt { return 1 }

    }
    
    return 0
}

#
# Create a new interface for the given convergence layer
#
proc config_interface {cl args} {
    global net::listen_addr
    foreach id [net::nodelist] {
	
    if {$cl == "bt"} {
    # first pass -- just take defaults ... figure out option parsing later
    conf::add dtnd $id [eval list interface add ${cl}0 $cl]
    } elseif {$cl == "ltp"} {
        set addr $net::listen_addr($id)
        set port [dtn::get_port $cl $id]
        conf::add dtnd $id [eval list interface add ${cl}0 $cl \
                local_addr=[info hostname] local_port=$port $args]

    } else {
	set addr $net::listen_addr($id)
	set port [dtn::get_port $cl $id]
	conf::add dtnd $id [eval list interface add ${cl}0 $cl \
		local_addr=$addr local_port=$port $args]
    }
    }
}

#
# Configure the console server
#
proc config_console {} {
    global opt net::listen_addr
    foreach id [net::nodelist] {
	conf::add dtnd $id "console set addr $net::listen_addr($id)"
	conf::add dtnd $id "console set port [dtn::get_port console $id]"
	if {! $opt(xterm)} {
	    conf::add dtnd $id "console set stdio false"
	}	    
    }
}

#
# Set up the API server
#
proc config_api_server {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "api set local_addr $net::listen_addr($id)"
	conf::add dtnd $id "api set local_port [dtn::get_port api $id]"
    }
}

#
# Set up a null link on each host
#
proc config_null_link {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "link add null /dev/null ALWAYSON null"
    }
}

#
# Configure storage
#
proc config_storage {type} {
    set extra ""

    if {$type == "berkeleydb-no-txn"} {
        set type "berkeleydb"
        append extra "storage set db_txn false\n"
        append extra "storage set db_log false\n"
    }
    
    conf::add dtnd * [subst {
storage set tidy_wait  0
storage set payloaddir bundles
storage set type       $type
storage set dbname     DTN
storage set dbdir      db
$extra
    }]
}

#
# Configure dtntest
#
proc config_dtntest {} {
    foreach id [net::nodelist] {
	conf::add dtntest $id \
		"console set stdio false"
	conf::add dtntest $id \
		"console set addr $net::listen_addr($id)"
	conf::add dtntest $id \
		"console set port [dtn::get_port dtntest $id]"
    }
}

# namespace dtn
}
