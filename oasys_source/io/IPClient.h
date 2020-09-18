/*
 *    Copyright 2004-2006 Intel Corporation
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


#ifndef _OASYS_IP_CLIENT_H_
#define _OASYS_IP_CLIENT_H_

#include "IPSocket.h"
#include "IOClient.h"

namespace oasys {

/**
 * Base class that unifies the IPSocket and IOClient interfaces. Both
 * TCPClient and UDPClient derive from IPClient.
 */
class IPClient : public IPSocket, public IOClient {
private:
    IPClient(const IPClient&);	///< Prohibited constructor
    
public:
    IPClient(int socktype, const char* logbase, Notifier* intr = 0);
    IPClient(int socktype, int sock,
             in_addr_t remote_addr, u_int16_t remote_port,
             const char* logbase, Notifier* intr = 0);
    virtual ~IPClient();
    
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

#endif /* _OASYS_IP_CLIENT_H_ */
