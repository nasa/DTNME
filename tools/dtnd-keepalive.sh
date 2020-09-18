#!/bin/sh

output=`dtnd-control status`
if [ ! $? = 0 ]; then
    echo -n $output
    echo "Re-running dtnd -d $*"
    dtnd -d $*
fi
