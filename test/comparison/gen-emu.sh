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



echo "ur command was # $*"
echo "no of args $#"

if ( ($#argv != 9) && ($#argv != 14) )then
	echo "Improper usage "
        echo "Usage: $0 <expname> <maxnodes> <nfiles> <size(KB)> <loss> <bw> <perhop> <proto1, proto2,..> <finishtime>"
	echo "Optional arguments after above: dynanics <linkdynamics> <uptime> <downtime> <offset> "
  	echo "Example: $0 try 4 10 100 0 100kb  [e2e ph] [tcp dtn mail] 711 "
	echo " This will generate <expname.emu> and will run tcp, dtn"
	echo
    exit
    endif




set exp = $1
set file = tmp/$exp.emu

echo "Automatic emulab topolgy generation. Output file $file" 



echo "#Automatic emulab topolgy generation" > $file
echo "#Generated using command $* " > $file
echo set exp $1 >> $file
echo set maxnodes $2 >> $file
echo set nfiles $3 >> $file
echo set size $4 >> $file
echo set loss $5 >> $file 
echo set bw $6 >> $file 
echo set perhops \"$7\" >> $file 
echo set protos \"$8\" >> $file
echo set finish $9 >> $file

echo >> $file
echo "# default link dynamic values "  >> $file

echo set  linkdynamics 0 >> $file
echo set  up 60  >> $file
echo set down 180 >> $file
echo set OFFSET_VAL 0 >> $file
#echo set ftp_impl "simple-ftp-noack.tcl" >> $file

echo set ftp_impl "ftp.tcl" >> $file

#if ($10 != "dynamics") then
#    exit "Usage problem: arugment 10 should be dynamics or nothing "
#endif

## ignore next argument  ($10) 
 if ($#argv > 10) then

echo >> $file
echo "# Overriding link dynamic values "  >> $file

#echo set ftp_impl "simple-ftp.tcl" >> $file

echo set ftp_impl "ftp.tcl" >> $file



echo set  linkdynamics $11 >> $file
echo set  up  $12  >> $file
echo set down $13 >> $file
echo set OFFSET_VAL $14 >> $file

#echo setenv HOME $HOME >> $file

endif


cat base-emu.tcl >> $file ;

echo 

echo "Now run: (make sure the directory tmp exists) "
echo "   startexp -i -p DTN -e $exp $file"
echo 
echo "To terminate "
echo "endexp -e DTN,$exp "
echo 
