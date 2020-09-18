#!/bin/bash

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



usage="$0 <maxnodes> <expname>"
example="$0 3 tryfoo/ [basepath]"


if [ "$#" = 2 ]; then
    basepath=/proj/DTN/nsdi/logs
elif [ "$#" = 3 ]; then
    basepath=$3
else
    echo "Usage: $usage"
	echo "Example: $example"
    exit
fi

exp=$2
maxnodes=$1
   

if [ ! -d $basepath/$exp ]; then
    echo "$basepath/$exp: does not exit"
    exit 5
fi

#echo "Make table: exp: $basepath/$exp  nodes:$maxnodes base=$basepath"

for proto in tcp0 tcp1 dtn0 dtn1 mail0 mail1 ; 
do
    exppath=$basepath/$exp/$proto
    
    if [ ! -d $exppath ]; then
        continue
    fi

    ftplog=$exppath/ftplog
    n=$maxnodes
    
    restmpfile=/tmp/$exp.$proto.res
    resfile=$exppath/times.txt
    rm -f $resfile
    rm -f $restmpfile
    
    start=0
    end=-1
    
    txlen=0
    if [ -e $ftplog.$n ]; then 
        txlen=`cat $ftplog.1 | wc -l`
    fi
    sentlen=$txlen 
    
    rcvlen=0
    if [ -e $ftplog.1 ]; then 
        rcvlen=`cat $ftplog.$maxnodes | wc -l`
    fi

    if [ "$rcvlen" -gt "$txlen" ]; then
        rcvlen=$txlen
    fi

    if [ $rcvlen -eq 0 ]; then
        echo "Nothing to paste for $exp/$proto ftplog:$ftplog"
        continue
    fi
    echo -n "$exppath : txlen:$txlen rcvlen:$rcvlen ::"
    
    
    rm -f /tmp/$exp.$proto.res
    rm -f /tmp/$exp.$proto.index
    rm -f /tmp/$exp.$proto.$n
    rm -f /tmp/$exp.$proto.1
        
    lineno=1
    while [ "$lineno" -le "$rcvlen" ] 
    do
        echo "$lineno " >> /tmp/$exp.$proto.index
        let lineno=$lineno+1
    done
    head -n $rcvlen $ftplog.1 | awk '{print $1}'  > /tmp/$exp.$proto.1
    head -n $rcvlen $ftplog.$n | awk '{print $1}'  > /tmp/$exp.$proto.$n
    
    paste /tmp/$exp.$proto.index /tmp/$exp.$proto.1 /tmp/$exp.$proto.$n > $restmpfile
    
    


    #echo "ftp duration is: start of sending first file to receiving last file"
    #echo
    
    
    send_start=`cat $restmpfile | head -n 1| awk '{print $2}'`
    send_end=`cat $restmpfile | tail -n 1| awk '{print $2}'`
    rcv_start=`cat $restmpfile | head -n 1| awk '{print $3}'`
    rcv_end=`cat $restmpfile | tail -n 1| awk '{print $3}'`
    
    let diff=$rcv_end-$send_start
    if [ "$rcvlen" -le "0" ]; then
	    diff=-1  
    fi
    echo  "Time:$diff "
    
    
    echo "# $send_start $send_end $rcv_start $rcv_end  $rcvlen $sentlen" > $resfile
    cat $restmpfile >> $resfile
done
