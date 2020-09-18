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
# Default config shared by all these topologies
#

namespace eval dtn {

set router_type "static"

set eid_pattern {dtn://host-$id}

proc get_eid {id} {
    return [subst $dtn::eid_pattern]
}

proc config_topology_common {with_routes} {
    global dtn::router_type dtn::local_eid_pattern
    
    foreach id [net::nodelist] {
	conf::add dtnd $id "route set type $router_type"
	conf::add dtnd $id "route local_eid [get_eid $id]"
	conf::add dtnd $id "route set add_nexthop_routes $with_routes"
    }
}

#
# Generate a unique link name
#
proc new_link_name {cl src dst} {
    global dtn::link_names

    set name "$cl-link:$src-$dst"
    if {![info exists link_names($name)] } {
	set link_names($name) 1
	return $name
    }

    set i 2
    while {1} {
	set name "$cl-link.$i:$src-$dst"
	if {![info exists link_names($name)]} {
	    set link_names($name) 1
	    return $name
	}
	incr i
    }
}

#
# Return the name of a link between a and b (if it's been configured)
#
proc get_link_name {cl src dst} {
    global dtn::link_names
    
    set name "$cl-link:$src-$dst"
    if {![info exists link_names($name)] } {
	return ""
    }
    return $name
}

#
# Add a link and optionally a route from a to b.
#
proc config_link {id peerid type cl link_args} {
    global dtn::link_names net::internal_host bluez::btaddr
    
    set peeraddr $net::internal_host($peerid)
    set peerport [dtn::get_port $cl $peerid]
    set localaddr [info hostname]
    set localport [dtn::get_port $cl $id]

    set link [new_link_name $cl $id $peerid]
    set peer_eid  [get_eid $peerid]

    # For bidirectional convergence layers, we configure an ONDEMAND
    # or ALWAYSON link in only one direction, since the other side
    # gets created in response to the connection establishment, and
    # hence is OPPORUNISTIC.
    if {[dtn::is_bidirectional $cl] && \
	    ([string toupper $type] == "ONDEMAND" || \
             [string toupper $type] == "ALWAYSON") && \
	    [get_link_name $cl $peerid $id] != ""} {
	set type OPPORTUNISTIC
    }

    set nexthop $peeraddr:$peerport

    if {$cl == "bt"} {
        puts "% bt link nexthop $bluez::btaddr($peerid) -> $peer_eid"
    set nexthop $bluez::btaddr($peerid)  
    } 
    
    if {$cl == "ltp"} {
    set nexthop $localaddr:$peerport
    conf::add dtnd $id [join [list \
            link add $link $nexthop $type $cl \
            local_addr=$localaddr local_port=$localport \
            remote_eid=$peer_eid $link_args]]
            }

    if {$cl == "tcp" || $cl == "udp"} { 
    conf::add dtnd $id [join [list \
	    link add $link $nexthop $type $cl \
	    remote_eid=$peer_eid $link_args]]
    }
    return $link

}
#
# Set up a linear topology using TCP, UDP or LTP
#
proc config_linear_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common $with_routes
    
    set last [expr [net::num_nodes] - 1]
    
    foreach id [net::nodelist] {

	# link to next hop in chain (except for last one) and routes
	# to non-immediate neighbors
	if { $id != $last } {
	    set link [config_link $id [expr $id + 1] $type $cl $link_args]

            if {$with_routes} {
                for {set dest [expr $id + 2]} {$dest <= $last} {incr dest} {
                    conf::add dtnd $id \
                            "route add [get_eid $dest]/* $link"
                }
            }
	}
	
	# and previous a hop in chain as well
	if { $id != 0 } {
	    set link [config_link $id [expr $id - 1] $type $cl $link_args]
            
	    if {$with_routes} {
		for {set dest [expr $id - 2]} {$dest >= 0} {incr dest -1} {
		    conf::add dtnd $id \
			    "route add [get_eid $dest]/* $link"
		}
	    }
	}
    }
}


#
# Set up a ltp schedule on linear topology using LTP
# Fist node is the receiver, middle node is a router and 
# last is the sender
#
proc config_ltp_schedule {cl sched} {
    set last [expr [net::num_nodes] - 1]
    foreach id [net::nodelist] {

        if { $id == $last } {
	set distdir test/schedule/ltp-schedule/sender
    	dbg "% copying files"
        set hostname $net::host($id)
        set targetdir [dist::get_rundir $hostname $id]

        dbg "% $distdir -> $hostname:$targetdir"

        if [net::is_localhost $hostname] {
            testlog "distributing $distdir/$sched to $targetdir/$sched on sending node $hostname"
            exec cp -r $distdir/$sched $targetdir/$sched
        } else {
	    testlog "distributing $distdir/$sched to $hostname:$targetdir/$sched"
            exec scp -C -r $distdir/$sched $hostname:$targetdir/$sched
        }


        } elseif { $id == 0 } {
        set distdir test/schedule/ltp-schedule/receiver
        dbg "% copying files"
        set hostname $net::host($id)
        set targetdir [dist::get_rundir $hostname $id]

        dbg "% $distdir -> $hostname:$targetdir"

        if [net::is_localhost $hostname] {
            testlog "distributing $distdir/$sched to $targetdir/$sched on receiving node $hostname"
            exec cp -r $distdir/$sched $targetdir/$sched
        } else {
            testlog "distributing $distdir/$sched to $hostname:$targetdir/$sched"
            exec scp -C -r $distdir/$sched $hostname:$targetdir/$sched
        }
	} else {
        set distdir test/schedule/ltp-schedule/router
        dbg "% copying files"
        set hostname $net::host($id)
        set targetdir [dist::get_rundir $hostname $id]

        dbg "% $distdir -> $hostname:$targetdir"

        if [net::is_localhost $hostname] {
            testlog "distributing $distdir/$sched to $targetdir/$sched on router node $hostname"
            exec cp -r $distdir/$sched $targetdir/$sched
        } else {
            testlog "distributing $distdir/$sched to $hostname:$targetdir/$sched"
            exec scp -C -r $distdir/$sched $hostname:$targetdir/$sched
        }
	}
	}
}


#
# Set up a mesh topology with TCP or UDP
#
proc config_mesh_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common $with_routes
    
