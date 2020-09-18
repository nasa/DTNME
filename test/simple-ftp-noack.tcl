#
#    Copyright 2004-2006 Intel Corporation
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
# Simple client/server code that scans a directory for new data and sends
# that data to a remote host.
#
# usage: simple_ftp server <dir> <logfile>
# usage: simple_ftp client <dir> <logfile> <host>
#
#

set port 17600
set period 1000

set blocksz 8192



proc time {} {
    global starttime
    return " [clock seconds] ::  [expr [clock clicks -milliseconds] - $starttime] :: "
}

#proc time {} {
#    return [clock seconds]
#}

proc timef {} {
    return [clock format [clock seconds]]
}

proc scan_dir {host dir} {
    global period

    puts "scanning dir $dir..."
    set files [glob -nocomplain -- "$dir/*"]
    
    foreach file $files {
	file stat $file file_stat
	
	if {$file_stat(type) != "file"} {
	    continue
	}

	set len $file_stat(size)
	if {$len == 0} {
	    continue
	}

	set sent_file 0
	while {$sent_file != 1} {
	    set sent_file [send_file $host $file]
	   # after 2000
	}

	file delete -force $file
    }

    after $period scan_dir $host $dir
}

proc ack_arrived {sock} {
    global got_ack ack_timer
    after cancel $ack_timer
    set got_ack 1

    set ack [gets $sock]
    if [eof $sock] {
	puts " [time] eof waiting for ack!"
	set got_ack 0
	fileevent $sock readable
	return
    }
    if {$ack != "ACK"} {
	puts "[time] ERROR in ack_arrived: got '$ack', expected ACK"
	set got_ack 0
	fileevent $sock readable 
	return
    }
}

proc ack_timeout {} {
    global got_ack
    set got_ack 0
    puts " [time] ack_timeout"
}

proc send_file {host file} {
    global port
    global logfd
    global blocksz
    global got_ack
    global ack_timer
    global sock
    set fd [open $file]

    puts "[time] trying to send  file $file size [file size $file]"

    if {! [info exists sock]} {
	while  { [  catch {socket $host $port } sock] } {
	    puts "[time] Trying to connect, will try again after  2 seconds "
	    after 2000
	}
    
	fconfigure $sock -translation binary
	fconfigure $sock -encoding binary
    }
	
    if [catch {
	puts -nonewline $sock "[file tail $file] "
	flush $sock
#	after 500
	puts $sock "[file size $file]"
	flush $sock
    }] {
	puts $sock "[time] failure in sending header "
	close $sock
	unset sock
	close $fd
	return -1
    }

    # read the handshake ack
    # set got_ack -1
    # fileevent $sock readable "ack_arrived $sock"
    # set ack_timer [after 10000 ack_timeout]
    # vwait got_ack
    
    # if {!$got_ack} {
    # 	puts "[time] timeout waiting for handshake ack"
    #	close $fd
    #	close $sock
    #	unset sock
    #	return -1
    #    }
    # puts "[time] got handshake ack -- sending file"


    while {![eof $fd]} {
	if {[catch {
	    set payload [read $fd $blocksz]
	    puts "[time] sending [string length $payload] byte chunk"
	    puts -nonewline $sock $payload
	    flush $sock
	} ]} {
	    
	    puts "[time] failure at sender "
	    close $sock
	    unset sock
	    close $fd
	    return -1
	}
    }	

    close $fd
 
    	puts "[time] :: file  actually sent $file"
    	puts $logfd "[time] :: file actually sent [file tail $file]   " 
    	flush $logfd

    return 1
    # wait for an ack or timeout
    # puts "[time] sent whole file, waiting for ack"
    # fconfigure $sock -blocking 0
    # set got_ack -1
    # fileevent $sock readable "ack_arrived $sock"
    # set ack_timer [after 5000 ack_timeout]
    # vwait got_ack

    # if {$got_ack} {
    #	puts "[time] :: file  actually sent $file"
    #	puts $logfd "[time] :: file actually sent [file tail $file]   " 
    #	flush $logfd
    #
    #   return 1
#    } else {
#	puts "[time] :: file sent but not acked"
#	close $sock
#	unset sock
#	return -1

    }
}

proc recv_files {dest_dir} {
    global port
    global conns
    puts "[time] waiting for files:  $dest_dir"
    set conns(main) [socket -server "new_client $dest_dir" $port]
}


proc new_client {dest_dir sock addr port} {
    global conns

    puts "[time] Accept $sock from $addr port $port"
    
    # Used for debugging
    set conns(addr,$sock) [list $addr $port]

    fconfigure $sock -translation binary
    fconfigure $sock -encoding binary
    fconfigure $sock -blocking 0 
    
    fileevent $sock readable "header_arrived $dest_dir $sock"
}

proc header_arrived {dest_dir sock} {
    global length_remaining
    global conns
    global partial

    puts "[time] header arrived"

    if {[eof $sock]} {
	puts "[time] Close $conns(addr,$sock) EOF received header.. "
	close $sock	
	unset conns(addr,$sock)
	return 
    }

    # try to read a line
    set L [gets $sock]
    while {$L == ""} {
	puts "[time] partial header line received... waiting for more"
	after 20
	set L [gets $sock]
    }
    
    if {[info exists partial($sock)]} {
	puts "[time] merging partial line '$partial($sock)' with '$L'"
	set L "$partial($sock)$L"
	unset partial($sock)
    }
    
    foreach {filename length} $L {}

    puts "[time] getting file $filename length $length"
    
    # puts $sock "ACK"
    # flush $sock
    
    set to_fd [open "$dest_dir/$filename" w]
    set length_remaining($sock) $length
    
    fileevent $sock readable "file_arrived $filename $to_fd $dest_dir $sock"
}


proc file_arrived {file to_fd dest_dir sock} {
    
    global logfd
    global conns
    global length_remaining
    
    if {[eof $sock]} {
	puts "[time] Close $conns(addr,$sock) EOF received file"
	close $sock	
	unset conns(addr,$sock)
	return
    } 


    set payload [read $sock $length_remaining($sock)] 
    puts -nonewline $to_fd $payload
    set got [string length $payload]
    set todo [expr $length_remaining($sock) - $got]
    puts "got $got byte chunk ($todo to go)"
    
    if {$todo < 0} {
	error "negative todo"
    }
    
    if {$todo == 0} {
	# puts $sock "ACK"
	# flush $sock
	# puts "[time] sending ack for file $file"
	
	fileevent $sock readable "header_arrived $dest_dir $sock"
	puts $logfd "[time] :: got file [file tail $file]  at [timef]" 
	close $to_fd
	flush $logfd
	
    }
    
    set length_remaining($sock) $todo
}


# Define a bgerror proc to print the error stack when errors occur in
# event handlers
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

proc usage {} {
    puts "usage: simple_ftp server <dir> <logfile>"
    puts "usage: simple_ftp client <dir> <logfile> <host>"
    exit -1
}

set argc [llength $argv]
if {$argc < 3} {
    usage
}


set mode [lindex $argv 0]
set dir  [lindex $argv 1]
set logfile  [lindex $argv 2]
set logfd [open $logfile w]

set starttime [clock clicks -milliseconds]
puts "Starting in $mode, dir is $dir at [timef]" 


if {$mode == "server"} {
    recv_files $dir
} elseif {$mode == "client"} {
    set host [lindex $argv 3]
    scan_dir $host $dir
} else {
    error "unknown mode $mode"
}

vwait forever
