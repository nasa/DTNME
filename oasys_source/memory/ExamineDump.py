#!/usr/bin/python

#
#    Copyright 2004-2006 Intel Corporation
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


import sys

class MemEntry:
    def __init__(self, live, stack):
	self.live  = live
	self.stack = stack
    def __str__(self):
	return "live = %d, stack = %s" % ( self.live, str(self.stack))

# read in memory usage dump
entries = []
f = open(sys.argv[1], 'r')
l = f.readline()
while l != "":
    l = l.strip().split(' ')
    entries.append(MemEntry(int(l[0]), l[1:]))
    l = f.readline()

# sort them by location
# XXX/bowei todo
