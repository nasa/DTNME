# Defaults to sleep for an hour before running but you can override 
# with a command line parameter:  run_tunnel_node3.sh <seconds>
#

delay=3600
# allow override of the sleep time
if [ $# -eq 1 ]; then
tmp="$1"
chr=${tmp:0:1}
case $chr in
    [0-9]) delay=$tmp ;;
esac
fi

echo Will run: dtntunnel -b 20480000 -w 20480000 -m -q -u -L
echo "Sleeping for $delay seconds..."
sleep $delay


#dtntunnel parameters:
#    -b & -w =  bump up socket buff sizes (could be limited by system parameters)
#    -m = monitor mode so that it displays bundles and Megabits per second
#    -q = process a 10 character sequence counter at the end of each payload
#    -u = UDP tunnel instead of TCP
#    -L = Listener (for bundles) mode - transmits payload to the address/port specified in each bundle
#                                       (after stripping off the last 10 characters if using -q)
#    --local_eid <eid> = can be used to specify a specific EID but dtntunnel defaults to reading bundles 
#                        using the local EID/dtntunnel which is dtn://node3.dtn/dtntunnel if using these 
#                        default config files and scripts.
dtntunnel -b 20480000 -w 20480000 -m -q -u -L
