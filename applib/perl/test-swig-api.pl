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
# SWIG exported dtn api example in perl
#

use dtnapi;
use strict;

my $h = dtnapi::dtn_open();
if ($h == -1) {
    print "error in dtn_open\n";
}
print "handle is $h\n";

my $src = dtnapi::dtn_build_local_eid($h, "src");
my $dst = dtnapi::dtn_build_local_eid($h, "dst");

print "src is $src, dst is $dst\n";

my $regid = dtnapi::dtn_find_registration($h, $dst);
if ($regid != -1) {
    print "found existing registration -- id $regid, calling bind...\n";
    dtnapi::dtn_bind($h, $regid);
} else {
    $regid = dtnapi::dtn_register($h, $dst, $dtnapi::DTN_REG_DROP, 10, 0, "");
    print("created new registration -- id $regid\n");
}

print "sending a bundle in memory...\n";
my $id = dtnapi::dtn_send($h, $regid, $src,$dst, "dtn:none", $dtnapi::COS_NORMAL,
		       0, 30, $dtnapi::DTN_PAYLOAD_MEM, "test payload");

print "bundle id: $id\n";

print "  source: "          . $id->{source}         . "\n";
print "  creation_secs: "   . $id->{creation_secs}  . "\n";
print "  creation_seqno: " . $id->{creation_seqno} . "\n";
$id->DESTROY();

print "receiving a bundle in memory...\n";
my $bundle = dtnapi::dtn_recv($h, $dtnapi::DTN_PAYLOAD_MEM, 10000);

print "bundle:\n";
print "  source: " . $bundle->{source} . "\n";
print "  dest: " . $bundle->{dest} . "\n";
print "  replyto: " . $bundle->{replyto} . "\n";
print "  priority: " . $bundle->{priority} . "\n";
print "  dopts: " . $bundle->{dopts} . "\n";
print "  expiration: " . $bundle->{expiration} . "\n";
print "  creation_secs: " . $bundle->{creation_secs} . "\n";
print "  creation_seqno: " . $bundle->{creation_seqno} . "\n";
print "  payload: " . $bundle->{payload} . "\n";

$bundle->DESTROY();

print "dtn_recv timeout:\n";
$bundle = dtnapi::dtn_recv($h, $dtnapi::DTN_PAYLOAD_MEM, 0);
if ($bundle || (dtnapi::dtn_errno($h) != $dtnapi::DTN_ETIMEOUT)) {
    print "  bundle is $bundle, errno is ". dtnapi::dtn_errno($h) . "\n";
} else {
    print "  " . dtnapi::dtn_strerror(dtnapi::dtn_errno($h)) . "\n";
}

dtnapi::dtn_close($h);


