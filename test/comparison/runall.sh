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





function run_set() {
	bwkb="$bw""kb"
	./gen-emu.sh rabin $nodes $num $size $lossrate $bwkb "$types"  "$prots" $waittime 
	modexp -r -s -e DTN,rabin tmp/rabin.emu
	sleep $sleeptime 
	key=rabin-N$nodes-L$lossrate-M$num-S$size-B$bw
	dir=/proj/DTN/nsdi/logs/$key
	mkdir -p $dir
	for prot in $prots; do
		for type in $types; do
			if [ "$type" == "ph" ]; then
				typeind=1
			else
				typeind=0
			fi
			rm -rf $dir/$prot$typeind
			mv -f /proj/DTN/nsdi/logs/rabin/$prot$typeind $dir
		done
	done
} 


bw=100


prots="dtn"
types="ph e2e"
nodes=5
lossrate=0

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;





lossrate=0.05

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;



lossrate=0.1

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;




