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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _CONTACT_MANAGER_H_
#define _CONTACT_MANAGER_H_

#include <oasys/debug/Log.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/StringUtils.h>
#include "bundling/BundleEventHandler.h"

namespace dtn {

class Contact;
class ConvergenceLayer;
class CLInfo;
class Link;
class LinkSet;

/**
 * A contact manager class. Maintains topological information and
 * connectivity state regarding available links and contacts.
 */
class ContactManager : public BundleEventHandler {
public:
    /**
     * Constructor / Destructor
     */
    ContactManager();
    virtual ~ContactManager();
 
    /**
     * Prevent most processing while shutting down
     */   
    virtual void set_shutting_down();
 
    /**
     * Dump a string representation of the info inside contact manager.
     */
    void dump(oasys::StringBuffer* buf) const;
    
    /**********************************************
     *
     * Link set accessor functions
     *
     *********************************************/
    /**
     * Add a link if the contact manager does not already have a link
     * by the same name.
     */
    bool add_new_link(const LinkRef & link);

    /**
     * Record old link information read from datastore. Used to check that link names
     * are consistent between runs of server so that recorded forwarding logs make sense.
     * The 'links' in this set are not active - they are only used to check the information
     * held about the link with prospective new links to ensure consistency. They can
     * all be of the base class variety - no need to worry about using sub-classes.
     */
    void add_previous_link(const LinkRef& link);

    /**
     * Delete a link
     */
    void del_link(const LinkRef& link, bool wait = false,
                  ContactEvent::reason_t reason = ContactEvent::NO_INFO);

    /**
     * Check if contact manager already has this link.
     */
    bool has_link(const LinkRef& link);
    
    /**
     *
     * Check if contact manager already has a link by the same name.
     */
    bool has_link(const char* name);
    
    /**
     * Finds link corresponding to this name
     */
    LinkRef find_link(const char* name);

    /**
     * Finds preeviously existing link corresponding to this name
     */
    LinkRef find_previous_link(const char* name);

    /**
     * Helper routine to find a current link based on criteria:
     *
     * @param cl 	 The convergence layer
     * @param nexthop	 The next hop string
     * @param remote_eid Remote endpoint id (NULL_EID for any)
     * @param type	 Link type (LINK_INVALID for any)
     * @param states	 Bit vector of legal link states, e.g. ~(OPEN | OPENING)
     *
     * @return The link if it matches or NULL if there's no match
     */
    LinkRef find_link_to(ConvergenceLayer* cl,
                         const std::string& nexthop,
                         const EndpointID& remote_eid = EndpointID::NULL_EID(),
                         Link::link_type_t type = Link::LINK_INVALID,
                         u_int states = 0xffffffff);
    
    /**
     * Helper routine to find a link that existed in a prior run based on criteria:
     *
     * @param cl 	 The convergence layer
     * @param nexthop	 The next hop string
     * @param remote_eid Remote endpoint id (NULL_EID for any)
     * @param type	 Link type (LINK_INVALID for any)
     * @param states	 Bit vector of legal link states, e.g. ~(OPEN | OPENING)
     *
     * @return The link if it matches or NULL if there's no match
     */
    LinkRef find_previous_link_to(ConvergenceLayer* cl,
								  const std::string& nexthop,
								  const EndpointID& remote_eid = EndpointID::NULL_EID(),
								  Link::link_type_t type = Link::LINK_INVALID,
								  u_int states = 0xffffffff);

    /**
     * Routine to check if usage of a newly created link name is consistent
     * with its previous usage so that forwarding log entries remain meaningful.
     */
    bool consistent_with_previous_usage(const LinkRef& link, bool *reincarnation);

    /**
     * Routine to check if links created by configuration file are consistent
     * with previous usage so that forwarding log entries remain meaningful.
     * This can't be done immediately on creation because the persistent storage
     * hasn't been initialized.  However the link creation hasn't been completed
     * because the event loop is not yet running.  This catches any inconsistencies
     * once previous_links_ has been loaded during BundleDaemon::run startup.
     */
    void config_links_consistent(void);

