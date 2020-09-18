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

#ifndef _IP_CONVERGENCE_LAYER_UTILS_H_
#define _IP_CONVERGENCE_LAYER_UTILS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>

#include "ConvergenceLayer.h"

namespace dtn {

/**
 * Utility class for shared functionality between ip-based convergence
 * layers.
 */
class IPConvergenceLayerUtils {
public:
    /**
     * Parse a next hop address specification of the form
     * <host>[:<port>?].
     *
     * @return true if the conversion was successful, false
     */
    static bool parse_nexthop(const char* logpath, const char* nexthop,
                              in_addr_t* addr, u_int16_t* port);
};

} // namespace dtn

#endif /* _IP_CONVERGENCE_LAYER_UTILS_H_ */
