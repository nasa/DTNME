/*
 *    Copyright 2022 United States Government as represented by NASA
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

#include "IMCRouter.h"
#include "RouteTable.h"
#include "bundling/BundleDaemon.h"
#include "bundling/CustodyTimer.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/IPNScheme.h"

#define CHECK_CBOR_ENCODE_ERR_RETURN \
    if (err && (err != CborErrorOutOfMemory)) \
    { \
      return CBORUTIL_FAIL; \
    }

namespace dtn {

IMCConfig IMCRouter::imc_config_;



//----------------------------------------------------------------------
IMCRouter::IMCRouter(const char* classname,
                     const std::string& name,
                     SPtr_RouteTable& sptr_route_table)
    : BundleRouter(classname, name),
      sptr_route_table_(sptr_route_table)
{
    if (sptr_route_table == nullptr) {
        manage_route_table_ = true;
        sptr_route_table_ = std::make_shared<RouteTable>(name);
    }
}

//----------------------------------------------------------------------
IMCRouter::~IMCRouter()
{
}

//----------------------------------------------------------------------
void IMCRouter::shutdown()
{
    //XXX/TODO clear the eventq?
    set_should_stop();

    while (!is_stopped()) {
        usleep(100);
    }

}

//----------------------------------------------------------------------
void
IMCRouter::post(Bundle* bundle)
{
    if (!bundle->dest()->is_imc_scheme()) {
        log_err("Attempt to post incorrect bundle type to IMCRouter: *%p", bundle);
        return;
    }

    BundleEvent* event = new RouteIMCBundleEvent(bundle);
    SPtr_BundleEvent sptr_event(event);

    me_eventq_.push(sptr_event, true);
}

//----------------------------------------------------------------------
void
IMCRouter::post(SPtr_BundleEvent& sptr_event)
{
    me_eventq_.push(sptr_event, true);
}

//----------------------------------------------------------------------
void
IMCRouter::run()
{
    // flag BundleRouter base class to start posting events rather than processing them
    thread_mode_ = true;

    char threadname[16] = "IMCRouter";
    pthread_setname_np(pthread_self(), threadname);

    log_always("IMCRouter starting");

    imc_config_.load_imc_config_overrides();
    imc_config_.imc_router_active_ = true;

    if (imc_config_.imc_router_node_designation()) {
        if (imc_config_.imc_sync_on_startup_) {
            // group zero triggers nodes to send IMC briefings
            issue_imc_briefing_request_to_all_nodes_in_region();
        }
    }


    imc_config_.issue_startup_imc_group_petitions();


    SPtr_BundleEvent sptr_event;

    while (1) {
        if (should_stop()) {
            break;
        }

        if (imc_config_.imc_do_manual_sync_) {
            if (imc_config_.imc_manual_sync_node_num_ == 0) {
                issue_imc_briefing_request_to_all_nodes_in_region();
                log_always("IMC Sync manually initiated to all nodes in region");
            } else {
                issue_imc_briefing_request_to_single_node(imc_config_.imc_manual_sync_node_num_);
                log_always("IMC Sync manually initiated to node number: %zu", imc_config_.imc_manual_sync_node_num_);
            }

            imc_config_.imc_do_manual_sync_ = false;
            imc_config_.imc_manual_sync_node_num_ = 0;
            log_always("IMC Sync manually initiated");
        }

        if (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);
            
            // handle the event
            handle_event(sptr_event);

            sptr_event.reset();
        } else {
            me_eventq_.wait_for_millisecs(100); // max millisecs to wait
        }
    }

    thread_mode_ = false;
    log_always("IMCRouter exiting");
}


//----------------------------------------------------------------------
void
IMCRouter::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
}

//----------------------------------------------------------------------
void
IMCRouter::handle_route_imc_bundle(SPtr_BundleEvent& sptr_event)
{
    RouteIMCBundleEvent* event = nullptr;
    event = dynamic_cast<RouteIMCBundleEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RouteIMCBundleEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    if (BundleDaemon::instance()->shutting_down()) return;

    Bundle* bundle = event->bref_.object();

    route_bundle(bundle);
} 

//----------------------------------------------------------------------
bool
IMCRouter::can_delete_imc_bundle(const BundleRef& bundle)
{
    ASSERT(bundle->dest()->is_imc_scheme());
    
    // check if the IMC Router has processed the bundle yet
    if (!bundle->router_processed_imc()) {
        return false;
    }

    // check if we still have local custody
    if (bundle->local_custody() || bundle->bibe_custody()) {
        return false;
    }

    // check if still queued for delivery
    if (0 != bundle->fwdlog()->get_count(ForwardingInfo::PENDING_DELIVERY)) {
        return false;
    }

    // check for not delivered but manually joined
    if (0 == bundle->fwdlog()->get_count(ForwardingInfo::DELIVERED)) {
        if (imc_config_.is_manually_joined_eid(bundle->dest())) {
            return false;
        }
    }


    // check if still trying to send to known nodes
    size_t total_unrouteable_nodes = bundle->num_imc_home_region_unrouteable() +
                                     bundle->num_imc_outer_regions_unrouteable();
    if (bundle->num_imc_nodes_not_handled() > total_unrouteable_nodes)
    {
        return false;
    }
    
    // check if need to keep bundle for currently unrouteable nodes
    if (!imc_config_.delete_unrouteable_bundles_) {
        if (bundle->num_imc_nodes_not_handled() > 0)
        {
            return false;
        }
    }

    return true;
}
    
//----------------------------------------------------------------------
bool
IMCRouter::should_fwd_imc(const Bundle* bundle, RouteEntry* route, bool& is_queued)
{
    is_queued = false;

    if (route == nullptr)
        return false;

    //use prevhop in bundle to prevent sending back to original sender for now
    if ((bundle->prevhop() == route->link()->remote_eid()) &&
        (bundle->prevhop() != BD_NULL_EID())) {
//        log_debug("should_fwd_imc bundle %" PRIbid ": "
//                  "skip %s since bundle arrived from the same node",
//                  bundle->bundleid(), route->link()->name());
        return false;
    }

    return BundleRouter::should_fwd_imc(bundle, route->link(), route->action(), is_queued);
}

//----------------------------------------------------------------------
bool
IMCRouter::should_fwd(const Bundle* bundle, RouteEntry* route)
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
IMCRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Route table for %s router:\n\n", name_.c_str());
    sptr_route_table_->dump(buf);
}

//----------------------------------------------------------------------
void
IMCRouter::tcl_dump_state(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(sptr_route_table_->lock(),
                       "IMCRouter::tcl_dump_state");

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
IMCRouter::route_bundle(Bundle* bundle)
{
    int count = 0;

    // check to see if forwarding is suppressed to all nodes
    if (bundle->fwdlog()->get_count(BD_WILD_PATTERN(),
                                    ForwardingInfo::SUPPRESSED) > 0)
    {
        log_info("route_bundle: "
                 "ignoring bundle %" PRIbid " since forwarding is suppressed",
                 bundle->bundleid());
        return 0;
    }

    
    if (!imc_config_.imc_router_node_designation()) {
        // if not a router node then do limited processing
        return route_bundle_as_non_router_node(bundle);
    }

    if (bundle->router_processed_imc()) {
        // just try to re-route bundle to nodes not handled yet
    } else {
        // If home region # has not been processed then add dest nodes
        imc_config_.update_bundle_imc_dest_nodes(bundle);

        // flag IMC bundles as processed
        bundle->set_router_processed_imc();
    }


    if (bundle->num_imc_nodes_not_handled() == 0) {
        // no need to actually transmit this bundle 
        // - issue a dummy transmitted event to see if it can be deleted
        BundleTryDeleteRequest* req_to_post;
        req_to_post = new BundleTryDeleteRequest(bundle);
        SPtr_BundleEvent sptr_req_to_post(req_to_post);
        BundleDaemon::post(sptr_req_to_post);


    } else {
        generate_dest_sublists_per_link(bundle);

        count = queue_bundle_to_links(bundle);
    }

    //log_debug("route_bundle bundle id %" PRIbid ": forwarded on %u links",
    //          bundle->bundleid(), count);
    return count;
}


//----------------------------------------------------------------------
void
IMCRouter::generate_dest_sublists_per_link(Bundle* bundle)
{
    bundle->clear_imc_unrouteable_dest_nodes();

    oasys::ScopeLock scoplok(bundle->lock(), __func__);

    // loop through the destination nodes to organize them by
    // which link to send them via
    SPtr_IMC_DESTINATIONS_MAP sptr_imc_dest_map = bundle->imc_dest_map();

    if (!sptr_imc_dest_map) {
        // no map to work with
        return;
    }

    auto iter_dest_map = sptr_imc_dest_map->begin();

    while (iter_dest_map != sptr_imc_dest_map->end()) {
        if (iter_dest_map->second == IMC_DEST_NODE_NOT_HANDLED) {
            uint64_t dest_node = iter_dest_map->first;

            RouteEntryVec matches;

            LinkRef null_link("IMCRouter::route_bundle");
            sptr_route_table_->get_matching_ipn(dest_node, null_link, &matches);

            // sort the matching routes by priority, allowing subclasses to
            // override the way in which the sorting occurs
            sort_routes(bundle, &matches);

            //log_debug("route_bundle bundle id %" PRIbid ": checking %zu route entry matches",
            //          bundle->bundleid(), matches.size());


            bool has_known_link = false;
            std::string known_link_name;
            bool is_queued = false;


            size_t count = 0;
            for (auto iter_matches = matches.begin(); iter_matches != matches.end(); ++iter_matches)
            {
                RouteEntry* route = *iter_matches;

                //log_debug("dzdebug - IMC dest node: %zu - *%p - checking route entry %p link %s (%p) [remote EID: %s]",
                //          dest_node, bundle, *iter_matches, route->link()->name(), route->link().object(), 
                //          route->link()->remote_eid()->c_str());

                if (! should_fwd_imc(bundle, route, is_queued)) {
                    continue;
                }

                const LinkRef& link = route->link();

                // save ALWAYSON route in case no open link is found
                if (!has_known_link) {
                    has_known_link = true;
                    known_link_name = link->name_str();
                }

                // if the link is available and not open, open it
                if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
                    log_debug("opening *%p because a message is intended for it",
                              link.object());
                    actions_->open_link(link);
                }

                if (!link->isopen()) {
                    log_debug("dzdebug - IMC dest node: %zu - *%p - continue because link is not open: %s",
                              dest_node, bundle, link->name_str().c_str());

                    continue;
                }

                // if the link queue doesn't have space (based on the low water
                // mark) don't do anything
                if (!link->queue_has_space()) {
                    continue;
                }

                ++count;

                // add this dest node to this link's sublink
                //log_debug("IMC dest_node: %zu - adding bundle *%p to imc via link: %s", dest_node, bundle, link->name_str().c_str());

                bundle->add_imc_dest_node_via_link(link->name_str(), dest_node);

                break;
            }

            if (!is_queued && (count == 0)) {
                if (imc_config_.treat_unreachable_nodes_as_unrouteable_ || !has_known_link) { 
                    // add to list of unrouteable nodes
                    bool in_home_region = imc_config_.is_node_in_home_region(dest_node);
                    bundle->add_imc_unrouteable_dest_node(dest_node, in_home_region);
                } else {
                    // add this dest node to the known link's sublist
                    bundle->add_imc_dest_node_via_link(known_link_name, dest_node);
                }
            }
        }

        ++iter_dest_map;
    }
}


//----------------------------------------------------------------------
int
IMCRouter::queue_bundle_to_links(Bundle* bundle)
{
    int count = 0;

    LinkRef linkref;

    // Not currently support Custody Transfer with IMC Bundles so use the
    // default custody spec
    CustodyTimerSpec custody_spec;

    // Loop through the IMC Link sublists to queue the bundle on the links
    size_t index = 0;
    std::string link_name = bundle->imc_link_name_by_index(index);

    while (!link_name.empty()) {
        // get the Link for this link name
        linkref = BundleDaemon::instance()->contactmgr()->find_link(link_name.c_str());

        if (linkref != nullptr) {
            if (linkref->isopen() && linkref->queue_has_space()) {
                actions_->queue_bundle(bundle , linkref,
                                       ForwardingInfo::FORWARD_ACTION, custody_spec);
                ++count;
            }
        }

        ++index;
        link_name = bundle->imc_link_name_by_index(index);
    }

    if (index == 0) {
        // Josh requested a fallback to allow a static route for otherwise unrouteable bundles
        // index equal to zreo indicates there were no links found for this bundle

        // see if a route matches the IMC destination
        RouteEntryVec matches;
        LinkRef null_link("TableBasedRouter::route_bundle");
        sptr_route_table_->get_matching(bundle->dest(), null_link, &matches);

        // sort the matching routes by priority, allowing subclasses to
        // override the way in which the sorting occurs
        sort_routes(bundle, &matches);

        for (auto iter = matches.begin(); iter != matches.end(); ++iter)
        {
            RouteEntry* route = *iter;

            //log_debug("checking IMC fallback route entry %p link %s (%p) [remote EID: %s]",
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
                continue;
            }

            // found a link to send the bundle over
            // configure bundle to indicate this link is for all unhandled destination nodes
            bundle->copy_all_unhandled_nodes_to_via_link(link->name());

            ++count;

            //log_debug("IMC fallback route bundle: sending *%p to *%p",
            //          bundle, link.object());

            actions_->queue_bundle(bundle , link,
                                   route->action(), route->custody_spec());

            // only send it through the primary path if so configured and one exists 
            if (BundleRouter::config_.static_router_prefer_always_on_ &&
                (route->link()->type() == Link::ALWAYSON)) {
                break;
            }
        }
    }

    return count;
}


//----------------------------------------------------------------------
int
IMCRouter::route_bundle_as_non_router_node(Bundle* bundle)
{
    int count = 0;

    // forward bundle to first available router node
    imc_config_.update_bundle_imc_alternate_dest_nodes_with_router_nodes_in_home_region(bundle);

    // flag IMC bundles as processed
    bundle->set_router_processed_imc();

    if (bundle->imc_alternate_dest_nodes_count() == 0) {
        // no need to actually transmit this bundle 
        // - issue a dummy transmitted event to see if it can be deleted
        BundleTryDeleteRequest* req_to_post;
        req_to_post = new BundleTryDeleteRequest(bundle);
        SPtr_BundleEvent sptr_req_to_post(req_to_post);
        BundleDaemon::post(sptr_req_to_post);


    } else {
        count = queue_bundle_to_first_alternate_open_link(bundle);
    }

    return count;
}

//----------------------------------------------------------------------
int
IMCRouter::queue_bundle_to_first_alternate_open_link(Bundle* bundle)
{
    int count = 0;

    oasys::ScopeLock scoplok(bundle->lock(), __func__);

    bundle->clear_imc_link_lists();

    // loop through the alternate destination nodes to find an open link
    SPtr_IMC_DESTINATIONS_MAP sptr_imc_dest_map = bundle->imc_alternate_dest_map();

    if (!sptr_imc_dest_map) {
        // no map to work with
        return count;
    }

    // choose a random starting point to prevent continually trying to send to 
    // the same one that may not be having issues
    size_t num_nodes = sptr_imc_dest_map->size();
    size_t working_index = 0;
    size_t nodes_checked = 0;
    bool   found_open_link = false;
    bool   dummy_is_queued = false;

    if (num_nodes == 0) {
        return count;
    }

    srand(time(nullptr));
    size_t starting_index = rand() % num_nodes;

    auto iter_dest_map = sptr_imc_dest_map->begin();

    while (!found_open_link && (iter_dest_map != sptr_imc_dest_map->end())) {
        if (working_index < starting_index) {
            ++iter_dest_map;
            ++working_index;
            continue;
        } else {
            ++nodes_checked;

            uint64_t dest_node = iter_dest_map->first;

            SPtr_EID sptr_ipn_eid = BD_MAKE_EID_IPN(dest_node, 0);


            RouteEntryVec matches;

            LinkRef null_link("IMCRouter::queue_bundle_to_first_alternate_open_link");
            sptr_route_table_->get_matching(sptr_ipn_eid, null_link, &matches);

            // sort the matching routes by priority, allowing subclasses to
            // override the way in which the sorting occurs
            sort_routes(bundle, &matches);

            //log_debug("queue_bundle_to_first_alternate_open_link bundle id %" PRIbid ": checking %zu route entry matches",
            //          bundle->bundleid(), matches.size());


            for (auto iter_matches = matches.begin(); iter_matches != matches.end(); ++iter_matches)
            {
                RouteEntry* route = *iter_matches;

                //log_debug("checking route entry %p link %s (%p) [remote EID: %s]",
                //          *iter_matches, route->link()->name(), route->link().object(), 
                //          route->link()->remote_eid()->c_str());

                if (! should_fwd_imc(bundle, route, dummy_is_queued)) {
                    continue;
                }

                const LinkRef& linkref = route->link();

                if (linkref->isopen()) {
                    if (!linkref->queue_has_space()) {
                        continue;
                    }

                    found_open_link = true;

                    // loop adding all of the orig nodes to the link
                    uint64_t orig_dest_node;
                    SPtr_IMC_DESTINATIONS_MAP sptr_imc_orig_dest_map = bundle->imc_orig_dest_map();
                    if (sptr_imc_orig_dest_map) {
                        auto iter_orig_dest_map = sptr_imc_orig_dest_map->begin();
                        while (iter_orig_dest_map != sptr_imc_orig_dest_map->end()) {
                            orig_dest_node = iter_dest_map->first;

                            // add this dest node to this link's sublink
                            bundle->add_imc_dest_node_via_link(linkref->name_str(), orig_dest_node);

                            ++iter_orig_dest_map;
                        }
                    }

                    CustodyTimerSpec custody_spec;
                    actions_->queue_bundle(bundle , linkref,
                                           ForwardingInfo::FORWARD_ACTION, custody_spec);

                    ++count;

                    break;
                } else if (linkref->isavailable() && (!linkref->isopen()) && (!linkref->isopening())) {
                    // try to open the link in case of need to retransmit
                    actions_->open_link(linkref);
                }
            }
        }

        if (nodes_checked == num_nodes) {
            // we have tried all of the IMC Router nodes so time to give up
            break;
        }

        if (!found_open_link) {
            ++iter_dest_map;

            if (iter_dest_map == sptr_imc_dest_map->end()) {
                // since we may have started in the middle
                // we need to wrap around and keep going
                // until the num_nodes have been checked
                iter_dest_map = sptr_imc_dest_map->begin();
            }
        }
    }

    return count;
}

//----------------------------------------------------------------------
void
IMCRouter::sort_routes(Bundle* bundle, RouteEntryVec* routes)
{
    (void)bundle;
    std::sort(routes->begin(), routes->end(), RoutePrioritySort());
}

//----------------------------------------------------------------------
void
IMCRouter::handle_registration_added(SPtr_BundleEvent& sptr_event)
{
    // issue join request whether or not an IMC router node

    RegistrationAddedEvent* event = nullptr;
    event = dynamic_cast<RegistrationAddedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationAddedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }


    // Issue IMC Join bundle if appropriate
    if (event->sptr_reg_->endpoint()->is_imc_scheme()) {
        size_t imc_group = event->sptr_reg_->endpoint()->node_num();
        if (imc_group > 0) {
           imc_config_.add_to_local_group_joins_count(imc_group);

           log_always("%s: sending IMC join petition for registration EID: %s", 
                      __func__, event->sptr_reg_->endpoint()->c_str());

           if (imc_config_.imc_issue_bp7_joins_) {
               imc_config_.issue_bp7_imc_join_petition(imc_group); 
           }
           if (imc_config_.imc_issue_bp6_joins_) {
               imc_config_.issue_bp6_imc_join_petitions(imc_group); 
           }
        }
    }
}

//----------------------------------------------------------------------
void
IMCRouter::handle_registration_removed(SPtr_BundleEvent& sptr_event)
{
    // issue unjoin request whether or not an IMC router node

    RegistrationRemovedEvent* event = nullptr;
    event = dynamic_cast<RegistrationRemovedEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a RegistrationRemovedEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // Issue IMC UnJoin bundle if appropriate
    if (event->sptr_reg_->endpoint()->is_imc_scheme()) {
        size_t imc_group = event->sptr_reg_->endpoint()->node_num();
        if (imc_group > 0) {
           if (imc_config_.subtract_from_local_group_joins_count(imc_group) == 0) {
               if (imc_config_.imc_issue_bp7_joins_) {
                   imc_config_.issue_bp7_imc_unjoin_petition(imc_group); 
               }
               if (imc_config_.imc_issue_bp6_joins_) {
                   imc_config_.issue_bp6_imc_unjoin_petitions(imc_group); 
               }
           }
        }
    }
}

//----------------------------------------------------------------------
void
IMCRouter::handle_contact_up(SPtr_BundleEvent& sptr_event)
{
    if (!imc_config_.imc_router_node_designation()) {
        return;
    }


    ContactUpEvent* event = nullptr;
    event = dynamic_cast<ContactUpEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a ContactUpEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    const ContactRef contact = event->contact_;
    LinkRef link = contact->link();
    ASSERT(link != nullptr);

    if (link->isdeleted()) {
        return;
    }

    //ignore stale notifications that an old contact is up
    oasys::ScopeLock scoplok(BundleDaemon::instance()->contactmgr()->lock(), "IMCRouter::handle_contact_up");
    if (link->contact() != contact)
    {
        log_info("CONTACT_UP *%p (contact %p) being ignored (old contact)",
                 link.object(), contact.object());
        return;
    }

    if (imc_config_.imc_issue_bp7_joins_ && link->remote_eid()->is_ipn_scheme()) {
        issue_imc_briefing_request_to_single_node(link->remote_eid()->node_num());

    }
}

//----------------------------------------------------------------------
void
IMCRouter::issue_imc_briefing_request_to_all_nodes_in_region()
{
    std::unique_ptr<Bundle> qbptr (imc_config_.create_bp7_imc_join_petition(IMC_GROUP_0_SYNC_REQ));

    if (!qbptr) {
        return;
    }

    imc_config_.update_bundle_imc_dest_nodes_with_all_nodes_in_home_region(qbptr.get());
    imc_config_.update_bundle_imc_dest_nodes_with_all_router_nodes(qbptr.get());
    qbptr->add_imc_region_processed(imc_config_.home_region());
    qbptr->set_imc_is_dtnme_node(true);
    qbptr->set_imc_sync_request(true);
    if (imc_config_.imc_router_enabled_ && imc_config_.imc_router_node_designation()) {
        qbptr->set_imc_is_router_node(true);
    }

    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
IMCRouter::issue_imc_briefing_request_to_single_node(size_t node_num)
{
    std::unique_ptr<Bundle> qbptr (imc_config_.create_bp7_imc_join_petition(IMC_GROUP_0_SYNC_REQ));

    if (!qbptr) {
        return;
    }

    qbptr->add_imc_dest_node(node_num);
    qbptr->add_imc_region_processed(imc_config_.home_region());

    qbptr->set_imc_is_dtnme_node(true);
    qbptr->set_imc_sync_request(true);
    if (imc_config_.imc_router_enabled_ && imc_config_.imc_router_node_designation()) {
        qbptr->set_imc_is_router_node(true);
    }

    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

} // namespace dtn
