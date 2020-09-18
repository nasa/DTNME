#!/bin/bash
toplogfile="$1"
# give dtnd a little time to start up...
sleep 5
top -b -d 60 -p "`/sbin/pidof dtnd`" > "${toplogfile}"
