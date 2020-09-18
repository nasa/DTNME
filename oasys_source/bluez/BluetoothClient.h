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

#ifndef _OASYS_BT_CLIENT_H_
#define _OASYS_BT_CLIENT_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "../io/IOClient.h"
#include "BluetoothSocket.h"

namespace oasys {

/**
 * Base class that unifies the BluetoothSocket and IOClient interfaces.
 */
class BluetoothClient : public BluetoothSocket, public IOClient {
public:
    BluetoothClient(int socktype, BluetoothSocket::proto_t proto,
                    const char* logbase, Notifier* intr=0);
    BluetoothClient(int socktype, BluetoothSocket::proto_t proto, int fd,
                    bdaddr_t remote_addr, u_int8_t remote_channel,
                    const char* logbase,Notifier* intr=0);
    virtual ~BluetoothClient();
    
    // Virtual from IOClient
    virtual int read(char* bp, size_t len);
    virtual int write(const char* bp, size_t len);
    virtual int readv(const struct iovec* iov, int iovcnt);
    virtual int writev(const struct iovec* iov, int iovcnt);
    
    virtual int readall(char* bp, size_t len);
    virtual int writeall(const char* bp, size_t len);
    virtual int readvall(const struct iovec* iov, int iovcnt);
    virtual int writevall(const struct iovec* iov, int iovcnt);
    
    virtual int timeout_read(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms);
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms);

    virtual int timeout_write(const char* bp, size_t len, int timeout_ms);
    virtual int timeout_writev(const struct iovec* iov, int iovcnt,
                               int timeout_ms);
    virtual int timeout_writeall(const char* bp, size_t len, int timeout_ms);
    virtual int timeout_writevall(const struct iovec* iov, int iovcnt,
                                  int timeout_ms);

    virtual int get_nonblocking(bool *nonblockingp);
    virtual int set_nonblocking(bool nonblocking);
};

} // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _OASYS_BT_CLIENT_H_ */
