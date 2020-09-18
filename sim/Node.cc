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

#include <oasys/thread/Timer.h>
#include "SimEvent.h"
#include "SimLog.h"
#include "Node.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "reg/Registration.h"

using namespace dtn;

namespace dtnsim {

//----------------------------------------------------------------------
Node::Node(const char* name)
    : BundleDaemon(), name_(name),
      storage_config_("storage",
                      "memorydb",
                      "DTN", "")
      
{
    logpathf("/node/%s", name);
    log_info("node %s initializing...", name);

    storage_config_.init_ = true;
    storage_config_.tidy_ = true;
    storage_config_.tidy_wait_ = 0;
    storage_config_.leave_clean_file_ = false;

    // XXX/demmer see comment in BundlePayload::init
    storage_config_.payload_dir_ = "NO_PAYLOAD_FILES"; 
}

//----------------------------------------------------------------------
void
Node::do_init()
{
    BundleDaemon::instance_ = this;
    
    actions_ = new BundleActions();
    eventq_ = new std::queue<BundleEvent*>();

    // forcibly create a new timer system and storage systems
    oasys::Singleton<oasys::TimerSystem>::force_set_instance(NULL);
    oasys::TimerSystem::create();
    timersys_ = oasys::TimerSystem::instance();

    // Create a faux-singleton memory storage - one per node
    // Note that the instance has to be left valid for begin_/end_transaction
    // at the end of creating nodes,
    // but it is irrelevant which one because memory db doesn't do transactions.
    oasys::DurableStore::force_set_instance(NULL);
    store_ = new oasys::DurableStore("/dtnsim/storage");
    store_->create_store(storage_config_);

    // we only have one GlobalStore because it's useful to have unique
    // bundle ids across all nodes in the system, so initialize it the
    // first node that gets created
    if (! GlobalStore::initialized()) {
        if (GlobalStore::init(storage_config_, store_) != 0) {
            PANIC("Error initializing GlobalStore");
        }
    }

    // the other stores are faux-singletons with an instance per node
    BundleStore::force_set_instance(NULL);

    LinkStore::force_set_instance(NULL);
    RegistrationStore::force_set_instance(NULL);

    log_info("creating storage tables");
    if ((BundleStore::init(storage_config_, store_) != 0) ||
        (LinkStore::init(storage_config_, store_) != 0) ||
        (RegistrationStore::init(storage_config_, store_) != 0))
    {
        PANIC("Error initializing storage tables");
    }


    bundle_store_  = BundleStore::instance();
    link_store_    = LinkStore::instance();
    reg_store_     = RegistrationStore::instance();

}

//----------------------------------------------------------------------
void
Node::set_active()
{
    if (instance_ == this) return;
    
    instance_ = this;
    oasys::Singleton<oasys::TimerSystem>::force_set_instance(timersys_);
    oasys::Log::instance()->set_prefix(name_.c_str());

    oasys::DurableStore::force_set_instance(store_);

    BundleStore::force_set_instance(bundle_store_);
    LinkStore::force_set_instance(link_store_);
    RegistrationStore::force_set_instance(reg_store_);

}

//----------------------------------------------------------------------
void
Node::configure()
{
    set_active();

    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
    router_->initialize();
}

//----------------------------------------------------------------------
void
Node::post_event(BundleEvent* event, bool at_back)
{
    (void)at_back;
    log_debug("posting event (%p) with type %s at %s ",
              event, event->type_str(),at_back ? "back" : "head");
        
    eventq_->push(event);
}

//----------------------------------------------------------------------
bool
Node::process_one_bundle_event()
{
    BundleEvent* event;
    if (!eventq_->empty()) {
        event = eventq_->front();
        eventq_->pop();
        handle_event(event);
        if ( store_->is_transaction_open() ) {
            log_debug("process_one_bundle_event closing transaction");
            store_->end_transaction();
        }
        delete event;
        log_debug("event (%p) %s processed and deleted",event,event->type_str());
        return true;
    }
    return false;
}

//----------------------------------------------------------------------
void
Node::run_one_event_now(BundleEvent* event)
{
    Node* cur_active = active_node();
    set_active();
    handle_event(event);
    if ( store_->is_transaction_open() ) {
        log_debug("run_one_event_now closing transaction");
        store_->end_transaction();
    }
    log_debug("event (%p) %s processed",event,event->type_str());
    cur_active->set_active();
}

//----------------------------------------------------------------------
void
Node::process(SimEvent* e)
{
    switch (e->type()) {
    case SIM_BUNDLE_EVENT:
        post_event(((SimBundleEvent*)e)->event_);
        break;
        
    default:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
Node::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    SimLog::instance()->log_arrive(this, event->bundleref_.object());
    BundleDaemon::handle_bundle_delivered(event);
}

//----------------------------------------------------------------------
void
Node::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object(); 
    SimLog::instance()->log_recv(this, bundle);

    // XXX/demmer this needs to look at the history of all duplicates,
    // not just the duplicates right now...
    
    Bundle* duplicate = find_duplicate(bundle);
    if (duplicate != NULL) {
        SimLog::instance()->log_dup(this, bundle);
    }
    BundleDaemon::handle_bundle_received(event);
}

//----------------------------------------------------------------------
void
Node::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    SimLog::instance()->log_xmit(this, event->bundleref_.object());
    BundleDaemon::handle_bundle_transmitted(event);
}


//----------------------------------------------------------------------
void
Node::handle_bundle_expired(BundleExpiredEvent* event)
{
    SimLog::instance()->log_expire(this, event->bundleref_.object());
    BundleDaemon::handle_bundle_expired(event);
}

} // namespace dtnsim
