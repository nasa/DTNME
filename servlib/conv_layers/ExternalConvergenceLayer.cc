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
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <oasys/io/NetUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/thread/Lock.h>
#include <oasys/util/OptParser.h>

#include "ExternalConvergenceLayer.h"
#include "ECLModule.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

//----------------------------------------------------------------------
bool ExternalConvergenceLayer::client_validation_ = true;
std::string ExternalConvergenceLayer::schema_ = "";
in_addr_t ExternalConvergenceLayer::server_addr_ = inet_addr("127.0.0.1");
u_int16_t ExternalConvergenceLayer::server_port_ = 5070;
bool ExternalConvergenceLayer::create_discovered_links_ = false;
bool ExternalConvergenceLayer::discovered_prev_hop_header_ = false;
xml_schema::namespace_infomap ExternalConvergenceLayer::namespace_map_;

//----------------------------------------------------------------------
ExternalConvergenceLayer::ExternalConvergenceLayer() :
ConvergenceLayer("ExternalConvergenceLayer", "extcl"),
global_resource_lock_("/dtn/cl/parts/global_resource_lock"),
module_mutex_("/dtn/cl/parts/module_mutex"),
resource_mutex_("/dtn/cl/parts/resource_mutex"),
listener_(*this)
{

}

//----------------------------------------------------------------------
ExternalConvergenceLayer::~ExternalConvergenceLayer()
{

}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::start()
{
    log_info("ExternalConvergenceLayer started");
    
    // Make sure that we were given a schema file to work with.
    if ( schema_ == std::string("") ) {
        log_err("No XML schema file specified."
                " ExternalConvergenceLayer is disabled.");
        return;
    }
    
    log_info( "Using XML schema file %s", schema_.c_str() );
    if (client_validation_)
        namespace_map_[""].schema = schema_.c_str();
    
    // Start the listener thread.
    listener_.start();
}

//----------------------------------------------------------------------
bool 
ExternalConvergenceLayer::set_cla_parameters(AttributeVector &params)
{
    ECLModule* module = NULL;
    KeyValueSequence param_sequence;
    cla_set_params_request request;

    AttributeVector::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
        if (iter->name() == "create_discovered_links") {
            request.create_discovered_links(iter->bool_val());
        }
        else if (iter->name() == "protocol") {
            // Get the protocol.
            module = get_module(iter->string_val());
            if (!module) {
                log_err(
                    "No CLA for protocol '%s' exists; not setting CLA parameters",
                    iter->string_val().c_str() );
                return false;
            }
        }
        else {
            param_sequence.push_back( clmessage::key_value_pair(iter->name(),
                iter->string_val()) );
        }
    }

    if (!module) {
        log_err( "No protocol provided; not setting CLA parameters" );
        return false;
    }

    request.key_value_pair(param_sequence);

    // Post the message.
    cl_message* message = new cl_message();
    message->cla_set_params_request(request);
    POST_MESSAGE(module, cla_set_params_request, request);

    return true;
}

