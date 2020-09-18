#!/bin/sh

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


#
# Simple script to run various autoconf related commands to generate
# the configure script. 
#
# This should not need to be run often, only when things like library
# dependencies change or when some changes are made to the configure.ac
# script. In particular, all generated files are cross-platform
# and are therefore stored in the version control repository.

echo "build-configure: building aclocal.m4..."
rm -f aclocal.m4
cat aclocal/*.ac > aclocal.m4

if [ -d "$1" ] ; then
    oasys_dir=$1

elif [ -d ../oasys ] ; then
    oasys_dir=../oasys

elif [ -d /usr/share/oasys ] ; then
    oasys_dir=/usr/share/oasys

else
    echo "build-configure: ERROR -- can't find oasys for autoconf macros"
    exit 1
fi

echo "build-configure: loading oasys functions from $oasys_dir into aclocal.m4..."
cat $oasys_dir/aclocal/*.ac >> aclocal.m4

echo "build-configure: running autoheader to build dtn-config.h.in..."
rm -f dtn-config.h dtn-config.h.in
autoheader

cat >> dtn-config.h.in <<EOF

/* Enable inclusion of the STD C Macros */
#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
    #define __STDC_LIMIT_MACROS
#endif
#include <inttypes.h>

/* Include oasys configuration state */
#include <oasys/oasys-config.h>
EOF

echo "build-configure: running autoconf to build configure..."
rm -f configure
autoconf

echo "build-configure: purging configure cache..."
rm -rf autom4te.cache
rm -f config.cache

echo "build-configure: done."
