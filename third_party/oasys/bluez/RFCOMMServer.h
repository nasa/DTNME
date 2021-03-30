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

#ifndef _OASYS_RFCOMM_SERVER_H_
#define _OASYS_RFCOMM_SERVER_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "BluetoothServer.h"
#include "../thread/Thread.h"

namespace oasys {

class RFCOMMServer : public BluetoothServer {
public:
    RFCOMMServer(char* logbase = "/rfcommserver")
    : BluetoothServer(SOCK_STREAM,BluetoothSocket::RFCOMM,logbase)
    {
    }
};

class RFCOMMServerThread : public BluetoothServerThread {
public:
    RFCOMMServerThread(char * logbase = "/rfcommserver", int flags = 0)
    : BluetoothServerThread(SOCK_STREAM,
                     BluetoothSocket::RFCOMM,
                     "oasys::RFCOMMServerThread",
                     logbase,
                     flags)
    {
    }

    void set_accept_timeout(int ms) {
        accept_timeout_ = ms;
    }

};

} // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _OASYS_RFCOMM_SERVER_H_ */
