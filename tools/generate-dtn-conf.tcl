#!/usr/bin/tclsh
#
# Simple tcl script to generate a static topology of dtn links and
# routes for a network.
# 

#
# Graph specification.
#
# The active connector (ALWAYSON) host is on the left, the passive
# acceptor (OPPORTUNISTIC) on the right. IP addresses can be specified
# either explicitly using host,addr or by using the addr array for
# single-homed machines.
#
set links {
    sandbox,1.1.1.1   pisco            
    wangari           sandbox,2.2.2.2  
    ica               sandbox,3.3.3.3  
    shirin            wangari
}

#
# Node address specification (optional) for single-homed machines.
#
set addr(pisco)   128.32.1.1
set addr(shirin)  3.4.5.7
set addr(wangari) 3.4.5.6
set addr(ica)     10.212.1.1

#
# Link options (not currently implemented)
#
set link_options(*)       segment_size=1024
set link_options(pisco-*) keepalive_interval=2

#
# Common config for all nodes
#
set COMMON_CONFIG \
{
set localhost [lindex [split [info hostname] .] 0]

console set prompt "$localhost dtn% "
console set addr localhost
console set port 5050

storage set type       filesysdb
storage set payloaddir /extra/dtn/bundles
storage set dbname     DTN
storage set dbdir      /extra/dtn/db

route set type static
interface add tcp0 tcp
}

#-----------------------------------------------------------------------------
#
# Graph IMPLEMENTATION STARTS HERE
#


#
# first build the list of all hosts
#
set allhosts [array names addr]
foreach h $links {
    set h [lindex [split $h ,] 0]
    if {[lsearch $allhosts $h] == -1} {
        lappend allhosts $h
    }
}

# build a full_links list as {host1 addr1 host2 addr2} quads
set l2 {}
foreach link $links {
    foreach {h a} [split $link ,] {}
    if {$a == ""} {
        if {! [info exists addr($h)] || [llength $addr($h)] > 1} {
            error "need unique address specification for host $h in link {$src - $dst}"
        }
        set a $addr($h)
    }

    lappend l2 $h
    lappend l2 $a
}
set links $l2
        
# find all reachable nodes starting from the link src -> dst
proc reachable {src dst} {
    set visited $src
    reachable2 visited $dst
    return $visited
}
proc reachable2 {visitedVar host} {
    upvar $visitedVar visited
    global links
    lappend visited $host

    foreach {host1 addr1 host2 addr2} $links {
        if {$host1 == $host && [lsearch $visited $host2] == -1} {
            reachable2 visited $host2
        } elseif {$host2 == $host && [lsearch $visited $host1] == -1} {
            reachable2 visited $host1
        }
    }
}

# proc to return the link options between two hosts
proc link_opts {parent child} {
    return ""
}

# generate the config
puts "#\n# DTN Configuration generated [clock format [clock seconds]]\n#"
puts $COMMON_CONFIG

puts "switch -exact \$localhost \{"
foreach host [lsort $allhosts] {
    puts "  $host \{"
    puts "    route local_eid dtn://$host.dtn"

    foreach {host1 addr1 host2 addr2} $links {
        if {$host == $host1} {
            set dst      $host2
            set dst_addr $addr2
            set type     ALWAYSON
        } elseif {$host == $host2} {
            set dst      $host1
            set dst_addr $addr1
            set type     OPPORTUNISTIC
        } else {
            continue
        }
        
        puts "    link add l-$dst $dst_addr:4556 $type tcp [link_opts $host $dst]"
        foreach n [reachable $host $dst] {
            if {$host != $n} {
                puts "    route add dtn://$n.dtn/* l-$dst"
            }
        }
    }
    puts "  \}"
}
puts "\}"
    
