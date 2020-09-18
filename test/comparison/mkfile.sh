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



#Usage: mkfile.sh <size> <name>
#Creates empty file of given size. k for KB, m for MB. default byte.
#Example: mkfile 1m creates a file named 1m.file of size 1 Megabytes

if ($#argv != 2) then
    echo " Usage  mkfile.sh <size> <name> "
    exit 0
endif

echo "Creating file $2  of size $1"
head -c $1 /dev/zero > $2
