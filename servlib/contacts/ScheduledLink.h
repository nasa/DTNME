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

#ifndef _SCHEDULED_LINK_H_
#define _SCHEDULED_LINK_H_

#include "Link.h"
#include <set>

namespace dtn {

/**
 * Abstraction for a SCHEDULED link.
 *
 * Scheduled links have a list  of future contacts.
 *
*/

class FutureContact;

class ScheduledLink : public Link {
public:
    typedef std::set<FutureContact*> FutureContactSet;

    /**
     * Constructor / Destructor
     */
    ScheduledLink(std::string name, ConvergenceLayer* cl,
                  const char* nexthop);
    
    virtual ~ScheduledLink();
    
    /**
     * Return the list of future contacts that exist on the link
     */
    FutureContactSet* future_contacts() { return fcts_; }

    // Add a future contact
    void add_fc(FutureContact* fc) ;

    // Remove a future contact
    void delete_fc(FutureContact* fc) ;

    // Convert a future contact to an active contact
    void convert_fc(FutureContact* fc) ;

    // Return list of all future contacts
    FutureContactSet* future_contacts_list() { return fcts_; }
    
protected:
    FutureContactSet*  fcts_;
};

/**
 * Abstract base class for FutureContact
 * Relevant only for scheduled links.
 */
class FutureContact {
public:
    /**
     * Constructor / Destructor
     */
    FutureContact()
        : start_(0), duration_(0)
    {
    }
    
    /// Time at which contact starts, 0 value means not defined
    time_t start_;
    
    /// Duration for this future contact, 0 value means not defined
    time_t duration_;
};


} // namespace dtn

#endif /* _SCHEDULED_LINK_H_ */
