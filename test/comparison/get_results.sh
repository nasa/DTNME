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





set protlist="dtn mail tcp"




set lossrate=0
set bandwidth=100
set size=10

set outfile=$1
rm -f $outfile

foreach nodes(2 3 4 5)
	set startline="#nodes"
	set line=$nodes
	foreach size(10 100)
		foreach name($protlist)
			foreach type(0 1)
				set startline="$startline S$size.$name.$type"
				set exp="rabin-N$nodes-L$lossrate-S$size-B$bandwidth"
				set ftime=`/proj/DTN/nsdi/bin//ftptime.sh $nodes $exp/$name$type | tail -n 1`
				set line="$line $ftime"
			end
		end
	end
	echo $line
	echo $line >> $outfile
end
echo $startline >> $outfile