//----------------------------------------------------------------------
bool 
ExternalConvergenceLayer::set_interface_defaults(int argc, const char* argv[],
                                                 const char** invalidp)
{
    std::string proto_option;
    oasys::OptParser parser;

    // Find the protocol in the arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    int parse_result = parser.parse_and_shift(argc, argv, invalidp);
    
    if (parse_result < 0) {
        log_err("Error parsing interface option '%s'", *invalidp);
        return false;
    }
    
    if (proto_option.size() == 0) {
        log_err("Error parsing interface options: no 'protocol' option given");
        return false;
    }
    
    // See if we have a module for this protocol.
    ECLModule* module = get_module(proto_option);
    if (!module) {
        log_err( "No CLA for protocol '%s' exists; not setting interface defaults",
                 proto_option.c_str() );
        return false;
    }
    
    argc -= parse_result;
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    KeyValueSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    
    // Create the request message.
    interface_set_defaults_request request;
    request.key_value_pair(param_sequence);
    
    // Post the message.
    cl_message* message = new cl_message();
    message->interface_set_defaults_request(request);
    POST_MESSAGE(module, interface_set_defaults_request, request);
    
    return true;
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::interface_up(Interface* iface, int argc,
                                       const char* argv[])
{
    std::string proto_option;
    oasys::OptParser parser;
    const char* invalidp;

    // Find the protocol for this interface in the arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    int parse_result = parser.parse_and_shift(argc, argv, &invalidp);
    
    if (parse_result < 0) {
        log_err("Error parsing interface option '%s'", invalidp);
        return false;
    }
    
    if (proto_option.size() == 0) {
        log_err("Error parsing interface options: no 'protocol' option given");
        return false;
    }
    
    argc -= parse_result;
    
    log_debug( "Adding interface %s for protocol %s", iface->name().c_str(),
               proto_option.c_str() );
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    KeyValueSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    
    // Create the request message.
    interface_create_request request;
    request.interface_name( iface->name() );
    request.key_value_pair(param_sequence);
    
    cl_message* message = new cl_message();
    message->interface_create_request(request);
    
    // Find the owner module for this interface, if it exists.
    ECLModule* module = get_module(proto_option);
    ECLInterfaceResource* resource =
            new ECLInterfaceResource(proto_option, message, iface);

    iface->set_cl_info(resource);
    
    oasys::ScopeLock l(&resource->lock_, "interface_up");

    // Send the interface to the module if one was found for this protocol...
    if (module) {
        module->take_resource(resource);
    }

    // otherwise, add it to the unclaimed resource list.
    else {
        log_warn("No CLA for protocol '%s' exists. "
                "Deferring initialization of interface %s",
                proto_option.c_str(), iface->name().c_str());

        add_resource(resource);
    }

    return true;
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::interface_down(Interface* iface)
{
    ECLInterfaceResource* resource =
            dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );
    ASSERT(resource);
    
    // Create and post the request message.
    interface_destroy_request request;
    request.interface_name( iface->name() );
    cl_message* message = new cl_message();
    message->interface_destroy_request(request);
    POST_MESSAGE(resource->module_, interface_destroy_request, request);
    
    // Remove this interface from the module's list and delete the resource.
    resource->module_->remove_interface( iface->name() );
    delete resource;

    return true;
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::dump_interface(Interface* iface,
                                         oasys::StringBuffer* buf)
{
    ECLInterfaceResource* resource =
            dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );
    ASSERT(resource);

    buf->appendf( "\tprotocol: %s\n", resource->protocol_.c_str() );
    
    const KeyValueSequence& param_sequence = resource->create_message_->
            interface_create_request().get().key_value_pair();
    
    KeyValueSequence::const_iterator param_i;
    for (param_i = param_sequence.begin(); param_i != param_sequence.end();
    ++param_i) {
        buf->appendf( "\t%s=%s\n", param_i->name().c_str(),
                      param_i->value().c_str() );
    }
}

//----------------------------------------------------------------------
bool 
ExternalConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                            const char** invalidp)
{
    bool reactive_fragment;
    bool reactive_fragment_set = false;
    bool is_usable;
    bool is_usable_set = false;
    std::string nexthop;
    std::string proto_option;
    oasys::OptParser parser;
    
    // Find the protocol and fixed arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    parser.addopt(new oasys::BoolOpt("is_usable", &is_usable, "", &is_usable_set));
    parser.addopt(new oasys::BoolOpt("reactive_fragment", &reactive_fragment, "",
                  &reactive_fragment_set));
    parser.addopt(new oasys::StringOpt("nexthop", &nexthop));    
    int parse_result = parser.parse_and_shift(argc, argv, invalidp);
    
    if (parse_result < 0) {
        log_err("Error parsing link option '%s'", *invalidp);
        return false;
    }
    
    // Make sure that we got a protocol.
    if (proto_option.size() == 0) {
        log_err("Error parsing link options: no 'protocol' option given");
        return false;
    }
    
    // See if we have a module for this protocol.
    ECLModule* module = get_module(proto_option);
    if (!module) {
        log_err( "No CLA for protocol '%s' exists; not setting link defaults",
                 proto_option.c_str() );
        return false;
    }
    
    argc -= parse_result;
    
    // Set the fixed parameters.
    link_config_parameters config_params;
    if (is_usable_set)
        config_params.is_usable(is_usable);
    if (reactive_fragment_set)
        config_params.reactive_fragment(reactive_fragment);
    if (nexthop.size() > 0)
        config_params.nexthop(nexthop);
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    KeyValueSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    config_params.key_value_pair(param_sequence);
    
    // Create the request message.
    link_set_defaults_request request;
    request.link_config_parameters(config_params);
    
    // Post the message.
    cl_message* message = new cl_message();
    message->link_set_defaults_request(request);
    POST_MESSAGE(module, link_set_defaults_request, request);
    
    return true;
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::init_link(const LinkRef& link,
                                    int argc, const char* argv[])
{
    bool reactive_fragment;
    bool reactive_fragment_set = false;
    bool is_usable;
    bool is_usable_set = false;
    std::string nexthop;
    std::string proto_option;
    oasys::OptParser parser;
    const char* invalidp;

    // If these are true, this is probably a request coming from
    // ContactManager::new_opportunistic_link(), which means the link already
    // exists.
    if (argc == 0 && argv == NULL)
        return true;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    // Find the protocol and fixed arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    parser.addopt(new oasys::BoolOpt("is_usable", &is_usable, "", &is_usable_set));
    parser.addopt(new oasys::BoolOpt("reactive_fragment", &reactive_fragment, "",
                  &reactive_fragment_set));
    int parse_result = parser.parse_and_shift(argc, argv, &invalidp);
    
    if (parse_result < 0) {
        log_err("Error parsing link option '%s'", invalidp);
        return false;
    }
    
    if (proto_option.size() == 0) {
        log_err("Error parsing link options: no 'protocol' option given");
        return false;
    }
    
    log_debug( "Adding link %s for protocol %s", link->name(),
               proto_option.c_str() );
    
    ECLModule* module = get_module(proto_option);
    
    if ( module && module->link_exists( link->name() ) ) {
        log_err( "Link %s already exists on CL %s", link->name(),
                 proto_option.c_str() );
        return false;
    }
    
    argc -= parse_result;
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    KeyValueSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    
    // Create the request message.
    link_create_request request;
    request.link_name( link->name_str() );
    request.type( XMLConvert::convert_link_type( link->type() ) );
    request.peer_eid( link->remote_eid().str() );
    
    // Create the params struct.
    link_config_parameters config_params;
    config_params.nexthop( link->nexthop() );
    
    // Set the fixed parameters.
    if (is_usable_set)
        config_params.is_usable(is_usable);
    if (reactive_fragment_set)
        config_params.reactive_fragment(reactive_fragment);
    
    // Set the key-value parameters.
    config_params.key_value_pair(param_sequence);
    request.link_config_parameters(config_params);
    
    cl_message* message = new cl_message();
    message->link_create_request(request);
    
    ECLLinkResource* resource =
            new ECLLinkResource(proto_option, message, link, false);
    
    oasys::ScopeLock l(&resource->lock_, "init_link");
    
    link->set_cl_info(resource);
    link->set_create_pending(true);
    
    if (module) {
        module->take_resource(resource);
    }
    
    else {
        log_warn( "No CL with protocol '%s' exists. "
                  "Deferring initialization of link %s",
                  proto_option.c_str(), link->name() );
        add_resource(resource);
    }

    return true;
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);

    log_debug("ExternalConvergenceLayer::delete_link: "
              "deleting link %s", link->name());
    
    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::delete_link");

    ASSERT(!link->isdeleted());

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    
    // This is the case if the link deletion originated in the CLA. In that
    // case, ECLModule already deleted the resource and there is nothing else
    // to do.
    /*if (resource == NULL) {
        log_debug("ExternalConvergenceLayer::delete_link: link %s was deleted "
                  "by the CLA", link->name());
        return;
    }*/
    
    oasys::ScopeLock lock(&resource->lock_, "delete_link");
    
    // Clear out this link's cl_info.
    link->set_cl_info(NULL);
    
    // If the link is unclaimed, just remove it from the resource list.
    if (resource->module_ == NULL) {
        lock.unlock();
        if (resource->should_delete_)
            delete_resource(resource);
    
        // NOTE: The ContactManager posts a LinkDeletedEvent, so we do not
        // need another one.
        return;
    }
    
    // Otherwise, send a message to the CLA to have it delete the link.
    link_delete_request request;
    request.link_name( link->name_str() );
    POST_MESSAGE(resource->module_, link_delete_request, request); 
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::dump_link(const LinkRef& link,
                                    oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    
    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::reconfigure_link");
    
    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will
    // ignore the command.
    if ( link->isdeleted() ) {
        log_err( "Cannot dump deleted link %s", link->name() );
        return;
    }
    
    if (link->cl_info() == NULL) {
        log_err( "Cannot dump deleted link %s", link->name() );
        return;
    }

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);

    buf->appendf( "\nprotocol: %s\n", resource->protocol_.c_str() );
    
    const KeyValueSequence& param_sequence = resource->create_message_->
            link_create_request().get().link_config_parameters().key_value_pair();
    
    KeyValueSequence::const_iterator param_i;
    for (param_i = param_sequence.begin(); param_i != param_sequence.end();
    ++param_i) {
        buf->appendf( "%s=%s\n", param_i->name().c_str(),
                      param_i->value().c_str() );
    }
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::reconfigure_link(const LinkRef& link, int argc,
                                                const char* argv[])
{
    bool reactive_fragment;
    bool reactive_fragment_set = false;
    bool is_usable;
    bool is_usable_set = false;
    std::string nexthop;
    oasys::OptParser parser;
    const char* invalidp;

    // If these are true, this is probably a request coming from
    // ContactManager::new_opportunistic_link(), which means the link already
    // exists.
    if (argc == 0 && argv == NULL)
        return true;
    
    ASSERT(link != NULL);
    
    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::reconfigure_link");
    
    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will
    // ignore the command.
    if ( link->isdeleted() ) {
        log_err( "Cannot reconfigure deleted link %s", link->name() );
        return false;
    }
    
    if (link->cl_info() == NULL) {
        log_err( "Cannot reconfigure deleted link %s", link->name() );
        return false;
    }

    // Find the protocol and fixed arguments.
    parser.addopt(new oasys::BoolOpt("is_usable", &is_usable, "", &is_usable_set));
    parser.addopt(new oasys::BoolOpt("reactive_fragment", &reactive_fragment, "",
                  &reactive_fragment_set));
    parser.addopt(new oasys::StringOpt("nexthop", &nexthop));    
    int parse_result = parser.parse_and_shift(argc, argv, &invalidp);
    
    if (parse_result < 0) {
        log_err("Error parsing link option '%s'", invalidp);
        return false;
    }

    cl_message* old_create_message_;
    cl_message* new_create_message_;
    KeyValueSequence::iterator new_param_i;
            
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    KeyValueSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    
    oasys::ScopeLock l(&resource->lock_, "reconfigure_link");
    
    // Get a pointer to the old link-create message.
    old_create_message_ = resource->create_message_;
    
    // Create a new link-create request as a copy of the old one.
    link_create_request new_create_request =
            old_create_message_->link_create_request().get();
    
    // Set the fixed parameters.
    if (is_usable_set)
        new_create_request.link_config_parameters().is_usable(is_usable);
    if (reactive_fragment_set)
        new_create_request.link_config_parameters().reactive_fragment(
                reactive_fragment);
    if (nexthop.size() > 0)
        new_create_request.link_config_parameters().nexthop(nexthop);
    
    // Get a copy of the existing key-value parameters so we can update them.
    KeyValueSequence& existing_params = 
            new_create_request.link_config_parameters().key_value_pair();
    
    // Run through the new parameters, updating the values in existing_params.
    for (new_param_i = param_sequence.begin();
    new_param_i != param_sequence.end(); ++new_param_i) {
        KeyValueSequence::iterator existing_param_i;
                
        for (existing_param_i = existing_params.begin();
        existing_param_i != existing_params.end(); ++existing_param_i) {
            if ( new_param_i->name() == existing_param_i->name() ) {
                *existing_param_i = *new_param_i;
                break;
            } // if
        } // for
    } // for
    
    // Create the new link-create message for this link, then delete the old.
    new_create_message_ = new cl_message();
    new_create_message_->link_create_request(new_create_request);
    resource->create_message_ = new_create_message_;
    delete old_create_message_;
    
    if (!resource->module_) {
        log_warn( "Link %s is unclaimed; new settings will apply when it is claimed",
                  link->name() );
        return true;
    }

    // Create the link-reconfigure message.
    link_reconfigure_request request;
    request.link_name( link->name_str() );
    
    // Create the params struct.
    link_config_parameters config_params;
    config_params.nexthop( link->nexthop() );
    
    // Set the fixed parameters.
    if (is_usable_set)
        config_params.is_usable(is_usable);
    if (reactive_fragment_set)
        config_params.reactive_fragment(reactive_fragment);
    
    // Set the key-value parameters.
    config_params.key_value_pair(param_sequence);
    
    // Create and send the message.
    request.link_config_parameters(config_params);    
    cl_message* message = new cl_message();
    message->link_reconfigure_request(request);
    POST_MESSAGE(resource->module_, link_reconfigure_request, request);
    
    return true;
}    

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::reconfigure_link(const LinkRef& link,
                                           AttributeVector& params)
{
    ASSERT(link != NULL);
    
    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::reconfigure_link");
    
    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will
    // ignore the command.
    if ( link->isdeleted() ) {
        log_err( "Cannot reconfigure deleted link %s", link->name() );
        return;
    }
    
    if (link->cl_info() == NULL) {
        log_err( "Cannot reconfigure deleted link %s", link->name() );
        return;
    }
        
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    if (!resource) {
        log_err( "Cannot reconfigure link that does not exist" );
        return;
    }

    link_reconfigure_request request;
    request.link_name( link->name_str() );
    link_config_parameters config_params;
    KeyValueSequence param_sequence;

    AttributeVector::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
        if (iter->name() == "is_usable") {
            config_params.is_usable( iter->bool_val() );
        }
        else if (iter->name() == "reactive_fragment") {
            config_params.reactive_fragment( iter->bool_val() );
        }
        else if (iter->name() == "nexthop") {
            config_params.nexthop( iter->string_val() );
        }
        else {
            param_sequence.push_back( clmessage::key_value_pair(iter->name(),
                iter->string_val()) );
        }
    }
    config_params.key_value_pair(param_sequence);
    request.link_config_parameters(config_params);
    
    POST_MESSAGE(resource->module_, link_reconfigure_request, request); 
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::open_contact(const ContactRef& contact)
{
    oasys::ScopeLock grl(&global_resource_lock_, "open_contact");
    LinkRef link = contact->link();
    
    ASSERT(link != NULL);

    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::open_contact");
    
    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will
    // ignore the command.
    if ( link->isdeleted() ) {
        log_err( "Cannot open contact on deleted link %s", link->name() );
        return false;
    }

    ASSERT(link->cl_info() != NULL);
    
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    if (!resource) {
        log_err( "Cannot open contact on link that does not exist" );
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactEvent::NO_INFO));
        return false;
    }
    
    oasys::ScopeLock l(&resource->lock_, "open_contact");
    
    if (!resource->module_) {
        log_err( "Cannot open contact on unclaimed link %s", link->name() );
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactEvent::NO_INFO));
        return false;
    }
    
    if (resource->known_state_ != Link::OPEN) {
        resource->known_state_ = Link::OPEN;
        link_open_request request;
        request.link_name( link->name_str() );
        POST_MESSAGE(resource->module_, link_open_request, request);
        return true;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
ExternalConvergenceLayer::close_contact(const ContactRef& contact)
{
    oasys::ScopeLock grl(&global_resource_lock_, "open_contact");
    LinkRef link = contact->link();

    ASSERT(link != NULL);

    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::close_contact");
    
    // Ignore destroyed links
    if (link->cl_info() == NULL) {
        log_warn( "Ignoring close contact request for destroyed link %s",
                  link->name() );
        return true; 
    }
    
    ECLLinkResource* resource =
                     dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "close_contact");
    
    // This indicates that the closing originated in the ECLModule.
    if (resource->known_state_ == Link::CLOSED)
        return true;
    
    if (resource->module_ == NULL) {
        log_err( "close_contact called for unclaimed link %s", link->name() );        
        return true;
    }
    
    resource->known_state_ = Link::CLOSED;
                         
    // Send a link close request to the CLA.
    link_close_request request;
    request.link_name( link->name_str() );
    POST_MESSAGE(resource->module_, link_close_request, request); 
    
    return true;
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)   
{
    oasys::ScopeLock grl(&global_resource_lock_, "send_bundle");
    
    ASSERT(link != NULL);

    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::send_bundle");

    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will silently
    // ignore the command.
    if ( link->isdeleted() )
        return;

    ASSERT(link->cl_info() != NULL);
    
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "send_bundle");

    // Make sure that this link is claimed by a module before trying to send
    // something through it.
    if (!resource->module_) {
        log_err("Cannot send bundle on nonexistent CL %s through link %s",
                resource->protocol_.c_str(), link->name());
        return;
    }
    
    log_debug( "Sending bundle %d on link %s", bundle->bundleid(),
               link->name() );
    
    // Figure out the relative and absolute path to the file.
    oasys::StringBuffer filename_buf("bundle%d", bundle->bundleid());
    std::string filename = link->name_str() + "/" +
            std::string( filename_buf.c_str() );
    
    // Create the request message.
    bundle_send_request request;
    request.location(filename);
    request.link_name( link->name_str() );
    
    // Create and fill in the bundle attributes for this bundle.
    bundle_attributes bundle_attribs;
    fill_bundle_attributes(bundle, bundle_attribs);
    request.bundle_attributes(bundle_attribs);
    
    // Pass the bundle and the message on to the ECLModule.
    resource->add_outgoing_bundle(bundle.object());
    POST_MESSAGE(resource->module_, bundle_send_request, request);
    
    // If this bundle causes the number of queued bytes to exceed the link's
    // high-water mark, set the link's state to BUSY. 
