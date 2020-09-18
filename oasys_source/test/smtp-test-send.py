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

import smtplib

def prompt(prompt):
    return raw_input(prompt).strip()

fromaddr = ['foo1@foo1.com', 'foo2@foo2.org', 'foo3@foo3.net']
toaddrs  = [ ["bar1@bar.com", "bar2@bar.com" ],
             ["bar3@bar.com", "bar4@bar.com" ],
             ["bar5@bar.com", "bar6@bar.com" ] ]

# Add the From: and To: headers at the start!
msg = ["line 01: This is the body of the message\r\n" + \
       "line 02: This is the body of the message\r\n" + \
       "line 03: This is the body of the message\r\n" + \
       "line 04: This is the body of the message\r\n" + \
       "line 05: This is the body of the message\r\n" + \
       "line 06: This is the last line\r\n", \
       \
       "\r\n" + \
       "line 01: This is a message with a leading newline\r\n" + \
       "\r\n" + \
       ".\r\n" + \
       "\r\n" + \
       ".line 02: This is a line with a leading dot\r\n", \
       \
       ".\r\n"]

server = smtplib.SMTP('localhost', 17760)
server.set_debuglevel(1)

for i in range(3):
    print "Message length is " + repr(len(msg[i]))
    server.sendmail(fromaddr[i], toaddrs[i], msg[i])
    
server.quit()
