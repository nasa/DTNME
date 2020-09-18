#
#    Copyright 2006 Intel Corporation
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

import iptables-utils.tcl

#
# Test options
#
test::name linear-perf

set check_large_timeout 0

# number of nodes
net::default_num_nodes 5

# mode: [dtn | ftp | dtntunnel | mail | theory]
set testopt(mode)      ""

# hop mode [hop | e2e]
set testopt(hop_mode) hop

# convergence layer
set testopt(cl)        tcp

# name of the emulab test (if any)
set testopt(emulab_test)   ""

# connectivity mode: [conn | partial | shift | random]
set testopt(conn)     conn

# DTN link options:
set testopt(link_opts) "min_retry_interval=1 max_retry_interval=1 data_timeout=30000 potential_downtime=99999999"
#set testopt(link_opts) "min_retry_interval=1 max_retry_interval=1 data_timeout=5000 reactive_frag_enabled=0"

set testopt(frag) true

# File size
set testopt(size) 1024

# File count
set testopt(count) 10

# Sleep interval
set testopt(sleep) 0

# Storage DB type
set testopt(db_type) filesysdb

# Max test length
set testopt(test_max) 35

run::parse_test_opts

testlog "Test options: "
foreach k [lsort [array names testopt]] {
    testlog "\t$k: $testopt($k)"
}

switch -- $testopt(mode) {
    dtn - ftp - dtntunnel - mail - clear - theory {}
    ""      {error "must specify mode"}
    default {error "unknown mode '$testopt(mode)'"}
}

switch -- $testopt(hop_mode) {
    hop - e2e {}
    ""      {error "must specify hop mode"}
    default {error "unknown hop mode '$testopt(hop_mode)'"}
}

if {$testopt(frag) != true} {
    append testopt(link_opts) " reactive_frag_enabled=0"
}

#
# Test file definitions
#
set user          $env(USER)

if {$testopt(emulab_test) != ""} {
    testlog "Rereading net definition file for emulab"
    set emulab_test $testopt(emulab_test)
    import emulab-net
    set dtnd_exe      /proj/DTN/$user/DTNME/daemon/dtnd
    set dtnsend_exe   /proj/DTN/$user/DTNME/apps/dtnsend/dtnsend
    set dtnrecv_exe   /proj/DTN/$user/DTNME/apps/dtnrecv/dtnrecv
    set dtntunnel_exe /proj/DTN/$user/DTNME/apps/dtntunnel/dtntunnel
    set randfile_exe  /proj/DTN/$user/oasys/tools/randfile
    set pproxy_exe    /proj/DTN/$user/PacketProxy/src/pproxy
    set copy_exe      0
    
    set dtn_config_opts "-no_copy_executables -storage_type $testopt(db_type)"
    
} else {
    set dtnd_exe      "dtnd"
    set dtnsend_exe   "dtnsend"
    set dtnrecv_exe   "dtnrecv"
    set dtntunnel_exe "dtntunnel"
    set randfile_exe  "[dist::get_rundir $net::host(0) 0]/randfile"

    manifest::file ../oasys/tools/randfile randfile
    set copy_exe      1

    set dtn_config_opts "-storage_type $testopt(db_type)"
}

if {$testopt(mode) == "ftp" || $testopt(mode) == "dtntunnel"} {
    manifest::file emulab/simple-ftp.tcl simple-ftp.tcl
}

if {$testopt(mode) == "mail"} {
    manifest::file emulab/send-mail.tcl send-mail.tcl
}

set N [net::num_nodes]
set last [expr $N - 1]

proc emulab_get_stats {what} {
    global emulab_test
    set output [run::run_cmd users.emulab.net /usr/testbed/bin/portstats dtn $emulab_test]

    set L [split $output "\n"]

    for {set i 3} {$i < [llength $L]} {incr i} {
        set line [lindex $L $i]
        
        foreach {link in_bytes in_unicast_pkts in_other_pkts out_bytes out_unicast_pkts out_other_pkts} $line {}

        global emulab_links emulab_stats_in emulab_stats_out
        set emulab_links($link) 1
        set emulab_stats_in($what,$link)  $in_bytes
        set emulab_stats_out($what,$link) $out_bytes
    }
}

