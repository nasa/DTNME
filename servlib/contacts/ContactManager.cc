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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/StringBuffer.h>

#include "ContactManager.h"
#include "Contact.h"
#include "Link.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "storage/LinkStore.h"
#include "conv_layers/ConvergenceLayer.h"

namespace dtn {

//----------------------------------------------------------------------
ContactManager::ContactManager()
    : BundleEventHandler("ContactManager", "/dtn/contact/manager"),
      opportunistic_cnt_(0),
      shutting_down_(false)
{
    links_ = new LinkSet();
    previous_links_ = new LinkSet();
}

//----------------------------------------------------------------------
ContactManager::~ContactManager()
{
    delete links_;
    delete previous_links_;
}

//----------------------------------------------------------------------
void
ContactManager::set_shutting_down()
{
    oasys::ScopeLock l(&lock_, "ContactManager::set_shutting_down");
    shutting_down_ = true;
}


//----------------------------------------------------------------------
bool
ContactManager::add_new_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::add_new_link");

    if (shutting_down_) {
        return false;
    }

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
	bool reincarnation = false;

    log_debug("adding NEW link %s", link->name());
    if (has_link(link->name())) {
        return false;
    }
    if (!consistent_with_previous_usage(link, &reincarnation))
    {
        log_err("rejecting link %s", link->name());
    	return false;
    }

    links_->insert(LinkRef(link.object(), "ContactManager"));

    if (reincarnation)
    {
    	link->set_reincarnated();
    }

    if (!link->is_create_pending()) {
        BundleDaemon::post(new LinkCreatedEvent(link));
    }

    return true;
}

//----------------------------------------------------------------------
void
ContactManager::add_previous_link(const LinkRef& link)
{
    if (BundleDaemon::instance()->params_.persistent_links_) {
        ASSERT(link != NULL);

        log_debug("adding OLD link %s", link->name());
        previous_links_->insert(LinkRef(link.object(), "From Datastore"));
    }

    return;
}
//----------------------------------------------------------------------
void
ContactManager::del_link(const LinkRef& link, bool wait,
                         ContactEvent::reason_t reason)
{
    oasys::ScopeLock l(&lock_, "ContactManager::del_link");
    ASSERT(link != NULL);

    if (!has_link(link)) {
        log_err("ContactManager::del_link: link %s does not exist",
                link->name());
        return;
    }
    ASSERT(!link->isdeleted());

    log_debug("ContactManager::del_link: deleting link %s", link->name());

    if (!wait) {
        link->delete_link();
        link->cancel_all_bundles();
    }

    // Close the link if it is open or in the process of being opened.
    if (link->isopen() || link->isopening()) {
        BundleDaemon::instance()->post(
            new LinkStateChangeRequest(link, Link::CLOSED, reason));
    }

    // Cancel the link's availability timer (if one exists).
    AvailabilityTimerMap::iterator iter = availability_timers_.find(link);
    if (iter != availability_timers_.end()) {
        LinkAvailabilityTimer* timer = iter->second;
        availability_timers_.erase(link);

        // Attempt to cancel the timer, relying on the timer system to clean
        // up the timer state once it bubbles to the top of the timer queue.
        // If the timer is in the process of firing (i.e., race condition),
        // the timer should clean itself up in the timeout handler.
        if (!timer->cancel()) {
            log_warn("ContactManager::del_link: "
                     "failed to cancel availability timer -- race condition");
        }
    }

    links_->erase(link);
    
    // If link has been used in some forwarding log entry then there should
    // be an entry in previous_links_ in case the user tries to add the same name
    // back with a different specification.  However, if this link was a
    // reincarnation, it will already be (correctly) in previous_links_ and
    // no action is required.
    if (BundleDaemon::instance()->params_.persistent_links_) {
        if (link->used_in_fwdlog() && !link->reincarnated())
        {
    	    previous_links_->insert(link);
        }
    }

    if (wait) {
        l.unlock();
        // If some parent calling del_link already locked the Contact Manager,
        // the lock will remain locked, and an event ahead of the
        // LinkDeletedEvent may wait for the lock, causing deadlock
        ASSERT(!lock()->is_locked_by_me());
        oasys::Notifier notifier("ContactManager::del_link");
        BundleDaemon::post_and_wait(new LinkDeletedEvent(link), &notifier);
        link->delete_link();
    } else {
        BundleDaemon::post(new LinkDeletedEvent(link));
    }
}

