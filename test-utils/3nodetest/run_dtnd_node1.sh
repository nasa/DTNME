#!/bin/bash
#
# This script is for node1 - edit node1.cfg to define the routing for your configuration
#                                                (mainly IP addresses)
node=node1


# node1 is usually started before data flow start so there is no delay by default
# but you can override with a command line parameter:  run_dtnd_node1.sh <seconds>
delay=0
# allow override of the sleep time
if [ $# -eq 1 ]; then
tmp="$1"
chr=${tmp:0:1}
case $chr in
    [0-9]) delay=$tmp ;;
esac
fi


# Get the dtn version to use in the log file names
verstr=`dtnd -v`
ver=
for word in $verstr
do
  if [[ "$word" != "version" ]]; then
    ver=$word
  fi
done

# create a new directory for the logs for this test 
# and generate the names for the log files
logdir=/tmp/logs
mkdir -p ${logdir}
testnum=1
while [ -d "${logdir}/test${testnum}" ]; do
  testnum=$(($testnum + 1))
done;
logdir=${logdir}/test${testnum}
logfile=${logdir}/test${testnum}_${node}_dtn-${ver}.log
toplogfile=${logdir}/test${testnum}_${node}_dtn-${ver}_top.log
mkdir $logdir

# output the log file info
echo ""
echo "Test $testnum (dtn-${ver})"
echo "LogFile: ${logfile}"
echo "TopLog: ${toplogfile}"
echo ""

echo "Sleeping for $delay seconds..."
sleep $delay

# kick off a background task to take CPU stats every minute
nohup ./run_top.sh "${toplogfile}" &
toppid=$!
echo "top task pid = $toppid"

# run the dtn server
dtnd -c ./${node}.cfg -l error -t -o ${logfile}

# stop the background task and output a reminder as to the log files
kill -9 $toppid
kill -9 `/sbin/pidof top`
echo "LogFile: ${logfile}"
echo ""
