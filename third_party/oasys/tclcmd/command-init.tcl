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
# This file is converted into a big C string during the build
# process and evaluated in the command interpreter at startup
# time.
#

#
# For the vwait in event_loop to work, we need to make sure there's at
# least one event outstanding at all times, otherwise 'vwait forever'
# doesn't work
#
proc after_forever {} {
    global forever_timer
    set forever_timer [after 1000000 after_forever]
}

#
# Run the event loop and no command line interpreter
#
proc event_loop {} {
    global event_loop_wait
    after_forever

    set event_loop_wait 1
    while {$event_loop_wait} {
	after 1000 { 
	    global event_loop_wait
	    if {$event_loop_wait} { set event_loop_wait 1 }
	}
	vwait event_loop_wait
    }
    command_log notice "exiting event loop"
}

proc do_nothing {} {
}

#
# Kill the event loop
#
proc exit_event_loop {} {
    global forever_timer event_loop_wait stdin
    command_log notice "kicking event loop to exit"
    set event_loop_wait 0
    if [catch {
	::oasys_tclreadline::readline eof
    } err] {
    }
    after 0 do_nothing
}

#
# Global var containing the command log path
#
set command_logpath "/command"

#
# Wrapper proc to handle the fact that we may or may not have a log
# procedure defined
#
proc command_log {level string} {
    global command_logpath
    if {[info commands log] != ""} {
        log $command_logpath $level $string
    } else {
        # hard-coded level filter
        switch -- $level {
            notice - warn - error - crit {
                puts $string
            }
        }
    }
}