//----------------------------------------------------------------------
bool
ContactManager::has_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::has_link");

    if (shutting_down_) {
        return false;
    }
    ASSERT(link != NULL);
    
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

//----------------------------------------------------------------------
bool
ContactManager::has_link(const char* name)
{
    oasys::ScopeLock l(&lock_, "ContactManager::has_link");

    if (shutting_down_) {
        return false;
    }
    ASSERT(name != NULL);
    
    LinkSet::iterator iter;
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        if (strcasecmp((*iter)->name(), name) == 0)
            return true;
    }
    return false;
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_link(const char* name)
{
    oasys::ScopeLock l(&lock_, "ContactManager::find_link");

    LinkSet::iterator iter;
    LinkRef link("ContactManager::find_link: return value");
    
    if (shutting_down_) {
        return link;
    }
    
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        if (strcasecmp((*iter)->name(), name) == 0) {
            link = *iter;
            ASSERT(!link->isdeleted());
            return link;
        }
    }
    return link;
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_previous_link(const char* name)
{
    LinkSet::iterator iter;
    LinkRef link("ContactManager::find_previous_link: return value");

    for (iter = previous_links_->begin(); iter != previous_links_->end(); ++iter) {
        if (strcasecmp((*iter)->name(), name) == 0) {
            link = *iter;
            ASSERT(!link->isdeleted());
            return link;
        }
    }
    return link;
}

//----------------------------------------------------------------------
bool
ContactManager::consistent_with_previous_usage(const LinkRef& link, bool *reincarnation)
{
	/**
	 * This routine checks that the mapping between the link name
	 * and the major parameters of a proposed new link is consistent
	 * with any recorded previous usage of the name.  There are
	 * essentially two cases:
	 * - (Originally) manually configured links with types other than OPPORTUNISTIC:
	 *   The link add command has to specify the nexthop, type and convergence layer
	 *   but the remote_eid will generally not be known until the link is actually
	 *   opened.  In this case for consistency we require name, type, nexthop and
	 *   convergence layer to match and allow the remote_eid to be undefined or
	 *   the same as it was when previously used.
	 * - New OPPORTUNISTIC links: In this case the remote_eid is always known and
	 *   is the interesting parameter.  The link to the remote_eid can be made over
	 *   any convergence layer and the nexthop address can be any value.
	 * If a consistent link is found, the reincarnation parameter is set true to flag
	 * that the new link is indeed a reincarnation of some previously known link with
	 * the same name. This is later used to select whether to add or update the
	 * corresponding persistent store entry.
	 */
    LinkSet::iterator iter;
    const char *name = link->name();
    bool test_name, test_spec;
    ASSERT(reincarnation != NULL);

    log_debug("check link name usage consistency for %s", link->name());
    *reincarnation = false;

    for (iter = previous_links_->begin(); iter != previous_links_->end(); ++iter)
    {
    	log_debug("testing against %s", (*iter)->name());
        test_name = (strcasecmp((*iter)->name(), name) == 0)? true : false;
        test_spec = (((link->remote_eid()	== EndpointID::NULL_EID())      ||
					  (link->remote_eid()	== (*iter)->remote_eid())		  ) &&
					 ((link->type()		== Link::OPPORTUNISTIC)             ||
					  ((strcmp(link->nexthop(), (*iter)->nexthop()) == 0) &&
					  (link->type() 			== (*iter)->type())       &&
					  (strcmp(link->cl_name(), (*iter)->cl_name()) == 0)     )));
        if (test_name)
        {
        	log_debug("Found previous_link with same name");
        	if (test_spec)
        	{
        		// We appear to be consistent
        		// Force remote_eid to be consistent
        		link->set_remote_eid((*iter)->remote_eid());

                        // copy the in_datastore flag
                        link->set_in_datastore((*iter)->in_datastore());

        		// Record that this is a reincarnation
        		*reincarnation = true;

        		return true;
        	} else {
        		return false;
        	}
        } else {
        	if (test_spec)
        	{
        		log_debug("Found previous link with same spec but different name");
        		return false;
        	} else {
        		// neither name nor spec match - continue with other entries
        		continue;
        	}
        }
    }

    // Neither name nor spec recorded as used - so must be consistent
    // because not previously used - and hence not a reincarnation
    return true;
}

