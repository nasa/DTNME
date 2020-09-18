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

#include <oasys/util/OptParser.h>
#include <oasys/bluez/Bluetooth.h>
#include "bundling/BundleDaemon.h"
#include "conv_layers/BluetoothConvergenceLayer.h"

#include "BluetoothAnnounce.h"

namespace dtn {

BluetoothAnnounce::BluetoothAnnounce()
    : sdp_reg_(BundleDaemon::instance()->local_eid().c_str())
{
    oasys::Bluetooth::hci_get_bdaddr(&cl_addr_);
}

bool
BluetoothAnnounce::configure(const std::string& name, ConvergenceLayer* cl,
                             int argc, const char* argv[])
{
    cl_ = cl;
    name_ = name;
    type_.assign(cl->name());

    oasys::OptParser p;
    p.addopt(new oasys::BdAddrOpt("cl_addr",&cl_addr_));
    bool setinterval = false;
    p.addopt(new oasys::UIntOpt("interval",&interval_,"","",&setinterval));
    cl_channel_ = BluetoothConvergenceLayer::BTCL_DEFAULT_CHANNEL;
    //p.addopt(new oasys::UInt8Opt("cl_channel",&cl_channel_));
    const char* invalid;
    if (! p.parse(argc, argv, &invalid))
    {
        log_err("bad parameter %s",invalid);
        return false;
    }

    if (! setinterval)
    {
        log_err("required parameter missing: interval");
        return false;
    }
    else
    {
        // convert to milliseconds
        interval_ *= 1000;
    }

    local_.assign(bd2str(cl_addr_));
    return true;
}

size_t
BluetoothAnnounce::format_advertisement(u_char*,size_t)
{
    return 0;
}

} // dtn

#endif // OASYS_BLUETOOTH_ENABLED
