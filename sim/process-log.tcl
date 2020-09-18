#!/usr/bin/tclsh
#
# process-log.tcl
#
# Parse through the dtnsim_log.txt output and generate an aggregated
# summary of the various statistics.

set summary 0
set ignore  ""

for {set i 0} {$i < [llength $argv]} {incr i} {
    set arg [lindex $argv $i]
    switch -- $arg {
        -summary -
        --summary 	{ set summary 1 }
        
        -ignore -
        --ignore 	{ lappend ignore [lindex $argv [incr i]] }
        
        default {puts "unknown argument $arg"; exit 1}
    }
}

array set counts {}
array set delays {}

while {![eof stdin]} {
    set L [gets stdin]

    foreach {time node what source dest creation_ts length delay} \
            [split $L \t] {

        # hack to consolidate routing messages destined for
        regsub {\?(.*)seqno=[\d]+} $dest {?\1seqno=*} dest
        
        if {![info exists counts($node,$source,$dest,$what)]} {
            set counts($node,$source,$dest,$what) 1
        } else {
            incr counts($node,$source,$dest,$what)
        }

        if {$what == "ARR"} {
            if {![info exists delays($node,$source,$dest)]} {
                set delays($node,$source,$dest) $delay
            } else {
                incr delays($node,$source,$dest) $delay
            }
        }
    }
}

set types {RECV XMIT DUP GEN ARR EXP INQ delay}

foreach what $types {
    set total($what) 0
}

set last_node   ""
set last_source ""
set last_dest   ""

foreach key [lsort [array names counts]] {
    foreach {node source dest what} [split $key , ]  {}

    set ign 0
    foreach pat $ignore {
        if {[string match $pat $dest] || [string match $dest $pat]} {
            set ign 1
            break
        }
    }
    if {$ign} { continue }

    if {! $summary} {
        if {$last_node != $node || $last_source != $source || $last_dest != $dest} {
            set last_node $node
            set last_source $source
            set last_dest $dest
            puts "$node: $source -> $dest:"
        }

        puts "\t$what: $counts($key)"
    }
    incr total($what) $counts($key)

    if [info exists delays($node,$source,$dest)] {
        incr total(delay) $delays($node,$source,$dest)
    }
}

puts "------"
foreach what $types {
    puts "$what total: $total($what)"
}
