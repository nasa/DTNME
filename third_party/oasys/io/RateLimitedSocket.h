/*
 *    Copyright 2006 Intel Corporation
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

/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _OASYS_RATE_LIMITED_SOCKET_H_
#define _OASYS_RATE_LIMITED_SOCKET_H_

// XXX/demmer for now this only works for IPSocket, but once Bowei
// finishes the IO porting, there will be a new base class that this
// class should use
#include "IPSocket.h"
#include "../debug/Log.h"
#include "../util/TokenBucket.h"
#include "../util/TokenBucketLeaky.h"

namespace oasys {

/**
 * The RateLimitedSocket class contains a socket class and a token
 * bucket and provides an interface to send data only if there is
 * enough space to send it out in.
 *
 * Note that the rate is configured in bits per second.
 */
class RateLimitedSocket : public Logger {

public:

enum BUCKET_TYPE {
    STANDARD    = 0,    ///  non leaky algorithm
    LEAKY       = 1,   ///  leaky algorithm
    UNKNOWN     = 2
};

    /**
     * Constructor.
     */
    RateLimitedSocket(const char* logpath,
                      uint64_t    rate,
                      uint64_t    depth,
                      BUCKET_TYPE bucket_type,
                      IPSocket*   socket = NULL);

    /**
     * Destructor
     */
    ~RateLimitedSocket();

    /**
     * Send the given data on the socket iff the rate controller
     * indicates that there is space.
     *
     * @return IORATELIMIT if there isn't space in the token bucket
     * for the given number of bytes, the return from IPSocket::send
     * if there is space.
     */
    int send(const char* bp, size_t len, int flags, bool wait_till_sent);

    /**
     * Send the given data on the socket iff the rate controller
     * indicates that there is space.
     *
     * @return IORATELIMIT if there isn't space in the token bucket
     * for the given number of bytes, the return from IPSocket::sendto
     * if there is space.
     */
    int sendto(char* bp, size_t len, int flags,
               in_addr_t addr, u_int16_t port, bool wait_till_sent);
    
    
    /// @{ Accessors
    TokenBucket* bucket() { return bucket_; }
    IPSocket*    socket() { return socket_; }
    /// @}

    /// @{ Setters
    void set_socket(IPSocket* sock) { socket_ = sock; }
    /// @}
    
protected:
    BUCKET_TYPE  bucket_type_;
    TokenBucket* bucket_;
    IPSocket*    socket_;
};

} // namespace oasys

#endif /* _OASYS_RATE_LIMITED_SOCKET_H_ */
