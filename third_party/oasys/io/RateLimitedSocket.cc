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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "RateLimitedSocket.h"

namespace oasys {

//----------------------------------------------------------------------
RateLimitedSocket::RateLimitedSocket(const char* logpath,
                                     uint64_t rate,
                                     uint64_t depth,  // ignored for now...
                                     BUCKET_TYPE  bucket_type,
                                     IPSocket* socket)
    : Logger("RateLimitedSocket", "%s", logpath),
      bucket_type_(bucket_type),
      socket_(socket)
{
    (void) depth;
 
    if(bucket_type_ == STANDARD) {
        bucket_ = new TokenBucket(logpath, 65535* 8, rate);
    } else { // BUCKET_TYPE.LEAKY
        bucket_ = new TokenBucketLeaky(logpath, 0, rate);
    }
}

//----------------------------------------------------------------------
RateLimitedSocket::~RateLimitedSocket()
{
   delete bucket_;
}

//----------------------------------------------------------------------
int
RateLimitedSocket::send(const char* bp, size_t len, int flags, bool wait_till_sent)
{
    ASSERT(socket_ != NULL);
    bool send_data = true;
    if (bucket_->rate() != 0) {
        while(send_data) {
            if(!wait_till_sent)send_data = false;
            bool can_send = bucket_->try_to_drain(len * 8);
            if (!can_send) {
                if(!send_data)
                {
                    log_debug("can't send %zu byte packet since only %llu tokens in bucket",
                          len, U64FMT(bucket_->tokens()));
                    return IORATELIMIT;
                }
            } else {
               log_debug("%llu tokens sufficient for %zu byte packet",
                      U64FMT(bucket_->tokens()), len);
               send_data = false; /// force loop to end
            }
        }
    }

    return socket_->send(bp, len, flags);
}

//----------------------------------------------------------------------
int
RateLimitedSocket::sendto(char* bp, size_t len, int flags,
                          in_addr_t addr, u_int16_t port,bool wait_till_sent)
{
    int sent = 0;
    ASSERT(socket_ != NULL);
    if (bucket_->rate() != 0) {
        while(sent == 0)
        {
            bool can_send = bucket_->try_to_drain(len * 8);
            if (!can_send) {
                if(!wait_till_sent) {
                    log_debug("can't send %zu byte packet since only %llu tokens in bucket",
                              len, U64FMT(bucket_->tokens()));
                    return IORATELIMIT; 
                }
                usleep(1);
            } else {
                log_debug("%llu tokens sufficient for %zu byte packet",
                      U64FMT(bucket_->tokens()), len);
                 sent = 1;
            }
        }
    }

    return socket_->sendto(bp, len, flags, addr, port);
}

} // namespace oasys
