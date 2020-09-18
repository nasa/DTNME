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

# Start of Base Script to setup Emulab DTNME experiments

# Use it to generate dummy link schedules 

# tclsh base-emu-dumb.tcl | sort -n -k 4.4

#set exp sched4
set maxnodes 5
set nfiles 100
set size 1600
set loss 0
set bw 100kb
set perhops "ph"
set protos "dtn"
set finish 1800
# Start of Base Script to setup Emulab DTNME experiments



set delay 5ms
set queue DropTail
set protocol Static


set WARMUPTIME 10 
#Time between ending of one protocol and starting of the next
set DELAY_FOR_SOURCE_NODE 2


## Adjust the following if links are dynamic 

set linkdynamics 1
set up 60
## Length of uptime
set down 180   
## Length of downtime

set OFFSET_VAL  10

proc max {a b } {

    if {$a > $b}  { return $a } 
    return $b

}

## generates output in two lists up and down 
proc sched {offset} {
global MAX_SIM_TIME
global ONE_CYCLE_LENGTH
global up
global down
global uplist
global downlist
global WARMUPTIME
## locals are only start, current, 
set uplist {}
set downlist {}


## Assumes that the link is up. (rather just up)
## The first event schedules is a down link event
set start $WARMUPTIME



while {$start < $MAX_SIM_TIME} {
    

    set thisloopuplist ""
    set thisloopdownlist ""

#    set count 0 
    ## Start up state (link is down before offset)
    if {$offset > 0} {
	
	lappend thisloopdownlist [expr $start + 1]
	lappend thisloopuplist [expr $start + $offset   ]
    } else {
	lappend thisloopuplist [expr $start + 1]
    }
    
    
    

   # puts "Outer loop $start"
    set current [expr $start + $offset]
    set limit [expr $start + $ONE_CYCLE_LENGTH ]
    while {$current  < $limit} {

#	=====
	set current [expr $current + $up]
#	set current [expr $current + $offset_up($count)]

	if {$current > $limit} break ; 
	lappend thisloopdownlist $current

	set current [expr $current + $down]
	if {$current > $limit} break ; 

	lappend thisloopuplist $current
#	=====

#	set current exp$current + $offset_up(i)
	
#	lappend thisloopuplist 
#	lappend thisloopdownlist $current + $offset_up(i) + $up
#	incr count

    }

#    puts "START  is $start  offset is $offset"
#    puts "DOWN LIST  $thisloopdownlist "
#    puts "UP  LIST  $thisloopuplist "

#    puts ""
#    puts ""

    lappend uplist $thisloopuplist
    lappend downlist $thisloopdownlist

    set start [expr $start + $ONE_CYCLE_LENGTH ]
}

## Finally make the link up (to allow debugging)
lappend uplist [expr $WARMUPTIME + $current]

}


set runs [expr [llength $protos]*[llength $perhops] ]
set MAX_SIM_TIME [expr $runs*$finish + $runs*$WARMUPTIME + $WARMUPTIME]

set ONE_CYCLE_LENGTH [expr $finish + $WARMUPTIME]
set uplist {}
set downlist {}




set emustr "tevexc -e DTN/exp "
set uptime_str  ""
set downtime_str ""



if {$linkdynamics == 1} {
    for {set i 1} {$i <  $maxnodes} {incr i} {
    #Do stuff for the ith link
#	set offset [expr $OFFSET_VAL*[expr $i - 1]]
	set offset [expr ($OFFSET_VAL*[expr $i - 1]) %  ($up + $down)  ]
	sched $offset
	## Now you have uplist and downlist and use them to schedule your events
	
	set linkname "link-$i"
	foreach uptime $uplist {
	#    $ns at $uptime "$linkname up"
	    append uptime_str "$emustr +$uptime $linkname UP \n"
	}
	foreach downtime $downlist {
	 #   $ns at $downtime "$linkname down"
	    append downtime_str "$emustr + $downtime $linkname DOWN \n"
	}
    }
}

#puts $uptime_str
#puts $downtime_str

## Code to generate the pattern theoritical max thruput of this 
## topology

proc min {a b}  {
    if {$a < $b }  { return  $a
    } else { return $b ; } 
}

set MAX_SIM_TIME  1400
#set OFFSET_VAL  0
#set maxnodes 4

## link(j,i) is link j up at time i
for {set j 1} {$j <  $maxnodes} {incr j} {
     
    if {$OFFSET_VAL == -1} {
	set  starttime [expr int(rand()*($down))]	
	
    } else {
	
	set  starttime [expr ($OFFSET_VAL*[expr $j - 1]) %  ($up + $down)  ]
    }
    
    for {set jj 1} {$jj <=  $starttime} {incr jj} {
	set linkup($j,$jj) 0
    }
    while {$starttime <= $MAX_SIM_TIME} {
	
	for {set k 1} {$k <=  $up} {incr k} { 
	    set linkup($j,[expr $k + $starttime]) 1
	}
	
	## change to support random schedule
	if {$OFFSET_VAL == -1} {
	    set  downhere [expr int(rand()*($down))]
	} else {
	    set downhere $down
	}
	
	for {set k 1} {$k <=  $downhere} {incr k} { 
	    set linkup($j,[expr $k + $up +  $starttime]) 0
	}
	set starttime [expr $starttime + $up + $downhere ]
    } 
    ## while loop
}
##  for all nodes


set firstnode 1
set bw__ 100


set i 1
for {set i 1} {$i <=  $MAX_SIM_TIME} {incr i} {
    for {set j 1} {$j <=  $maxnodes} {incr j} {
	set dataATnode($i,$j) 0
    }
}

for {set i 1} {$i <=  $MAX_SIM_TIME} {incr i} {
    set linkup($maxnodes,$i) 0
}

set dataATnode(1,$firstnode) 1000000


for {set i 1} {$i <  $MAX_SIM_TIME} {incr i} {

    if { $dataATnode($i,1) <= 0 } {
	puts "not enuf data "
	exit
    }


    for {set j 1} {$j <=  $maxnodes} {incr j} {
	puts -nonewline " :: $dataATnode($i,$j)"
    }
    puts ""
    flush stdout

    for {set j 1} {$j <=  $maxnodes} {incr j} {
	## link(j,i) is link j up at time t
	set islinkup $linkup($j,$i)
	set capacity 0
	if {$islinkup == 1} {

	    if {$j == $maxnodes} {
		puts "ERRORR  $j "
	    }
#	    puts "link $j is up at time $i "

	    set capacity [expr $bw__ *  1]
	    set tmp [min $dataATnode($i,$j) $capacity]
	    set nextnode [expr $j + 1]
	    set nexttime [expr $i + 1]

	    puts " moving data from $j at time $i -- $tmp "

	    ## dataATnode (time, nodeid) j is node, i is time
	    set dataATnode($i,$nextnode) [expr  $dataATnode($i,$nextnode) + $tmp]

	    set dataATnode($nexttime,$j) [expr  $dataATnode($i,$j) - $tmp]


	    

	} else {
	    set nexttime [expr $i + 1]
	    set dataATnode($nexttime,$j) [expr  $dataATnode($i,$j)]
	}
    }
}




for {set i 1} {$i <  $MAX_SIM_TIME} {incr i} {
    puts   -nonewline "$i" 
    for {set j 1} {$j <=  $maxnodes} {incr j} {
        puts -nonewline " $dataATnode($i,$j) "
    }
    puts " end"
}
    
