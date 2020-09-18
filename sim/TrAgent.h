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

#ifndef _TR_AGENT_H
#define _TR_AGENT_H

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Timer.h>

#include "SimEvent.h"
#include "SimEventHandler.h"
#include "naming/EndpointID.h"

namespace dtnsim {
class Node;

class TrAgent : public oasys::Timer, public oasys::Logger {
public:
    static TrAgent* init(const EndpointID& src, const EndpointID& dst,
                         int argc, const char** argv);

    virtual ~TrAgent() {}

    void timeout(const struct timeval& now);

private:
    TrAgent(const EndpointID& src, const EndpointID& dst);
    
    void send_bundle();

    EndpointID src_;    ///< source eid
    EndpointID dst_;    ///< destination eid
    u_int64_t size_;    ///< size of each message
    u_int expiration_;  ///< expiration time for each bundle
    u_int reps_;        ///< total number of reps/batches
    u_int batch_;       ///< no of messages in each batch
    double interval_;   ///< time gap between two batches
};

} // namespace dtnsim

#endif /* _TR_AGENT_H */