#
# Callback when there's data ready to be processed.
#
proc command_process {input output} {
    global command command_prompt command_info tell_encode event_loop_wait

    # Grab the line, and check for eof
    if {[gets $input line] == -1} {
	if {"$input" == "stdin"} {
	    set event_loop_wait 1
	    return
	} else {
	    command_log debug "closed connection $command_info($input)"
	    fileevent $input readable ""
	    catch {close $input}
	    return
	}
    }

    # handle exit from a socket connection
    if {($input != "stdin") && ($line == "exit")} {
	command_log notice "connection $command_info($input) exiting"
	fileevent $input readable ""
	catch {close $input}
	return
    }
    
    # handle tell_encode / no_tell_encode commands
    if {$line == "tell_encode"} {
	set tell_encode($output) 1
	puts $output "\ntell_encode"
	flush $output
	return
    } elseif {$line == "no_tell_encode"} {
	set tell_encode($output) 0
	puts $output "\nno_tell_encode"
	flush $output
	return
    }

	global display_error_stack
	if {$line == "display_error_stack"} {
		set display_error_stack 1
		puts $output "\ndisplay_error_stack enabled"
    	if {! $tell_encode($output)} {
			puts -nonewline $output $command_prompt
    	}
    	flush $output
		return
	} elseif {$line == "no_display_error_stack"} {
		set display_error_stack 0
		puts $output "\ndisplay_error_stack disabled"
    	if {! $tell_encode($output)} {
			puts -nonewline $output $command_prompt
    	}
		flush $output
		return
    }

    if {$tell_encode($output)} {
	# if we're in tell encoding mode, decode the message

	if {$command($input) != ""} {
	    error "unexpected partial command '$command($input)' in tell mode"
	}
	regsub -all -- {\\n} $line "\n" command($input)
    } else {
	# otherwise, append the line to the batched up command, and
	# check if it's complete
	
	append command($input) $line
	if {![info complete $command($input)]} {
	    return
	}
    }
    
    # trim and evaluate the command
    set command($input) [string trim $command($input)]
    set cmd_error 0

    command_log debug "executing command $command($input)"
    if {[catch {uplevel \#0 $command($input)} result]} {
        command_log debug "error result is $result"
		if {$result == "exit_command"} {
		    if {$input == "stdin"} {
				set event_loop_wait 1
				return
		    } else {
				real_exit
		    }
		}

	    if {$display_error_stack == 1} {
		  	global errorInfo
			set result "error: $result\n\nwhile executing\n$errorInfo\n"
		}
		set cmd_error 1
    }
    command_log debug "result is $result"
    set command($input) ""

    if {$tell_encode($output)} {
	regsub -all -- {\n} $result {\\n} result
	puts $output "$cmd_error $result"
    } else {
	puts $output $result
    }    
    
    if {! $tell_encode($output)} {
	puts -nonewline $output $command_prompt
    }
    flush $output
}

#
# Run the simple (i.e. no tclreadline) command loop
#
proc simple_command_loop {prompt} {
    global command command_prompt forever tell_encode
    global display_error_stack
    set command_prompt "$prompt"
    
    puts -nonewline $command_prompt
    flush stdout

    set command(stdin)      ""
    set tell_encode(stdout) 0
    set event_loop_wait        0
    set display_error_stack 0
    fileevent stdin readable "command_process stdin stdout"

    vwait event_loop_wait

    command_log notice "exiting simple command loop"
}

#
# Run the command loop with the given prompt
#
proc command_loop {prompt} {
    global command_prompt event_loop_wait
    
    set command_prompt "$prompt"
    set event_loop_wait 0

    # Handle the behavior that we want for the 'exit' proc -- when running
    # as the console loop (either tclreadline or not), we just want it to
    # exit the loop so the caller knows to clean up properly. To implement
    # that, we error with the special string "exit_command" which is
    # caught by callers who DTRT with it.
    rename exit real_exit
    proc exit {} {
	error "exit_command"
    }

    if [catch {
	package require oasys_tclreadline
	oasys_tclreadline::readline initialize ""
	oasys_tclreadline::readline eofchar "error exit_command"
	tclreadline_loop
	
    } err] {
	command_log info "can't load oasys_tclreadline: $err"
	command_log info "fall back to simple command loop"
	simple_command_loop $prompt
    }
    puts ""

    # fix up the exit proc
    rename exit ""
    rename real_exit exit
}

#
#
proc tclreadline_completer {text start end line} {
    global event_loop_wait
    if {$event_loop_wait == 1} {
	error "exit_command"
    }
    return ""
}

#
# Custom main loop for tclreadline (allows us to exit on eof)
# Copied from tclreadline's internal Loop method
#
proc tclreadline_loop {} {
    global event_loop_wait
    
    oasys_tclreadline::readline builtincompleter 0
    oasys_tclreadline::readline customcompleter tclreadline_completer
    
    uplevel \#0 {
	while {1} {
	    if {$event_loop_wait == 1} {
		return
	    }
	    
	    if [info exists tcl_prompt2] {
		set prompt2 $tcl_prompt2
	    } else {
		set prompt2 ">"
	    }

	    if {[catch {
		set LINE [::oasys_tclreadline::readline read $command_prompt]
		while {![::oasys_tclreadline::readline complete $LINE]} {
		    append LINE "\n"
		    append LINE [oasys_tclreadline::readline read ${prompt2}]
		}
		
	    } errorMsg]} {
		if {$errorMsg == "exit_command"} {
		    break
		}
		puts stderr "tclreadline_loop error. $errorMsg"
		continue
	    }

	    # Magnus Eriksson <magnus.eriksson@netinsight.se> proposed
	    # to add the line also to tclsh's history.
	    #
	    # I decided to add only lines which are different from
	    # the previous one to the history. This is different
	    # from tcsh's behaviour, but I found it quite convenient
	    # while using mshell on os9.
	    #
	    if {[string length $LINE] && [history event 0] != $LINE} {
		history add $LINE
	    }

	    if [catch {
		
		set result [eval $LINE]
		if {$result != ""} {
		    puts $result
		}
		set result ""
	    } ::oasys_tclreadline::errorMsg] {
		if {$::oasys_tclreadline::errorMsg == "exit_command"} {
		    break
		}
		puts stderr $::oasys_tclreadline::errorMsg
		puts stderr [list while evaluating $LINE]
	    }
	}
    }
}


#
# Proc that's called when a new command connection arrives
#
proc command_connection {chan host port} {
    global command command_info command_prompt tell_encode

    set command_info($chan) "$host:$port"
    set command($chan)      ""
    set tell_encode($chan)  0
    command_log debug "new command connection $chan from $host:$port"
    fileevent $chan readable "command_process $chan $chan"

    puts -nonewline $chan $command_prompt
    flush $chan
}

#
# Run a command server on the given addr:port
#
proc command_server {prompt addr port} {
    global command_prompt
    set command_prompt "$prompt"
    socket -server command_connection -myaddr $addr $port 
}

#
# Define a bgerror proc to print the error stack when errors occur in
# event handlers
#
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