//----------------------------------------------------------------------
void
ContactManager::config_links_consistent(void)
{
    oasys::ScopeLock l(&lock_, "ContactManager::find_link");

    LinkSet::iterator iter;
    LinkRef link;
    bool reincarnation;

    for (iter = links_->begin(); iter != links_->end(); ) {
    	// Need to do this to avoid erasing the element the iterator is pointing at
    	link = *iter;
    	++iter;
        if (consistent_with_previous_usage(link, &reincarnation))
        {
        	if (reincarnation)
        	{
        		link->set_reincarnated();
        	}
        }
        else
        {
        	// Deletion is permissible because the link has not yet been completely created
        	// This just undoes what had been done in add_new_link.
        	log_err("Link %s created by configuration file is inconsistent with previous usage - deleting",
        			link->name());
        	link->delete_link();
        	links_->erase(link);
        }
    }
    return;
}

//----------------------------------------------------------------------
void
ContactManager::reincarnate_links(void)
{
	/**
	 * Reincarnate any non-OPPORTUNISTIC links not set up by configuration file
	 */
	log_debug("reincarnate_links");
}

//----------------------------------------------------------------------
const LinkSet*
ContactManager::links()
{
    ASSERTF(lock_.is_locked_by_me(),
            "ContactManager::links must be called while holding lock");
    return links_;
}

//----------------------------------------------------------------------
const LinkSet*
ContactManager::previous_links()
{
    return previous_links_;
}

//----------------------------------------------------------------------
void
ContactManager::LinkAvailabilityTimer::timeout(const struct timeval& now)
{
    (void)now;
    cm_->reopen_link(link_);
    delete this;
}

//----------------------------------------------------------------------
void
ContactManager::reopen_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::reopen_link");

    if (shutting_down_) {
        return;
    }

    ASSERT(link != NULL);

    log_debug("reopen link %s", link->name());

    availability_timers_.erase(link);

    if (!has_link(link)) {
        log_warn("ContactManager::reopen_link: "
                 "link %s does not exist", link->name());
        return;
    }
    ASSERT(!link->isdeleted());
    
    if (link->state() == Link::UNAVAILABLE) {
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::OPEN,
                                       ContactEvent::RECONNECT));
    } else {
        // state race (possibly due to user action)
        log_err("availability timer fired for link %s but state is %s",
                link->name(), Link::state_to_str(link->state()));
    }
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_created(LinkCreatedEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_created");

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_created: "
                "link %s is being deleted", link->name());
        return;
    }
        
    if (!has_link(link)) {
        log_err("ContactManager::handle_link_created: "
                "link %s does not exist", link->name());
        return;
    }

    // Post initial state events; MOVED from Link::create_link().
    link->set_initial_state();
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_available(LinkAvailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_available");

    if (shutting_down_) {
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_available: "
                "link %s is being deleted", link->name());
        return;
    }

    if (!has_link(link)) {
        log_warn("ContactManager::handle_link_available: "
                 "link %s does not exist", link->name());
        return;
    }

    AvailabilityTimerMap::iterator iter;
    iter = availability_timers_.find(link);
    if (iter == availability_timers_.end()) {
        return; // no timer for this link
    }

    LinkAvailabilityTimer* timer = iter->second;
    availability_timers_.erase(link);

    // try to cancel the timer and rely on the timer system to clean
    // it up once it bubbles to the top of the queue... if there's a
    // race and the timer is in the process of firing, it should clean
    // itself up in the timeout handler.
    if (!timer->cancel()) {
        log_warn("ContactManager::handle_link_available: "
                 "can't cancel availability timer: race condition");
    }
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_unavailable(LinkUnavailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_unavailable");

    if (shutting_down_) {
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != NULL);

    if (!has_link(link)) {
        log_warn("ContactManager::handle_link_unavailable: "
                 "link %s does not exist", link->name());
        return;
    }
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_unavailable: "
                "link %s is being deleted", link->name());
        return;
    }

    // don't do anything for links that aren't ondemand or alwayson
    if (link->type() != Link::ONDEMAND && link->type() != Link::ALWAYSON) {
        log_debug("ContactManager::handle_link_unavailable: "
                  "ignoring link unavailable for link of type %s",
                  link->type_str());
        return;
    }
    
    // or if the link wasn't broken but instead was closed by user
    // action or by going idle
    if (event->reason_ == ContactEvent::USER ||
        event->reason_ == ContactEvent::IDLE)
    {
        log_debug("ContactManager::handle_link_unavailable: "
                  "ignoring link unavailable due to %s",
                  event->reason_to_str(event->reason_));
        return;
    }
    
    // adjust the retry interval in the link to handle backoff in case
    // it continuously fails to open, then schedule the timer. note
    // that if this is the first time the link is opened, the
    // retry_interval will be initialized to zero so we set it to the
    // minimum here. the retry interval is reset in the link open
    // event handler.
    if (link->retry_interval_ == 0) {
        link->retry_interval_ = link->params().min_retry_interval_;
    }

    int timeout = link->retry_interval_;
    link->retry_interval_ *= 2;
    if (link->retry_interval_ > link->params().max_retry_interval_) {
        link->retry_interval_ = link->params().max_retry_interval_;
    }

    LinkAvailabilityTimer* timer = new LinkAvailabilityTimer(this, link);

    AvailabilityTimerMap::value_type val(link, timer);
    if (availability_timers_.insert(val).second == false) {
        log_err("ContactManager::handle_link_unavailable: "
                "error inserting timer for link %s into table!", link->name());
        delete timer;
        return;
    }

    // need to release the lock or a deadly embrace can happen if the 
    // TimerSystem is processing a LinkAvailabilityTimer at this time!!
    l.unlock();


    log_debug("link %s unavailable (%s): scheduling retry timer in %d seconds",
              link->name(), event->reason_to_str(event->reason_), timeout);
    timer->schedule_in(timeout * 1000);
}

