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


#ifndef _OASYS_NET_UTILS_H_
#define _OASYS_NET_UTILS_H_

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../compat/inttypes.h"

/**
 * Wrapper macro to give the illusion that intoa() is a function call.
 * Which it is, really... or more accurately two inlined calls and one
 * function call.
 */
#define intoa(addr) oasys::Intoa(addr).buf()

namespace oasys {

/**
 * Faster wrapper around inet_ntoa.
 */
extern const char* _intoa(u_int32_t addr, char* buf, size_t bufsize);

/**
 * Class used to allow for safe concurrent calls to _intoa within an
 * argument list.
 */
class Intoa {
public:
    Intoa(in_addr_t addr) {
        str_ = _intoa(addr, buf_, bufsize_);
    }
    
    ~Intoa() {
        buf_[0] = '\0';
    }
    
    const char* buf() { return str_; }

    static const int bufsize_ = sizeof(".xxx.xxx.xxx.xxx");
    
protected:
    char buf_[bufsize_];
    const char* str_;
};

/**
 * Utility wrapper around the ::gethostbyname() system call
 */
extern int gethostbyname(const char* name, in_addr_t* addrp);

/*
 * Various overrides of {ntoh,hton}{l,s} that take a char buffer,
 * which can be used on systems that require alignment for integer
 * operations.
 */

inline u_int32_t
safe_ntohl(const char* bp)
{
    u_int32_t netval;
    memcpy(&netval, bp, sizeof(netval));
    return ntohl(netval);
}

inline u_int16_t
safe_ntohs(const char* bp)
{
    u_int16_t netval;
    memcpy(&netval, bp, sizeof(netval));
    return ntohs(netval);
}

inline void
safe_htonl(u_int32_t val, char* bp)
{
    val = htonl(val);
    memcpy(bp, &val, sizeof(val));
}
    
inline void
safe_htons(u_int16_t val, char* bp)
{
    val = htons(val);
    memcpy(bp, &val, sizeof(val));
}


} // namespace oasys

#endif /* _OASYS_NET_UTILS_H_ */
