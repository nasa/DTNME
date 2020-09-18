/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <typeinfo>

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)

#include <oasys/io/NetUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/IO.h>
#include <oasys/thread/Lock.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>
#include <oasys/thread/SpinLock.h>

#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "ECLModule.h"
#include "bundling/BundleDaemon.h"
#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "contacts/ContactManager.h"

 
namespace dtn {

const size_t ECLModule::READ_BUFFER_SIZE;
const size_t ECLModule::MAX_BUNDLE_IN_MEMORY;

ECLModule::ECLModule(int fd,
                     in_addr_t remote_addr,
                     u_int16_t remote_port,
                     ExternalConvergenceLayer& cl) :
    CLEventHandler("ECLModule", "/dtn/cl/module"),
    Thread("/dtn/cl/module", Thread::CREATE_JOINABLE),
    cl_(cl),
    iface_list_lock_("/dtn/cl/parts/iface_list_lock"),
    socket_(fd, remote_addr, remote_port, logpath_),
    message_queue_("/dtn/cl/parts/module"),
    parser_( true, cl.schema_.c_str() )
{
    name_ = "(unknown)";
    was_shutdown_ = false;
    sem_init(&link_list_sem_, 0, 2);
}

ECLModule::~ECLModule()
{
    while (message_queue_.size() > 0)
        delete message_queue_.pop_blocking();
}

void
ECLModule::run()
{
    struct pollfd pollfds[2];

    struct pollfd* message_poll = &pollfds[0];
    message_poll->fd = message_queue_.read_fd();
    message_poll->events = POLLIN;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = socket_.fd();
    sock_poll->events = POLLIN;

    while ( !should_stop() ) {
        // Poll for activity on either the event queue or the socket.
        int ret = oasys::IO::poll_multiple(pollfds, 2, -1);
        
        if (ret == oasys::IOINTR) {
            log_err("Module server interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("Module server error");
            set_should_stop();
            continue;
        }

        if (message_poll->revents & POLLIN) {
            cl_message* message;
            if ( message_queue_.try_pop(&message) ) {
                ASSERT(message != NULL);
                int result;
                
                // We need to handle bundle-send messages as a special case,
                // in order to get the bundle written to disk first.
                if ( message->bundle_send_request().present() )
                    result = prepare_bundle_to_send(message);
                
                else
                    result = send_message(message);
                
                delete message;

                if (result < 0) {
                    set_should_stop();
                    continue;
                } // if
            } // if
        } // if

        // Check for input on the socket and read whatever is available.
        if (sock_poll->revents & POLLIN)
            read_cycle();
    } // while

    log_info( "CL %s is shutting down", name_.c_str() );
    
    oasys::ScopeLock lock(&cl_.global_resource_lock_, "ECLModule::run");
    
    if (!was_shutdown_) {
        set_flag(Thread::DELETE_ON_EXIT);
        cl_.remove_module(this);
    }
    
    socket_.close();
    cleanup();
}

void
ECLModule::post_message(cl_message* message)
{
    message_queue_.push_back(message);
}

ECLInterfaceResource*
ECLModule::remove_interface(const std::string& name)
{
    oasys::ScopeLock lock(&iface_list_lock_, "remove_interface");
    std::list<ECLInterfaceResource*>::iterator iface_i;

    for (iface_i = iface_list_.begin(); iface_i != iface_list_.end(); ++iface_i) {
        if ( (*iface_i)->interface_->name() == name) {
            iface_list_.erase(iface_i);
            return *iface_i;
        }
    }

    return NULL;
}

void
ECLModule::shutdown()
{
    oasys::ScopeLock lock(&cl_.global_resource_lock_, "ECLModule::run");
    log_debug("ECLModule::shutdown() for CLA '%s'", name_.c_str());
    was_shutdown_ = true;
    set_should_stop();
    
    // This seems to be the only effective way to interrupt the thread.
    message_queue_.notify();
}

void
ECLModule::handle(const cla_add_request& message)
{
    if (cl_.get_module( message.name() ) != NULL) {
        log_err("A CLA with name '%s' already exists", message.name().c_str());
        set_should_stop();
        return;
    }
    
    name_ = message.name();
    logpathf( "/dtn/cl/%s", name_.c_str() );
    message_queue_.logpathf( "/dtn/cl/parts/%s/message_queue", name_.c_str() );
    socket_.logpathf( "/dtn/cl/parts/%s/socket", name_.c_str() );
    
    log_info( "New external CL: %s", name_.c_str() );

    // Figure out the bundle directory paths.
    BundleStore* bs = BundleStore::instance();
    std::string payload_dir = bs->payload_dir();
    oasys::FileUtils::abspath(&payload_dir);
    
    oasys::StringBuffer in_dir( "%s/%s-in", payload_dir.c_str(),
                                name_.c_str() );
    oasys::StringBuffer out_dir( "%s/%s-out", payload_dir.c_str(),
                                 name_.c_str() );
    
    // Save the bundle directory paths.
    bundle_in_path_ = std::string( in_dir.c_str() );
    bundle_out_path_ = std::string( out_dir.c_str() );
    
    // Delete the module's incoming and outgoing bundle directories just in
    // case the already exist and contain stale bundle files.
    if (oasys::FileUtils::rm_all_from_dir(bundle_in_path_.c_str(), true) != 0) {
        log_warn( "Unable to clean incoming bundle directory %s: %s", 
                  bundle_in_path_.c_str(), strerror(errno) );
    }
    ::rmdir( bundle_in_path_.c_str() );
    
    if (oasys::FileUtils::rm_all_from_dir(bundle_out_path_.c_str(), true) != 0) {
        log_warn( "Unable to clean outgoing bundle directory %s: %s", 
                  bundle_out_path_.c_str(), strerror(errno) );
    }
    ::rmdir( bundle_out_path_.c_str() );
    
    // Create the incoming bundle directory.
    if (oasys::IO::mkdir(in_dir.c_str(), 0777) < 0) {
        log_err( "Unable to create incoming bundle directory %s: %s",
                 in_dir.c_str(), strerror(errno) );
        
        set_should_stop();
        return;
    }
    
    // Create the outgoing bundle directory.
    if (oasys::IO::mkdir(out_dir.c_str(), 0777) < 0) {
        log_err( "Unable to create outgoing bundle directory %s: %s",
                 out_dir.c_str(), strerror(errno) );
        
        set_should_stop();
        return;
    }
    
    cla_set_params_request request;
    request.create_discovered_links(
            ExternalConvergenceLayer::create_discovered_links_);
    //request.create_discovered_links(true);
    request.local_eid( BundleDaemon::instance()->local_eid().str() );
    request.bundle_pass_method(bundlePassMethodType::filesystem);
    request.reactive_fragment_enabled(
            BundleDaemon::params_.reactive_frag_enabled_);
    
    KeyValueSequence params;
    params.push_back( key_value_pair("incoming_bundle_dir", in_dir.c_str() ) );
    params.push_back( key_value_pair("outgoing_bundle_dir", out_dir.c_str() ) );
    request.key_value_pair(params);
    
    POST_MESSAGE(this, cla_set_params_request, request);
    
    // take appropriate resources for this CLA module
    take_resources();
}

void 
ECLModule::take_resource(ECLResource* resource) 
{
    oasys::ScopeLock lock(&resource->lock_, "ECLModule::take_resource()");
    resource->module_ = this;
    resource->should_delete_ = false;
    
    // Handle an Interface.
    if ( typeid(*resource) == typeid(ECLInterfaceResource) ) {
        ECLInterfaceResource* iface = (ECLInterfaceResource*)resource;
        
        iface_list_lock_.lock("take_resource");
        iface_list_.push_back(iface);
        iface_list_lock_.unlock();

        log_info( "Module %s acquiring interface %s", name_.c_str(),
                  iface->interface_->name().c_str() );
        
        cl_message* message = new cl_message(*iface->create_message_);
        post_message(message);
    }

    // Handle a Link.
    else if ( typeid(*resource) == typeid(ECLLinkResource) ) {
        ECLLinkResource* link = (ECLLinkResource*)resource;
            
        sem_wait(&link_list_sem_);
        sem_wait(&link_list_sem_);
        
        link_list_.insert( LinkHashMap::value_type(link->link_->name_str(),
                              link) );
        
        sem_post(&link_list_sem_);
        sem_post(&link_list_sem_);

        log_info( "Module %s acquiring link %s", name_.c_str(),
                  link->link_->name() );
        cl_message* message = new cl_message(*link->create_message_);
        post_message(message);
    }
    
    else {
        log_err( "Cannot take unknown resource type %s",
                 typeid(*resource).name() );
    } 
}

void
ECLModule::handle(const cla_delete_request& message)
{
    (void)message;
    set_should_stop();
}

void
ECLModule::take_resources()
{
    log_info("Module %s is acquiring appropriate CL resources", name_.c_str());
    // Find all existing resources that belong to this CLA.
    std::list<ECLResource*> resource_list_ = cl_.take_resources(name_);
    std::list<ECLResource*>::iterator resource_i;

    for ( resource_i = resource_list_.begin();
          resource_i != resource_list_.end(); 
          ++resource_i ) {
        ECLResource* resource = (*resource_i);
        take_resource(resource);
    }
}

void 
ECLModule::handle(const cla_params_set_event& message)
{
    (void)message;
    BundleDaemon::post( new CLAParamsSetEvent(&cl_, name_) );    
}

void
ECLModule::handle(const interface_created_event& message)
{
    ECLInterfaceResource* resource = get_interface( message.interface_name() );

    if (!resource) {
        log_warn( "Got interface_created_event for unknown interface %s",
                  message.interface_name().c_str() );
        return;
    }
}

void 
ECLModule::handle(const interface_reconfigured_event& message)
{
    (void)message;
}

void
ECLModule::handle(const eid_reachable_event& message)
{
    ECLInterfaceResource* resource = get_interface( message.interface_name() );

    if (!resource) {
        log_warn( "Got eid_reachable_event for unknown interface %s",
                  message.interface_name().c_str() );
        return;
    }

    BundleDaemon::post(
        new NewEIDReachableEvent( resource->interface_, message.peer_eid() ) );
}

void
ECLModule::handle(const link_created_event& message)
{
    ECLLinkResource* resource;
    LinkRef link("handle(link_created_event) temporary");
    
    resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got link_created_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    link = resource->link_;
    oasys::ScopeLock l(link->lock(), "handle(link_created_event)");
    if ( link->isdeleted() ) {
        // XXX there may be some steps to take to handle the link having been
        // deleted, but probably they have already been done at deletion time
        log_info( "Link %s has already been deleted", 
                  message.link_name().c_str());
        return;
    }
    
    // Create the outgoing bundle directory for this link.
    std::string outgoing_dir = bundle_out_path_ + "/" + message.link_name(); 
    if (oasys::IO::mkdir(outgoing_dir.c_str(), 0777) < 0) {
        log_err( "Unable to create outgoing bundle directory %s: %s",
                 outgoing_dir.c_str(), strerror(errno) );
        
        set_should_stop();
        return;
    }
    
    link->set_create_pending(false);
    
    if (link->state() == Link::UNAVAILABLE)
        link->set_state(Link::AVAILABLE);
    
    // Check for a high-water mark.
    if ( message.link_attributes().high_water_mark().present() ) {
        resource->set_high_water_mark(
                message.link_attributes().high_water_mark().get() );
    }
    
    // Check for a low-water mark.
    if ( message.link_attributes().low_water_mark().present() ) {
        resource->set_low_water_mark(
                message.link_attributes().low_water_mark().get() );
    }
    
    BundleDaemon::post(new LinkCreatedEvent(link));
    
    if (link->type() == Link::OPPORTUNISTIC) {
        BundleDaemon::post( 
               new LinkAvailableEvent(link, ContactEvent::NO_INFO) );
    }
}

void
ECLModule::handle(const link_opened_event& message)
{
    ECLLinkResource* resource = get_link( message.link_name() );
    
    // If no link can be found by that name, it may have just been created.
    // We will wait for an event to get completely through the event queue
    // before giving up on calling the link open.
    if (!resource) {
        oasys::Notifier* notifier = new oasys::Notifier("/dtn/cl/external");
        BundleDaemon::post_and_wait(new StatusRequest(), notifier);
        delete notifier;
        resource = get_link( message.link_name() );
        if (!resource) {
            log_err( "Got link_opened_event for unknown link %s",
                     message.link_name().c_str() );
            return;
        }
    }

    oasys::ScopeLock l(resource->link_->lock(), "ECLModule::link_opened_evet");

    ContactRef contact = resource->link_->contact();
    if (contact == NULL) {
        contact = new Contact(resource->link_);
        resource->link_->set_contact( contact.object() );
    }

    l.unlock();

    update_contact_attributes(message.contact_attributes(), contact);
    BundleDaemon::post( new ContactUpEvent(contact) );
}

void
ECLModule::handle(const link_closed_event& message)
{
    ECLLinkResource* resource = get_link( message.link_name() );
    
    if (!resource) {
        log_err( "Got link_closed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    resource->known_state_ = Link::CLOSED;
    
    if (resource->link_->contact() != NULL) {
        update_contact_attributes( message.contact_attributes(),
                                   resource->link_->contact() );
        
        // It seems like this should be a ContactDownEvent, but that doesn't
        // actually clear the contact, so DTNME thinks this link is still open.
        BundleDaemon::post( new LinkStateChangeRequest(resource->link_,
                            Link::CLOSED,
                            ContactEvent::NO_INFO) );
    } // if
}

void
ECLModule::handle(const link_state_changed_event& message)
{
    Link::state_t new_state;
    ECLLinkResource* resource = get_link( message.link_name() );

    if (!resource) {
        log_err( "Got link_state_changed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    new_state = XMLConvert::convert_link_state( message.new_state() );

    resource->known_state_ = new_state;
    BundleDaemon::post( new LinkStateChangeRequest( resource->link_,
        new_state, XMLConvert::convert_link_reason( message.reason() ) ) );
}

void
ECLModule::handle(const link_deleted_event& message)
{
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got link_deleted_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    // Lock the resource and clear its module field so that the BundleDaemon
    // thread can't do anything with it.
    resource->lock_.lock("handle(link_deleted_event)");
    resource->module_ = NULL;
    resource->lock_.unlock();
    
    // We need to actually lock the list to erase an element (normally, neither
    // thread will lock on this just to read it).
    sem_wait(&link_list_sem_);
    sem_wait(&link_list_sem_);
    
    link_list_.erase( message.link_name() );
        
    // Unlock the lists.
    sem_post(&link_list_sem_);
    sem_post(&link_list_sem_);
    
    // If the link's cl_info is still set, then the deletion originated at the
    // CLA, not the BPA, and we need remove the link from the contact manager
    // and then delete the resource. Setting the module field NULL (above) will
    // cause ExternalConvergenceLayer::delete_link (called through 
    // ContactManager::del_link) to just return without sending a request back
    // down here.
    if (resource->link_->cl_info() != NULL) {
        //resource->link_->set_cl_info(NULL);
        BundleDaemon::instance()->contactmgr()->del_link(resource->link_, true);
    }
    
    cl_.delete_resource(resource);
    
    // NOTE: The ContactManager posts a LinkDeletedEvent, so we do not need
    // another one.
}

void
ECLModule::handle(const link_attribute_changed_event& message) 
{
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got link_attribute_changed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    // Check for a changed high-water mark.
    if ( message.link_attributes().high_water_mark().present() ) {
        resource->set_high_water_mark(
                message.link_attributes().high_water_mark().get() );
    }
    
    // Check for a changed low-water mark.
    if ( message.link_attributes().low_water_mark().present() ) {
        resource->set_low_water_mark(
                message.link_attributes().low_water_mark().get() );
    }
    
    ContactEvent::reason_t reason = 
            XMLConvert::convert_link_reason( message.reason() );
    
    AttributeVector params;
    const clmessage::link_attributes& attributes = message.link_attributes();
    // These are the only attributes that should be changed by the CLA; should
    // there be yet another XSD type?
    
    if ( attributes.peer_eid().present() ) {
        resource->link_->set_remote_eid( 
            EndpointID( attributes.peer_eid().get() ) );
    }

    if (attributes.nexthop().present()) {
        params.push_back(
            NamedAttribute("nexthop", attributes.nexthop().get()) );
    }
    if (attributes.is_reachable().present()) {
        params.push_back(
            NamedAttribute("is_reachable", attributes.is_reachable().get()) );
    }
    if (attributes.how_reliable().present()) {
        params.push_back(
            NamedAttribute("how_reliable", static_cast<int>(attributes.how_reliable().get())) );
    }
    if (attributes.how_available().present()) {
        params.push_back(
            NamedAttribute("how_available", static_cast<int>(attributes.how_available().get())) );
    }
    // put in the key_value_pairs
    KeyValueSequence::const_iterator iter;
    for (iter = attributes.key_value_pair().begin();
         iter != attributes.key_value_pair().end();
         iter++) {
        params.push_back( NamedAttribute(iter->name(), iter->value()) );
    }
    
    BundleDaemon::post(
        new LinkAttributeChangedEvent(resource->link_, params, reason) );
}

void
ECLModule::handle(const contact_attribute_changed_event& message) 
{
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got link_attribute_changed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    update_contact_attributes( message.contact_attributes(),
                               resource->link_->contact() );
    
    ContactEvent::reason_t reason = 
            XMLConvert::convert_link_reason( message.reason() );
    
    BundleDaemon::post(
            new ContactAttributeChangedEvent(resource->link_->contact(), reason) );
}

void
ECLModule::handle(const link_add_reachable_event& message)
{
    // Check if the contact manager has a link with that name already
    // If it does, it may be in the process of deletion.
    // We will wait for an event to get completely through the event queue
    // before giving up on creating the new link.
    LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(message.link_name().c_str());
    if (link != NULL){
        oasys::Notifier* notifier = new oasys::Notifier("/dtn/cl/external");
        BundleDaemon::post_and_wait(new StatusRequest(), notifier);
        delete notifier;
        link = BundleDaemon::instance()->contactmgr()->find_link(message.link_name().c_str());
        if (link != NULL){
            log_err( "Got link_add_reachable_event for link '%s' that already exists",
                 message.link_name().c_str() );
            return;
        }
    }

    ECLLinkResource* resource;
    const clmessage::link_config_parameters& params =
            message.link_config_parameters();
    resource = get_link( message.link_name() );
    if (resource) {
        log_err( "Got link_add_reachable_event for link '%s' that already exists",
                 message.link_name().c_str() );
        return;
    }
    
    if ( !params.nexthop().present() ) {
        log_err("Got link_add_reachable_event with no nexthop field");
        return;
    }
    
    resource = create_discovered_link( message.peer_eid(),
                                       params.nexthop().get(),
                                       message.link_name() );
}

void
ECLModule::handle(const bundle_transmitted_event& message)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got bundle_transmitted_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }

    BundleRef bundle =
            resource->get_outgoing_bundle( message.bundle_attributes() );
    if ( !bundle.object() ) {
        log_err("Got bundle_transmitted_event for unknown bundle");
        return;
    }
    
    // Take this off the outgoing bundle list for this link.
    if ( !resource->erase_outgoing_bundle( bundle.object() ) ) {
        log_err("Unable to remove bundle %d from the link's outgoing bundle list",
                bundle->bundleid());
    }
    
    // Figure out the absolute path to the file.
    oasys::StringBuffer filename("bundle%d", bundle->bundleid());
    std::string abs_path = bundle_out_path_ + "/" + resource->link_->name_str() +
            "/" + filename.c_str();
    
    // Delete the bundle file.
    ::remove( abs_path.c_str() );
    
    // If we were in state BUSY, see if sending this bundle made us un-busy.
//     if (resource->link_->state() == Link::BUSY) {
//         BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(resource->link_);
//         ASSERT(blocks != NULL);
        
//         size_t total_len = BundleProtocol::total_length(blocks);
//         int queued_bytes = resource->link_->stats()->bytes_queued_ - total_len;
        
//         if ( resource->low_water_mark_crossed(queued_bytes) ) {
//             log_info( "Link %s crossed low-water mark; setting OPEN",
//                       resource->link_->name() ); 
//             // Post a state-change to AVAILABLE in order to get back to OPEN.
//             BundleDaemon::post_at_head(
//                     new LinkStateChangeRequest(resource->link_,
//                                                Link::AVAILABLE,
//                                                ContactEvent::UNBLOCKED) );
//         } // if
        
//         else {
//             log_debug("Low-water mark not crossed; queued bytes: %d",
//                       queued_bytes);
//         }
//     } // if

    // Tell the BundleDaemon about this.
    BundleTransmittedEvent* b_event =
            new BundleTransmittedEvent(bundle.object(),
                                       resource->link_->contact(),
                                       resource->link_,
                                       message.bytes_sent(),
                                       message.reliably_sent());
    BundleDaemon::post(b_event);
}

void
ECLModule::handle(const bundle_canceled_event& message)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got bundle_canceled_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }

    // Find this bundle on the link's outgoing bundle list.
    BundleRef bundle =
            resource->get_outgoing_bundle( message.bundle_attributes() );
    if ( !bundle.object() ) {
        log_err("Got bundle_canceled_event for unknown bundle");
        return;
    }
    
    // Clean up after the bundle and tell the BPA about it.
    bundle_send_failed(resource, bundle.object(), true);
    BundleDaemon::post( new BundleSendCancelledEvent(bundle.object(),
                        resource->link_) );
    
    // If we were in state BUSY, see if sending this bundle made us un-busy.
//     if (resource->link_->state() == Link::BUSY) {
//         BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(resource->link_);
//         ASSERT(blocks != NULL);
        
//         size_t total_len = BundleProtocol::total_length(blocks);
//         int queued_bytes = resource->link_->stats()->bytes_queued_ - total_len;
        
//         if ( resource->low_water_mark_crossed(queued_bytes) ) {
//             log_info( "Link %s crossed low-water mark; setting OPEN",
//                       resource->link_->name() ); 
//             // Post a state-change to AVAILABLE in order to get back to OPEN.
//             BundleDaemon::post_at_head(
//                     new LinkStateChangeRequest(resource->link_,
//                                                Link::AVAILABLE,
//                                                ContactEvent::UNBLOCKED) );
//         } // if
        
//         else {
//             log_debug("Low-water mark not crossed; queued bytes: %d",
//                       queued_bytes);
//         }
//     } // if
}

void
ECLModule::handle(const bundle_receive_started_event& message)
{
    IncomingBundleRecord record;
    record.location = message.location();    
    if ( message.peer_eid().present() )
        record.peer_eid = message.peer_eid().get();
    
    incoming_bundle_list_.push_back(record);
}

void
ECLModule::handle(const bundle_received_event& message)
{
    // A bytes_received of 0 means that nothing (or not enough) was received.
    // We only need to delete the bundle file if it exists.
    if (message.bytes_received() == 0) {
        std::string file_path = bundle_in_path_ + "/" + message.location();
        
        // If the bundle file exists, delete it.
        if (oasys::FileUtils::size( file_path.c_str() ) >= 0) {
            if (::remove( file_path.c_str() ) < 0) {
                log_err( "Unable to remove bundle file %s: %s", 
                         file_path.c_str(), strerror(errno) );
            } // if
        } // if
    } // if

    else {
        std::string peer_eid = EndpointID::NULL_EID().c_str();
        if ( message.peer_eid().present() )
            peer_eid = message.peer_eid().get();
        
        read_bundle_file(message.location(), peer_eid);
    }
    
    // Remove the bundle from the incoming bundle list (if we got a
    // bundle_receive_started_event for it).
    std::list<IncomingBundleRecord>::iterator incoming_i;
    for (incoming_i = incoming_bundle_list_.begin();
    incoming_i != incoming_bundle_list_.end(); ++incoming_i) {
        if ( incoming_i->location == message.location() ) {
            incoming_bundle_list_.erase(incoming_i);
            break;
        } // if
    } // for
}

void 
ECLModule::handle(const report_eid_reachable& message)
{
    BundleDaemon::post(
            new EIDReachableReportEvent( message.query_id(),
                                         message.is_reachable() ) );    
}

void 
ECLModule::handle(const report_link_attributes& message)
{
    AttributeVector attrib_vector;
    
    KeyValueSequence::const_iterator iter;
    for (iter = message.key_value_pair().begin();
         iter != message.key_value_pair().end();
         iter++) {
        attrib_vector.push_back( NamedAttribute(iter->name(), iter->value()) );
    }
    
    BundleDaemon::post( new LinkAttributesReportEvent( message.query_id(),
                        attrib_vector) );   
}

void 
ECLModule::handle(const report_interface_attributes& message) 
{
    AttributeVector attrib_vector;
    
    KeyValueSequence::const_iterator iter;
    for (iter = message.key_value_pair().begin();
         iter != message.key_value_pair().end();
         iter++) {
        attrib_vector.push_back( NamedAttribute(iter->name(), iter->value()) );
    }
    
    BundleDaemon::post( new IfaceAttributesReportEvent( message.query_id(),
                        attrib_vector) );
}

void 
ECLModule::handle(const report_cla_parameters& message) 
{
    AttributeVector attrib_vector;
    
    KeyValueSequence::const_iterator iter;
    for (iter = message.key_value_pair().begin();
         iter != message.key_value_pair().end();
         iter++) {
        attrib_vector.push_back( NamedAttribute(iter->name(), iter->value()) );
    }

    BundleDaemon::post( new CLAParametersReportEvent( message.query_id(), 
                        attrib_vector) );
}

        
void
ECLModule::read_bundle_file(const std::string& location,
                            const std::string& peer_eid)
{
    int bundle_fd;
    bool finished = false;
    off_t file_offset = 0;
    struct stat file_stat;
    
    std::string file_path = bundle_in_path_ + "/" + location;
    
    // Open up the file.
    bundle_fd = oasys::IO::open(file_path.c_str(), O_RDONLY);
    if (bundle_fd < 0) {
        log_err( "Unable to read bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
        return;
    }
    
    // Stat the file so we know how big it is.
    if (oasys::IO::stat(file_path.c_str(), &file_stat) < 0) {
        log_err( "Unable to stat bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
        oasys::IO::close(bundle_fd);
        return;
    }
    
    Bundle* bundle = new Bundle();
    
    // Keep feeding BundleProtocol::consume() chunks until either it indicates
    // that the bundle is finished or we run out of bytes in the file (these
    // two SHOULD happen at the same time). This loop is to ensure that only
    // MAX_BUNDLE_IN_MEMORY bytes are actually mapped in memory at a time.
    while (!finished && file_offset < file_stat.st_size) {
        size_t map_size = std::min(file_stat.st_size - file_offset,
                                   (off_t)MAX_BUNDLE_IN_MEMORY);
        
        // Map the next chunk of file.
        void* bundle_ptr = oasys::IO::mmap(bundle_fd, file_offset, map_size,
                                           oasys::IO::MMAP_RO);
        if (bundle_ptr == NULL) {
            log_err( "Unable to map bundle file %s: %s", file_path.c_str(),
                     strerror(errno) );
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Feed data to BundleProtocol.
        int result = BundleProtocol::consume(bundle, (u_char*)bundle_ptr,
                                             map_size, &finished);
        
        // Unmap this chunk.
        if (oasys::IO::munmap(bundle_ptr, map_size) < 0) {
            log_err("Unable to unmap bundle file");
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Check the result of consume().
        if (result < 0) {
            log_err("Unable to process bundle");
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Update the file offset.
        file_offset += map_size;
    }
    
    // Close the bundle file and then delete it.
    oasys::IO::close(bundle_fd);
    if (::remove( file_path.c_str() ) < 0) {
        log_err( "Unable to remove bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
    }
    
    if (bundle->recv_blocks().size() < 1) {
        log_err("Received bundle does not contain enough information");
        delete bundle;
        return;
    }
    
    // If there are unused bytes in the file, log a warning, but
    // continue anyway.
    if (file_offset < file_stat.st_size) {
        log_warn("Used only %llu of %llu bytes for the bundle", 
                 U64FMT(file_offset), U64FMT(file_stat.st_size));
    }
    
    // Tell the BundleDaemon about this bundle.
    BundleReceivedEvent* b_event =
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, file_stat.st_size, peer_eid);
    BundleDaemon::post(b_event);
}

void
ECLModule::read_cycle() {
    size_t buffer_i = 0;

    // Peek at what's available.
    int result = socket_.recv(read_buffer_, READ_BUFFER_SIZE, MSG_PEEK);

    if (result <= 0) {
        log_err("Connection to CL %s lost: %s", name_.c_str(),
                (result == 0 ? "Closed by other side" : strerror(errno)));

        set_should_stop();
        return;
    } // if

    // Reserve enough room in the message buffer for this chunk and
    // a null terminating character.
    if (msg_buffer_.capacity() < msg_buffer_.size() + (size_t)result + 1)
        msg_buffer_.reserve(msg_buffer_.size() + (size_t)result + 1);

    // Push bytes onto the message buffer until we see the document root
    // closing tag (</dtn>) or run out of bytes.
    while (buffer_i < (size_t)result) {
        msg_buffer_.push_back(read_buffer_[buffer_i++]);

        // Check for the document root closing tag.
        if (msg_buffer_.size() > 12 && 
        strncmp(&msg_buffer_[msg_buffer_.size() - 13], "</cl_message>", 13) == 0) {
            // If we found the closing tag, add the null terminator and
            // parse the document into a CLEvent.
            msg_buffer_.push_back('\0');
            process_cl_event(&msg_buffer_[0], parser_);
                
            msg_buffer_.clear();
            break;
        } // if
    } // while

    // Read to the end of the document for real (we just peeked earlier).
    socket_.recv(read_buffer_, buffer_i, 0);
}

int
ECLModule::send_message(const cl_message* message)
{
    xercesc::MemBufFormatTarget buf;

    try {
        // Create the message and dump it out to 'buf'.
        cl_message_(buf, *message, ExternalConvergenceLayer::namespace_map_,
                    "UTF-8", xml_schema::flags::dont_initialize);
    }
    
    catch (xml_schema::serialization& e) {
        xml_schema::errors::const_iterator err_i;
        for (err_i = e.errors().begin(); err_i != e.errors().end(); ++err_i)
            log_err( "XML serialize error: %s", err_i->message().c_str() );
        
        return 0;
    }
    
    catch (std::exception& e) {
        log_err( "XML serialize error: %s", e.what() );
        return 0;
    }
    
    std::string msg_string( (char*)buf.getRawBuffer(), buf.getLen() );
    log_debug_p("/dtn/cl/XML", "Sending message to module %s:\n%s",
                name_.c_str(), msg_string.c_str() );
    
    // Send the message out the socket.
    int err = socket_.send( (char*)buf.getRawBuffer(), buf.getLen(), 0 );

    if (err < 0) {
        log_err("Socket error: %s", strerror(err));
        log_err("Connection with CL %s lost", name_.c_str());

        return -1;
    }
    
    return 0;
}

int 
ECLModule::prepare_bundle_to_send(cl_message* message)
{
    bundle_send_request request = message->bundle_send_request().get();
    
    // Find the link that this is going on.
    ECLLinkResource* link_resource = get_link( request.link_name() );
    if (!link_resource) {
        log_err( "Got bundle_send_request for unknown link %s",
                 request.link_name().c_str() );
        return 0;
    }
    
    // Find the bundle on the outgoing bundle list.
    BundleRef bundle = 
            link_resource->get_outgoing_bundle( request.bundle_attributes() );
    if ( !bundle.object() ) {
        log_err( "Got bundle_send_request for unknown bundle");
        return 0;
    }
    
    // Grab the bundle blocks for this bundle on this link.
    BlockInfoVec* blocks =
        bundle->xmit_blocks()->find_blocks(link_resource->link_);
    if (!blocks) {
        log_err( "Bundle id %d on link %s has no block vectors",
                 bundle->bundleid(), request.link_name().c_str() );
        return 0;
    }
    
    // Calculate the total length of this bundle.
    off_t total_length = BundleProtocol::total_length(blocks);
    
    // Figure out the path to the file.
    std::string abs_path = bundle_out_path_ + "/" + request.location();
    
    // Create and open the file.
    int bundle_fd = oasys::IO::open(abs_path.c_str(), O_RDWR | O_CREAT | O_EXCL,
                                    0644);
    if (bundle_fd < 0) {
        log_err( "Unable to create bundle file %s: %s",
                 request.location().c_str(), strerror(errno) );
        return 0;
    }
    
    // "Truncate" (expand, really) the file to the size of the bundle.
    if (oasys::IO::truncate(bundle_fd, total_length) < 0) {
        log_err( "Unable to resize bundle file %s: %s",
                 request.location().c_str(), strerror(errno) );
        oasys::IO::close(bundle_fd);
        bundle_send_failed(link_resource, bundle.object(), true);
        return 0;
    }
    
    off_t offset = 0;
    bool done = false;
    
    while (offset < total_length) {
        // Calculate the size of the next chunk and map it.
        off_t map_size = std::min(total_length - offset,
                                  (off_t)MAX_BUNDLE_IN_MEMORY);
        void* bundle_ptr = oasys::IO::mmap(bundle_fd, offset, map_size,
                                           oasys::IO::MMAP_RW);
        if (bundle_ptr == NULL) {
            log_err( "Unable to map output file %s: %s",
                     request.location().c_str(), strerror(errno) );
            oasys::IO::close(bundle_fd);
            bundle_send_failed(link_resource, bundle.object(), true);
            return -1;
        }
        
        // Feed the next piece of bundle through BundleProtocol.
        BundleProtocol::produce(bundle.object(), blocks, (u_char*)bundle_ptr,
                                offset, map_size, &done);
        
        // Unmap this chunk.
        oasys::IO::munmap(bundle_ptr, map_size);
        offset += map_size;
    } 
    
    oasys::IO::close(bundle_fd);
    
    // Send this event to the module.
    return send_message(message);
}

void
ECLModule::bundle_send_failed(ECLLinkResource* link_resource,
                              Bundle* bundle,
                              bool erase_from_list)
{
    ContactRef contact = link_resource->link_->contact();
    
    // Take the bundle off of the outgoing bundles list.
    if (erase_from_list)
        link_resource->erase_outgoing_bundle(bundle);
    
    // Figure out the relative and absolute path to the file.
    oasys::StringBuffer filename_buf("bundle%d", bundle->bundleid());

    // Delete the bundle file.
    ::remove( (bundle_out_path_ + "/" + link_resource->link_->name_str() + "/" +
            filename_buf.c_str()).c_str() );
}

ECLInterfaceResource*
ECLModule::get_interface(const std::string& name) const
{
    oasys::ScopeLock l(&iface_list_lock_, "get_interface");
    std::list<ECLInterfaceResource*>::const_iterator iface_i;

    for (iface_i = iface_list_.begin(); iface_i != iface_list_.end();
    ++iface_i) {
        if ( (*iface_i)->interface_->name() == name)
            return *iface_i;
    }

    return NULL;
}

ECLLinkResource*
ECLModule::get_link(const std::string& name) const
{
    sem_wait(&link_list_sem_);
    
    // First, check on the normal link list.
    LinkHashMap::const_iterator link_i = link_list_.find(name);
    if ( link_i == link_list_.end() ) {
        sem_post(&link_list_sem_);
        return NULL;
    }
    
    sem_post(&link_list_sem_);
    return link_i->second;
}

bool
ECLModule::link_exists(const std::string& name) const
{
    sem_wait(&link_list_sem_);
    
    // First, check on the normal link list.
    LinkHashMap::const_iterator link_i = link_list_.find(name);
    if ( link_i == link_list_.end() ) {
        sem_post(&link_list_sem_);
        return false;
    }

    sem_post(&link_list_sem_);
    return true;
}

ECLLinkResource*
ECLModule::create_discovered_link(const std::string& peer_eid,
                                  const std::string& nexthop,
                                  const std::string& link_name)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    
    //lock the contact manager so no one opens the link before we do
    oasys::ScopeLock l(cm->lock(), "ECLModule::create_discovered_link");
    
    if (cm->has_link(link_name.c_str())) {
        log_err("A link with name %s already exists; can't create duplicate",
                link_name.c_str());
        return NULL;
    }
    
    LinkRef link = Link::create_link(link_name, Link::OPPORTUNISTIC, &cl_,
                                     nexthop.c_str(), 0, NULL);
    if (link == NULL) {
        log_err("Unexpected error creating opportunistic link");
        return NULL;
    }
    
    LinkRef new_link(link.object(),
                     "ECLModule::create_discovered_link: the new link");
    
    new_link->set_remote_eid(peer_eid);

    // The LinkCreatedEvent is posted below.
    new_link->set_create_pending(true);
    
    if (ExternalConvergenceLayer::discovered_prev_hop_header_)
        new_link->params().prevhop_hdr_ = true;
    
    if (!cm->add_new_link(new_link)) {
        new_link->delete_link();
        log_err( "Failed to add new opportunistic link %s", new_link->name() );
        new_link = NULL;
        return NULL;
    }
    
    // Create the resource holder for this link.
    ECLLinkResource* resource =
            new ECLLinkResource(name_, NULL, new_link, true);
    oasys::ScopeLock res_lock(&resource->lock_, "create_discovered_link");
    new_link->set_cl_info(resource);
    new_link->set_state(Link::AVAILABLE);
    resource->module_ = this;
    resource->should_delete_ = false;

    // The link object must be fully created before releasing this lock.
    l.unlock();
    
    // Wait twice on the semaphore to actually lock it.
    sem_wait(&link_list_sem_);
    sem_wait(&link_list_sem_);
    
    // Add this link to our list of links.
    link_list_.insert( LinkHashMap::value_type(link_name.c_str(), resource) );
    
    // Unlock the semaphore.
    sem_post(&link_list_sem_);
    sem_post(&link_list_sem_);

    // Notify the system that the new link is available for use.
    new_link->set_create_pending(false);
    //BundleDaemon::post(new LinkCreatedEvent(new_link));
    
    return resource;
}

void
ECLModule::cleanup() {
    LinkHashMap::const_iterator link_i;
    
    // First, let the BundleDaemon know that all of the links are closed.
    for (link_i = link_list_.begin(); link_i != link_list_.end();
    ++link_i) {
        ECLLinkResource* resource = link_i->second;
        
        oasys::ScopeLock res_lock(&resource->lock_, "ECLModule::cleanup");
        resource->module_ = NULL;
        resource->known_state_ = Link::CLOSED;
        
        // Only report the closing if the link is currently open (otherwise,
        // the bundle daemon nags).
        Link::state_t current_state = resource->link_->state();
        if (current_state == Link::OPEN || current_state == Link::OPENING) {
            BundleDaemon::post( new LinkStateChangeRequest(resource->link_,
                                Link::CLOSED, ContactEvent::NO_INFO) );
        }
        
        // Get this link's outgoing bundles.
        BundleList& bundle_set = resource->get_bundle_set();
        oasys::ScopeLock bundle_lock(bundle_set.lock(), "ECLModule::cleanup");
        
        // For each outgoing bundle, call bundle_send_failed to clean the
        // bundle.
        BundleList::iterator bundle_i;
        for (bundle_i = bundle_set.begin(); bundle_i != bundle_set.end();
        ++bundle_i) {
            bundle_send_failed(resource, *bundle_i, false);
        }
        
        // Clear the list of bundles that we just canceled.
        bundle_set.clear();
        
        // Remove the link's outgoing bundle directory.
        std::string outgoing_dir = bundle_out_path_ + "/" +
                resource->link_->name_str();
        ::remove( outgoing_dir.c_str() );
    }
    
    // Clean up any bundles for which we received a bundle_receive_started_event
    // but no bundle_received_event. This will post BundleReceivedEvents for
    // the partial bundles.
    std::list<IncomingBundleRecord>::iterator incoming_i;
    for (incoming_i = incoming_bundle_list_.begin();
    incoming_i != incoming_bundle_list_.end(); ++incoming_i)
        read_bundle_file( incoming_i->location, EndpointID::NULL_EID().c_str() );
    
    // At this point, we know that there are no links or interfaces pointing
    // to this module, so no new messages will come in.
    cl_message* message;
    while ( message_queue_.try_pop(&message) )
        delete message;

    // Give our interfaces and non-temporary links back to the CL.
    cl_.give_resources(iface_list_);
    cl_.give_resources(link_list_);
    
    // Delete the module's incoming and outgoing bundle directories.
    ::remove( bundle_in_path_.c_str() );
    ::remove( bundle_out_path_.c_str() );
}

void
ECLModule::update_contact_attributes(const contact_attributes& attributes,
                                     const ContactRef& contact)
{
    // XXX/demmer I don't think this should be able to set the start
    // time, but I'll leave the hook in there for now
    contact->set_start_time(oasys::Time(attributes.start_time() / 1000,
                                        attributes.start_time() * 1000));
    contact->set_duration(attributes.duration());
    contact->set_bps(attributes.bps());
    contact->set_latency(attributes.latency());
}

} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_CL_ENABLED
