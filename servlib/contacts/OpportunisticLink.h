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

#ifndef _OPPORTUNISTIC_LINK_H_
#define _OPPORTUNISTIC_LINK_H_

class Link;

#include "Link.h"

namespace dtn {

/**
 * Abstraction for a OPPORTUNISTIC link. It has to be opened
 * everytime one wants to use it. It has by definition only
 * -one contact- that is associated with the current
 * opportunity. The difference between opportunistic link
 * and ondemand link is that the ondemand link does not have
 * a queue of its own.
 */
class OpportunisticLink : public Link {
public:
    /**
     * Constructor / Destructor
     */
    OpportunisticLink(std::string name,
                      ConvergenceLayer* cl,
                      const char* nexthop)
        : Link(name, OPPORTUNISTIC, cl, nexthop)
    {
    }
};

} // namespace dtn

#endif /* _OPPORTUNISTIC_LINK_H_ */
