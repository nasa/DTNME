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
# that data to a remote dest_addr.
#
# send-files.tcl <dir> <log> <dest_addr> <expid>
set period 1000


proc time {} {
    return [clock seconds]
}

proc timef {} {
    return [clock format [clock seconds]]
}

proc scan_dir {dest_addr dir} {
    global period

    puts "scanning dir $dir..."
    set files [glob -nocomplain -- "$dir/*"]
    
    set index 1 
    foreach file $files {
    	file stat $file file_stat
	
	    if {$file_stat(type) != "file"} {
	        continue
    	}

	    set len $file_stat(size)
    	if {$len == 0} {
	        continue
    	}

	    send_file $dest_addr $file $index

    	file delete $file
        incr index
    }

    after $period scan_dir $dest_addr $dir
}

proc send_file {dest_addr file index} {
    global logfd
	global expid

    puts "sending file $file for $expid"
    
    set subject "sendmail_$index\_$expid"
    #append subject "sendmail"
    set foo [exec mail -s $subject $dest_addr < $file]
    
    puts $logfd "[time] :: sending subject:$subject file:$file at [timef] " 
    flush $logfd
}

set dir  [lindex $argv 0]
set ftplogfile  [lindex $argv 1]
set logfile [lindex $argv 2]
set dest_addr [lindex $argv 3]
set expid [lindex $argv 4]

set logfd [open $ftplogfile w]

puts "Starting in dir is $dir at [timef] and going to $dest_addr" 

scan_dir $dest_addr $dir

vwait forever