//----------------------------------------------------------------------
void
ContactManager::handle_contact_up(ContactUpEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_contact_up");

    if (shutting_down_) {
        return;
    }

    LinkRef link = event->contact_->link();
    ASSERT(link != NULL);

    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_contact_up: "
                 "link %s is being deleted, not marking its contact up", link->name());
        return;
    }

    if (!has_link(link)) {
        log_warn("ContactManager::handle_contact_up: "
                 "link %s does not exist", link->name());
        return;
    }

    if (link->type() == Link::ONDEMAND || link->type() == Link::ALWAYSON) {
        log_debug("ContactManager::handle_contact_up: "
                  "resetting retry interval for link %s: %d -> %d",
                  link->name(),
                  link->retry_interval_,
                  link->params().min_retry_interval_);
        link->retry_interval_ = link->params().min_retry_interval_;
    }
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_link_to(ConvergenceLayer* cl,
                             const std::string& nexthop,
                             const EndpointID& remote_eid,
                             Link::link_type_t type,
                             u_int states)
{
	log_debug("find current link using find_any_link_to and links_");
	return find_any_link_to(links_, cl, nexthop, remote_eid, type, states);
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_previous_link_to(ConvergenceLayer* cl,
									  const std::string& nexthop,
									  const EndpointID& remote_eid,
									  Link::link_type_t type,
									  u_int states)
{
	log_debug("find previously existing link using find_any_link_to and previous_links_");
	return find_any_link_to(previous_links_, cl, nexthop, remote_eid, type, states);
}
//----------------------------------------------------------------------
LinkRef
ContactManager::find_any_link_to(LinkSet *link_set,
								 ConvergenceLayer* cl,
								 const std::string& nexthop,
								 const EndpointID& remote_eid,
								 Link::link_type_t type,
								 u_int states)
{
    oasys::ScopeLock l(&lock_, "ContactManager::find_link_to");

    LinkSet::iterator iter;
    LinkRef link("ContactManager::find_link_to: return value");
    
    if (shutting_down_) {
        return link;
    }
    
    log_debug("find_link_to: cl %s nexthop %s remote_eid %s "
              "type %s states 0x%x",
              cl ? cl->name() : "ANY",
              nexthop.c_str(), remote_eid.c_str(),
              type == Link::LINK_INVALID ? "ANY" : Link::link_type_to_str(type),
              states);

    // make sure some sane criteria was specified
    ASSERT((cl != NULL) ||
           (nexthop != "") ||
           (remote_eid != EndpointID::NULL_EID()) ||
           (type != Link::LINK_INVALID));
    
    for (iter = link_set->begin(); iter != link_set->end(); ++iter) {
        if ( ((type == Link::LINK_INVALID) || (type == (*iter)->type())) &&
             ((cl == NULL) || ((*iter)->clayer() == cl)) &&
             ((nexthop == "") || (nexthop == (*iter)->nexthop())) &&
             ((remote_eid == EndpointID::NULL_EID()) ||
              (remote_eid == (*iter)->remote_eid())) &&
             ((states & (*iter)->state()) != 0) )
        {
            link = *iter;
            log_debug("ContactManager::find_link_to: "
                      "matched link *%p", link.object());
            //ASSERT(!link->isdeleted());
            if (link->isdeleted())
            {
                //dz - allow for manually deleting a link
                log_crit("removing manually deleted link from persistent list: *%p", link.object());
                link_set->erase(link);
            }
            else
            {
                return link;
            }
        }
    }

    log_debug("ContactManager::find_any_link_to: no match - link %s NULL",
    		  (link==NULL)? "is" : "is not");
    return link;
}

//----------------------------------------------------------------------
// Opportunistic link names are of form <name>-<decimal number>
// Constrains maximum length for opportunistic link names
#define OPP_NAME_LEN_BUFFER_SZ 64
// Maximum number of digits in opportunistic link name suffix
#define MAX_OPP_LINK_DIGITS 8
LinkRef
ContactManager::new_opportunistic_link(ConvergenceLayer* cl,
                                       const std::string& nexthop,
                                       const EndpointID& remote_eid,
                                       const std::string* link_name)
{
    log_debug("new_opportunistic_link: cl %s nexthop %s remote_eid %s",
              cl->name(), nexthop.c_str(), remote_eid.c_str());
    LinkRef link;


    char name[OPP_NAME_LEN_BUFFER_SZ], name_fmt[OPP_NAME_LEN_BUFFER_SZ];
    

    oasys::ScopeLock l(&lock_, "ContactManager::new_opportunistic_link");
    
    if (shutting_down_) {
        return link;
    }

    // See if an OPPORTUNISTIC link went to this remote_eid during a previous
    // run of the daemon - if so we will use the same name so that the forwarding log
    // will not be made inconsistent.- Scan existing links for one to this remote_eid.
    // Don't worry about either the convergence layer or nexthop string - only the
    // remote_eid and the type are relevant.  The state is irrelevant since we are
    // dealing with information about previous links.
    // xxx/Elwyn: Arguably not even the type should be relevant - if the user changes
    // over from statically configured to OPPORTUNISTIC or vice versa between runs
    // then the routing might send bundles again, but this is a nuisance rather than
    // a big problem.
    link = find_previous_link_to(NULL, "", remote_eid, Link::OPPORTUNISTIC);
    
    if (link != NULL)
    {
    	strcpy(name, link->name());
    }
    else
    {
		// find a unique link name
		if (link_name)
		{
			strncpy(name_fmt, link_name->c_str(), sizeof(name_fmt) - MAX_OPP_LINK_DIGITS - 2);
			strcat(name_fmt, "-%d");
		}
		else
		{
			strcpy(name_fmt, "opplink-%d");
		}

		do {
			snprintf(name, sizeof(name), name_fmt, opportunistic_cnt_);
			opportunistic_cnt_++;
			//dz ASSERT(opportunistic_cnt_ < 1000000);
			if (opportunistic_cnt_ >= 100000000)
                        {
                            opportunistic_cnt_ = 0;
                        }
		} while ((find_link(name) != NULL) || (find_previous_link(name) != NULL));
    }

    link = Link::create_link(name, Link::OPPORTUNISTIC, cl,
                             nexthop.c_str(), 0, NULL);
    if (link == NULL) {
        log_warn("ContactManager::new_opportunistic_link: "
                 "unexpected error creating opportunistic link");
        return link;
    }

    LinkRef new_link(link.object(),
                     "ContactManager::new_opportunistic_link: return value");
    
    new_link->set_remote_eid(remote_eid);

    if (!add_new_link(new_link)) {
        new_link->delete_link();
        log_err("ContactManager::new_opportunistic_link: "
                 "failed to add new opportunistic link %s", new_link->name());
        new_link = NULL;
    }
    
    return new_link;
}
    
//----------------------------------------------------------------------
void
ContactManager::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, "ContactManager::dump");
    
    if (shutting_down_) {
        return;
    }
    
    buf->append("Links:\n");
    LinkSet::iterator iter;
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        buf->appendf("*%p\n", (*iter).object());
    }
}

} // namespace dtn
