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

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)

#include <exception>
#include <oasys/serialize/XMLSerialize.h>

#include "CLEventHandler.h"


namespace dtn {


void
CLEventHandler::dispatch_cl_event(cl_message* message)
{
#define DISPATCH_MESSAGE(message_name) do { \
    if ( message->message_name().present() ) { \
        log_debug_p("/dtn/cl/dispatcher", "Dispatching " #message_name); \
        handle( message->message_name().get() ); \
    } } while (false);

    DISPATCH_MESSAGE(cla_add_request);
    DISPATCH_MESSAGE(cla_delete_request);
    DISPATCH_MESSAGE(cla_params_set_event);
    DISPATCH_MESSAGE(interface_created_event);
    DISPATCH_MESSAGE(interface_reconfigured_event);
    DISPATCH_MESSAGE(eid_reachable_event);
    DISPATCH_MESSAGE(link_created_event);
    DISPATCH_MESSAGE(link_opened_event);
    DISPATCH_MESSAGE(link_closed_event);
    DISPATCH_MESSAGE(link_state_changed_event);
    DISPATCH_MESSAGE(link_deleted_event);
    DISPATCH_MESSAGE(link_attribute_changed_event);
    DISPATCH_MESSAGE(contact_attribute_changed_event);
    DISPATCH_MESSAGE(link_add_reachable_event);
    DISPATCH_MESSAGE(bundle_transmitted_event);
    DISPATCH_MESSAGE(bundle_canceled_event);
    DISPATCH_MESSAGE(bundle_receive_started_event);
    DISPATCH_MESSAGE(bundle_received_event);
    DISPATCH_MESSAGE(report_eid_reachable);
    DISPATCH_MESSAGE(report_link_attributes);
    DISPATCH_MESSAGE(report_interface_attributes);
    DISPATCH_MESSAGE(report_cla_parameters);
    
#undef DISPATCH_MESSAGE
}

void
CLEventHandler::process_cl_event(const char* msg_buffer,
                                 oasys::XercesXMLUnmarshal& parser)
{
    // clear any error condition before next parse
    parser.reset_error();
    
    std::auto_ptr<cl_message> message;
    
    const xercesc::DOMDocument* document = parser.doc(msg_buffer);
    if (!document) {
        log_debug_p("/dtn/cl/parse", "Unable to parse document");
        return;
    }        
    
    try { message = cl_message_(*document); }
    catch (std::exception& e) {
        log_debug_p( "/dtn/cl/parse", "Parse error: %s", e.what() );
        return;
    }        

    dispatch_cl_event( message.get() );
}

/*void
CLEventHandler::clear_parser(oasys::XMLUnmarshal& parser)
{
    const char* event_tag;

    while ( ( event_tag = parser.parse(NULL) ) )
        delete event_tag;
}*/

} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_CL_ENABLED
