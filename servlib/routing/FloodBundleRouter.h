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

#ifndef _FLOOD_BUNDLE_ROUTER_H_
#define _FLOOD_BUNDLE_ROUTER_H_

#include "TableBasedRouter.h"

namespace dtn {

/**
 * This is the implementation of a flooding based bundle router.
 *
 * The implementation is very simple: The class maintains an internal
 * BundleList in which all bundles are kept until their expiration
 * time. This prevents the main daemon logic from opportunistically
 * deleting bundles when they've been transmitted.
 *
 * Whenever a new link arrives, we add a wildcard route to the table.
 * Then when a bundle arrives, we can stick it on the all_bundles list
 * and just call the base class route_bundle function. The core base
 * class logic then makes sure that a copy of the bundle is forwarded
 * exactly once to each neighbor.
 *
 * XXX/demmer This should be extended to avoid forwarding a bundle
 * back to the node from which it arrived. With the upcoming
 * bidirectional link changes, this should be able to be done easily.
 */
class FloodBundleRouter : public TableBasedRouter {
public:
    /**
     * Constructor.
     */
    FloodBundleRouter();

    /**
     * Initializer.
     */
    void initialize();

    /// @{ Event handlers / router callbacks
    void handle_bundle_received(BundleReceivedEvent* event);
    void handle_link_created(LinkCreatedEvent* event);
    bool can_delete_bundle(const BundleRef& bundle);
    void delete_bundle(const BundleRef& bundle);
    
protected:
    /// To ensure bundles aren't deleted by the system just after they
    /// are forwarded, we hold them all in this separate list.
    BundleList all_bundles_;

    /// Wildcard pattern to match all bundles.
    EndpointIDPattern all_eids_;
};

} // namespace dtn

#endif /* _FLOOD_BUNDLE_ROUTER_H_ */