proc emulab_stat_diff {start end} {
    global emulab_links emulab_stats_in emulab_stats_out

    foreach link [lsort [array names emulab_links]] {
        set in_a $emulab_stats_in($start,$link)
        set in_b $emulab_stats_in($end,$link)
        set out_a $emulab_stats_out($start,$link)
        set out_b $emulab_stats_out($end,$link)
        puts "$link:\tin [expr $in_b - $in_a]\tout [expr $out_b - $out_a]"
    }
}

#
# DTN configuration
#
if {$testopt(mode) == "dtn" || $testopt(mode) == "dtntunnel"} {
    eval dtn::config $dtn_config_opts
    dtn::config_app_manifest $copy_exe {dtnsend dtnrecv dtntunnel}
    dtn::config_interface $testopt(cl)

    if {$testopt(hop_mode) == "hop"} {
        dtn::config_linear_topology ALWAYSON $testopt(cl) true $testopt(link_opts)
    } else {
        dtn::config_topology_common true
        dtn::config_link 0 $last ALWAYSON $testopt(cl) $testopt(link_opts)
    }
}

if {$testopt(mode) == "theory"} {
    set opt(dry_run) 1
}

test::script {
    foreach id [net::nodelist] {
        iptables::flush $id
    }
    
    testlog "Running ping test..."
    set output ""
    catch {
        set output [run::run_cmd $net::host(0) ping -c 4 $net::internal_host($last)]
    }
    puts $output

    if {$testopt(emulab_test) != ""} {
        testlog "Getting initial packet counts"
        emulab_get_stats start
    }

    testlog "Generating $testopt(size) byte test file"
    set srcdir [dist::get_rundir $net::host(0) 0]
    set dstdir [dist::get_rundir $net::host($last) $last]
    set file   testfile.data

    run::run_cmd $net::host(0) $randfile_exe -A -L $testopt(size) -O $srcdir/$file 2>/dev/null
    
    #
    # Mode-specific setup phase
    #
    if {$testopt(mode) == "dtn" || $testopt(mode) == "dtntunnel"} {
        set src_eid dtn://host-0/test
        set dst_eid dtn://host-$last/test
        
        testlog "Running dtnds"
        dtn::run_dtnd * $dtnd_exe
        
        testlog "Waiting for dtnds to start up"
        dtn::wait_for_dtnd *

        #dtn::dump_routes
        
        if {$testopt(mode) == "dtn"} {
            testlog "Running dtn receiver"
            set rcvpid [dtn::run_app $last dtnrecv \
                    "-q -n $testopt(count) -o $dstdir/testfile#####.data ${dst_eid}*" $dtnrecv_exe]
        }

        if {$testopt(mode) == "dtntunnel"} {
            testlog "Running dtn tunnels"
            set expiration [expr $testopt(test_max) * 60]
            set tun1pid [dtn::run_app 0 dtntunnel \
                    "-t -T 17600:localhost:17601 -e $expiration dtn://host-$last/dtntunnel" $dtntunnel_exe]
            set tun2pid [dtn::run_app $last dtntunnel "-L -e $expiration" $dtntunnel_exe]
        }
    }
    
    if {$testopt(mode) == "ftp" && $testopt(hop_mode) == "hop"}  {
        testlog "Running packet proxies"
        foreach id [net::nodelist] {
            if {$id == 0 || $id == $last} { continue }
            set proxypid_$id [dtn::run_app $id pproxy "-d node-[expr $id + 1] -p 17600 -l 17600 -v 2" $pproxy_exe]
        }
    }
    
    if {$testopt(mode) == "ftp"} {
        testlog "Running ftp server"
        set ftp_server_pid [dtn::run_app $last simple-ftp.tcl "server"]
    }

    if {$testopt(mode) == "dtntunnel"} {
        testlog "Running ftp server"
        set ftp_server_pid [dtn::run_app $last simple-ftp.tcl "server -port 17601"]
    }

    if {$testopt(mode) == "mail"} {
        testlog "Setting up sendmail config"
        if [catch {
            cd emulab/mail
            exec ./sendmail-install.sh $emulab_test $N $testopt(hop_mode) >&@stdout
            cd ../..
        } err] {
            puts "error installing sendmail conf: $err"
            exit 1
        }

        testlog "Dumping mailq"
        catch {
            foreach id [net::nodelist] {
                set output [run::run_cmd $net::host($id) sudo mailq]
                puts $output
            }
        }
        
        testlog "Cleaning delivery directory"
        run::run_cmd $net::host($last) rm -rf /tmp/test-delivery
        run::run_cmd $net::host($last) mkdir  /tmp/test-delivery
        run::run_cmd $net::host($last) rm -f /tmp/delivery_count
    }

    #
    # Set up the iptables blocking script
    #
    if {$testopt(conn) != "conn"} {
        testlog "Setting up iptables port blocking"
        switch $testopt(mode) {
            dtn - dtntunnel    { set port [dtn::get_port tcp $last] }
            ftp                { set port 17600 }
            mail               { set port 25 }
        }
        
        if {$testopt(conn) == "onelink"} {
            iptables::add_port $last $port fixed [list 60 60 0]
        }
        
        if {$testopt(conn) == "all"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 60 0]
            }
        }
        
        if {$testopt(conn) == "shift"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 60 [expr 60 - ($id * 15)]]
            }
        }

        if {$testopt(conn) == "shift4"} {
            for {set id 1} {$id < [expr $N - 1]} {incr id} {
                iptables::add_port $id $port fixed [list 240 240 [expr 240 - $id * 60]]
            }
            iptables::add_port [expr $N - 1] $port fixed [list 240 240 0]
        }

        if {$testopt(conn) == "offset"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 60 [expr ($id % 2) * 60]]
            }
        }

        if {$testopt(conn) == "all2"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 180 0]
            }
        }
       
        if {$testopt(conn) == "offset2"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 180 [expr (($id + 1) % 2) * 120]]
            }
        }

        if {$testopt(conn) == "sequential"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 180 [expr ($id-1) * 60]]
            }
        }

        if {$testopt(conn) == "shift10"} {
            for {set id 1} {$id < $N} {incr id} {
                iptables::add_port $id $port fixed [list 60 180 [expr ($id-1) * 10]]
            }
        }

        iptables::run_async
    }
    
    #
    # Start the clock
    #
    set starttime [clock seconds]
    puts "START: $starttime"

    #
    # Copy phase
    #
    if {$testopt(mode) == "dtn"} {
        testlog "Running dtnsend for $testopt(count) iterations"
        set sndpid [dtn::run_app 0 dtnsend "-s $src_eid -d $dst_eid -t t -p $file \
                -n $testopt(count) -z $testopt(sleep)" $dtnsend_exe]

        testlog "Waiting for senders / receivers to complete (up to $testopt(test_max) mins)"
        
        set loop_start [clock seconds]
        set elapsed_mins 0
        do_until "waiting for data to flow" [expr $testopt(test_max) * 60] {
            if {[run::check_pid $net::host($last) $rcvpid] == 0} {
                break
            }

            set elapsed [expr ([clock seconds] - $loop_start) / 60]
            if {$elapsed > $elapsed_mins} {
                set elapsed_mins $elapsed
                
                testlog "$elapsed_mins minutes elapsed... dumping stats"
                foreach n [net::nodelist] {
                    puts "Node $n stats: [tell_dtnd $n bundle stats]"
                }
            }

            after 500
        }

        testlog "All bundles received..."
        dtn::dump_stats
        
    } elseif {$testopt(mode) == "ftp" || $testopt(mode) == "dtntunnel"} {

        if {$testopt(mode) == "dtntunnel"} {
            set dest localhost
            set timeout 0
            
        } else {
            # ftp mode
            set timeout 30000
            if {$testopt(conn) == "conn"} {
                puts "XXX Overriding timeout for connected mode"
                set timeout 0
            }
            
            if {$testopt(hop_mode) == "hop"} {
                set dest $net::internal_host(1)
                
            } else {
                set dest $net::internal_host($last)
            }
        }

        puts "Running ftp client..."
        set ftp_client_pid [dtn::run_app 0 simple-ftp.tcl \
                "client $file $testopt(count) $dest -port 17600 -timeout $timeout"]

        set loop_start [clock seconds]
        set elapsed_mins 0
        do_until "waiting for data to flow" [expr $testopt(test_max) * 60] {
            if {[run::check_pid $net::host(0) $ftp_client_pid] == 0} {
                break
            }

            set elapsed [expr ([clock seconds] - $loop_start) / 60]
            if {$elapsed > $elapsed_mins} {
                set elapsed_mins $elapsed
                
                testlog "$elapsed_mins minutes elapsed... "
                if {$testopt(mode) == "dtntunnel"} {
                    foreach n [net::nodelist] {
                        puts "Node $n stats: [tell_dtnd $n bundle stats]"
                    }
                }
            }
            
            after 500
        }

        testlog "All files sent..."

    } elseif {$testopt(mode) == "mail"} {
        puts "Running mail injector..."
        
        set mail_dest "$user@$net::host($last)"
        dtn::run_app 0 send-mail.tcl "$file $testopt(count) $mail_dest"

        set loop_start [clock seconds]
        set elapsed_mins 0
            
        do_until "waiting for mail to flow" [expr $testopt(test_max) * 60] {
            set num -1
            if [catch {
                set num [run::run_cmd $net::host($last) cat /tmp/delivery_count]
            } err] {
                puts "warning: reading delivery number: $err"
            }
            
            if {$num == $testopt(count)} {
                puts "all $num messages delivered"
                break
            }
            
            set elapsed [expr ([clock seconds] - $loop_start) / 60]
            if {$elapsed > $elapsed_mins} {
                set elapsed_mins $elapsed
                
                testlog "$elapsed_mins minutes elapsed... "
                testlog "$num messages delivered"
            }

            after 500
        }
    }

    set endtime [clock seconds]
    puts "END: $endtime"

    puts "ELAPSED: [expr [clock seconds] - $starttime]"
    if {$testopt(mode) == "dtn" || $testopt(mode) == "dtntunnel"} {
        puts "making sure that no bundles are pending in the network..."
        foreach id [net::nodelist] {
            dtn::wait_for_bundle_stats $id {0 pending}
        }
    }
    
    if {$testopt(mode) == "dtn"} {
        puts "checking that exactly $testopt(count) bundles were delivered..."
        dtn::wait_for_bundle_stats $last [list 0 pending $testopt(count) delivered]
    }
}

test::exit_script {
    if {$testopt(emulab_test) != ""} {
        testlog "Getting final packet counts:"
        emulab_get_stats end
        emulab_stat_diff start end
    }
    
    if {$testopt(mode) == "ftp" || $testopt(mode) == "dtntunnel"} {
        testlog "Stopping ftp server"
        run::kill_pid $last $ftp_server_pid TERM
    }

    if {$testopt(mode) == "dtntunnel"} {
        testlog "Stopping dtntunnels"
        run::kill_pid 0     $tun1pid TERM
        run::kill_pid $last $tun2pid TERM
    }

    if {$testopt(mode) == "ftp" && $testopt(hop_mode) == "hop"}  {
        testlog "Stopping packet proxies"
        foreach id [net::nodelist] {
            if {$id == 0 || $id == $last} { continue }
            run::kill_pid $id proxypid_$id TERM
        }
    }

    if {$testopt(mode) == "dtn" || $testopt(mode) == "dtntunnel"} {
        testlog "Stopping all dtnds"
        dtn::stop_dtnd *
    }
}
