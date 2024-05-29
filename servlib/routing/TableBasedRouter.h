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

#ifndef _TABLE_BASED_ROUTER_H_
#define _TABLE_BASED_ROUTER_H_

#include "BundleRouter.h"
#include "IMCRouter.h"
#include "RouterInfo.h"
#include "RouteTable.h"

namespace dtn {

class BundleList;
class RouteEntryVec;
class RouteTable;

/**
 * This is an abstract class that is intended to be used for all
 * routing algorithms that store routing state in a table.
 */
class TableBasedRouter : public BundleRouter
{
protected:
    /**
     * Constructor -- protected since this class is never instantiated
     * by itself.
     */
    TableBasedRouter(const char* classname, const std::string& name);

    /**
     * Destructor.
     */
    virtual ~TableBasedRouter();
    virtual void shutdown() override;


    /**
     * Event handler overridden from BundleRouter / BundleEventHandler
     * that dispatches to the type specific handlers where
     * appropriate.
     */
    virtual void handle_event(SPtr_BundleEvent& sptr_event) override;
    
    /// @{ Event handlers
    virtual void handle_bundle_received(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_transmitted(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_cancelled(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_route_add(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_route_del(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_up(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_down(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_available(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_created(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_deleted(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_custody_timeout(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_added(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_removed(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_expired(SPtr_BundleEvent& sptr_event) override;
    /// @}


    /**
     * Dump the routing state.
     */
    void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Get a tcl version of the routing state.
     */
    void tcl_dump_state(oasys::StringBuffer* buf);

    /**
     * Add a route entry to the routing table. 
     * Set skip_changed_routes to true to skip the call to 
     * handle_changed_routes if the initiating method is going to call it.
     */
    void add_route(RouteEntry *entry, bool skip_changed_routes=true);

    /**
     * Remove matching route entry(s) from the routing table. 
     */
    void del_route(const SPtr_EIDPattern& sptr_id);

    /**
     * Update forwarding state due to changed routes.
     */
    void handle_changed_routes();

    /**
     * Check if the bundle should be forwarded to the given next hop.
     * Reasons why it would not be forwarded include that it was
     * already transmitted or is currently in flight on the link, or
     * that the route indicates ForwardingInfo::FORWARD_ACTION and it
     * is already in flight on another route.
     */
    virtual bool should_fwd(const Bundle* bundle, RouteEntry* route);
    
    /**
     * Check the route table entries that match the given bundle and
     * have not already been found in the bundle history. If a match
     * is found, call fwd_to_nexthop on it.
     * Set skip_check_next_hop to true to skip the call to 
     * check_next_hop().
     *
     * @param bundle		the bundle to forward
     *
     * Returns the number of links on which the bundle was queued
     * (i.e. the number of matching route entries.
     */
    virtual int route_bundle(Bundle* bundle, bool skip_check_next_hop=false);

    /**
     * Once a vector of matching routes has been found, sort the
     * vector. The default uses the route priority, breaking ties by
     * using the number of bytes queued.
     */
    virtual void sort_routes(Bundle* bundle, RouteEntryVec* routes);
    
    /**
     * Called when the next hop link is available for transmission
     * (i.e. either when it first arrives and the contact is brought
     * up or when a bundle is completed and it's no longer busy).
     *
     * Loops through the bundle list and calls fwd_to_matching on all
     * bundles.
     */
    virtual void check_next_hop(const LinkRef& next_hop);

    /**
     * Go through all known bundles in the system and try to re-route them.
     */
    virtual void reroute_all_bundles();

    /**
     * Generic hook in response to the command line indication that we
     * should reroute all bundles.
     */
    virtual void recompute_routes();

    /**
     * When new links are added or opened, and if we're configured to
     * add nexthop routes, try to add a new route for the given link.
     * Set skip_changed_routes to true to skip the call to 
     * handle_changed_routes if the initiating method is going to call it.
     */
    void add_nexthop_route(const LinkRef& link, bool skip_changed_routes=false);

    /**
     * Hook to ask the router if the bundle can be deleted.
     */
    bool can_delete_bundle(const BundleRef& bundle);
    
    /**
     * Hook to tell the router that the bundle should be deleted.
     */
    void delete_bundle(const BundleRef& bundle);

    /// The routing table
    SPtr_RouteTable sptr_route_table_;

    /// Timer class used to cancel transmission on down links after
    /// waiting for them to potentially reopen
    class RerouteTimer;
    typedef std::shared_ptr<RerouteTimer> SPtr_RerouteTimer;

    class RerouteTimer : public oasys::SharedTimer {
    public:
        RerouteTimer(TableBasedRouter* router, const LinkRef& link);

        virtual ~RerouteTimer();

        virtual void timeout(const struct timeval& now) override;

    protected:
        TableBasedRouter* router_;
        LinkRef link_;
        oasys::SPtr_Timer sptr_;
        uint32_t seconds_ = 30;
    };

    friend class RerouteTimer;

    /// Helper function for rerouting
    void reroute_bundles(const LinkRef& link);
    
    /// Table of reroute timers, indexed by the link name
    typedef oasys::StringMap<SPtr_RerouteTimer> RerouteTimerMap;
    RerouteTimerMap reroute_timers_;

    /// Pointer to the instantiation of the IMC Router
    SPtr_IMCRouter sptr_imc_router_;
};

} // namespace dtn

#endif /* _TABLE_BASED_ROUTER_H_ */
