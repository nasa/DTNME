#!/usr/bin/tclsh

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


if [catch {
    package require Tcl
    package require mime
    package require smtp
} err] {
    exit 99
}

set msg0 "line 01: This is the body of the message
line 02: This is the body of the message
line 03: This is the body of the message
line 04: This is the body of the message
line 05: This is the body of the message
line 06: This is the last line"

set msg1 "
line 01: This is a message with a leading newline

.

.line 02: This is a line with a leading dot"

set msg2 ".\n"

set fromtomsg [list \
	"foo1@foo1.com" "bar1@bar.com,bar2@bar.com" $msg0 \
	"foo2@foo2.org" "bar3@bar.com,bar4@bar.com" $msg1 \
 	"foo3@foo3.net" "bar5@bar.com,bar6@bar.com" $msg2
	]
	

foreach {from to msg} $fromtomsg {
    set token [::mime::initialize -canonical text/plain -string $msg]

    ::smtp::sendmessage $token -servers localhost -ports 17760 \
	    -originator $from  -recipients $to \
	    -header [list From $from] -header [list To $to] \
	    -header [list Subject "test subject"]
    
    ::mime::finalize $token
}
    
