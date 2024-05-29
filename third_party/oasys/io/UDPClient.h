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

#ifndef _OASYS_UDP_CLIENT_H_
#define _OASYS_UDP_CLIENT_H_

#include "IPClient.h"

namespace oasys {

/**
 * Wrapper class for a udp client socket.
 */
class UDPClient : public IPClient {
private:
    UDPClient(const UDPClient&);	///< Prohibited constructor
    
public:
    UDPClient(const char* logbase = "/udpclient");
};

} // namespace oasys

#endif /* _OASYS_UDP_CLIENT_H_ */