    set last [expr [net::num_nodes] - 1]
    
    foreach a [net::nodelist] {
        foreach b [net::nodelist] {
            if {$a != $b} {
                set link [config_link $a $b $type $cl $link_args]
            }

            if {$with_routes} {
		conf::add dtnd $id \
			"route add [get_eid $dest]/* $link"
	    }
	}
    }
}

#
# Set up a tree-based routing topology on nodes 0-254
#
#                                 0
#                                 |
#           /--------------/-----/|\----\--------------\
#          1              2     .....    8              9
#   /---/-----\---\      /-\            /-\      /---/-----\---\
#  10  11 ... 18  19     ...            ...     90  91 ... 98  99
#  |   |  ...  |   |                            |   |       |   |
# 110 111     118 119                          190 191     198 199
#  |   |       |   |  
# 210 211     218 219
#
proc config_tree_topology {type cl {link_args ""}} {
    global hosts ports num_nodes id route_to_root

    error "XXX/demmer fixme"

    # the root has routes to all 9 first-hop descendents
    if { $id == 0 } {
	for {set child 1} {$child <= 9} {incr child} {
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
	    route add [get_eid $child]/* link-$child
	}
    }

    # otherwise, the first hop nodes have routes to the root and their
    # 9 second tier descendents
    if { $id >= 1 && $id <= 9 } {
	set parent 0
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $link_args
	
	route add [get_eid $parent]/* link-$parent
	set route_to_root [get_eid $parentaddr]
	
	for {set child [expr $id * 10]} \
		{$child <= [expr ($id * 10) + 9]} \
		{incr child}
	{
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
	    route add [get_eid $child]/* link-$child
	}
    }

    # for third-tier nodes 100-199, set their upstream route. for
    # 100-154, can also set a fourth-tier route to 200-254
    if { $id >= 100 && $id <= 199 } {
	set parent [expr $id / 10]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $link_args
	
	route add [get_eid $parent]/* link-$parent
	set route_to_root [get_eid $parentaddr]

	if {$id <= 154} {
	    set child [expr $id + 100]
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
	    route add [get_eid $child]/* link-$child
	}
    }

    # finally, the fourth tier nodes 200-254 just set a route to their parent
    if { $id >= 200 && $id <= 255 } {
	set parent [expr $id - 100]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $link_args
	
	route add [get_eid $parent]/* link-$parent
	set route_to_root [get_eid $parentaddr]
    }
}

# A two level tree with a single root and N children
#            0
#      /  / ...  \  \
#     1     ...      n
#
proc config_twolevel_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common $with_routes
 
    foreach id [net::nodelist] {
	if { $id != 0 } {
	    config_link 0   $id $type $cl $link_args
	    config_link $id 0   $type $cl $link_args
	}
    }
}

# A simple four node diamond topology with multiple
# routes of the same priority for two-hop neighbors.
#
#      0
#     / \
#    1   2
#     \ /
#      3
proc config_diamond_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common $with_routes
     
    config_link 0 1 $type $cl $link_args
    config_link 0 2 $type $cl $link_args

    if {$with_routes} {
        conf::add dtnd 0 "route add [get_eid 3]/* $cl-link:0-1"
        conf::add dtnd 0 "route add [get_eid 3]/* $cl-link:0-2"
    }
    
    config_link 1 0 $type $cl $link_args
    config_link 1 3 $type $cl $link_args

    if {$with_routes} {
        conf::add dtnd 1 "route add [get_eid 2]/* $cl-link:1-0"
        conf::add dtnd 1 "route add [get_eid 2]/* $cl-link:1-3"
    }
    
    config_link 2 0 $type $cl $link_args
    config_link 2 3 $type $cl $link_args

    if {$with_routes} {
        conf::add dtnd 2 "route add [get_eid 1]/* $cl-link:2-0"
        conf::add dtnd 2 "route add [get_eid 1]/* $cl-link:2-3"
    }
    
    config_link 3 1 $type $cl $link_args
    config_link 3 2 $type $cl $link_args

    if {$with_routes} {
        conf::add dtnd 3 "route add [get_eid 0]/* $cl-link:3-1"
        conf::add dtnd 3 "route add [get_eid 0]/* $cl-link:3-2"
    }
}

# namespace dtn
}
