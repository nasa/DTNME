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

#ifndef _NODE_H_
#define _NODE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>
#include <oasys/storage/DurableStore.h>

#include "SimEventHandler.h"
#include "bundling/BundleDaemon.h"
#include "storage/DTNStorageConfig.h"
#include "storage/BundleStore.h"
#include "storage/LinkStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"


using namespace dtn;

namespace dtn {

class ContactManager;

}

namespace dtnsim {

/**
 * Class representing a node in the simulator (i.e. a router plus
 * associated links, etc).
 *
 * Derives from the core dtn BundleDaemon and whenever an event is
 * processed that relates to a node, that node is installed as the
 * BundleDaemon::instance().
 */
class Node : public SimEventHandler, public BundleDaemon {
public:
    /**
     * Constructor
     */
    Node(const char* name);

    /**
     * Virtual initialization function.
     */
    void do_init();

    /**
     * Override of BundleDaemon::event_queue_size since eventq_ is
     * shadowed to be a simple std::queue instead of a MsgQueue.
     */
    size_t event_queue_size()
    {
    	return eventq_->size();
    }
    
    /**
     * Second pass at initialization, called by the simulator once the
     * whole config has been parsed.
     */
    void configure();

    /**
     * Destructor
     */
    virtual ~Node() {}
        
    /**
     * Virtual post function, overridden in the simulator to use the
     * modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);
    
    /**
     * Virtual function from SimEventHandler
     */
    virtual void process(SimEvent *e);

    /**
     * Drain and process a bundle event from the queue, if one exists.
     */
    bool process_one_bundle_event();

    /**
     * Run the given event immediately.
     */
    void run_one_event_now(BundleEvent* event);

    /**
     * Overridden event handlers from BundleDaemon
     */
    void handle_bundle_delivered(BundleDeliveredEvent* event);
    void handle_bundle_received(BundleReceivedEvent* event);
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
    void handle_bundle_expired(BundleExpiredEvent* event);
    
    /**
     * Accessor for name.
     */
    const char* name() { return name_.c_str(); }
    
    /**
     * Accessor for router.
     */
    BundleRouter* router() { return router_; }

    /**
     * Set the node as the "active" node in the simulation. This
     * swings the static instance_ pointers to point to the node and
     * its state so all singleton accesses throughout the code will
     * reference the correct object(s).
     *
     * It also sets the node name as the logging prefix in oasys.
     */
    void set_active();

    /**
     * Return the current active node.
     */
    static Node* active_node()
    {
        return (Node*)instance_;
    }

    /**
     * Accessor for the storage config at this node.
     */
    DTNStorageConfig* storage_config() { return &storage_config_; }

protected:
    const std::string   name_;
    bundleid_t          next_bundleid_;
    u_int32_t		next_regid_;
    std::queue<BundleEvent*>* eventq_;
    oasys::TimerSystem* timersys_;

    /// @{ Fake-Durable storage
    DTNStorageConfig     storage_config_;
    oasys::DurableStore* store_;
    BundleStore*         bundle_store_;
    LinkStore*           link_store_;
    RegistrationStore*   reg_store_;

    /// @}
};

} // namespace dtnsim

#endif /* _NODE_H_ */
