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

#ifndef _IP_CONVERGENCE_LAYER_H_
#define _IP_CONVERGENCE_LAYER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>

#include "ConvergenceLayer.h"

namespace dtn {

/**
 * Base class for shared functionality between the TCP and UDP
 * convergence layers (currently none).
 */
class IPConvergenceLayer : public ConvergenceLayer {
protected:
    /**
     * Constructor.
     */
    IPConvergenceLayer(const char* classname, const char* name)
        : ConvergenceLayer(classname, name)
    {
    }

    /**
     * Parse a next hop address specification of the form
     * <host>[:<port>?].
     *
     * @return true if the conversion was successful, false
     */
    bool parse_nexthop(const char* nexthop,
                       in_addr_t* addr, u_int16_t* port);
    
public:
};

} // namespace dtn

#endif /* _IP_CONVERGENCE_LAYER_H_ */
