#!/bin/bash
#
#dtntunnel parameters:
#    -b & -w =  bump up socket buff sizes (could be limited by system parameters)
#    -m = monitor mode so that it displays bundles and Megabits per second
#    -q = process a 10 character sequence counter at the end of each payload
#    -u = UDP tunnel instead of TCP
#    -T = 33442 (port to read incoming packets) 127.0.0.1:33441 (IP Address & Port to transmit to on other end of tunnel)
#    dtn://node3.dtn/dtntunnel = destination EID for the bundles
#           <change the destination EID to match your configuration>
#
dtntunnel -b 20480000 -w 20480000 -e 864000 -m -q -u -T 33442:127.0.0.1:33441 dtn://node3.dtn/dtntunnel
