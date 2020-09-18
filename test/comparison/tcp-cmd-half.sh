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



#Use by base-cmd.sh. dont use it directly

echo "Inside TCP specific setting " >> $info

# Now generate daemon for the node
if ($id == 1) then  
    set dst = node-$maxnodes
    if ($perhop == 1) then  
	set dst =  node-2-link-1
    endif
    
    echo "executing ftp: client $txdir  $logroot/ftplog.$id $dst  " >> $info
    tclsh $dtnmeroot/test/$proto_ftp client $txdir  $logroot/ftplog.$id $dst  >>& $info

    exit

endif

if ($id == $maxnodes) then 
    echo "executing ftp: server $rcvdir  $logroot/ftplog.$id  " >> $info
    tclsh $dtnmeroot/test/$proto_ftp server $rcvdir  $logroot/ftplog.$id >>& $info
    exit
endif     

## For other nodes which are in between
if ($perhop == 1) then
    ## set up TCP forwarding proxies
    set idplus  = `expr $id + 1`
    set idminus = `expr $id - 1`

    set myip = node-$id-link-$idminus
    set dstip = node-$idplus-link-$id
    set rconf = "$logroot/$id.rinetd-conf"
    set rinetdlogfile = "$logroot/$id.rinetd-log"

    echo $myip 17600 $dstip 17600 > $rconf 
    echo "logfile $rinetdlogfile" >> $rconf 

    echo " executing rinetd at router ($myip, $dstip) " >> $info 
    echo " rule is:  " >> $info
    cat $rconf >> $info 
    $dtnroot/rinetd/rinetd -c $rconf ;

endif
 