//     if ( resource->high_water_mark_crossed(link->stats()->bytes_queued_) ) {
//         log_info( "Link %s has crossed its high-water mark; setting it to BUSY",
//                   link->name() );
//         link->set_state(Link::BUSY);
//         BundleDaemon::post_at_head(new LinkBusyEvent(link));
//     }
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::cancel_bundle(const LinkRef& link, const BundleRef& bundle)
{
    oasys::ScopeLock grl(&global_resource_lock_, "cancel_bundle");
    
    ASSERT(link != NULL);
    oasys::ScopeLock link_lock(link->lock(), 
                               "ExternalConvergenceLayer::cancel_bundle");
    
    // 7/13/07 - jward - this had been an assert, but it was happening
    // frequently on the testbed. If the link is deleted, we will silently
    // ignore the command.
    if ( link->isdeleted() )
        return;
    
    if ( link->cl_info() == NULL)
        return;

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "cancel_bundle");

    // Make sure that this link is claimed by a module.
    if (!resource->module_) {
        log_err("Cannot cancel bundle on nonexistent CL %s through link %s",
                resource->protocol_.c_str(), link->name());
        return;
    }
    
    log_info( "Cancelling bundle %d on link %s", bundle->bundleid(),
               link->name() );
    
    // Create the request message.
    bundle_cancel_request request;
    request.link_name( link->name_str() );
    
    // Create and fill in the bundle attributes for this bundle.
    bundle_attributes bundle_attribs;
    fill_bundle_attributes(bundle, bundle_attribs);
    request.bundle_attributes(bundle_attribs);
    
    POST_MESSAGE(resource->module_, bundle_cancel_request, request);
    return;
}

