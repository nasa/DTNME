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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#ifndef _BUNDLE_ROUTER_H_
#define _BUNDLE_ROUTER_H_

#include <vector>
#include <oasys/debug/Logger.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/StringUtils.h>

#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleEventHandler.h"
#include "naming/EndpointID.h"

namespace dtn {

class BundleActions;
class BundleRouter;
class StringBuffer;

/**
 * Typedef for a list of bundle routers.
 */
typedef std::vector<BundleRouter*> BundleRouterList;

/**
 * The BundleRouter is the main decision maker for all routing
 * decisions related to bundles.
 *
 * It receives events from the BundleDaemon having been posted by
 * other components. These events include all operations and
 * occurrences that may affect bundle delivery, including new bundle
 * arrival, contact arrival, timeouts, etc.
 *
 * In response to each event the router may call one of the action
 * functions implemented by the BundleDaemon. Note that to support the
 * simulator environment, all interactions with the rest of the system
 * should go through the singleton BundleAction classs.
 *
 * To support prototyping of different routing protocols and
 * frameworks, the base class has a list of prospective BundleRouter
 * implementations, and at boot time, one of these is selected as the
 * active routing algorithm. As new algorithms are added to the
 * system, new cases should be added to the "create_router" function.
 */
class BundleRouter : public BundleEventHandler {
public:
    /**
     * Factory method to create the correct subclass of BundleRouter
     * for the registered algorithm type.
     */
    static BundleRouter* create_router(const char* type);

    /**
     * Config variables. These must be static since they're set by the
     * config parser before any router objects are created.
     */
    static struct Config {
        Config();
        
        /// The routing algorithm type
        std::string type_;
        
        /// Whether or not to add routes for nexthop links that know
        /// the remote endpoint id (default true)
        bool add_nexthop_routes_;

        /// Whether or not to open discovered opportunistic links when
        /// they become available (default true)
        bool open_discovered_links_;
        
        /// Default priority for new routes
        int default_priority_;

        /// Maximum number of route_to entries to follow for a lookup
        /// (default 10)
        int max_route_to_chain_;

        /// Storage quota for bundle payloads (default unlimited)
        u_int64_t storage_quota_;

        /// Timeout for upstream session subscriptions in seconds
        /// (default is ten minutes)
        u_int subscription_timeout_;

        /// Configure TableBasedRouter to only send bundle on the
        /// link that is ALWAYSON and isopen() if possible
        bool static_router_prefer_always_on_;
        
    } config_;
    
    /**
     * Destructor
     */
    virtual ~BundleRouter();

    /*
     *  called after all the global data structures are set up
     */
    virtual void initialize();

    /**
     * Pure virtual event handler function (copied from
     * BundleEventHandler for clarity).
     */
    virtual void handle_event(BundleEvent* event) = 0;

    /**
     * Synchronous probe indicating whether or not this bundle should
     * be accepted by the system.
     *
     * The default implementation checks if the bundle size will
     * exceed the configured storage quota (if any).
     *
     * @return true if the bundle was accepted. if false, then errp is
     * set to a value from BundleProtocol::status_report_reason_t
     */
    virtual bool accept_bundle(Bundle* bundle, int* errp);

    /**
     * Synchronous probe indicating whether or not custody of this bundle 
     * should be accepted by the system.
     *
     * The default implementation returns true to match DTNME.9 
     * functionality which did not check with the router.
     *
     * @return true if okay to accept custody of the bundle.
     */
    virtual bool accept_custody(Bundle* bundle);

    /**
     * Synchronous probe indicating whether or not this bundle can be
     * deleted by the system.
     *
     * The default implementation returns true if the bundle is queued
     * on more than one list (i.e. the pending bundles list).
     */
    virtual bool can_delete_bundle(const BundleRef& bundle);

    /**
     * Synchronous call indicating that the bundle is being deleted
     * from the system and that the router should remove it from any
     * lists where it may be queued.
     */
    virtual void delete_bundle(const BundleRef& bundle);
    
    /**
     * Format the given StringBuffer with current routing info.
     */
    virtual void get_routing_state(oasys::StringBuffer* buf) = 0;

    /**
     * Check if the bundle should be forwarded to the given next hop.
     * Reasons why it would not be forwarded include that it was
     * already transmitted or is currently in flight on the link, or
     * that the route indicates ForwardingInfo::FORWARD_ACTION and it
     * is already in flight on another route.
     */
    virtual bool should_fwd(const Bundle* bundle, const LinkRef& link,
            ForwardingInfo::action_t action = ForwardingInfo::COPY_ACTION);

    /**
     * Format the given StringBuffer with a tcl-parsable version of
     * the routing state.
     *
     * The expected format is:
     *
     *  {
     *    {dest_eid1 nexthop_link1 [params]}
     *    {dest_eid2 nexthop_link2 [params]}
     *  }
     *
     * where [params] is a var val list.
     */
    virtual void tcl_dump_state(oasys::StringBuffer* buf);

    /**
     * Hook to force route recomputation from the command interpreter.
     * The default implementation does nothing.
     */
    virtual void recompute_routes();

    /**
     * for registration with the BundleDaemon
     */
    virtual void shutdown();

    /**
     * seconds a connection should delay after contact up to allow router time
     * to reject the connection before reading bundles
     */
    virtual int delay_after_contact_up() { return 0; }
    
protected:
    /**
     * Constructor
     */
    BundleRouter(const char* classname, const std::string& name);

    /// Name of this particular router
    std::string name_;
    
    /// The list of all bundles still pending delivery
    pending_bundles_t* pending_bundles_;

    /// The list of all bundles that I have custody of
    custody_bundles_t* custody_bundles_;

    /// The actions interface, set by the BundleDaemon when the router
    /// is initialized.
    BundleActions* actions_;
    
};

} // namespace dtn

#endif /* _BUNDLE_ROUTER_H_ */
