/*
 *    Copyright 2007-2008 Darren Long, darren.long@mac.com
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

#ifndef _OASYS_AX25CLIENT_H_
#define _OASYS_AX25CLIENT_H_

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

// for AX25 support
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include "AX25Socket.h"

// for oasys integration
#include "../io/IO.h"
#include "../io/IOClient.h"
#include "../compat/inttypes.h"
//#include "../debug/Log.h"


#include "../thread/Thread.h"

namespace oasys {


/*****************************************************************************/
//                  Class AX25Client Specification
/*****************************************************************************/
/**
 * Base class that unifies the AX25Socket and IOClient interfaces. Both
 * AX25ConnectedModeClient and AX25DisconnectedModeClient can derive from AX25Client.
 */
class AX25Client : public AX25Socket, public IOClient {
private:
    AX25Client(const AX25Client&);  ///< Prohibited constructor
    
public:
    AX25Client(int socktype, const char* logbase, Notifier* intr = 0);
    AX25Client(int socktype, int sock, const std::string& remote_addr,
             const char* logbase, Notifier* intr = 0);
    virtual ~AX25Client();
    
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
#endif /* #ifdef OASYS_AX25_ENABLED  */
#endif /* #ifndef _OASYS_AX25CLIENT_H_ */