    /**
     * Reincarnate any non-OPPORTUNISTIC links not set up by configuration file
     */
    void reincarnate_links(void);

    /**
     * Return the list of links. Asserts that the CM spin lock is held
     * by the caller.
     */
    const LinkSet* links();

    /**
     * Return the reloaded list of links reloaded from previous runs.
     * Lock is not essential here.
     */
    const LinkSet* previous_links();

    /**
     * Accessor for the ContactManager internal lock.
     */
    oasys::Lock* lock() { return &lock_; }

    /**********************************************
     *
     * Event handler routines
     *
     *********************************************/
    /**
     * Generic event handler.
     */
    void handle_event(BundleEvent* event)
    {
        dispatch_event(event);
    }
    
    /**
     * Event handler when a link has been created.
     */
    void handle_link_created(LinkCreatedEvent* event);
    
    /**
     * Event handler when a link becomes unavailable.
     */
    void handle_link_available(LinkAvailableEvent* event);
    
    /**
     * Event handler when a link becomes unavailable.
     */
    void handle_link_unavailable(LinkUnavailableEvent* event);

    /**
     * Event handler when a link is opened successfully
     */
    void handle_contact_up(ContactUpEvent* event);

    /**********************************************
     *
     * Opportunistic contact routines
     *
     *********************************************/
    /**
     * Notification from a convergence layer that a new opportunistic
     * link has come knocking.
     *
     * @return An idle link to represent the new contact
     */
    LinkRef new_opportunistic_link(ConvergenceLayer* cl,
                                   const std::string& nexthop,
                                   const EndpointID& remote_eid,
                                   const std::string* link_name = NULL);
    
protected:
    
    /**
     * Helper routine to find a link based on criteria:
     *
     * @param link_set links_ or previous_links_
     * @param cl 	 The convergence layer
     * @param nexthop	 The next hop string
     * @param remote_eid Remote endpoint id (NULL_EID for any)
     * @param type	 Link type (LINK_INVALID for any)
     * @param states	 Bit vector of legal link states, e.g. ~(OPEN | OPENING)
     *
     * @return The link if it matches or NULL if there's no match
     */
    LinkRef find_any_link_to(LinkSet * link_set,
							 ConvergenceLayer* cl,
							 const std::string& nexthop,
							 const EndpointID& remote_eid = EndpointID::NULL_EID(),
							 Link::link_type_t type = Link::LINK_INVALID,
							 u_int states = 0xffffffff);

    LinkSet* links_;			///< Set of all links
    int opportunistic_cnt_;		///< Counter for opportunistic links
    LinkSet* previous_links_;	///< Set of links used in prior runs of daemon

    /**
     * Reopen a broken link.
     */
    void reopen_link(const LinkRef& link);

    /**
     * Timer class used to re-enable broken ondemand links.
     */
    class LinkAvailabilityTimer : public oasys::Timer {
    public:
        LinkAvailabilityTimer(ContactManager* cm, const LinkRef& link)
            : cm_(cm), link_(link.object(), "LinkAvailabilityTimer") {}
        
        virtual void timeout(const struct timeval& now);

        ContactManager* cm_;	///< The contact manager object
        LinkRef link_;		///< The link in question
    };
    friend class LinkAvailabilityTimer;

    /**
     * Table storing link -> availability timer class.
     */
    typedef std::map<LinkRef, LinkAvailabilityTimer*> AvailabilityTimerMap;
    AvailabilityTimerMap availability_timers_;

    /**
     * Lock to protect internal data structures.
     */
    mutable oasys::SpinLock lock_;

    /// Flag indicating dtnd is shutting down
    bool shutting_down_;

    friend class LinkCommand;

};


} // namespace dtn

#endif /* _CONTACT_MANAGER_H_ */
