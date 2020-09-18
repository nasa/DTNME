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

#ifndef _OASYS_AX25CONNECTEDMODECLIENT_H_
#define _OASYS_AX25CONNECTEDMODECLIENT_H_

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

// for AX25 support
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include "AX25Client.h"

// for oasys integration
#include "../io/IO.h"
#include "../io/IOClient.h"
#include "../compat/inttypes.h"
//#include "../debug/Log.h"


#include "../thread/Thread.h"

namespace oasys {




/*****************************************************************************/
//                  Class AX25ConnectedModeClient Specification
/*****************************************************************************/
/*
 *
 * Wrapper class for a AX.25 Connected mode (SOCK_SEQPACKET) client socket.
 */
class AX25ConnectedModeClient : public AX25Client {
private:
    AX25ConnectedModeClient(const AX25ConnectedModeClient&);    ///< Prohibited constructor
    
public:
    AX25ConnectedModeClient(const char* logbase = "/oasys/ax25client",
              bool init_socket_immediately = false);
    AX25ConnectedModeClient(int fd, const std::string& remote_addr, 
              const char* logbase = "/oasys/ax25client");

    /**
     * Try to connect to the remote host, but don't block for more
     * than timeout milliseconds. If there was an error (either
     * immediate or delayed), return it in *errp.
     *
     * @return 0 on success, IOERROR on error, and IOTIMEOUT on
     * timeout.
     */
    virtual int timeout_connect(const std::string& axport, const std::string& remote_attr, 
                                std::vector<std::string> digi_list,
                                int timeout_ms, int* errp = 0);
    
    
protected:
    int internal_connect(const std::string& remote_attr, 
                        std::vector<std::string> digi_list);
};



} // namespace oasys
#endif /* #ifdef OASYS_AX25_ENABLED  */
#endif /* #ifndef _OASYS_AX25CONNECTEDMODECLIENT_H_ */
