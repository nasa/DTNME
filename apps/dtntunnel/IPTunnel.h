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

#ifndef _IPTUNNEL_H_
#define _IPTUNNEL_H_

#include <oasys/debug/Log.h>

#include <dtn_api.h>
#include <APIBundleQueue.h>

namespace dtntunnel {

/**
 * Pure virtual base class for tunneling of IP protocols.
 */
class IPTunnel : public oasys::Logger {
public:
    /// Constructor
    IPTunnel(const char* classname, const char* logpath)
        : Logger(classname, "%s", logpath) {}
    
    /// Destructor
    virtual ~IPTunnel() {}
    
    /// Add a new listener
    virtual void add_listener(in_addr_t listen_addr,
                              u_int16_t listen_port,
                              in_addr_t remote_addr,
                              u_int16_t remote_port) = 0;
    
    /// Handle a newly arriving bundle. The tunnel should take
    /// ownership of the bundle structure and handle cleanup.
    virtual void handle_bundle(dtn::APIBundle* data) = 0;
};

} // namespace dtntunnel

#endif /* _IPTUNNEL_H_ */
