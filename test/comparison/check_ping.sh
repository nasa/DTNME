#!/bin/sh

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



OTHERHOST=$1

    
COUNT=4
PACKET_SIZE=50;
INTERVAL=0.2
TIMEOUT=3
RETRY=5
rm -f /tmp/ping.$OTHERHOST


starttime=`date +%s`
ps -eo cmd > /tmp/$starttime
#CHeck if already running, if yes then exitcat
num=`grep check_ping /tmp/$starttime | wc -l` 
echo $num
if [ $num -gt 1 ]; then
    exit 0
fi

while [ 1 ]
do
    timenow=`date +%s`
    let incr=$timenow-$starttime
    ping -q -w $TIMEOUT -i $INTERVAL -c $COUNT -s $PACKET_SIZE $OTHERHOST 2>/dev/null >/dev/null 
    ret=$?
    if [ "$ret" == "0" ]; then
        echo $incr UP " At:$timenow :Retry after $RETRY seconds to $OTHERHOST"  
    else
        echo $incr DOWN " At:$timenow :Retry after $RETRY seconds to $OTHERHOST"
    fi
    sleep $RETRY 
done
        
