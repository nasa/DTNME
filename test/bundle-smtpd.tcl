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
# test script that runs an smtp daemon within the bundle daemon
#

proc smtpd_deliver_mime {token} {
    set sender [lindex [mime::getheader $token From] 0]
    set recipients [lindex [mime::getheader $token To] 0]
    set mail "From $sender [clock format [clock seconds]]"
    append mail "\n" [mime::buildmessage $token]
    puts $mail
    
    # mail inject $to $body
}

proc smtpd_start {port {deliver_mime smtpd_deliver_mime}} {
    package require smtp
    package require smtpd
    package require mime
    
    smtpd::configure -deliverMIME $deliver_mime
    smtpd::start 0 $port
}
    
proc send_test_mail {} {
    global smtp_port
    
    set token [mime::initialize -canonical text/plain \
	    -string "this is a test"]
    mime::setheader $token To "test@test.com"
    mime::setheader $token Subject "test subject"
    smtp::sendmessage $token \
	    -recipients test@test.com -servers localhost -ports $smtp_port
    mime::finalize $token
}
