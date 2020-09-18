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



## First See base-cmd.sh 
## This script is called by above.
## base-cmd.sh defines all the variables used in this script


echo "Inside DTN specific setting " >> $info

set mylogroot  =    $logroot/node-$id
set myexeroot  =    $dtnmeroot/daemon
set baseconfig = $dtnmetestroot/base-dtn.conf


if (! -e $mylogroot) then
      mkdir  $mylogroot
endif

# Now generate daemon for the node

if (($id == 1) || ($id == $maxnodes) || ($perhop == 1)) then

    set file =  $mylogroot/node-$id.conf

    
    echo  set id $id > $file
    echo  set maxnodes $maxnodes >>  $file
    echo  set perhop $perhop >> $file

  
    echo  set tmp_logdir  $localtmpdir >> $file

    echo set dtnmetestroot $dtnmetestroot >> $file
    echo set localdir $txdir >> $file
    echo set ftplogfile $ftplogfile >> $file
    cat $baseconfig >> $file


    setenv LD_LIBRARY_PATH /usr/local/BerkeleyDB.4.2/lib
    setenv HOME /tmp/ 

    #-l $mylogroo
     set cmd = "$myexeroot/bundleDaemon -t   -c $file  -o $info.bd >>& $info "
     echo $cmd  >> $info

     ## Actually execute the command 
 	$myexeroot/bundleDaemon  -d -t  -c $file -o $info.bd  >>& $info

endif
