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

#ifndef _OASYS_RFCOMM_CLIENT_H_
#define _OASYS_RFCOMM_CLIENT_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "BluetoothClient.h"

namespace oasys {

class RFCOMMClient : public BluetoothClient {
public:
    RFCOMMClient(const char* logbase = "/rfcommclient")
        : BluetoothClient(SOCK_STREAM,BluetoothSocket::RFCOMM,logbase)
    {
    }
    RFCOMMClient(int fd, bdaddr_t remote_addr, u_int8_t remote_channel,
                 const char* logbase = "/rfcommclient")
        : BluetoothClient(SOCK_STREAM,BluetoothSocket::RFCOMM,fd,remote_addr,
                   remote_channel,logbase)

    {
    }
    /**
     * Since RFCOMM channels only range from [1 .. 30], scan the whole
     * space for an available channel on the remote Bluetooth host
     */
    int rc_connect(bdaddr_t remote_addr);
    int rc_connect();
private:
    static int rc_channel_;
};

}  // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _OASYS_RFCOMM_CLIENT_H_ */
