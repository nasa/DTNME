#
#    Copyright 2006 Intel Corporation
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
# Tcl based registration that forwards bundles via the mote serial
# source protocol to incoming network connections.
#

set mote_proxy_tinyos_version 1

#
# Creates a DTN registration on endpoint EID and runs the
# mote serial forwarder protocol listening on the given port
#
proc mote_proxy_registration {eid server_port} {
    # create the registration
    set regid [tcl_registration $eid mote_proxy_bundle_arrived]
    
    # start the listener socket
    socket -server "mote_proxy_conn_arrived $regid" $server_port
}

#
# Connects to the given address and port that's running the serial
# forwarder protocol and injects packets as bundles.
#
proc mote_proxy_injector {addr port source_eid dest_eid {bundle_opts ""}} {

    set chan [socket $addr $port]
    mote_proxy_handshake $chan

    global mote_proxy_inject_state
    set mote_proxy_inject_state($chan) [list $source_eid $dest_eid $bundle_opts]

    fileevent $chan readable "mote_proxy_chan_readable $chan"
}

######################################################################

proc mote_proxy_handshake {chan} {
    global mote_proxy_tinyos_version

    if {$mote_proxy_tinyos_version == 1} {
	set handshake "T "
    } elseif {$mote_proxy_tinyos_version == 2} {
	set handshake "U "
    } else {
	error "unknown mote proxy tinyos version $mote_proxy_tinyos_version"
    }
    
    fconfigure $chan -buffering none
    fconfigure $chan -translation binary
    puts -nonewline $chan $handshake
    flush $chan

    set check [read $chan 2]

    if {$check != $handshake} {
	error "unexpected value for handshake: '$check'"
    }
    
    log /mote-proxy debug "handshake complete"
}

proc mote_proxy_bundle_arrived {regid bundle_data} {
    array set b $bundle_data
    
    if ($b(is_admin)) {
	error "Unexpected admin bundle arrival $b(source) -> $b(dest)"
    }
    
    log /mote-proxy debug "got bundle of length $b(length) for registration $regid"

    global mote_proxy_chan
    if {![info exists mote_proxy_chan($regid)]} {
	log /mote-proxy warn "bundle arrived with no channel"
	return
    }
    
    set chan $mote_proxy_chan($regid) 

    if [catch {
	puts -nonewline $chan [binary format ca* $b(length) $b(payload)]
	flush $chan
    } err] {
	log /mote-proxy warn "channel $chan closed"
	close $chan
	unset mote_proxy_chan($regid)
    }
}

proc mote_proxy_conn_arrived {regid chan addr port} {
    global mote_proxy_chan

    log /mote-proxy debug "got new mote proxy connection for registration $regid"

    if [catch {
	mote_proxy_handshake $chan
    } err] {
	log /mote-proxy warn "error in handshake from $addr:$port"
	close $chan
	return
    }
    
    set mote_proxy_chan($regid) $chan
}

proc mote_proxy_chan_readable {chan} {
    global mote_proxy_inject_state

    foreach {source_eid dest_eid bundle_opts} $mote_proxy_inject_state($chan) {}

    log /mote-proxy debug "mote proxy channel readable"

    set byte [read $chan 1]
    binary scan $byte c length

    if [eof $chan] {
	log /mote-proxy warn "eof on mote proxy channel"
	close $chan
	return
    }

    log /mote-proxy debug "mote proxy channel read length byte $length"

    set data [read $chan $length]

    log /mote-proxy debug "injecting $length byte bundle"

    bundle inject $source_eid $dest_eid $data $bundle_opts
}
