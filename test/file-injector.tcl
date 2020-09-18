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
# Simple test code that scans a directory for new data and injects
# whatever it finds as a bundle.
#

#
# Starts up a file injector to scan the given directory and create
# new bundles from any files that are found.
#
# usage: file_injector <dir> <source> <dest> <-period ms>
#
proc file_injector_start {dir source dest args} {
    global file_injector_handles
    
    set period 1000
    
    if {$args != {}} {
	error "XXX/demmer implement optional args"
    }

    set after_handle [after 0 \
	    [list file_injector_scan $dir $source $dest $period]]
    set file_injector_handles($dir) $after_handle

    return $dir
}

#
# Stops a file injector on the given directory
#
proc file_injector_stop {dir} {
    global file_injector_handles

    if [info exists file_injector_handles($dir)] {
	after cancel $file_injector_handles($dir)
    } else {
	error "no injector running on dir $dir"
    }
}

#
# Internal proc that actually does the scanning
#
proc file_injector_scan {dir source dest period} {
    global file_injector_handles
    global fd_ftplog
    
    set files [glob -nocomplain -- "$dir/*"]

#    log /file_injector DEBUG "scanning $dir... found [llength $files] files"
    foreach file $files {
	file stat $file file_stat
	
	log /file_injector DEBUG "stat type $file_stat(type)"
	
	if {$file_stat(type) != "file"} {
	    log /file_injector DEBUG \
		    "ignoring file $file type $file_stat(type)"
	    continue
	}

	set len $file_stat(size)
	if {$len == 0} {
	    log /file_injector DEBUG "ignoring empty file $file"
	    continue
	}

	log /file_injector DEBUG "reading payload from bundle file $file";
	set fd [open $file]
	fconfigure $fd -translation binary
	set payload [read $fd]
	close $fd

	log /file_injector DEBUG "gotnow $len byte payload"
	
	log /file_injector INFO " sending bundle [file tail $file] $len byte payload"

	if {[info exists fd_ftplog]} {
	    puts $fd_ftplog "[time] :: sending bundle [file tail $file]  $len byte  "
	    flush $fd_ftplog
	
	}
	bundle inject $source "$dest/[file tail $file]" $payload option [list length $len]

	file delete $file
    }

    # re-schedule the scan
    set after_handle [after $period \
	    [list file_injector_scan $dir $source $dest $period]]
    set file_injector_handles($dir) $after_handle
    return ""
}
