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

#ifndef _CL_EVENT_HANDLER_H_
#define _CL_EVENT_HANDLER_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)

#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>
#include <oasys/serialize/XercesXMLSerialize.h>

#include "clevent.h"

namespace dtn {

using namespace dtn::clmessage;

class CLEventHandler : public oasys::Logger {
protected:
    CLEventHandler(const char *classname, const std::string &logpath)
        : oasys::Logger(classname, logpath) { }
    virtual ~CLEventHandler() { }
    void process_cl_event(const char* msg_buffer,
                          oasys::XercesXMLUnmarshal& parser);
    void dispatch_cl_event(cl_message* message);
    //void clear_parser(oasys::XMLUnmarshal& parser);
    
    virtual void handle(const cla_add_request& message) { (void)message; }
    virtual void handle(const cla_delete_request& message) { (void)message; }
    virtual void handle(const cla_params_set_event& message) { (void)message; }
    virtual void handle(const interface_created_event& message) { (void)message; }
    virtual void handle(const interface_reconfigured_event& message) { (void)message; }
    virtual void handle(const eid_reachable_event& message) { (void)message; }
    virtual void handle(const link_created_event& message) { (void)message; }
    virtual void handle(const link_opened_event& message) { (void)message; }
    virtual void handle(const link_closed_event& message) { (void)message; }
    virtual void handle(const link_state_changed_event& message) { (void)message; }
    virtual void handle(const link_deleted_event& message) { (void)message; }
    virtual void handle(const link_attribute_changed_event& message) { (void)message; }
    virtual void handle(const contact_attribute_changed_event& message) { (void)message; }
    virtual void handle(const link_add_reachable_event& message) { (void)message; }
    virtual void handle(const bundle_transmitted_event& message) { (void)message; }
    virtual void handle(const bundle_canceled_event& message) { (void)message; }
    virtual void handle(const bundle_receive_started_event& message) { (void)message; }
    virtual void handle(const bundle_received_event& message) { (void)message; }
    virtual void handle(const report_eid_reachable& message) { (void)message; }
    virtual void handle(const report_link_attributes& message) { (void)message; }
    virtual void handle(const report_interface_attributes& message) { (void)message; }
    virtual void handle(const report_cla_parameters& message) { (void)message; }
}; // class CLEventHandler

} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_CL_ENABLED
#endif
