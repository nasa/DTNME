#!/bin/sh

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



#
# Simple un installation script for the dtn reference implementation
#

echo "***"
echo "*** Unstalling dtn..."
echo "***"
echo ""

echo "This script will remove all dtn executables, configuration"
echo "files and data files."
echo ""
echo -n "Are you sure you want to continue? [n]: "
read y
if [ ! "$y" = "y" -o "$y" = "yes" ]; then
   echo "uninstall aborted"
   exit 0
fi

echo -n "removing dtn executables: "
apps="dtnsend dtnrecv dtnping dtncp dtncpd"
for f in dtnd $apps ; do
   echo -n "$f... "
   rm -f /usr/bin/$f
done
echo ""

echo -n "removing dtn directories: "
for d in /var/dtn/db /var/dtn/bundles /var/dtn ; do
   echo -n "$d... "
   rm -rf $d
done
echo ""

echo "uninstallation complete."
