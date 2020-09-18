#!/bin/csh

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



set usage =  "./base-cmd <nodeid> <exp-name> <maxnodes> <nfiles> <size> <perhop> <proto> <ftpproto>"
set example = "Example: ./base-cmd.sh   1 try   4        10      10   1      tcp simple-ftp"

if ($#argv != 8) then
	echo "Runs a particular protocol at a certain node id "
	echo 
        echo "Usage: $usage"
	echo "Example: $example"
	echo
	
    exit 1
    endif

#echo Script: $0 
set  id =  $1
set  exp     = $2    
set  maxnodes = $3    
set nfiles     = $4
set size      = $5   
set  perhop  = $6    
set  proto_orig  = $7


set  proto_ftp  = $8

set proto  = $proto_orig$perhop




## place to find DTNME. This will link to exe files
set dtnroot = /proj/DTN/nsdi
set dtnmeroot = $dtnroot/DTNME
set dtnmetestroot = $dtnmeroot/test

# Root location to store log files

#set logroot = /proj/DTN/exp/$exp/logs/$proto
set logrootbase = $dtnroot/logs/$exp
set logroot     = $logrootbase/$proto
set rcvdir      = "$logroot/rcvfiles.$id"
set ftplogfile =  $logroot/ftplog.$id

set tcptimerfile = $dtnmetestroot/tcp_retries2.sush

## stuff that u dont have to log in NFS
set localtmpdir       =  /tmp/log-$proto.$id
set txdir       = $localtmpdir/txfiles.$id
set rcvdir      = $localtmpdir/rcvfiles.$id
rm -rf $localtmpdir



#user etc
set username=`whoami`
set userhome=/users/$username

# Make the log directories

if (! -e $localtmpdir) then
    mkdir  $localtmpdir
endif


if (! -e $logrootbase) then
    mkdir  $logrootbase
endif


if (! -e $logrootbase) then
    mkdir  $logrootbase
endif

if (! -e $logroot) then
    mkdir  $logroot
endif

if (! -e $txdir) then
mkdir  $txdir
endif

if (! -e $rcvdir) then
mkdir  $rcvdir
endif

set info = $logroot/log.$id
echo "Logging commands executed by $proto (node id $id)" > $info


# Set TCP timers
#sudo "cp $tcptimerfile  /proc/sys/net/ipv4/tcp_retries2"
sudo sh -c "echo 3 >  /proc/sys/net/ipv4/tcp_retries2"
sudo sh -c "echo 0 >  /proc/sys/net/ipv4/tcp_syn_retries"


# If this is the source node generate workload
if ($id == 1) then  
   echo "Executing workload:  tclsh $dtnmeroot/test/workload.tcl $nfiles $size $txdir  " >> $info
   tclsh $dtnmeroot/test/workload.tcl $nfiles $size $txdir 
endif


set pingfile = $logrootbase/pinglog.$id
echo "#Check ping commands ...." > $pingfile 
set idplus  = `expr $id + 1`
#set linkname = link-$id
set nextnode=node-$idplus 
sh $dtnmetestroot/check_ping.sh $nextnode >> $pingfile &


switch ($proto_orig)
	       case tcp:
                              source $dtnmetestroot/tcp-cmd-half.sh
			      breaksw
               case dtn:
                              source $dtnmetestroot/dtn-cmd-half.sh
                              breaksw
	       case mail:
                              source $dtnmetestroot/mail/mail-cmd-half.sh
                              breaksw

               default:
                              echo "Not implemented protocol $proto_orig " >> $info
			      breaksw ;
endsw

