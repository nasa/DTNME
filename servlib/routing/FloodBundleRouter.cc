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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "contacts/Contact.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "FloodBundleRouter.h"

namespace dtn {

//----------------------------------------------------------------------
FloodBundleRouter::FloodBundleRouter()
    : TableBasedRouter("FloodBundleRouter", "flood"),
      all_bundles_("FloodBundleRouter::all_bundles"),
      all_eids_(EndpointIDPattern::WILDCARD_EID())
{
    log_info("FloodBundleRouter initialized");
    ASSERT(all_eids_.valid());
}

//----------------------------------------------------------------------
void
FloodBundleRouter::initialize()
{
    TableBasedRouter::initialize();
    config_.add_nexthop_routes_ = false;
}

//----------------------------------------------------------------------
void
FloodBundleRouter::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("bundle received *%p", bundle);
    all_bundles_.push_back(bundle);

    TableBasedRouter::handle_bundle_received(event);
}

//----------------------------------------------------------------------
void
FloodBundleRouter::handle_link_created(LinkCreatedEvent* event)
{
    TableBasedRouter::handle_link_created(event);
    
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    log_debug("FloodBundleRouter::handle_link_created: "
              "link_created *%p", link.object());

    RouteEntry* entry = new RouteEntry(all_eids_, link);
    entry->set_action(ForwardingInfo::COPY_ACTION);

    // adds the route to the table and checks for bundles that may
    // need to be sent to the new link
    add_route(entry);
}

//----------------------------------------------------------------------
bool
FloodBundleRouter::can_delete_bundle(const BundleRef& bundle)
{
    (void)bundle;
    return false;
}

//----------------------------------------------------------------------
void
FloodBundleRouter::delete_bundle(const BundleRef& bundle)
{
    log_debug("bundle_expired *%p", bundle.object());
    all_bundles_.erase(bundle);
}

} // namespace dtn
