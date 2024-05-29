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

#include "TableBasedRouter.h"
#include "RouteTable.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/IMCScheme.h"
#include "reg/Registration.h"

namespace dtn {

void table_based_router_shutdown(void*)
{
    BundleDaemon::instance()->router()->shutdown();
}

//----------------------------------------------------------------------
TableBasedRouter::TableBasedRouter(const char* classname,
                                   const std::string& name)
    : BundleRouter(classname, name)
{
    sptr_route_table_ = std::make_shared<RouteTable>(name);

    sptr_imc_router_ = std::make_shared< IMCRouter>("IMCRouter", "imcrouter", sptr_route_table_);
    sptr_imc_router_->start();

    // register the global shutdown function
    BundleDaemon::instance()->set_rtr_shutdown(
            table_based_router_shutdown, (void *) 0);

}

//----------------------------------------------------------------------
TableBasedRouter::~TableBasedRouter()
{
}

//----------------------------------------------------------------------
void TableBasedRouter::shutdown()
{
    sptr_imc_router_->shutdown();

    BundleRouter::shutdown();
}

//----------------------------------------------------------------------
void
TableBasedRouter::add_route(RouteEntry *entry, bool skip_changed_routes)
{
    sptr_route_table_->add_entry(entry);
    if (!skip_changed_routes) {
        handle_changed_routes();
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::del_route(const SPtr_EIDPattern& sptr_dest)
{
    sptr_route_table_->del_entries(sptr_dest);

    // XXX/demmer this should really call handle_changed_routes...
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_changed_routes()
{
    reroute_all_bundles();
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_received(SPtr_BundleEvent& sptr_event)
{
    BundleReceivedEvent* event = nullptr;
    event = dynamic_cast<BundleReceivedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleReceivedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (BundleDaemon::instance()->shutting_down()) return;

    if (event->local_delivery_ && event->singleton_dest_) return;

    Bundle* bundle = event->bundleref_.object();

    route_bundle(bundle);
} 

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_transmitted(SPtr_BundleEvent& sptr_event)
{
    BundleTransmittedEvent* event = nullptr;
    event = dynamic_cast<BundleTransmittedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleTransmittedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (BundleDaemon::instance()->shutting_down()) return;

    const BundleRef& bundle = event->bundleref_;
    //log_debug("handle bundle transmitted (%s): *%p", 
    //          event->success_?"success":"failure", bundle.object());

    if (!event->success_) {
        // try again if not successful the first time 
        // (only applicable to the LTPUDP CLA as of 2014-12-04)
        route_bundle(bundle.object());
    }

    // check if the transmission means that we can send another bundle
    // on the link
//dzdebug    const LinkRef& link = event->contact_->link();
//dzdebug    check_next_hop(link);
}

//----------------------------------------------------------------------
bool
TableBasedRouter::can_delete_bundle(const BundleRef& bundle)
{
    if (IMCRouter::imc_config_.imc_router_enabled_ &&
        bundle->dest()->is_imc_scheme())
    {
        return sptr_imc_router_->can_delete_imc_bundle(bundle);
    }

    // check if we still have local custody
    if (bundle->local_custody() || bundle->bibe_custody()) {
        return false;
    }

    // check if we haven't yet done anything with this bundle
    if (0 == bundle->fwdlog()->get_count(ForwardingInfo::TRANSMITTED |
                                         ForwardingInfo::DELIVERED))
    {
        return false;
    }

    return true;
}
    
//----------------------------------------------------------------------
void
TableBasedRouter::delete_bundle(const BundleRef& bundle)
{
    (void) bundle;
    //log_debug("delete *%p", bundle.object());
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_cancelled(SPtr_BundleEvent& sptr_event)
{
    BundleSendCancelledEvent* event = nullptr;
    event = dynamic_cast<BundleSendCancelledEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a BundleSendCancelledEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (BundleDaemon::instance()->shutting_down()) return;

    Bundle* bundle = event->bundleref_.object();
    //log_debug("handle bundle cancelled: *%p", bundle);

    // if the bundle has expired, we don't want to reroute it.
    // XXX/demmer this might warrant a more general handling instead?
    if (!bundle->expired()) {
        route_bundle(bundle);
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_route_add(SPtr_BundleEvent& sptr_event)
{
    RouteAddEvent* event = nullptr;
    event = dynamic_cast<RouteAddEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RouteAddEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    add_route(event->entry_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_route_del(SPtr_BundleEvent& sptr_event)
{
    RouteDelEvent* event = nullptr;
    event = dynamic_cast<RouteDelEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RouteDelEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    del_route(event->sptr_dest_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::add_nexthop_route(const LinkRef& link, bool skip_changed_routes)
{
    // If we're configured to do so, create a route entry for the eid
    // specified by the link when it connected, using the
    // scheme-specific code to transform the URI to wildcard
    // the service part
    SPtr_EID sptr_eid = link->remote_eid();
    if (config_.add_nexthop_routes_ && sptr_eid != BD_NULL_EID())
    { 
        SPtr_EIDPattern sptr_pattern = BD_APPEND_PATTERN_WILD(link->remote_eid());

        // XXX/demmer this shouldn't call get_matching but instead
        // there should be a RouteTable::lookup or contains() method
        // to find the entry
        RouteEntryVec ignored;
        if (sptr_route_table_->get_matching(sptr_pattern, link, &ignored) == 0) {
            RouteEntry *entry = new RouteEntry(sptr_pattern, link);
            entry->set_action(ForwardingInfo::FORWARD_ACTION);
            add_route(entry, skip_changed_routes);
        }
    }
}

//----------------------------------------------------------------------
bool
TableBasedRouter::should_fwd(const Bundle* bundle, RouteEntry* route)
{
    if (route == nullptr)
        return false;

    //use prevhop in bundle to prevent sending back to original sender for now
    if ( (bundle->prevhop() == route->link()->remote_eid()) &&
        (bundle->prevhop() != BD_NULL_EID()) ) {
        log_debug("should_fwd bundle %" PRIbid ": "
                  "skip %s since bundle arrived from the same node",
                  bundle->bundleid(), route->link()->name());
        return false;
    }

    return BundleRouter::should_fwd(bundle, route->link(), route->action());
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_contact_up(SPtr_BundleEvent& sptr_event)
{
    ContactUpEvent* event = nullptr;
    event = dynamic_cast<ContactUpEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ContactUpEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // Pass this event through to the IMCRouter
    if (IMCRouter::imc_config_.imc_router_enabled_) {
        sptr_imc_router_->post(sptr_event);
    }


    LinkRef link = event->contact_->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());

    if (! link->isopen()) {
        log_err("contact up(*%p): event delivered but link not open",
                link.object());
    }

    add_nexthop_route(link);
//dzdebug    check_next_hop(link);

    // check if there's a pending reroute timer on the link, and if
    // so, cancel it.
    // 
    // note that there's a possibility that a link just bounces
    // between up and down states but can't ever really send a bundle
    // (or part of one), which we don't handle here since we can't
    // distinguish that case from one in which the CL is actually
    // sending data, just taking a long time to do so.
    oasys::SPtr_Timer base_sptr;
    SPtr_RerouteTimer t;

    RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
    if (iter != reroute_timers_.end()) {
        base_sptr = iter->second;
        reroute_timers_.erase(iter);
        oasys::SharedTimerSystem::instance()->cancel_timer(base_sptr);
        base_sptr.reset();
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_contact_down(SPtr_BundleEvent& sptr_event)
{
    ContactDownEvent* event = nullptr;
    event = dynamic_cast<ContactDownEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ContactDownEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // Pass this event through to the IMCRouter
    if (IMCRouter::imc_config_.imc_router_enabled_) {
        sptr_imc_router_->post(sptr_event);
    }

    LinkRef link = event->contact_->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());

    // if there are any bundles queued on the link when it goes down,
    // schedule a timer to cancel those transmissions and reroute the
    // bundles in case the link takes too long to come back up

    size_t num_queued = link->queue()->size();
    if (num_queued != 0) {
        RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
        if (iter == reroute_timers_.end()) {
            log_debug("link %s went down with %zu bundles queued, "
                      "scheduling reroute timer in %u seconds",
                      link->name(), num_queued,
                      link->params().potential_downtime_);

            SPtr_RerouteTimer t = std::make_shared<RerouteTimer>(this, link);
            oasys::SPtr_Timer base_sptr = t;
            oasys::SharedTimerSystem::instance()->schedule_in(link->params().potential_downtime_ * 1000, base_sptr);
            
            reroute_timers_[link->name_str()] = t;
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::reroute_bundles(const LinkRef& link)
{
    ASSERT(!link->isdeleted());

    // if the reroute timer fires, the link should be down and there
    // should be at least one bundle queued on it.
    if (link->state() != Link::UNAVAILABLE) {
        log_warn("reroute timer fired but link *%p state is %s, not UNAVAILABLE",
                 link.object(), Link::state_to_str(link->state()));
        return;
    }
    
    log_debug("reroute timer fired -- cancelling %zu bundles on link *%p",
              link->queue()->size(), link.object());
    
    // cancel the queued transmissions and rely on the
    // BundleSendCancelledEvent handler to actually reroute the
    // bundles, being careful when iterating through the lists to
    // avoid STL memory clobbering since cancel_bundle removes from
    // the list
    oasys::ScopeLock l(link->queue()->lock(),
                       "TableBasedRouter::reroute_bundles");
    BundleRef bundle("TableBasedRouter::reroute_bundles");
    while (! link->queue()->empty()) {
        bundle = link->queue()->front();
        actions_->cancel_bundle(bundle.object(), link);

        //XXX/dz TCPCL can't guarantee that bundle can be immediately removed from the queue
        //ASSERT(! bundle->is_queued_on(link->queue()));
    }

    // there should never have been any in flight since the link is
    // unavailable
    //dz - ION3.6.2 killm and restart did not clear inflight so rerouting them also
    //ASSERT(link->inflight()->empty());

    while (! link->inflight()->empty()) {
        bundle = link->inflight()->front();
        actions_->cancel_bundle(bundle.object(), link);

        //XXX/dz TCPCL can't guarantee that bundle can be immediately removed from the inflight queue
        //ASSERT(! bundle->is_queued_on(link->inflight()));
    }

}    

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_available(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_created(SPtr_BundleEvent& sptr_event)
{
    LinkCreatedEvent* event = nullptr;
    event = dynamic_cast<LinkCreatedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkCreatedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());

    // true=skip changed routes because we are about to call it
    add_nexthop_route(link, true); 
    handle_changed_routes();
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_deleted(SPtr_BundleEvent& sptr_event)
{
    LinkDeletedEvent* event = nullptr;
    event = dynamic_cast<LinkDeletedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a LinkDeletedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    LinkRef link = event->link_;
    ASSERT(link != nullptr);

    sptr_route_table_->del_entries_for_nexthop(link);

    RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
    if (iter != reroute_timers_.end()) {
        log_debug("link %s deleted, cancelling reroute timer", link->name());
        oasys::SPtr_Timer base_sptr = iter->second;
        reroute_timers_.erase(iter);
        oasys::SharedTimerSystem::instance()->cancel_timer(base_sptr);
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_custody_timeout(SPtr_BundleEvent& sptr_event)
{
    CustodyTimeoutEvent* event = nullptr;
    event = dynamic_cast<CustodyTimeoutEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a CustodyTimeoutEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // the bundle daemon should have recorded a new entry in the
    // forwarding log for the given link to note that custody transfer
    // timed out, and of course the bundle should still be in the
    // pending list.
    //
    // therefore, trying again to forward the bundle should match
    // either the previous link or any other route

    
    // Check to see if the bundle is still pending
    BundleRef bref("handle_custody_timerout");
    bref = pending_bundles_->find(event->bundle_id_);
    
    if (bref != nullptr) {
        route_bundle(bref.object());
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::get_routing_state(oasys::StringBuffer* buf)
{
    if (IMCRouter::imc_config_.imc_router_enabled_) {
        IMCRouter::imc_config_.imc_region_dump(0, *buf);
        IMCRouter::imc_config_.imc_group_dump(0, *buf);
    }

    buf->appendf("Route table for %s router:\n\n", name_.c_str());
    sptr_route_table_->dump(buf);
}

//----------------------------------------------------------------------
void
TableBasedRouter::tcl_dump_state(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(sptr_route_table_->lock(),
                       "TableBasedRouter::tcl_dump_state");

    RouteEntryVec::const_iterator iter;
    for (iter = sptr_route_table_->route_table()->begin();
         iter != sptr_route_table_->route_table()->end(); ++iter)
    {
        const RouteEntry* e = *iter;
        buf->appendf(" {%s %s source_eid %s priority %d} ",
                     e->dest_pattern()->c_str(),
                     e->next_hop_str().c_str(),
                     e->source_pattern()->c_str(),
                     e->priority());
    }
}

//----------------------------------------------------------------------
int
TableBasedRouter::route_bundle(Bundle* bundle, bool skip_check_next_hop)
{
    (void) skip_check_next_hop;

    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    // check to see if forwarding is suppressed to all nodes
    if (bundle->fwdlog()->get_count(BD_WILD_PATTERN(),
                                    ForwardingInfo::SUPPRESSED) > 0)
    {
        log_info("route_bundle: "
                 "ignoring bundle %" PRIbid " since forwarding is suppressed",
                 bundle->bundleid());
        return 0;
    }

    // Pass this bundle through to the IMCRouter?
    if (IMCRouter::imc_config_.imc_router_enabled_) {
        //dzdebug  if (bundle->is_bpv7() && bundle->dest().is_imc_scheme()) {
        if (bundle->dest()->is_imc_scheme()) {
            sptr_imc_router_->post(bundle);
            return 1;
        }
    }

    LinkRef null_link("TableBasedRouter::route_bundle");
    sptr_route_table_->get_matching(bundle->dest(), null_link, &matches);

    // sort the matching routes by priority, allowing subclasses to
    // override the way in which the sorting occurs
    sort_routes(bundle, &matches);

    //log_debug("route_bundle bundle id %" PRIbid ": checking %zu route entry matches",
    //          bundle->bundleid(), matches.size());

    unsigned int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        RouteEntry* route = *iter;

        //log_debug("checking route entry %p link %s (%p) [remote EID: %s]",
        //          *iter, route->link()->name(), route->link().object(), 
        //          route->link()->remote_eid()->c_str());

        if (! should_fwd(bundle, *iter)) {
            continue;
        }

        const LinkRef& link = route->link();

        // if the link is available and not open, open it
        if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
            log_debug("opening *%p because a message is intended for it",
                      link.object());
            actions_->open_link(link);
        }

        if (!link->isopen()) {
            continue;
        }
    
        // if the link queue doesn't have space (based on the low water
        // mark) don't do anything
        if (!link->queue_has_space()) {
            //log_debug("route_bundle: %s -> %s: no space in queue...",
            //          link->name(), link->nexthop());
            continue;
        }

        // if the link is available and not open, open it
        if (link->isavailable() &&
            (!link->isopen()) && (!link->isopening()))
        {
            log_debug("route_bundle: "
                      "opening *%p because a message is intended for it",
                      link.object());
            actions_->open_link(link);
        }

        ++count;

        //log_debug("route_bundle: sending *%p to *%p",
        //          bundle, link.object());

        actions_->queue_bundle(bundle , link,
                               route->action(), route->custody_spec());

        // only send it through the primary path if so configured and one exists 
        if (BundleRouter::config_.static_router_prefer_always_on_ &&
            (route->link()->type() == Link::ALWAYSON)) {
            break;
        }
    }

    // flag both BPv6 and BPv7 IMC bundles as processed if an open link was found
    if (bundle->dest()->is_imc_scheme()) {
        if (count > 0) {
            bundle->set_router_processed_imc();
        }
    }

    //log_debug("route_bundle bundle id %" PRIbid ": forwarded on %u links",
    //          bundle->bundleid(), count);
    return count;
}


//----------------------------------------------------------------------
void
TableBasedRouter::sort_routes(Bundle* bundle, RouteEntryVec* routes)
{
    (void)bundle;
    std::sort(routes->begin(), routes->end(), RoutePrioritySort());
}

//----------------------------------------------------------------------
void
TableBasedRouter::check_next_hop(const LinkRef& next_hop)
{
    (void) next_hop;
}

//----------------------------------------------------------------------
void
TableBasedRouter::reroute_all_bundles()
{
    // XXX/dz Instead of locking the pending_bundles_ during this whole
    // process, the lock is now only taken when the iterator is being
    // updated.
 
    log_debug("reroute_all_bundles... %zu bundles on pending list",
              pending_bundles_->size());

    // XXX/demmer this should cancel previous scheduled transmissions
    // if any decisions have changed

    pending_bundles_->lock()->lock("TableBasedRouter::reroute_all_bundles");
    pending_bundles_t::iterator iter = pending_bundles_->begin();
    if ( iter == pending_bundles_->end() ) {
      pending_bundles_->lock()->unlock();
      return;
    }
    pending_bundles_->lock()->unlock();

    bool skip_check_next_hop = false;
    while (true)
    {
        route_bundle(iter->second, skip_check_next_hop); // for <map> lists

        // only do the check_next_hop for the first bundle which 
        // primes the pump for all of the other bundles
        if (!skip_check_next_hop) {
            skip_check_next_hop = true;
        }

        pending_bundles_->lock()->lock("TableBasedRouter::reroute_all_bundles");
        ++iter;
        if ( iter == pending_bundles_->end() ) {
          pending_bundles_->lock()->unlock();
          break;
        }
        pending_bundles_->lock()->unlock();
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::recompute_routes()
{
    reroute_all_bundles();
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_registration_added(SPtr_BundleEvent& sptr_event)
{
    RegistrationAddedEvent* event = nullptr;
    event = dynamic_cast<RegistrationAddedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationAddedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    SPtr_Registration sptr_reg = event->sptr_reg_;
    
    if (sptr_reg == nullptr) {
        return;
    }

    if (sptr_reg->endpoint()->is_imc_scheme()) {
        // Pass this event through to the IMCRouter?
        if (IMCRouter::imc_config_.imc_router_enabled_) {
            if (sptr_reg->endpoint()->is_imc_scheme()) {
                sptr_imc_router_->post(sptr_event);
            }
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_registration_removed(SPtr_BundleEvent& sptr_event)
{
    RegistrationRemovedEvent* event = nullptr;
    event = dynamic_cast<RegistrationRemovedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationRemovedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    SPtr_Registration sptr_reg = event->sptr_reg_;
    
    if (sptr_reg == nullptr || sptr_reg->session_flags() == 0) {
        return;
    }

    if (sptr_reg->endpoint()->is_imc_scheme()) {
        // Pass this event through to the IMCRouter?
        if (IMCRouter::imc_config_.imc_router_enabled_) {
            if (sptr_reg->endpoint()->is_imc_scheme()) {
                sptr_imc_router_->post(sptr_event);
            }
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_registration_expired(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
}

//----------------------------------------------------------------------
TableBasedRouter::RerouteTimer::RerouteTimer(TableBasedRouter* router, const LinkRef& link)
            : router_(router), link_(link)
{
}

//----------------------------------------------------------------------
TableBasedRouter::RerouteTimer::~RerouteTimer()
{
}

//----------------------------------------------------------------------
void
TableBasedRouter::RerouteTimer::timeout(const struct timeval& now)
{
    (void) now;
    router_->reroute_bundles(link_);
}


} // namespace dtn
