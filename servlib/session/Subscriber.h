/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _SUBSCRIBER_H_
#define _SUBSCRIBER_H_

#include <vector>

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Formatter.h>

#include "naming/EndpointID.h"

namespace dtn {

class Registration;

/**
 * A subscriber for a session is either a local registration or a next
 * hop destination.
 */
class Subscriber : public oasys::Formatter {
public:
    /// Constructor for a NULL subscriber (used for 
    Subscriber()
        : reg_(NULL), nexthop_(EndpointID::NULL_EID()) {}
    
    /// Constructor for a local subscriber
    Subscriber(Registration* reg)
        : reg_(reg), nexthop_(EndpointID::NULL_EID()) {}

    /// Constructor for a remote subscriber
    Subscriber(const EndpointID& nexthop)
        : reg_(NULL), nexthop_(nexthop) {}

    /// Destructor
    virtual ~Subscriber();

    /// Virtual from Formatter
    int format(char* buf, size_t sz) const;

    /// Comparison operator
    bool operator==(const Subscriber& other) const;

    /// @{ Accessors
    bool              is_null()  const;
    bool              is_local() const { return reg_ != NULL; }
    Registration*  reg()         const { ASSERT(is_local()); return reg_; }
    const EndpointID& nexthop()  const { ASSERT(!is_local()); return nexthop_; }
    /// @}

protected:
    Registration* reg_;
    EndpointID    nexthop_;
};

//----------------------------------------------------------------------
inline bool
Subscriber::is_null() const
{
    return (reg_ == NULL && nexthop_ == EndpointID::NULL_EID());
}

/**
 * Type for a vector of subscriber objects.
 */
typedef std::vector<Subscriber> SubscriberList;

} // namespace dtn

#endif /* _SUBSCRIBER_H_ */
