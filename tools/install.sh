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
# Simple installation script for the dtn reference implementation
#

echo "***"
echo "*** Installing dtn..."
echo "***"
echo ""

if [ ! -f daemon/dtnd ] ; then
    echo "This script must be run from the top level DTNME directory"
    exit 1
fi

#
# Select and create a user account
#
echo -n "user account to use for the dtn daemon [dtn]: "
read DTN_USER
[ "$DTN_USER" = "" ] && DTN_USER=dtn

grep $DTN_USER /etc/passwd >/dev/null 2> /dev/null

if [ ! $? = 0 ]; then
   echo -n "create account for $DTN_USER? [y]: "
   read y
   if [ "$y" = "" -o "$y" = "y" -o "$y" = "yes" ]; then
      echo "creating account for $DTN_USER..."
      adduser $DTN_USER
   else 
      echo "can't find account for $DTN_USER... please create and re-run install"
      exit 1
   fi
fi

#
# Now run the makefile rule to do the real installation
#
echo "installing files"
make install

echo "running dtn daemon to create initial database..."
/usr/bin/dtnd --init-db

echo "installation complete."
