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


#ifndef _OASYS_TCP_CLIENT_H_
#define _OASYS_TCP_CLIENT_H_

#include "IPClient.h"

namespace oasys {

/**
 * Wrapper class for a tcp client socket.
 */
class TCPClient : public IPClient {
private:
    TCPClient(const TCPClient&);	///< Prohibited constructor
    
public:
    TCPClient(const char* logbase = "/oasys/tcpclient",
              bool init_socket_immediately = false);
    TCPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
              const char* logbase = "/oasys/tcpclient");

    /**
     * Try to connect to the remote host, but don't block for more
     * than timeout milliseconds. If there was an error (either
     * immediate or delayed), return it in *errp.
     *
     * @return 0 on success, IOERROR on error, and IOTIMEOUT on
     * timeout.
     */
    virtual int timeout_connect(in_addr_t remote_attr, u_int16_t remote_port,
                                int timeout_ms, int* errp = 0);
    
    
protected:
    int internal_connect(in_addr_t remote_attr, u_int16_t remote_port);
};

} // namespace oasys

#endif /* _OASYS_TCP_CLIENT_H_ */
