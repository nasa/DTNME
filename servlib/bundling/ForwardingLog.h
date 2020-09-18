/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _FORWARDINGLOG_H_
#define _FORWARDINGLOG_H_

#include <vector>

#include <oasys/serialize/SerializableVector.h>
#include "ForwardingInfo.h"
#include "contacts/Link.h"

namespace oasys {
class SpinLock;
class StringBuffer;
}

namespace dtn {

class ForwardingLog;
class Registration;

/**
 * Class to maintain a log of informational records as to where and
 * when a bundle has been forwarded.
 *
 * This state can be (and is) used by the router logic to prevent it
 * from naively forwarding a bundle to the same next hop multiple
 * times. It also is used to store the custody timer spec as supplied
 * by the router so the main forwarding engine knows how to set the
 * appropriate timer.
 *
 * Although a bundle can be sent over multiple links, and may even be
 * sent over the same link multiple times, the forwarding logic
 * assumes that for a given link and bundle, there is only one active
 * transmission. Thus the accessors below always return / update the
 * last entry in the log for a given link.
 */
class ForwardingLog : public oasys::SerializableObject,
                      public oasys::Logger {
public:
    typedef ForwardingInfo::state_t state_t;

    /**
     * Constructor that takes a pointer to the relevant Bundle's lock,
     * used when querying or updating the log.
     */
    ForwardingLog(oasys::SpinLock* lock, Bundle *b);

    /**
     * Get the most recent entry for the given link from the log.
     */
    bool get_latest_entry(const LinkRef& link, ForwardingInfo* info) const;
    
    /**
     * Get the most recent state for the given link from the log.
     */
    state_t get_latest_entry(const LinkRef& link) const;
    
    /**
     * Get the most recent entry for the given registration from the log.
     */
    bool get_latest_entry(const Registration* reg, ForwardingInfo* info) const;
    
    /**
     * Get the most recent state for the given registration from the log.
     */
    state_t get_latest_entry(const Registration* reg) const;
    
    /**
     * Get the most recent entry for the given state from the log.
     */
    bool get_latest_entry(state_t state, ForwardingInfo* info) const;
    
    /**
     * Return the count of matching entries. The states and actions
     * parameters should contain a concatenation of the requested
     * states/actions to filter the count.
     */
    size_t get_count(unsigned int states   = ForwardingInfo::ANY_STATE,
                     unsigned int actions  = ForwardingInfo::ANY_ACTION) const;

    /**
     * Return the count of matching entries for the given remote
     * endpoint id. The states and actions parameters should contain a
     * concatenation of the requested states/actions to filter the
     * count.
     */
    size_t get_count(const EndpointID& eid,
                     unsigned int states   = ForwardingInfo::ANY_STATE,
                     unsigned int actions  = ForwardingInfo::ANY_ACTION) const;
    
    /**
     * Add a new forwarding info entry for the given link.
     */
    void add_entry(const LinkRef&           link,
                   ForwardingInfo::action_t action,
                   state_t                  state,
                   const CustodyTimerSpec&  custody_timer);
    
    /**
     * Add a new forwarding info entry for the given link using the
     * default custody timer info. Used for states other than
     * TRANSMITTED for which the custody timer is irrelevant.
     */
    void add_entry(const LinkRef&           link,
                   ForwardingInfo::action_t action,
                   state_t                  state);
    
    /**
     * Add a new forwarding info entry for the given registration.
     */
    void add_entry(const Registration*      reg,
                   ForwardingInfo::action_t action,
                   state_t                  state);
    
    /**
     * Add a new forwarding info entry for the remote EID without a
     * specific link or registration. (used for session management).
     *
     * Also, if the EID is "*:*" and the state is SUPPRESSED, then the
     * bundle can be prevented from being forwarded to any other
     * nodes.
     */
    void add_entry(const EndpointID&        eid,
                   ForwardingInfo::action_t action,
                   state_t                  state);
    
    /**
     * Update the state for the latest forwarding info entry for the
     * given link.
     *
     * @return true if the next hop entry was found
     */
    bool update(const LinkRef& link, state_t state);

    /**
     * Update the state for the latest forwarding info entry for the
     * given link.
     *
     * @return true if the next hop entry was found
     */
    bool update(Registration* reg, state_t state);

    /**
     * Update all entries in the given state to the new state.
     */
    void update_all(state_t old_state, state_t new_state);
    
    /**
     * Dump a string representation of the log.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Clear the log (used for testing).
     */
    void clear();

    void serialize(oasys::SerializeAction* a);

    /**
     * Typedef for the log itself.
     */
    // typedef std::vector<ForwardingInfo> Log;
    typedef oasys::SerializableVector<ForwardingInfo> Log;

    /*
     * Accessor for the log
     */
    Log log() { return log_; }

    oasys::SpinLock* lock() { return lock_; }

protected:
    oasys::SpinLock* lock_;	///< Copy of the bundle's lock
    Bundle* bundle_;
    Log log_;			///< The actual log
};

} // namespace dtn


#endif /* _FORWARDINGLOG_H_ */
