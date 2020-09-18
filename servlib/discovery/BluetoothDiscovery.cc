/*
 *    Copyright 2006 Baylor University
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <climits>
#include <oasys/util/Random.h>
#include <oasys/bluez/Bluetooth.h>
#include <oasys/bluez/BluetoothSDP.h>
#include <oasys/bluez/BluetoothInquiry.h>
#include <oasys/util/OptParser.h>
#include "bundling/BundleDaemon.h"

#include "BluetoothAnnounce.h"
#include "BluetoothDiscovery.h"

namespace dtn {

void
BluetoothDiscovery::shutdown()
{
    shutdown_ = true;
    notifier_.notify();
}

BluetoothDiscovery::BluetoothDiscovery(const std::string& name)
    : Discovery(name,"bt"),
      oasys::Thread("BluetoothDiscovery", DELETE_ON_EXIT),
      notifier_("/dtn/discovery/bt")
{
    oasys::Bluetooth::hci_get_bdaddr(&local_addr_);
    shutdown_ = false;
}

bool
BluetoothDiscovery::configure(int argc, const char* argv[])
{
    if (oasys::Thread::started())
    {
        log_warn("reconfiguration of BluetoothDiscovery not supported");
        return false;
    }

    oasys::OptParser p;
    const char* invalid;
    p.addopt(new oasys::BdAddrOpt("local_addr",&local_addr_));
    if (! p.parse(argc,argv,&invalid))
    {
        log_err("bad option: %s",invalid);
        return false;
    }

    local_.assign(bd2str(local_addr_));

    start();
    return true;
}

void
BluetoothDiscovery::run()
{
    oasys::BluetoothInquiry inquiry;
    while (! shutdown_)
    {
        u_int interval = INT_MAX;
        // grab the minimum interval from registered CL's
        for (iterator i = list_.begin(); i != list_.end(); i++)
        {
            BluetoothAnnounce* announce = dynamic_cast<BluetoothAnnounce*>(*i);
            if (announce->interval_remaining() < interval)
            {
                interval = announce->interval_remaining();
            }
        }
        // randomize the sleep time:
        // the point is that two nodes with zero prior knowledge of each other
        // need to be able to discover each other in a reasonably short time.
        // if common practice is that all set their poll_interval to 1 or 30
        // or x then there's the chance of synchronization or syncopation such
        // that discovery doesn't happen.
        
        if (interval > 0) 
        {
            u_int sleep_time = oasys::Random::rand(interval);
            log_debug("sleep time %d",sleep_time);

            notifier_.wait(NULL,sleep_time);
        }

        if (shutdown_) break;

        // initiate Bluetooth inquiry on local Bluetooth chipset
        // this call blocks until inquiry is complete (10+ sec)
        int nr = inquiry.inquire();

        if (shutdown_) break;

        // Polling instead of beaconing means that the act of polling
        // satisfies everyone's interval, so reset them all
        for (iterator i = list_.begin(); i != list_.end(); i++)
        {
            BluetoothAnnounce* announce = dynamic_cast<BluetoothAnnounce*>(*i);
            announce->reset_interval();
        }

        if (nr < 0) continue; // nobody's around

        // iterate over discovered neighbors
        bdaddr_t remote;
        while (inquiry.next(remote) != -1)
        {

            // query for DTN to remote host's SDP daemon
            oasys::BluetoothServiceDiscoveryClient sdpclient;
            if (sdpclient.is_dtn_router(remote))
            {
                std::string nexthop(bd2str(remote));
                EndpointID eid = sdpclient.remote_eid();
                log_info("discovered DTN peer %s at %s channel %d",eid.c_str(),
                         nexthop.c_str(),sdpclient.channel());
                handle_neighbor_discovered("bt",nexthop,eid); 
            }

            if (shutdown_) break;
        }
    }
}

}  // dtn

#endif // OASYS_BLUETOOTH_ENABLED
