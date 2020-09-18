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

#ifndef _BLUETOOTH_ANNOUNCE_H_
#define _BLUETOOTH_ANNOUNCE_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <oasys/bluez/BluetoothSDP.h>
#include "Announce.h"

namespace dtn {

/**
 * Helper class that represents Bluetooth CL and 
 * initiates contact with neighbor nodes
 * discovered by BluetoothDiscovery
 */
class BluetoothAnnounce : public Announce
{
public:
    /**
     * Not used by Bluetooth, since queries and beacons
     * use builtin Inquiry and SDP mechanisms
     */
    size_t format_advertisement(u_char*,size_t);

    void reset_interval() { ::gettimeofday(&data_sent_,0); }

    virtual ~BluetoothAnnounce() {}

protected:
    friend class Announce;

    BluetoothAnnounce();

    bool configure(const std::string& name, ConvergenceLayer* cl,
                   int argc, const char* argv[]);

    bdaddr_t cl_addr_; // address of parent CL instance
    u_int8_t cl_channel_; // channel used by parent CL
    oasys::BluetoothServiceRegistration sdp_reg_;
};

} // dtn

#endif // OASYS_BLUETOOTH_ENABLED
#endif // _BLUETOOTH_ANNOUNCE_H_