//----------------------------------------------------------------------
bool 
ExternalConvergenceLayer::is_queued(const LinkRef& link, Bundle* bundle)
{
    oasys::ScopeLock grl(&global_resource_lock_, "is_queued");

    ASSERT(link != NULL);
    oasys::ScopeLock link_lock(link->lock(),
                               "ExternalConvergenceLayer::is_queued");
    if (link->isdeleted()) {
        log_debug("ExternalConvergenceLayer::is_queued: "
                  "cannot check bundle queue on deleted link %s", link->name());
        return false;
    }
    
    if (link->cl_info() == NULL)
        return false;

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "is_queued");
    
    // Check with the resource to see if the bundle is queued on it.
    return resource->has_outgoing_bundle(bundle);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::is_eid_reachable(const std::string& query_id,
                                           Interface* iface,
                                           const std::string& endpoint)
{
    ECLInterfaceResource* resource =
        dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "is_eid_reachable");

    // Make sure that this link is claimed by a module.
    if (!resource->module_) {
        log_err( "Cannot query unclaimed interface %s", iface->name().c_str() );
        BundleDaemon::post(new EIDReachableReportEvent(query_id, false));
        return;
    }

    query_eid_reachable query;
    query.query_id(query_id);
    query.interface_name( iface->name() );
    query.peer_eid(endpoint);
    
    POST_MESSAGE(resource->module_, query_eid_reachable, query);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::query_link_attributes(const std::string& query_id,
                                                const LinkRef& link,
                                                const AttributeNameVector& attributes)
{
    ASSERT(link != NULL);
    
    oasys::ScopeLock link_lock(link->lock(),
                               "ExternalConvergenceLayer::query_link_attributes");

    if (link->isdeleted()) {
        log_debug("ConvergenceLayer::query_link_attributes: "
                  "link %s already deleted", link->name());
        return;
    }

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "query_link_attributes");
    if (resource->module_ == NULL) {
        log_err( "query_link_attributes called for unclaimed link %s",
                 link->name() );
        BundleDaemon::post( new LinkAttributesReportEvent( query_id,
                            AttributeVector() ) );
                
        return;
    }
    
    clmessage::query_link_attributes query;
    query.query_id(query_id);
    query.link_name( link->name_str() );

    AttributeNameVector::const_iterator attrib_i;
    for (attrib_i = attributes.begin(); attrib_i != attributes.end(); ++attrib_i)
        query.attribute_name().push_back( attribute_name( attrib_i->name() ) );
    
    POST_MESSAGE(resource->module_, query_link_attributes, query);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::query_iface_attributes(const std::string& query_id, 
                                        Interface* iface,
                                        const AttributeNameVector& attributes)
{
    ECLInterfaceResource* resource =
            dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock_, "query_iface_attributes");
    if (resource->module_ == NULL) {
        log_err( "query_iface_attributes called for unclaimed interface %s",
                 iface->name().c_str() );
        BundleDaemon::post( new IfaceAttributesReportEvent( query_id, 
                            AttributeVector() ) );
        return;
    }
    
    query_interface_attributes query;
    query.query_id(query_id);
    query.interface_name( iface->name() );

    AttributeNameVector::const_iterator attrib_i;
    for (attrib_i = attributes.begin(); attrib_i != attributes.end(); ++attrib_i)
        query.attribute_name().push_back( attribute_name( attrib_i->name() ) );
    
    POST_MESSAGE(resource->module_, query_interface_attributes, query);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::query_cla_parameters(const std::string& query_id,
                                         const AttributeNameVector& parameters)
{
    clmessage::query_cla_parameters query;
    std::string protocol;
    
    AttributeNameVector::const_iterator param_i;
    for (param_i = parameters.begin();
         param_i != parameters.end();
         ++param_i) {
        // One of the "desired parameters" must be "protocol={something}"
        // so we know which module to send the request to.
        if (param_i->name().find("protocol=") != std::string::npos) {
            protocol = param_i->name().substr(9);
        }
            
        // Otherwise, pass this param name on to the ECLA.
        else {
            query.attribute_name().push_back( 
                    attribute_name( param_i->name() ) );
        } // else
    } // for
    
    // Make sure we were given a protocol.
    if (protocol.size() == 0) {
        log_err("No protocol given to query_cla_parameters; don't know where "
                "to send the query");
        BundleDaemon::post( new CLAParametersReportEvent( query_id,
                            AttributeVector() ) );
        return;
    }
    
    // Find the module and make sure it exists.
    ECLModule* module = get_module(protocol);    
    if (!module) {
        log_err("No external CLA with protocol '%s' exists", protocol.c_str());
        BundleDaemon::post( new CLAParametersReportEvent( query_id,
                            AttributeVector() ) );
        return;
    }

    // Fill in the rest of the query values and post the message.
    query.query_id(query_id);
    POST_MESSAGE(module, query_cla_parameters, query);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::shutdown() 
{
    std::list<ECLModule*>::iterator list_i;
    for (list_i = module_list_.begin(); list_i != module_list_.end(); ++list_i) {
        log_info("Shutting down external CLA '%s'", (*list_i)->name().c_str());
        (*list_i)->shutdown();
        (*list_i)->join();
        delete *list_i;
    }
}

//----------------------------------------------------------------------
CLInfo*
ExternalConvergenceLayer::new_link_params()
{
    return new ECLResource(std::string(""), NULL);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::add_module(ECLModule* module)
{
    oasys::ScopeLock lock(&module_mutex_, "add_module");
    module_list_.push_back(module);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::remove_module(ECLModule* module)
{
    oasys::ScopeLock lock(&module_mutex_, "remove_module");
    std::list<ECLModule*>::iterator list_i = module_list_.begin();

    while ( list_i != module_list_.end() ) {
        if (*list_i == module) {
            module_list_.erase(list_i);
            return;
        }

        ++list_i;
    }
}

//----------------------------------------------------------------------
ECLModule*
ExternalConvergenceLayer::get_module(const std::string& name)
{
    std::string proto_option;
    std::list<ECLModule*>::iterator list_i;

    oasys::ScopeLock lock(&module_mutex_, "get_module");

    for (list_i = module_list_.begin(); list_i != module_list_.end();
    ++list_i) {
        if ((*list_i)->name() == name)
            return *list_i;
    }

    return NULL;
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::add_resource(ECLResource* resource)
{
    oasys::ScopeLock lock(&resource_mutex_, "add_interface");
    resource_list_.push_back(resource);
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::build_param_sequence(int argc, const char* argv[],
        KeyValueSequence& param_sequence)
{
    for (int arg_i = 0; arg_i < argc; ++arg_i) {
        std::string arg_string(argv[arg_i]);
        
        // Split the string in two around the '='.
        size_t index = arg_string.find('=');
        if (index == std::string::npos) {
            log_warn("Invalid parameter: %s", argv[arg_i]);
            continue;
        }
        
        // Create a Parameter object from the two sides of the string.
        std::string lhs = arg_string.substr(0, index);
        std::string rhs = arg_string.substr(index + 1);
        param_sequence.push_back( clmessage::key_value_pair(lhs, rhs) );
    }
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::fill_bundle_attributes(const BundleRef& bundle,
                                                 bundle_attributes& attribs)
{
    attribs.source_eid( bundle->source().str() );
    attribs.is_fragment(bundle->is_fragment());
    
    // The timestamp in the XML element is two longs, rather than the
    // struct timeval used in DTNME or the SDNV specified in the BP.
    attribs.timestamp_seconds(bundle->creation_ts().seconds_);
    attribs.timestamp_sequence(bundle->creation_ts().seqno_);
    
    // fragment_offset and fragment_length are required only if is_fragment
    // is true.
    if (bundle->is_fragment()) {
        attribs.fragment_offset(bundle->frag_offset());
        attribs.fragment_length(bundle->payload().length());
    }
}

//----------------------------------------------------------------------
std::list<ECLResource*>
ExternalConvergenceLayer::take_resources(std::string name)
{
    oasys::ScopeLock lock(&resource_mutex_, "take_resources");
    std::list<ECLResource*> new_list;
    std::list<ECLResource*>::iterator list_i = resource_list_.begin();

    while ( list_i != resource_list_.end() ) {
        if ( (*list_i)->protocol_ == name ) {
            new_list.push_back(*list_i);
            
            // Erase this resource from the unclaimed list.
            std::list<ECLResource*>::iterator erase_i = list_i;
            ++list_i;
            resource_list_.erase(erase_i);
        }

        else {
            ++list_i;
        }
    }

    return new_list;
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::delete_resource(ECLResource* resource)
{
    oasys::ScopeLock lock(&resource_mutex_, "delete_resource");
    resource_list_.remove(resource);
    delete resource;
}    

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::give_resources(std::list<ECLInterfaceResource*>& list)
{
    oasys::ScopeLock l(&resource_mutex_, "give_resources(ECLInterfaceResource)");
    
    for ( std::list<ECLInterfaceResource*>::iterator list_i = list.begin();
          list_i != list.end(); 
          ++list_i) {
        ECLInterfaceResource* resource = *list_i;
        
        oasys::ScopeLock res_lock(&resource->lock_, "give_resources");
        resource->module_ = NULL;
        resource->should_delete_ = true;
        
        resource->module_ = NULL;
        resource_list_.push_back(resource);
    }

    log_debug( "Got %zu ECLInterfaceResources", list.size() );
    list.clear();
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::give_resources(LinkHashMap& list)
{
    int actual_taken = 0;
    oasys::ScopeLock l(&resource_mutex_, "give_resources(ECLLinkResource)");
    
    for ( LinkHashMap::iterator list_i = list.begin();
          list_i != list.end(); 
          ++list_i) {
        ECLLinkResource* resource = list_i->second;
        
        oasys::ScopeLock res_lock(&resource->lock_, "give_resources");
        resource->module_ = NULL;
        resource->should_delete_ = true;

        // Discovered links should be deleted when their associated CL
        // goes away.
        if (resource->is_discovered_)
            BundleDaemon::post( new LinkDeleteRequest(resource->link_) );
        
        else {
            resource_list_.push_back(resource);
            ++actual_taken;
        }
    }

    log_debug("Got %d ECLLinkResources", actual_taken);
    list.clear();
}

//----------------------------------------------------------------------
ExternalConvergenceLayer::Listener::Listener(ExternalConvergenceLayer& cl)
    : TCPServerThread("ExternalConvergenceLayer::Listener",
                      "/dtn/cl/Listener"),
      cl_(cl)
{
    Thread::set_flag(Thread::DELETE_ON_EXIT);
    set_logfd(false);
}

//----------------------------------------------------------------------
ExternalConvergenceLayer::Listener::~Listener()
{

}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::Listener::start()
{
    log_info("Listening for external CLAs on %s:%d", intoa(server_addr_),
             server_port_);

    if (bind_listen_start(server_addr_, server_port_) < 0)
            log_err("Listener thread failed to start");
}

//----------------------------------------------------------------------
void
ExternalConvergenceLayer::Listener::accepted(int fd, in_addr_t addr,
                                             u_int16_t port)
{
    if ( schema_ == std::string() ) {
        log_err("ECLA module is connecting before the XSD file is specified"
                "Closing the socket");
        ::close(fd);
        return;
    }
    
    log_debug("Accepted connection from a new CLA module");
    ECLModule* module = new ECLModule(fd, addr, port, cl_);

    // Add this module to our list and start its thread.
    cl_.add_module(module);
    module->start();
}


//----------------------------------------------------------------------
void
ECLResource::serialize(oasys::SerializeAction *a)
{
    a->process("protocol", &protocol_);
}

//----------------------------------------------------------------------
ECLLinkResource::ECLLinkResource(std::string p, clmessage::cl_message* create,
                                 const LinkRef& l, bool disc) :
    ECLResource(p, create),
    link_(l.object(), "ECLLinkResource"),
    outgoing_bundles_("outgoing_bundles")
{
    known_state_ = Link::UNAVAILABLE;
    is_discovered_ = disc;
    high_water_mark_ = -1;
    low_water_mark_ = 0;
}

//----------------------------------------------------------------------
void
ECLLinkResource::add_outgoing_bundle(Bundle* bundle) {
    outgoing_bundles_.push_back(bundle);
}

//----------------------------------------------------------------------
BundleRef
ECLLinkResource::get_outgoing_bundle(clmessage::bundle_attributes bundle_attribs)
{
    GbofId gbof_id;
        gbof_id.mutable_source()->assign( EndpointID( bundle_attribs.source_eid() ) );
        gbof_id.mutable_creation_ts()->seconds_ = (u_int32_t)bundle_attribs.timestamp_seconds();
        gbof_id.mutable_creation_ts()->seqno_ = (u_int32_t)bundle_attribs.timestamp_sequence();
        gbof_id.set_is_fragment( bundle_attribs.is_fragment() );
        if (gbof_id.is_fragment()) {
            if ( bundle_attribs.fragment_length().present() )
                gbof_id.set_frag_length( (u_int32_t)bundle_attribs.fragment_length().get() );
            else
                gbof_id.set_frag_length( 0 );

            if ( bundle_attribs.fragment_length().present() )
                gbof_id.set_frag_offset( (u_int32_t)bundle_attribs.fragment_offset().get() );
            else
                gbof_id.set_frag_offset( 0 );
        }

    return outgoing_bundles_.find(gbof_id);
}

//----------------------------------------------------------------------
bool
ECLLinkResource::has_outgoing_bundle(Bundle* bundle)
{
    return outgoing_bundles_.contains(bundle);
}

//----------------------------------------------------------------------
bool
ECLLinkResource::erase_outgoing_bundle(Bundle* bundle)
{
    return outgoing_bundles_.erase(bundle);
}

//----------------------------------------------------------------------
BundleList&
ECLLinkResource::get_bundle_set() 
{
    return outgoing_bundles_;
}


//----------------------------------------------------------------------
clmessage::linkTypeType
XMLConvert::convert_link_type(Link::link_type_t type)
{
    switch (type) {
        case Link::ALWAYSON: return linkTypeType(linkTypeType::alwayson);
        case Link::ONDEMAND: return linkTypeType(linkTypeType::ondemand);
        case Link::SCHEDULED: return linkTypeType(linkTypeType::scheduled);
        case Link::OPPORTUNISTIC: 
            return linkTypeType(linkTypeType::opportunistic);
        default: break;
    }
    
    return linkTypeType();
}

//----------------------------------------------------------------------
Link::link_type_t
XMLConvert::convert_link_type(clmessage::linkTypeType type)
{
    switch (type) {
        case linkTypeType::alwayson: return Link::ALWAYSON;
        case linkTypeType::ondemand: return Link::ONDEMAND;
        case linkTypeType::scheduled: return Link::SCHEDULED;
        case linkTypeType::opportunistic: return Link::OPPORTUNISTIC;
        default: break;
    }
        
    return Link::LINK_INVALID;
}

//----------------------------------------------------------------------
Link::state_t 
XMLConvert::convert_link_state(clmessage::linkStateType state) 
{
    switch (state) {
        case linkStateType::unavailable: return Link::UNAVAILABLE;
        case linkStateType::available: return Link::AVAILABLE;
        case linkStateType::opening: return Link::OPENING;
        case linkStateType::open: return Link::OPEN;
//        case linkStateType::busy: return Link::BUSY;
        case linkStateType::closed: return Link::CLOSED;
        default: break;
    }
    
    ASSERT(false);
    return Link::CLOSED;
}

//----------------------------------------------------------------------
ContactEvent::reason_t
XMLConvert::convert_link_reason(clmessage::linkReasonType reason)
{
    switch (reason) {
        case linkReasonType::no_info: return ContactEvent::NO_INFO;
        case linkReasonType::user: return ContactEvent::USER;
        case linkReasonType::broken: return ContactEvent::BROKEN;
        case linkReasonType::shutdown: return ContactEvent::SHUTDOWN;
        case linkReasonType::reconnect: return ContactEvent::RECONNECT;
        case linkReasonType::idle: return ContactEvent::IDLE;
        case linkReasonType::timeout: return ContactEvent::TIMEOUT;
//        case linkReasonType::unblocked: return ContactEvent::UNBLOCKED;
        default: break;
    }
    
    return ContactEvent::INVALID;
}
    
} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_CL_ENABLED
