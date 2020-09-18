#
#    Copyright 2007 Intel Corporation
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
# SWIG exported dtn api example in tcl
#

load libdtntcl[info sharedlibextension] dtnapi

set h [dtn_open]
if {$h == -1} {
    error "error in dtn_open"
}
puts "handle is $h"

set src [dtn_build_local_eid $h "src"]
set dst [dtn_build_local_eid $h "dst"]

puts "src is $src, dst is $dst"

set regid [dtn_find_registration $h $dst]
if {$regid != -1} {
    puts "found existing registration -- id $regid, calling bind..."
    dtn_bind $h $regid
} else {
    set regid [dtn_register $h $dst $DTN_REG_DROP 10 false ""]
    puts "created new registration -- id $regid"
}

puts "sending a bundle in memory..."
set id [dtn_send $h $regid $src $dst dtn:none $COS_NORMAL 0 30 $DTN_PAYLOAD_MEM "test payload"]

puts "bundle id:"
puts "  source: [dtn_bundle_id_source_get $id]"
puts "  creation_secs: [dtn_bundle_id_creation_secs_get $id]"
puts "  creation_seqno: [dtn_bundle_id_creation_seqno_get $id]"

delete_dtn_bundle_id $id

puts "receiving a bundle in memory..."
set bundle [dtn_recv $h $DTN_PAYLOAD_MEM 10000]
puts "bundle:"
puts "  source: [dtn_bundle_source_get $bundle]"
puts "  dest: [dtn_bundle_dest_get $bundle]"
puts "  replyto: [dtn_bundle_replyto_get $bundle]"
puts "  priority: [dtn_bundle_priority_get $bundle]"
puts "  dopts: [dtn_bundle_dopts_get $bundle]"
puts "  expiration: [dtn_bundle_expiration_get $bundle]"
puts "  creation_secs: [dtn_bundle_creation_secs_get $bundle]"
puts "  creation_seqno: [dtn_bundle_creation_seqno_get $bundle]"
puts "  payload: [dtn_bundle_payload_get $bundle]"

delete_dtn_bundle $bundle

puts "dtn_recv timeout:"
set bundle [dtn_recv $h $DTN_PAYLOAD_MEM 0]
if {($bundle != "NULL") || ([dtn_errno $h] != $DTN_ETIMEOUT) } {
    puts "  bundle is $bundle, errno is [dtn_errno $h]"
} else {
    puts "  [dtn_strerror [dtn_errno $h]]"
}

dtn_close $h
