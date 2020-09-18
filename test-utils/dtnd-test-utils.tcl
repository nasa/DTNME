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

############################################################
#
# dtnd-test-utils.tcl
#
# Test utility functions that are sourced by dtnd's config
# file when run through the testing infrastructure.
#
############################################################

#
# Callback function that is triggered whenever a bundle is put on the
# registration's delivery list. Loops until there are no more bundles
# on the list, calling the delivery callback on each one
#
proc tcl_registration_bundle_ready {regid endpoint callback callback_data} {
    while {1} {
	set bundle_data [registration tcl $regid get_bundle_data]
	if {$bundle_data == {}} {
	    return
	}

	if {$callback_data != ""} {
	    $callback $regid $bundle_data $callback_data
	} else {
	    $callback $regid $bundle_data
	}
    }
}

#
# Set up a new tcl callback registration
#
proc tcl_registration {endpoint {callback default_bundle_arrived} {callback_data ""}} {
    set regid [registration add tcl $endpoint]
    set chan [registration tcl $regid get_list_channel]
    fileevent $chan readable [list tcl_registration_bundle_ready \
	    $regid $endpoint $callback $callback_data]
    return $regid
}

proc default_bundle_arrived {regid bundle_data} {
    array set b $bundle_data
    global bundle_payloads
    global bundle_info
    global bundle_sr_info

    # loop through the key/value pairs in the bundle structure,
    # printing them out and also dumping them into the bundle_info
    # array indexed by guid. at the same time, status report bundles
    # are put into a separate array indexed via a GUID that can be
    # determined without knowing the SR bundle's creation timestamp
    # (which we can't easily get because SRs are automatically
    # generated)
    log /test notice "bundle arrival"
    set guid "$b(source),$b(creation_ts)"
    if { $b(is_admin) && [string match "Status Report" $b(admin_type)] } {
	set sr_guid "$b(orig_source),$b(orig_creation_ts),$b(source)"
    }
    
    foreach {key val} [array get b] {
	if {($key == "payload" || $key == "payload_data")} {
	    continue
	}

        if {$key == "recv_blocks"} {
            # A SerializableVector is serialized as:
            #   size <size> element <elt1> element <elt2>...
            set nblocks [lindex $val 1]
            lappend bundle_info($guid) $key $nblocks
            log /test notice "recv_blocks: ($nblocks)\n"

            set isprimary 1
            foreach {xxx block} [lrange $val 2 end] {
                array set block_info $block
                set type [format "0x%x" $block_info(owner_type)]
                
                if {$isprimary} {
                    set type2 primary
                    binary scan $block_info(contents) cc version flags
                    set flags [format "0x%x" $flags]
                    set isprimary 0
                } else {
                    binary scan $block_info(contents) cc type2 flags
                    set type2 [format "0x%x" $type2]
                    set flags [format "0x%x" $flags]
                }
                log /test notice "\t\
                        type $type ($type2) flags $flags\
                        length $block_info(length)\
                        data_length $block_info(data_length)\
                        data_offset $block_info(data_offset)"

                set block_type "block,$type2,flags"
                lappend bundle_info($guid) "block,$type2,flags" $flags
            }

            continue
        }

	log /test notice "$key:\t $val"
	
	lappend bundle_info($guid) $key $val
	if {[info exists sr_guid]} {
	    lappend bundle_sr_info($sr_guid) $key $val
	}
    }
    
    # record the bundle payload separately
    set bundle_payloads($guid) $b(payload)
}


#
# test proc for sending a bundle
#
proc sendbundle {source_eid dest_eid args} {
    global id

    # assume args consists of a list of valid "bundle inject" options
    set length 5000
    set i [lsearch -glob $args length=*]
    if {$i != -1} {
	set length [lindex $args [expr $i + 1]]
	set length [string map {length= ""} [lindex $args $i]]
    }

    set payload ""
    if {$length != 0} {
        while {$length - [string length $payload] > 32} {
            append payload [format "%4d: 0123456789abcdef\n" \
    		[string length $payload]]
        }
        while {$length > [string length $payload]} {
	    append payload "."
        }
    }

    return [eval [concat {bundle inject $source_eid \
			      $dest_eid $payload length=$length} $args]]
}

