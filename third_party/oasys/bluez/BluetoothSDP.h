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

#ifndef _OASYS_BTSDP_H_
#define _OASYS_BTSDP_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "../debug/Log.h"

#include <cstdio>
#include <cerrno>
#include <cstdlib>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

// generated using uuidgen on Mac OS X ... a completely arbitrary number :)
// maybe eventually register something with Bluetooth SIG?
#define OASYS_BLUETOOTH_SDP_UUID { 0xDCA38352, 0xBF6011DA, \
                                   0xA23B0003, 0x931B7960 }

namespace oasys {

// for reference, from <bluetooth/sdp.h>
#if 0

typedef struct _sdp_list sdp_list_t;
struct _sdp_list {
    sdp_list_t *next;
    void *data;
};

#endif

/**
 * Connect to remote Bluetooth device and query its SDP server
 * for DTN service
 */
class BluetoothServiceDiscoveryClient : public Logger
{
public:
    BluetoothServiceDiscoveryClient(const char*
                                    logpath="/dtn/disc/bt/sdp/client");
    virtual ~BluetoothServiceDiscoveryClient();

    bool is_dtn_router(bdaddr_t remote);
    void get_local_addr(bdaddr_t& addr) {
        bacpy(&addr,&local_addr_);
    }

    u_int8_t channel() { return channel_; }
    const std::string& remote_eid() { return remote_eid_; }

protected:
    /* member data */
    bdaddr_t      remote_addr_;     /* physical address of device to query */
    bdaddr_t      local_addr_;      /* physical address of local adapter */
    std::string   remote_eid_;      /* EID of remote router */
    u_int8_t      channel_;         /* RFCOMM channel */
};

class BluetoothServiceRegistration : public Logger 
{
public:

#define OASYS_BLUETOOTH_SDP_NAME "dtnd"

    BluetoothServiceRegistration(const char* name = OASYS_BLUETOOTH_SDP_NAME,
                                 const char* logpath = "/dtn/cl/bt/sdp/reg");
    virtual ~BluetoothServiceRegistration();

    bool success() {return status_;};
    void get_local_addr(bdaddr_t& addr) {
        bacpy(&addr,&local_addr_);
    }

protected:
    bool register_service(const char *name);

    sdp_session_t* sess_;
    bool status_;
    bdaddr_t local_addr_;
};

} // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _OASYS_BTSDP_H_ */
