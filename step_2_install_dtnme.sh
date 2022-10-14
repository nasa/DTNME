#/bin/bash

# Build file structure at designated root directory

# Generate Run Script

tee /$1/run_dtnme.sh <<<"
#!/bin/bash

# dtnd - DTN2 server daemon
#    -c <cfg file>   = configuration file
#    -l <level>      = logging level (crit, err, warn, notice, info, debug)
#    -t              = tidy  (reset database)
#    -o <file>       = log file path
#
./bin/dtnme -c ./dtnme_daemon.cfg -l err -o node$2/dtn.log
"

# Copy Config File
cp daemon/dtnme_daemon.cfg /$1/dtnme_daemon.cfg

#Create Bin Directory to Store Binaries
mkdir /$1/bin

cp daemon/dtnme /$1/bin/dtnme
cp apps/ping_me/ping_me /$1/bin/ping_me
cp apps/recv_me/recv_me /$1/bin/recv_me
cp apps/report_me/report_me /$1/bin/report_me
cp apps/trace_me/trace_me /$1/bin/trace_me
cp apps/sink_me/sink_me /$1/bin/sink_me
cp apps/echo_me/echo_me /$1/bin/echo_me
cp apps/sdnv_convert_me/sdnv_convert_me /$1/bin/sdnv_convert_me
cp apps/dtpc_send_me/dtpc_send_me /$1/bin/dtpc_send_me
cp apps/dtpc_recv_me/dtpc_recv_me /$1/bin/dtpc_recv_me
cp apps/send_me/send_me /$1/bin/send_me
cp apps/source_me/source_me /$1/bin/source_me
cp apps/tunnel_me/tunnel_me /$1/bin/tunnel_me
cp apps/deliver_me/deliver_me /$1/bin/deliver_me

#Create Working Directory for DTNME to Store Bundles, Logs, and Databases
mkdir /$1/node_$2

