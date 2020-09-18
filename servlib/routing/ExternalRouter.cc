/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sstream>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "ExternalRouter.h"
#include "bundling/GbofId.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "bundling/MetadataBlockProcessor.h"
#include "contacts/ContactManager.h"
#include "contacts/NamedAttribute.h"
#include "reg/RegistrationTable.h"
#include "conv_layers/ConvergenceLayer.h"

/* dzdebug
#include "conv_layers/TCPConvergenceLayer.h"
#include "conv_layers/UDPConvergenceLayer.h"
#ifdef LTP_ENABLED
#    include "conv_layers/LTPConvergenceLayer.h"
#endif
#ifdef OASYS_BLUETOOTH_ENABLED
#    include "conv_layers/BluetoothConvergenceLayer.h"
#endif
*/


#include <oasys/io/UDPClient.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/io/IO.h>
#include <oasys/io/NetUtils.h>

/*dzdebug - use this version to log each message type and its sequence counter
#define DEBUG_RTR_MSGS

#define SEND(event, data) \
    rtrmessage::bpa message; \
    message.event(data); \
    send(message, #event);
*/

#define SEND(event, data) \
    rtrmessage::bpa message; \
    message.event(data); \
    send(message, NULL);

#define CATCH(exception) \
    catch (exception &e) { log_warn("%s", e.what()); }

namespace dtn {

using namespace rtrmessage;

ExternalRouter::ExternalRouter()
    : BundleRouter("ExternalRouter", "external"),
      initialized_(false)
{
    shutting_down_ = false;

    log_notice("Creating ExternalRouter");
}

ExternalRouter::~ExternalRouter()
{
    delete route_table_;
    delete reg_;
}

// Initialize inner classes
void
ExternalRouter::initialize()
{
    log_notice("Initializing ExternalRouter");

    send_seq_ctr_ = 0;

    // Create the static route table
    route_table_ = new RouteTable("external");

    // Register as a client app with the forwarder
    reg_ = new ERRegistration(this);

    // Register the global shutdown function
    BundleDaemon::instance()->set_rtr_shutdown(
        external_rtr_shutdown, (void *) 0);

    // Start module server thread
    srv_ = new ModuleServer();
    srv_->start();

    // Create a hello timer
    hello_ = new HelloTimer(this);

    initialized_ = true;

    bpa message;
    message.alert(dtnStatusType(std::string("justBooted")));
    message.hello_interval(ExternalRouter::hello_interval);
    send(message, "justBooted");
    hello_->schedule_in(ExternalRouter::hello_interval * 1000);
}

void
ExternalRouter::shutdown()
{
    shutting_down_ = true;

    // hello_->cancel();  -- canceled by TimerSystem shutdown processing

    dtnStatusType e(std::string("shuttingDown"));
    SEND(alert, e)

    sleep(1);

    //XXX/dz - TODO - shutdown cleanly - segfault in ModuleServer if delete while processing

    srv_->do_shutdown();

}

// Format the given StringBuffer with static routing info
void
ExternalRouter::get_routing_state(oasys::StringBuffer* buf)
{
    if (!shutting_down_) {
        buf->appendf("Static route table for %s router(s):\n", name_.c_str());
        route_table_->dump(buf);
    }
}

//----------------------------------------------------------------------
bool
ExternalRouter::can_delete_bundle(const BundleRef& bundle)
{
    // check if we haven't yet done anything with this bundle
    if (bundle->fwdlog()->get_count(ForwardingInfo::TRANSMITTED |
                                    ForwardingInfo::DELIVERED) == 0)
    {
        //log_debug("ExternalRouter::can_delete_bundle(%" PRIbid "): "
        //          "not yet transmitted or delivered",
        //          bundle->bundleid());
        return false;
    }

    // check if we have local custody
    if (bundle->local_custody()) {
        //log_debug("ExternalRouter::can_delete_bundle(%" PRIbid "): "
        //          "not deleting because we have custody",
        //          bundle->bundleid());
        return false;
    }

    if (bundle->ecos_critical()) {
        //log_debug("ExternalRouter::can_delete_bundle(%" PRIbid "): "
        //          "not deleting because ECOS critical bundles can be sent over multiple links",
        //          bundle->bundleid());
        return false;
    }


    //XXX/dz - Need to add support for sessions??
    //
    // check if the bundle is part of a session with subscribers
    //Session* session = get_session_for_bundle(bundle.object());
    //if (session && !session->subscribers().empty())
    //{
    //    log_debug("ExternalRouter::can_delete_bundle(%u): "
    //              "session has subscribers",
    //              bundle->bundleid());
    //    return false;
    //}

    return true;
}
 
bool
ExternalRouter::accept_custody(Bundle* bundle)
{
    // External part of this router will issue a request to accept 
    // custody of the bundle after getting the BundleReceived 
    // message if it decides it is warranted.

    (void)bundle;
    return false;
}



// Serialize events and UDP multicast to external routers
void
ExternalRouter::handle_event(BundleEvent *event)
{
    if (initialized_ && !shutting_down_) {
        dispatch_event(event);
    }
}

/*
std::string
ExternalRouter::link_remote_addr(LinkRef& lref)
{
    std::string result = "";

    if (lref == NULL) {
        // Bundle did not come in from a link - probably a generated bundle
        return result;
    }

    CLInfo* clinfo = lref->cl_info();

    if (clinfo) {    
        if (0 == strcmp("tcp", lref->clayer()->name())) {
            typedef TCPConvergenceLayer::TCPLinkParams tcp_params;
            tcp_params *params = dynamic_cast<tcp_params*>(clinfo);
            if (params != 0) {
                oasys::Intoa raddr(params->remote_addr_);
                result = raddr.buf();
            }
        } else if (0 == strcmp("udp", lref->clayer()->name())) {
            typedef UDPConvergenceLayer::Params udp_params;
            udp_params *params = dynamic_cast<udp_params*>(clinfo);
            if (params != 0) {
                oasys::Intoa raddr(params->remote_addr_);
                result = raddr.buf();
            }
        } 

#ifdef LTP_ENABLED
        else if (0 == strcmp("ltp", lref->clayer()->name())) {
            typedef LTPConvergenceLayer::Params ltp_params;
            ltp_params *params = dynamic_cast<ltp_params*>(clinfo);
            if (params != 0) {
                oasys::Intoa raddr(params->remote_addr_);
                result = raddr.buf();
            }
        } 
#endif

#ifdef OASYS_BLUETOOTH_ENABLED
        else if (0 == strcmp("bt", lref->clayer()->name())) {
            typedef BluetoothConvergenceLayer::Params bt_params;
            bt_params *params = dynamic_cast<bt_params*>(clinfo);
            if (params != 0) {
                oasys::Intoa raddr(bd2str(params->remote_addr_));
                result = raddr.buf();
            }
        } 
#endif

    }

    return result;
}
*/

void
ExternalRouter::handle_bundle_received(BundleReceivedEvent *event)
{
    // Pull the remote address from the CLInfo if it is available
    // which is only from TCP connections 
    std::string link_id = "";
    if (event->link_ != NULL) {
        link_id = event->link_->name();
    }
    


    bpa::bundle_received_event::type e(
        event->bundleref_->source().uri().uri(),
        event->bundleref_->dest().uri().uri(),
        event->bundleref_->custodian().uri().uri(),
        event->bundleref_->replyto().uri().uri(),
        event->bundleref_->prevhop().uri().uri(),
//dzdebug        bundle_ts_to_long(event->bundleref_->extended_id()),
        event->bundleref_->bundleid(),
        event->bundleref_->gbofid_str(),

#ifdef ACS_ENABLED
        event->bundleref_->custodyid(),
#else
        0, // custodyid placeholder
#endif
        event->bundleref_->expiration(),

//dzdebug        event->bytes_received_,
        event->bundleref_->payload().length(),

        event->bundleref_->custody_requested(),
        link_id,

        
        bundlePriorityType(lowercase(event->bundleref_->prioritytoa(event->bundleref_->priority()))),
#ifdef ECOS_ENABLED
        event->bundleref_->ecos_flags(),
        event->bundleref_->ecos_ordinal()
#else
        0, // ecos_flags placeholder
        0  // ecos_ordinal placeholder
#endif

        );

#ifdef ECOS_ENABLED
        // set the option ECOS flow label value
        e.ecos_flowlabel(event->bundleref_->ecos_flowlabel());
#endif


    LinkRef null_link("ExternalRouter::handle_bundle_received");
    MetadataVec * gen_meta = event->bundleref_->generated_metadata().
                             find_blocks(null_link);
    unsigned int num_meta_blocks = event->bundleref_->recv_metadata().size() +
                                   ((gen_meta == NULL)? 0 : gen_meta->size());
    e.num_meta_blocks(num_meta_blocks);

    SEND(bundle_received_event, e)
}

void ExternalRouter::handle_bundle_custody_accepted(BundleCustodyAcceptedEvent* event)
{
    bpa::bundle_custody_accepted_event::type e(
        event->bundleref_->bundleid(),
#ifdef ACS_ENABLED
        event->bundleref_->custodyid(),
#else
        0, // custodyid placeholder
#endif
        event->bundleref_->custodian().c_str(),
        event->bundleref_->gbofid_str().c_str());

    SEND(bundle_custody_accepted_event, e)
}

void
ExternalRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    //log_debug("External Router Transmitted Event Transmit %s",event->success_?"True":"False");
    //if (event->contact_ == NULL) return;
    //log_debug("External Router Transmitted Event Transmit Contact Found");

    bpa::data_transmitted_event::type e(
        event->bundleref_->bundleid(),
        event->link_.object()->name_str(),
        event->bytes_sent_,
        event->reliably_sent_,
        event->bundleref_->gbofid_str());
    SEND(data_transmitted_event, e)
}

void
ExternalRouter::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    bpa::bundle_delivered_event::type e(
        event->bundleref_->bundleid(),
        event->bundleref_->gbofid_str());
    SEND(bundle_delivered_event, e)
}

void
ExternalRouter::handle_bundle_expired(BundleExpiredEvent* event)
{
    bpa::bundle_expired_event::type e(
        event->bundleref_->bundleid(),
        event->bundleref_->gbofid_str());
    SEND(bundle_expired_event, e)
}

void
ExternalRouter::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
    bpa::bundle_send_cancelled_event::type e(
        event->link_.object()->name_str(),
        event->bundleref_->bundleid(),
        event->bundleref_->gbofid_str());
    SEND(bundle_send_cancelled_event, e)
}

void
ExternalRouter::handle_bundle_injected(BundleInjectedEvent* event)
{
    bpa::bundle_injected_event::type e(
        event->request_id_,
        event->bundleref_->bundleid(),
        event->bundleref_->gbofid_str());
    SEND(bundle_injected_event, e)
}

void
ExternalRouter::handle_contact_up(ContactUpEvent* event)
{
    ASSERT(event->contact_->link() != NULL);
    ASSERT(!event->contact_->link()->isdeleted());
        
    bpa::link_opened_event::type e(
        event->contact_.object(),
        event->contact_->link().object()->name_str());
    SEND(link_opened_event, e)
}

void
ExternalRouter::handle_contact_down(ContactDownEvent* event)
{
    bpa::link_closed_event::type e(
        event->contact_.object(),
        event->contact_->link().object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_closed_event, e)
}

void
ExternalRouter::handle_link_created(LinkCreatedEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_created_event::type e(
        event->link_.object(),
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_created_event, e)
}

void
ExternalRouter::handle_link_deleted(LinkDeletedEvent *event)
{
    ASSERT(event->link_ != NULL);

    bpa::link_deleted_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_deleted_event, e)
}

void
ExternalRouter::handle_link_available(LinkAvailableEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_available_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_available_event, e)
}

void
ExternalRouter::handle_link_unavailable(LinkUnavailableEvent *event)
{
    bpa::link_unavailable_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_unavailable_event, e)
}

void
ExternalRouter::handle_link_attribute_changed(LinkAttributeChangedEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_attribute_changed_event::type e(
        event->link_.object(),
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_attribute_changed_event, e)
}

void
ExternalRouter::handle_new_eid_reachable(NewEIDReachableEvent* event)
{
    bpa::eid_reachable_event::type e(
        event->endpoint_,
        event->iface_->name());
    SEND(eid_reachable_event, e)
}

void
ExternalRouter::handle_contact_attribute_changed(ContactAttributeChangedEvent *event)
{
    ASSERT(event->contact_->link() != NULL);
    ASSERT(!event->contact_->link()->isdeleted());
        
    // @@@ is this right? contact_eid same as link's remote_eid?
    bpa::contact_attribute_changed_event::type e(
        eidType(event->contact_->link()->remote_eid().str()), 
        event->contact_.object(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(contact_attribute_changed_event, e)
}

// void
// ExternalRouter::handle_link_busy(LinkBusyEvent *event)
// {
//     bpa::link_busy_event::type e(
//         event->link_.object());
//     SEND(link_busy_event, e)
// }

void
ExternalRouter::handle_registration_added(RegistrationAddedEvent* event)
{
    bpa::registration_added_event::type e(
        event->registration_,
        (xml_schema::string)source_to_str((event_source_t)event->source_));
    SEND(registration_added_event, e)
}

void
ExternalRouter::handle_registration_removed(RegistrationRemovedEvent* event)
{
    bpa::registration_removed_event::type e(
        event->registration_);
    SEND(registration_removed_event, e)
}

void
ExternalRouter::handle_registration_expired(RegistrationExpiredEvent* event)
{
    bpa::registration_expired_event::type e(
        event->registration_->regid());
    SEND(registration_expired_event, e)
}

void
ExternalRouter::handle_route_add(RouteAddEvent* event)
{
    // update our own static route table first
    route_table_->add_entry(event->entry_);

    bpa::route_add_event::type e(
        event->entry_);
    SEND(route_add_event, e)
}

void
ExternalRouter::handle_route_del(RouteDelEvent* event)
{
    // update our own static route table first
    route_table_->del_entries(event->dest_);

    bpa::route_delete_event::type e(
        eidType(event->dest_.str()));
    SEND(route_delete_event, e)
}

void
ExternalRouter::handle_custody_signal(CustodySignalEvent* event)
{
    custodySignalType attr(
        event->data_.admin_type_,
        event->data_.admin_flags_,
        event->data_.succeeded_,
        event->data_.reason_,
        event->data_.orig_frag_offset_,
        event->data_.orig_frag_length_,
        event->data_.custody_signal_tv_.seconds_,
        event->data_.custody_signal_tv_.seqno_,
        event->data_.orig_creation_tv_.seconds_,
        event->data_.orig_creation_tv_.seqno_);

    // In order to provide the correct local_id we have to go find the
    // bundle in our system that has this GBOF-ID and that we are custodian
    // of. There should only be one such bundle.

    GbofId gbof_id;
        gbof_id.mutable_source()->assign(event->data_.orig_source_eid_);
        gbof_id.mutable_creation_ts()->seconds_ = event->data_.orig_creation_tv_.seconds_;
        gbof_id.mutable_creation_ts()->seqno_ = event->data_.orig_creation_tv_.seqno_;
        gbof_id.set_is_fragment( event->data_.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT );
        if (gbof_id.is_fragment()) {
            gbof_id.set_frag_length(event->data_.orig_frag_length_);
            gbof_id.set_frag_offset(event->data_.orig_frag_offset_);
        }

    bpa::custody_signal_event::type e(
        attr,
        event->bundle_id_,
        gbof_id.str());

    SEND(custody_signal_event, e)
}

void
ExternalRouter::handle_external_router_acs(ExternalRouterAcsEvent* event)
{
    aggregate_custody_signal_event::acs_data::type hex_data(event->acs_data_.c_str(), event->acs_data_.length());

    bpa::aggregate_custody_signal_event::type e(hex_data);

    //dzdebug
    //log_debug("handle_external_router_acs - send external router an ACS notification: %s", 
    //         hex_data.encode().c_str());

    SEND(aggregate_custody_signal_event, e)
}

void
ExternalRouter::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    // Check to see if the bundle is still pending
    BundleRef br = BundleDaemon::instance()->all_bundles()->find_for_storage(event->bundle_id_);
    if (! BundleDaemon::instance()->pending_bundles()->contains(br)) {
        br.release();
    }

    if (br != NULL) {
        bpa::custody_timeout_event::type e(
            event->bundle_id_,
            br->gbofid_str());

        SEND(custody_timeout_event, e)
    }
}

void
ExternalRouter::handle_link_report(LinkReportEvent *event)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator i = links->begin();
    LinkSet::const_iterator end = links->end();

    link_report report;
    link_report::link::container c;

    for(; i != end; ++i)
        c.push_back(link_report::link::type((*i).object()));

    report.link(c);
    SEND(link_report, report)
}

void
ExternalRouter::handle_link_attributes_report(LinkAttributesReportEvent *event)
{
    AttributeVector::const_iterator iter = event->attributes_.begin();
    AttributeVector::const_iterator end = event->attributes_.end();

    link_attributes_report::report_params::container c;

    for(; iter != end; ++iter) {
      c.push_back( key_value_pair(*iter) );
    }

    link_attributes_report e(event->query_id_);
    e.report_params(c);
    SEND(link_attributes_report, e)
}

void
ExternalRouter::handle_contact_report(ContactReportEvent* event)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "ExternalRouter::handle_event");

    (void) event;
    
    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator i = links->begin();
    LinkSet::const_iterator end = links->end();

    contact_report report;
    contact_report::contact::container c;
    
    for(; i != end; ++i) {
        if ((*i)->contact() != NULL) {
            c.push_back((*i)->contact().object());
        }
    }

    report.contact(c);
    SEND(contact_report, report)
}

void
ExternalRouter::handle_bundle_report(BundleReportEvent *event)
{
    (void) event;

#ifdef PENDING_BUNDLES_IS_MAP
    generate_bundle_report_from_map();
#else
    generate_bundle_report_from_list();
#endif
}

#ifdef PENDING_BUNDLES_IS_MAP

void
ExternalRouter::generate_bundle_report_from_map()
{
    BundleDaemon *bd = BundleDaemon::instance();
    BundleRef bref("generate_bundle_report_from_map");

    //log_debug("generata_bundle_report_from_map - pending_bundles size %zu", bd->pending_bundles()->size());
    pending_bundles_t* bundles = bd->pending_bundles();

    bundles->lock()->lock("ExternalRouter::handle_event");
    pending_bundles_t::iterator iter = bundles->begin();

    if (iter != bundles->end()) {
        bref = iter->second;
    }
    bundles->lock()->unlock();

    int ctr = 0;
    while (bref != NULL) {
        bundle_report report;
        bundle_report::bundle::container c;

        // 25 bundles per message is ~15-20 KB (unless EIDs get large)
        while (bref != NULL && ctr++<25) {
           c.push_back(bundle_report::bundle::type(bref.object()));

           bref = bundles->find_next(bref->bundleid());
        }

        report.bundle(c);
        SEND(bundle_report, report)

        ctr = 0;
    }
}

#else

void
ExternalRouter::generate_bundle_report_from_list()
{

    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->pending_bundles()->lock(),
        "ExternalRouter::handle_event");


    //log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
    pending_bundles_t* bundles = bd->pending_bundles();
    pending_bundles_t::iterator i = bundles->begin();
    pending_bundles_t::iterator end = bundles->end();

    int ctr = 0;
    while (i != end) {
        bundle_report report;
        bundle_report::bundle::container c;

        // 25 bundles per message is ~15-20 KB (unless EIDs get large)
        while(i != end && ctr++<25) {
#ifdef PENDING_BUNDLES_IS_MAP
           c.push_back(bundle_report::bundle::type(i->second));
#else
           c.push_back(bundle_report::bundle::type(*i));
#endif
           ++i;
        }

        report.bundle(c);
        SEND(bundle_report, report)

        ctr = 0;
    }
}

#endif //PENDING_BUNDLES_IS_MAP

void
ExternalRouter::handle_bundle_attributes_report(BundleAttributesReportEvent *event)
{
    BundleRef br = event->bundle_;

    bundleAttributesReportType response;

    AttributeNameVector::iterator i = event->attribute_names_.begin();
    AttributeNameVector::iterator end = event->attribute_names_.end();

    for (; i != end; ++i) {
        const std::string& name = i->name();

        if (name == "bundleid")
            response.bundleid( br->bundleid() );
        else if (name == "is_admin")
            response.is_admin( br->is_admin() );
        else if (name == "do_not_fragment")
            response.do_not_fragment( br->do_not_fragment() );
        else if (name == "priority")
            response.priority(
                bundlePriorityType(lowercase(br->prioritytoa(br->priority()))) );
        else if (name == "custody_requested")
            response.custody_requested( br->custody_requested() );
        else if (name == "local_custody")
            response.local_custody( br->local_custody() );
        else if (name == "singleton_dest")
            response.singleton_dest( br->singleton_dest() );
        else if (name == "custody_rcpt")
            response.custody_rcpt( br->custody_rcpt() );
        else if (name == "receive_rcpt")
            response.receive_rcpt( br->receive_rcpt() );
        else if (name == "forward_rcpt")
            response.forward_rcpt( br->forward_rcpt() );
        else if (name == "delivery_rcpt")
            response.delivery_rcpt( br->delivery_rcpt() );
        else if (name == "deletion_rcpt")
            response.deletion_rcpt( br->deletion_rcpt() );
        else if (name == "app_acked_rcpt")
            response.app_acked_rcpt( br->app_acked_rcpt() );
        else if (name == "expiration")
            response.expiration( br->expiration() );
        else if (name == "orig_length")
            response.orig_length( br->orig_length() );
        else if (name == "owner")
            response.owner( br->owner() );
        else if (name == "location")
            response.location(
                bundleLocationType(
                    bundleType::location_to_str(br->payload().location())) );
        else if (name == "dest")
            response.dest( br->dest() );
        else if (name == "custodian")
            response.custodian( br->custodian() );
        else if (name == "replyto")
            response.replyto( br->replyto() );
        else if (name == "prevhop")
            response.prevhop( br->prevhop() );
        else if (name == "payload_file") {
            response.payload_file( br->payload().filename() );
        }
    }

    if (event->metadata_blocks_.size() > 0) {

        // We don't want to send duplicate blocks, so keep track of which
        // blocks have been added to the outgoing list with this.
        std::vector<unsigned int> added_blocks;

        // Iterate through the requests.
        MetaBlockRequestVector::iterator requested;
        for (requested = event->metadata_blocks_.begin(); 
             requested != event->metadata_blocks_.end();
             ++requested) {

            for (unsigned int i = 0; i < 2; ++i) {
                MetadataVec * block_vec = NULL;
                if (i == 0) {
                    block_vec = br->mutable_recv_metadata();
                    ASSERT(block_vec != NULL);
                } else if (i == 1) {
                    LinkRef null_link("ExternalRouter::"
                                      "handle_bundle_attributes_report");
                    block_vec = br->generated_metadata().find_blocks(null_link);
                    if (block_vec == NULL) {
                        continue;
                    }
                } else {
                    ASSERT(block_vec != NULL);
                }

                MetadataVec::iterator block_i;
                for (block_i  = block_vec->begin();
                     block_i != block_vec->end();
                     ++block_i) {

                    MetadataBlockRef block = *block_i;

                    // Skip this block if we already added it.
                    bool added = false;
                    for (unsigned int i = 0; i < added_blocks.size(); ++i) {
                        if (added_blocks[i] == block->id()) {
                            added = true;
                            break;
                        }
                    }
                    if (added) {
                        continue;
                    }

                    switch (requested->query_type()) {
                    // Match the block's identifier (index).
                    case MetadataBlockRequest::QueryByIdentifier:
                        if (requested->query_value() != block->id())
                            continue;
                        break;

                    // Match the block's type.
                    case MetadataBlockRequest::QueryByType:
                        if (block->ontology() != requested->query_value())
                            continue;
                        break;

                    // All blocks were requested.
                    case MetadataBlockRequest::QueryAll:
                        break;
                    }

                    // Note that we have added this block.
                    added_blocks.push_back(block->id());

                    // Add the block to the list.
                    response.meta_blocks().push_back(
                        rtrmessage::metadataBlockType(
                            block->id(), false, block->ontology(),
                            xml_schema::base64_binary(
                                    block->metadata(),
                                    block->metadata_len())));
                }
            }
        }
    }

    bundle_attributes_report e(event->query_id_, response);
    SEND(bundle_attributes_report, e)
}

void
ExternalRouter::handle_route_report(RouteReportEvent* event)
{
    oasys::ScopeLock l(route_table_->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    const RouteEntryVec *re = route_table_->route_table();
    RouteEntryVec::const_iterator i = re->begin();
    RouteEntryVec::const_iterator end = re->end();

    route_report report;
    route_report::route_entry::container c;

    for(; i != end; ++i)
        c.push_back(route_report::route_entry::type(*i));

    report.route_entry(c);
    SEND(route_report, report)
}

void
ExternalRouter::send(bpa &message, const char* type_str)
{
    oasys::ScopeLock l(&lock_, "send");

    xercesc::MemBufFormatTarget buf;
    xml_schema::namespace_infomap map;

    message.eid(BundleDaemon::instance()->local_eid().c_str());
    message.eid_ipn(BundleDaemon::instance()->local_eid_ipn().c_str());

    message.sequence_ctr(send_seq_ctr_++);

    if (ExternalRouter::client_validation)
        map[""].schema = ExternalRouter::schema.c_str();

    try {
        bpa_(buf, message, map, "UTF-8",
             xml_schema::flags::dont_initialize);
        srv_->post_to_send(new std::string((char *)buf.getRawBuffer()));

        //dzdebug - to determine missed messages
        if (NULL != type_str) {
            log_info("%lu - %s", send_seq_ctr_-1, type_str);
        }

    }
    catch (xml_schema::serialization &e) {
        const xml_schema::errors &elist = e.errors();
        xml_schema::errors::const_iterator i = elist.begin();
        xml_schema::errors::const_iterator end = elist.end();

        for (; i < end; ++i) {
            std::cout << (*i).message() << std::endl;
        }
    }
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::no_namespace_mapping)
    CATCH(xml_schema::no_prefix_mapping)
    CATCH(xml_schema::xsi_already_in_use)
}

const char *
ExternalRouter::reason_to_str(int reason)
{
    switch(reason) {
        case ContactEvent::NO_INFO:     return "no_info";
        case ContactEvent::USER:        return "user";
        case ContactEvent::SHUTDOWN:    return "shutdown";
        case ContactEvent::BROKEN:      return "broken";
        case ContactEvent::CL_ERROR:    return "cl_error";
        case ContactEvent::CL_VERSION:  return "cl_version";
        case ContactEvent::RECONNECT:   return "reconnect";
        case ContactEvent::IDLE:        return "idle";
        case ContactEvent::TIMEOUT:     return "timeout";
        default: return "";
    }
}

ExternalRouter::ModuleServer::ModuleServer()
    : Thread("/router/external/moduleserver"),
      Logger("ExternalRouter::ModuleServer", "/router/external/moduleserver"),
      parser_(new oasys::XercesXMLUnmarshal(
                  ExternalRouter::server_validation,
                  ExternalRouter::schema.c_str()))
{
    set_logpath("/router/external/moduleserver");

    // we always delete the thread object when we exit
    //Thread::set_flag(Thread::DELETE_ON_EXIT);

    last_recv_seq_ctr_ = 0;
    eventq_ = new oasys::MsgQueue< std::string * >(logpath_);


    receiver_ = NULL;
    sender_ = NULL;

    tcp_interface_ = NULL;

    if (! use_tcp_interface_)
    {
        receiver_ = new Receiver(this);
        receiver_->start();

        sender_ = new Sender();
        sender_->start();
    }
    else
    {
        tcp_interface_ = new TcpInterface(this, ExternalRouter::network_interface_, ExternalRouter::server_port);

        //tcp_interface->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
   
        int ret = tcp_interface_->bind(ExternalRouter::network_interface_, ExternalRouter::server_port);

        // be a little forgiving -- if the address is in use, wait for a
        // bit and try again
        while (ret != 0 && errno == EADDRINUSE && !should_stop()) {
            tcp_interface_->logf(oasys::LOG_ERR,
                           "WARNING: error binding to requested socket: %s  -- waiting 10 seconds to try again",
                           strerror(errno));
            sleep(10);
        
            ret = tcp_interface_->bind(ExternalRouter::network_interface_, ExternalRouter::server_port);
        }
       
        if (ret != 0) {
            return; // error already logged
        }
  
        // start listening and then start the thread to loop calling accept()
        tcp_interface_->listen();
        tcp_interface_->start();

    }
}

ExternalRouter::ModuleServer::~ModuleServer()
{
    do_shutdown();

    // free all pending events
    std::string *event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;

    delete parser_;
}

void
ExternalRouter::ModuleServer::do_shutdown()
{
    set_should_stop();

    if (receiver_ != NULL) {
        receiver_->set_should_stop();
        sender_->set_should_stop();
    }


    if (tcp_interface_ != NULL) {
        tcp_interface_->do_shutdown();
        int ctr = 0;
        while ((tcp_interface_ != NULL) && (++ctr < 20)) {
            usleep(100000);
        }
    }

}


/// Post a string to send 
void
ExternalRouter::ModuleServer::post(std::string* event)
{
    eventq_->push_back(event);
}

/// Post a string to send 
void
ExternalRouter::ModuleServer::post_to_send(std::string* event)
{
    if (sender_ != NULL )
    {
        sender_->post(event);
    }
    else if (tcp_interface_ != NULL)
    {
        tcp_interface_->post_to_send(event);
    }
    else
    {
        delete event;
    }
}

// ModuleServer main loop
void
ExternalRouter::ModuleServer::run() 
{
    // block on input from event queue
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

        if (ret == oasys::IOINTR) {
            //log_debug("module server interrupted");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                process_action(event->c_str());

                delete event;
            }    
        }
    }
}

// Handle a message from an external router
void
ExternalRouter::ModuleServer::process_action(const char *payload)
{
    // clear any error condition before next parse
    parser_->reset_error();
    
    // parse the xml payload received
    const xercesc::DOMDocument *doc = parser_->doc(payload);
    
    // was the message valid?
    if (parser_->error()) {
        //log_debug("received invalid message");
        return;
    }

    std::auto_ptr<bpa> instance;

    try {
        instance = bpa_(*doc);
    }
    CATCH(xml_schema::expected_element)
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::expected_attribute)
    CATCH(xml_schema::unexpected_enumerator)
    CATCH(xml_schema::no_type_info)
    CATCH(xml_schema::not_derived)

    // Check that we have an instance object to work with
    if (instance.get() == 0) {
        // dzdebug
        log_warn("ExternalRouter::ModuleServer - no object extracted from message");
        return;
    }

    if (!instance->server_eid().present()) {
        //log_debug("received message without server_eid - probably a loopback: %s", payload);
        return;
    } else {
        if ((0 != BundleDaemon::instance()->local_eid().compare(instance->server_eid().get())) &&
            (0 != BundleDaemon::instance()->local_eid_ipn().compare(instance->server_eid().get())))
        {
            //log_debug("received message for different server node: %s", instance->server_eid().get().c_str());
            return;
        }
        else {
            //log_debug("processing message for server node: %s", instance->server_eid().get().c_str());
        }
    }


    uint64_t seq_ctr = instance->sequence_ctr();

    // check for gaps in sequence counter
    if (0 != last_recv_seq_ctr_) {
        if (seq_ctr != last_recv_seq_ctr_+1) {
            if (seq_ctr > last_recv_seq_ctr_) {
                log_err("Possible missed messages - Last SeqCtr: %" PRIu64 " Curr SeqCtr: %" PRIu64 " - diff: %" PRIu64,
                        last_recv_seq_ctr_, seq_ctr, (seq_ctr - last_recv_seq_ctr_));
            } else if (0 != seq_ctr) {
                log_err("Sequence Counter jumped backwards - Last SeqCtr: %" PRIu64 " Curr SeqCtr: %" PRIu64,
                        last_recv_seq_ctr_, seq_ctr);
            }
        }
    }
    last_recv_seq_ctr_ = seq_ctr; 


    //dzdebug
    bool msg_processed = false;

    // @@@ Need to add:
    //      broadcast_send_bundle_request

    // Examine message contents
    if (instance->send_bundle_request().present()) {
        msg_processed = true;
        //log_debug("posting BundleSendRequest");
        send_bundle_request& in_request = instance->send_bundle_request().get();

        std::string link = in_request.link_id();
        int action = convert_fwd_action(in_request.fwd_action());

        bundleid_t bid = in_request.local_id();
        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (!bd->pending_bundles()->contains(br)) {
            br.release();
        }

        if (br.object()) {
            BundleSendRequest *request = new BundleSendRequest(br, link, action);
    
            // @@@ need to handle optional params frag_size, frag_offset
            
            LinkRef link_ref = bd->contactmgr()->find_link(link.c_str());
            if (link_ref == NULL) {
                if (in_request.metadata_block().size() > 0) {
                    log_err("link %s does not exist; failed to "
                            "modify/generate metadata for send bundle request",
                            link.c_str());
                }
    
            } else if (in_request.metadata_block().size() > 0) {
    
                typedef ::xsd::cxx::tree::sequence<rtrmessage::metadataBlockType>
                        MetaBlockSequence;
                
                MetadataBlockProcessor* meta_processor = 
                        dynamic_cast<MetadataBlockProcessor*>(
                            BundleProtocol::find_processor(
                            BundleProtocol::METADATA_BLOCK));
                ASSERT(meta_processor != NULL);
                
                oasys::ScopeLock bundle_lock(br->lock(), "ExternalRouter");
                
                MetaBlockSequence::const_iterator block_i;
                for (block_i = in_request.metadata_block().begin();
                     block_i != in_request.metadata_block().end();
                     ++block_i) {
    
                    if (!block_i->generated()) {
    
                        MetadataBlockRef existing("ExternalRouter metadata block search");
                        for (unsigned int i = 0;
                             i < br->recv_metadata().size(); ++i) {
                            if (br->recv_metadata()[i]->id() ==
                                block_i->identifier()) {
                                existing = br->recv_metadata()[i];
                                break;
                            }
                        }
    
                        if (existing != NULL) {
                            // Lock the block so nobody tries to read it while
                            // we are changing it.
                            oasys::ScopeLock metadata_lock(existing->lock(),
                                                           "ExternalRouter");
                        
                            // If the new block size is zero, it is being removed
                            // for this particular link.
                            if (block_i->contents().size() == 0) {
                                existing->remove_outgoing_metadata(link_ref);                        
                                log_info("Removing metadata block %u from bundle "
                                         "%" PRIbid " on link %s", block_i->identifier(),
                                         br->bundleid(), link.c_str());
                            }
                        
                            // Otherwise, if the new block size is non-zero, it
                            // it is being modified for this link.
                            else {
                                log_info("Modifying metadata block %u on bundle "
                                         "%" PRIbid" on link %s", block_i->identifier(),
                                         br->bundleid(), link.c_str());
                                existing->modify_outgoing_metadata(
                                              link_ref,
                                              (u_char*)block_i->contents().data(),
                                              block_i->contents().size());
                            }
                            continue;
                        }
    
                        ASSERT(existing == NULL);
    
                        LinkRef null_link("ExternalRouter::process_action");
                        MetadataVec * nulldata = br->generated_metadata().
                                                         find_blocks(null_link);
                        if (nulldata != NULL) {
                            for (unsigned int i = 0; i < nulldata->size(); ++i) {
                                if ((*nulldata)[i]->id() == block_i->identifier()) {
                                    existing = (*nulldata)[i];
                                    break;
                                }
    	                }
                        }
    
                        if (existing != NULL) {
                            MetadataVec * link_vec = br->generated_metadata().
                                                     find_blocks(link_ref);
                            if (link_vec == NULL) {
                                link_vec = br->mutable_generated_metadata()->
                                           create_blocks(link_ref);
                            }
                            ASSERT(link_vec != NULL);
    
                            ASSERT(existing->ontology() == block_i->type());
    
                            MetadataBlock * meta_block =
                                new MetadataBlock(
                                        existing->id(),
                                        block_i->type(),
                                        (u_char *)block_i->contents().data(),
                                        block_i->contents().size());
                            meta_block->set_flags(existing->flags());
    
    			link_vec->push_back(meta_block);
    
                            log_info("Adding a metadata block to bundle %" PRIbid " on "
                                     "link %s", br->bundleid(), link.c_str());
                            continue;
                        }
    
                        log_err("bundle %" PRIbid " does not have a block %u",
                                br->bundleid(), block_i->identifier());
    
                    } else {
                        ASSERT(block_i->generated());
    
                        MetadataVec* vec = 
                            br->generated_metadata().find_blocks(link_ref);
                        if (vec == NULL)
                            vec = br->mutable_generated_metadata()->create_blocks(link_ref);
                        
                        MetadataBlock* meta_block = new MetadataBlock(
                                block_i->type(),
                                (u_char*)block_i->contents().data(),
                                block_i->contents().size());
                        
                        vec->push_back(meta_block);
                        log_info("Adding an metadata block to bundle %" PRIbid " on "
                                 "link %s", br->bundleid(), link.c_str());
                    }
                 }
            }
            
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to send nonexistent bundle: %" PRIbid, bid);
        }
    } else if (instance->open_link_request().present()) {
        msg_processed = true;
        BundleDaemon *bd = BundleDaemon::instance();
        //log_debug("posting LinkStateChangeRequest");

        std::string lstr =
            instance->open_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::OPENING,
                                           ContactEvent::NO_INFO));
        } else {
            log_warn("attempt to open link %s that doesn't exist!",
                     lstr.c_str());
        }
    } else if (instance->close_link_request().present()) {
        msg_processed = true;
        BundleDaemon *bd = BundleDaemon::instance();
        //log_debug("posting LinkStateChangeRequest");

        std::string lstr =
            instance->close_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::CLOSED,
                                           ContactEvent::NO_INFO));
        } else {
            log_warn("attempt to close link %s that doesn't exist!",
                     lstr.c_str());
        }
    } else if (instance->add_link_request().present()) {
        msg_processed = true;
        //log_debug("posting LinkCreateRequest");

        rtrmessage::add_link_request request
            = instance->add_link_request().get();
        std::string clayer = request.clayer();
        ConvergenceLayer *cl = ConvergenceLayer::find_clayer(clayer.c_str());
        if (!cl) {
            log_warn("attempt to create link using non-existent CLA %s",
                     clayer.c_str());
        }
        else {
            std::string name = request.link_id();
            Link::link_type_t type = convert_link_type( request.link_type() );
            std::string eid = request.remote_eid().uri();

            AttributeVector params;

            if (request.link_config_params().present()) {
                linkConfigType::cl_params::container
                    c = request.link_config_params().get().cl_params();
                linkConfigType::cl_params::container::iterator iter;

                for (iter = c.begin(); iter < c.end(); iter++) {
                    if (iter->bool_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->bool_value()));
                    else if (iter->u_int_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->u_int_value()));
                    else if (iter->int_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->int_value()));
                    else if (iter->str_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->str_value()));
                    else
                        log_warn("unknown value type in key-value pair");
                }
            }

            BundleDaemon::post(
                new LinkCreateRequest(name, type, eid, cl, params));
        }
    } else if (instance->delete_link_request().present()) {
        msg_processed = true;
        BundleDaemon *bd = BundleDaemon::instance();
        //log_debug("posting LinkDeleteRequest");

        std::string lstr = instance->delete_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(new LinkDeleteRequest(link));
        } else {
            log_warn("attempt to delete link %s that doesn't exist!",
                     lstr.c_str());
        }
    } else if (instance->reconfigure_link_request().present()) {
        msg_processed = true;
        BundleDaemon *bd = BundleDaemon::instance();

        rtrmessage::reconfigure_link_request request
            = instance->reconfigure_link_request().get();
        std::string lstr = request.link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        rtrmessage::linkConfigType request_params=request.link_config_params();

        if (link.object() != 0) {
            AttributeVector params;
            if (request_params.is_usable().present()) {
                params.push_back(NamedAttribute("is_usable", request_params.is_usable().get()));
            }
            if (request_params.reactive_frag_enabled().present()) {
                // xlate between router.xsd/clevent.xsd
                params.push_back(NamedAttribute("reactive_fragment", request_params.reactive_frag_enabled().get()));
            }
            if (request_params.nexthop().present()) {
                params.push_back(NamedAttribute("nexthop", request_params.nexthop().get()));
            }
            // Following are DTNME parameters not listed in the DP interface
            if (request_params.min_retry_interval().present()) {
                params.push_back(
                    NamedAttribute("min_retry_interval",
                                   request_params.min_retry_interval().get()));
            }
            if (request_params.max_retry_interval().present()) {
                params.push_back(
                    NamedAttribute("max_retry_interval",
                                   request_params.max_retry_interval().get()));
            }
            if (request_params.idle_close_time().present()) {
                params.push_back(
                    NamedAttribute("idle_close_time",
                                   request_params.idle_close_time().get()));
            }

            linkConfigType::cl_params::container
                c = request_params.cl_params();
            linkConfigType::cl_params::container::iterator iter;

            for (iter = c.begin(); iter < c.end(); iter++) {
              if (iter->bool_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->bool_value().get()));
              else if (iter->u_int_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->u_int_value().get()));
              else if (iter->int_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->int_value().get()));
              else if (iter->str_value().present()) 
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->str_value().get()));
              else
                log_warn("unknown value type in key-value pair");
            }

            // Post at head in case the event queue is backed up
            // XXX/dz configure start/stop transmitting should not wait
            BundleDaemon::post_at_head(new LinkReconfigureRequest(link, params));
        } else {
            log_warn("attempt to reconfigure link %s that doesn't exist!",
                     lstr.c_str());
        }
    } else if (instance->inject_bundle_request().present()) {
        msg_processed = true;
        //log_debug("posting BundleInjectRequest");

        inject_bundle_request fields = instance->inject_bundle_request().get();

        //XXX Where did the other BundleInjectRequest constructor go?
        BundleInjectRequest *request = new BundleInjectRequest();

        request->src_ = fields.source().uri();
        request->dest_ = fields.dest().uri();
        request->link_ = fields.link_id();
        request->request_id_ = fields.request_id();
        
        request->payload_file_ = fields.payload_file();

        if (fields.replyto().present())
            request->replyto_ = fields.replyto().get().uri();
        else
            request->replyto_ = "";

        if (fields.custodian().present())
            request->custodian_ = fields.custodian().get().uri();
        else
            request->custodian_ = "";

        if (fields.priority().present())
            request->priority_ = convert_priority( fields.priority().get() );
        else
            request->priority_ = Bundle::COS_BULK; // default

        if (fields.expiration().present())
            request->expiration_ = fields.expiration().get();
        else
            request->expiration_ = 0; // default will be used

        BundleDaemon::post(request);
    } else if (instance->cancel_bundle_request().present()) {
        msg_processed = true;
        //log_debug("posting BundleCancelRequest");

        std::string link =
            instance->cancel_bundle_request().get().link_id();

        bundleid_t bid = instance->cancel_bundle_request().get().local_id();
        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (!bd->pending_bundles()->contains(br)) {
            br.release();
        }

        if (br.object()) {
            BundleCancelRequest *request = new BundleCancelRequest(br, link);
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to cancel send of nonexistent bundle: %" PRIbid, bid);
        }
    } else if (instance->delete_bundle_request().present()) {
        msg_processed = true;
        //log_debug("posting BundleDeleteRequest");

        bundleid_t bid = instance->delete_bundle_request().get().local_id();

        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (br.object())
        {
            br->set_manually_deleting(true);

            if (!bd->pending_bundles()->contains(br)) {
                br.release();
            }

            if (br.object()) {
                BundleDeleteRequest *request =
                    new BundleDeleteRequest(br,
                                            BundleProtocol::REASON_NO_ADDTL_INFO);
                BundleDaemon::post(request);
            }
        }
        else {
            log_warn("attempt to delete nonexistent bundle: %" PRIbid, bid);
        }
    } else if (instance->take_custody_of_bundle_request().present()) {
        msg_processed = true;
        //log_debug("posting TakeCustodyOfBundleRequest");

        bundleid_t bid = instance->take_custody_of_bundle_request().get().local_id();
        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (!bd->pending_bundles()->contains(br)) {
            br.release();
        }

        if (br.object()) {
            BundleTakeCustodyRequest *request =
                new BundleTakeCustodyRequest(br);
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to take custody of nonexistent bundle: %" PRIbid, bid);
        }
    } else if (instance->set_cl_params_request().present()) {
        msg_processed = true;
        //log_debug("posting CLASetParamsRequest");

        std::string clayer = instance->set_cl_params_request().get().clayer();
        ConvergenceLayer *cl = ConvergenceLayer::find_clayer(clayer.c_str());
        if (!cl) {
            log_warn("attempt to set parameters for non-existent CLA %s",
                     clayer.c_str());
        }
        else {
            AttributeVector params;

            set_cl_params_request::cl_params::container
                c = instance->set_cl_params_request().get().cl_params();
            set_cl_params_request::cl_params::container::iterator iter;

            for (iter = c.begin(); iter < c.end(); iter++) {
                if (iter->bool_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->bool_value()));
                else if (iter->u_int_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->u_int_value()));
                else if (iter->int_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->int_value()));
                else if (iter->str_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->str_value()));
                else
                    log_warn("unknown value type in key-value pair");
            }

            BundleDaemon::post(new CLASetParamsRequest(cl, params));
        }
    } else if (instance->bundle_attributes_query().present()) {
        msg_processed = true;
        //log_debug("posting BundleAttributesQueryRequest");
        bundle_attributes_query query =
            instance->bundle_attributes_query().get();
        std::string query_id = query.query_id();

        bundleid_t bid = query.local_id();
        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (!bd->pending_bundles()->contains(br)) {
            br.release();
        }

        // XXX note, if we want to send a report even when the bundle does
        // not exist (instead of ignoring the request), we have to not test
        // for the existence of the object here (it is tested again in
        // BundleDaemon::handle_bundle_attributes_query)
        if (br.object()) {
            AttributeNameVector attribute_names;
            MetaBlockRequestVector metadata_blocks;
            bundle_attributes_query::query_params::container
                c = query.query_params();
            bundle_attributes_query::query_params::container::iterator iter;
    
            for (iter = c.begin(); iter != c.end(); iter++) {
                bundleAttributesQueryType& q = *iter;
    
                if (q.query().present()) {
                    attribute_names.push_back( AttributeName(q.query().get()) );
                }
                if (q.meta_blocks().present()) {
                    bundleMetaBlockQueryType& block = q.meta_blocks().get();
                    int query_value = -1;
                    MetadataBlockRequest::QueryType query_type;
                    
                    if (block.identifier().present()) {
                        query_value = block.identifier().get();
                        query_type = MetadataBlockRequest::QueryByIdentifier;
                    }
                    
                    else if (block.type().present()) {
                        query_value = block.type().get();
                        query_type = MetadataBlockRequest::QueryByType;
                    }
                    
                    if (query_value < 0)
                        query_type = MetadataBlockRequest::QueryAll;
                    
                    metadata_blocks.push_back(
                        MetadataBlockRequest(query_type, query_value) );
                }
            }
    
            BundleAttributesQueryRequest* request
                = new BundleAttributesQueryRequest(query_id, br, attribute_names);
    
            if (metadata_blocks.size() > 0) {
                request->metadata_blocks_ = metadata_blocks;
            }
    
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to query nonexistent bundle: %" PRIbid, bid);
        }
    } else if (instance->link_query().present()) {
        msg_processed = true;
        //log_debug("posting LinkQueryRequest");
        BundleDaemon::post(new LinkQueryRequest());
    } else if (instance->link_attributes_query().present()) {
        msg_processed = true;
        BundleDaemon *bd = BundleDaemon::instance();
        //log_debug("posting LinkAttributesQueryRequest");

        link_attributes_query query = instance->link_attributes_query().get();
        std::string query_id = query.query_id();
        std::string lstr = query.link_id();

        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
           AttributeNameVector attribute_names;

           link_attributes_query::query_params::container 
             c = query.query_params();
           link_attributes_query::query_params::container::iterator iter;

           for (iter = c.begin(); iter < c.end(); iter++) {
             attribute_names.push_back( AttributeName(*iter) );
           }

          BundleDaemon::post(new LinkAttributesQueryRequest(query_id, 
                                                            link, 
                                                            attribute_names));
        } else {
            log_warn("attempt to query attributes of link %s that doesn't exist!",
                     lstr.c_str());
        }
    } else if (instance->bundle_query().present()) {
        msg_processed = true;
        //log_debug("posting BundleQueryRequest");
        BundleDaemon::post(new BundleQueryRequest());
    } else if (instance->contact_query().present()) {
        msg_processed = true;
        //log_debug("posting ContactQueryRequest");
        BundleDaemon::post(new ContactQueryRequest());
    } else if (instance->route_query().present()) {
        msg_processed = true;
        //log_debug("posting RouteQueryRequest");
        BundleDaemon::post(new RouteQueryRequest());
    } else if (instance->deliver_bundle_to_app_request().present()) {
        /* This is needed for delivering to LB applications */
        msg_processed = true;
        //log_debug("posting DeliverBundleToAppRequest");
        deliver_bundle_to_app_request& in_request = 
            instance->deliver_bundle_to_app_request().get();

        eidType reg = in_request.endpoint();
        EndpointID reg_eid;
        reg_eid.assign(reg.uri());

        bundleid_t bid = in_request.local_id();
        BundleDaemon *bd = BundleDaemon::instance();
        BundleRef br = bd->all_bundles()->find_for_storage(bid);
        if (!bd->pending_bundles()->contains(br)) {
            br.release();
        }

        if (br.object()) {
            bd->check_and_deliver_to_registrations(br.object(), reg_eid);
        }
        else {
            log_warn("attempt to deliver nonexistent bundle %" PRIbid " to app %s",
                     bid, reg_eid.c_str());
        }
    }
    else if (instance->shutdown_request().present()) {
        msg_processed = true;
        log_info("Initiating Shutdown procedure");
        // XXX/dz Note that if running with the console then
        // the program does not actually exit until the user hits
        // <Return> because it is waiting in a readline for input
        oasys::TclCommandInterp::instance()->exec_command("shutdown");
        oasys::TclCommandInterp::instance()->exit_event_loop();
        oasys::TclCommandInterp::instance()->exec_command("shutdown");
    }

    //dzdebug
    if (!msg_processed) {
        log_crit("ExternalRouter::ModuleServer - message type ignored - probably loopback");
    }
}

Link::link_type_t
ExternalRouter::ModuleServer::convert_link_type(rtrmessage::linkTypeType type)
{
    if (type==linkTypeType(linkTypeType::alwayson)){
        return Link::ALWAYSON;
    }
    else if (type==linkTypeType(linkTypeType::ondemand)){
        return Link::ONDEMAND;
    }
    else if (type==linkTypeType(linkTypeType::scheduled)){
        return Link::SCHEDULED;
    }
    else if (type==linkTypeType(linkTypeType::opportunistic)){
        return Link::OPPORTUNISTIC;
    }

    return Link::LINK_INVALID;
}

ForwardingInfo::action_t
ExternalRouter::ModuleServer::convert_fwd_action(rtrmessage::bundleForwardActionType action)
{
    if (action==bundleForwardActionType(bundleForwardActionType::forward)){
        return ForwardingInfo::FORWARD_ACTION;
    }
    else if (action== bundleForwardActionType(bundleForwardActionType::copy)){
        return ForwardingInfo::COPY_ACTION;
    }

    return ForwardingInfo::INVALID_ACTION;
}

Bundle::priority_values_t
ExternalRouter::ModuleServer::convert_priority(rtrmessage::bundlePriorityType priority)
{
    if (priority == bundlePriorityType(bundlePriorityType::bulk)){
        return Bundle::COS_BULK;
    }
    else if (priority == bundlePriorityType(bundlePriorityType::normal)){
        return Bundle::COS_NORMAL;
    }
    else if (priority == bundlePriorityType(bundlePriorityType::expedited)){
        return Bundle::COS_EXPEDITED;
    }

    return Bundle::COS_INVALID;
}

ExternalRouter::ModuleServer::Receiver::Receiver(ModuleServer* parent)
    : IOHandlerBase(new oasys::Notifier("/router/external/moduleserver/rcvr")),
      Thread("/router/external/moduleserver/rcvr"),
      parent_(parent)
{
    set_logpath("/router/external/moduleserver/rcvr");

    params_.reuseport_ = true;

    last_recv_seq_ctr_ = 0;

    // router interface and external routers must be able to bind
    // to the same port
    params_.recv_bufsize_ = 10240000;
    if (fd() == -1) {
        init_socket();
    }

    bind(ExternalRouter::multicast_addr_, ExternalRouter::server_port);

    // join the "all routers" (or configured) multicast group
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = ExternalRouter::multicast_addr_;
    mreq.imr_interface.s_addr = ExternalRouter::network_interface_;
    if (setsockopt(fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0)
        log_err("ExternalRouter::ModuleServer::Receiver::Receiver():  "
                "Failed to join multicast group:  %s", strerror(errno));

    // source messages from the loopback interface (or configured I/F)
    in_addr src_if;
    src_if.s_addr = ExternalRouter::network_interface_;
    if (setsockopt(fd(), IPPROTO_IP, IP_MULTICAST_IF,
                   &src_if, sizeof(src_if)) < 0)
        log_err("ExternalRouter::ModuleServer::Receiver::Receiver():  "
                "Failed to set IP_MULTICAST_IF:  %s", strerror(errno));

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    set_logfd(false);
}

ExternalRouter::ModuleServer::Receiver::~Receiver()
{
}

// ModuleServer::Receiver main loop
void
ExternalRouter::ModuleServer::Receiver::run() 
{
     
    int rcv_bufsize = 0;
    int snd_bufsize = 0;
    socklen_t optlen = sizeof(rcv_bufsize);
    getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcv_bufsize, &optlen);
    getsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &snd_bufsize, &optlen);
    log_always("ExternalRouter::ModuleServer::Receiver::run - SocketBuffer sizes rcv/send: %u/%u",
               rcv_bufsize, snd_bufsize);

    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* sock_poll = &pollfds[0];
    sock_poll->fd = fd();
    sock_poll->events = POLLIN;
    sock_poll->revents = 0;

    char buf[MAX_UDP_PACKET];
    in_addr_t raddr;
    u_int16_t rport;
    int bytes;

    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10,
            get_notifier());

        if (ret == oasys::IOINTR) {
            //log_debug("module server interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            //log_debug("module server error");
            set_should_stop();
            continue;
        }

        if (sock_poll->revents & POLLIN) {
            bytes = recvfrom(buf, MAX_UDP_PACKET, 0, &raddr, &rport);
            buf[bytes] = '\0';

            parent_->post(new std::string(buf));
        }
    }
}


ExternalRouter::ModuleServer::Sender::Sender()
    : IOHandlerBase(new oasys::Notifier("/router/external/moduleserver/sndr")),
      Thread("/router/external/moduleserver/sndr"),
      bucket_(logpath(), 50000000, 65535*8)
{
    set_logpath("/router/external/moduleserver/sndr");

    bytes_sent_ = 0;

    // router interface and external routers must be able to bind
    // to the same port
    params_.send_bufsize_ = 1024000;
    if (fd() == -1) {
        init_socket();
    }

    //unsigned char off = 0;
    //if (setsockopt(fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off)) < 0)
    //    log_err("ExternalRouter::ModuleServer::Server::Server():  "
    //            "Failed to disable multicast loopback:  %s", strerror(errno));

    // source messages from the loopback interface
    in_addr src_if;
    src_if.s_addr = ExternalRouter::network_interface_;
    if (setsockopt(fd(), IPPROTO_IP, IP_MULTICAST_IF,
                   &src_if, sizeof(src_if)) < 0)
        log_err("ExternalRouter::ModuleServer::Server::Server():  "
                "Failed to set IP_MULTICAST_IF:  %s", strerror(errno));

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    set_logfd(false);

    eventq_ = new oasys::MsgQueue< std::string * >(logpath_);
}

ExternalRouter::ModuleServer::Sender::~Sender()
{
    // free all pending events
    std::string *event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}

/// Post a string to send 
void
ExternalRouter::ModuleServer::Sender::post(std::string* event)
{
    eventq_->push_back(event);
}

/// ModuleServer Sender main loop
void
ExternalRouter::ModuleServer::Sender::run() 
{
     
    int rcv_bufsize = 0;
    int snd_bufsize = 0;
    socklen_t optlen = sizeof(rcv_bufsize);
    getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcv_bufsize, &optlen);
    getsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &snd_bufsize, &optlen);
    log_always("ExternalRouter::ModuleServer::Sender::run - SocketBuffer sizes rcv/send: %u/%u",
               rcv_bufsize, snd_bufsize);

    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    bool first_transmit = true;
    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10,
            get_notifier());

        if (ret == oasys::IOINTR) {
            //log_debug("module server interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            //log_debug("module server error");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                if (first_transmit) {
                    first_transmit = false;
                    transmit_timer_.get_time();
                }

//dzdebug??                while (!bucket_.try_to_drain(event->size()*8)) {
//dzdebug??                    spin_yield();
//dzdebug??                }

                int cc = sendto(const_cast< char * >(event->c_str()),
                    event->size(), 0,
                    ExternalRouter::multicast_addr_,
                    ExternalRouter::server_port);

                bytes_sent_ += cc;
                if (transmit_timer_.elapsed_ms() >= 1000) {
                    //dzdebug --- < 24Mbps with no rate limiting
                    bytes_sent_ *= 8;
                    //log_debug("ExternalRouter sent %" PRIu64 " bits in %u ms", 
                    //         bytes_sent_, transmit_timer_.elapsed_ms());
                    transmit_timer_.get_time();
                    bytes_sent_ = 0;
                }

                delete event;
            }
        }
    }

}

//----------------------------------------------------------------------
ExternalRouter::ModuleServer::TcpInterface::TcpInterface(ModuleServer* parent, in_addr_t listen_addr, u_int16_t listen_port)
    : TCPServerThread("ExternalRouter::ModuleServer::TcpInterface",
			"/router/external/moduleserver/TcpInterface")
{
    logfd_       = false;

    // we always delete the thread object when we exit
    //Thread::set_flag(Thread::DELETE_ON_EXIT);

    parent_ = parent;
    listen_addr_ = listen_addr;
    listen_port_ = listen_port;

    router_client_ = NULL;
}

//----------------------------------------------------------------------
ExternalRouter::ModuleServer::TcpInterface::~TcpInterface()
{
    do_shutdown();
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpInterface::do_shutdown()
{
    stop();

    if (router_client_ != NULL)
    {
        router_client_->set_should_stop();
    }
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpInterface::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    if (router_client_ == NULL)
    {
      router_client_ = new TcpConnection(parent_, fd, addr, port);
      log_info("External Router accepted TCP connection from %s:%d", intoa(addr), port);
      router_client_->start();
    }
    else
    {
      log_warn("Rejected External Router TCP connection while already connectd");
      ::close(fd);
    }
}


//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::client_closed ( TcpConnection* cl )
{
    if (tcp_interface_ != NULL)
    {
      tcp_interface_->client_closed(cl);
    }
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpInterface::client_closed ( TcpConnection* cl )
{
    if (router_client_ == cl)
    {
      router_client_ = NULL;
      log_info("External Router closed TCP connection");
      //log_info("External Router closed TCP connection from %s:%d", intoa(addr), port);
    }
    else
    {
      log_crit("External Router attempt to close unknown TCP connection??");
    }
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpInterface::post_to_send(std::string* event)
{
    if (router_client_ != NULL )
    {
        router_client_->post_to_send(event);
    }
    else
    {
        delete event;
        //log_debug("Message dropped since external router is not connected");
    }
}

//----------------------------------------------------------------------
ExternalRouter::ModuleServer::TcpConnection::TcpConnection(ModuleServer* parent,
                                             int fd, in_addr_t client_add,
                                             u_int16_t client_port) 
            : TCPClient(fd, client_add, client_port),
              Thread("ExternalRouterTcpConnection") 
{
    parent_ = parent;
    client_addr_ = client_add;
    client_port_ = client_port;
    tcp_sender_ = NULL;
}

//----------------------------------------------------------------------
ExternalRouter::ModuleServer::TcpConnection::~TcpConnection()
{
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpConnection::do_shutdown()
{
    set_should_stop();

    if (tcp_sender_ != NULL)
    {
        tcp_sender_->set_should_stop();
    }

    int ctr = 0;
    while ((tcp_sender_ != NULL) && (++ctr < 20)) {
        usleep(100000);
    }

    delete this;
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpConnection::process_data(u_char* bp, size_t len)
{
    (void) bp;
    (void) len;
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpConnection::post_to_send(std::string* event)
{
    if (!should_stop()) {
        if (tcp_sender_ != NULL)
        {
            tcp_sender_->post(event);
        }
        else
        {
            log_err("Deleting message posted while TCPSender thread not started");
            delete event;
        }
    }
    else {
        log_err("Deleting message posted while stopping");
        delete event;
    }
}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpConnection::run()
{
    int ret;

    int all_data     = 0;
    int data_ptr     = 0;
    int data_to_read = 0;

    union US {
       u_char char_size[4];
       int    int_size;
    };

    US union_size;

    oasys::ScratchBuffer<u_char*, 0> buffer;


    // spawn a sender thread
    tcp_sender_ = new TcpSender(this);
    tcp_sender_->start();


    while (!should_stop()) {
        
        data_to_read = 4;       
        //
        // get the size off the wire and then read the data
        //
        data_ptr     = 0;
        while (!should_stop() && (data_to_read > 0))
        {
            ret = timeout_read((char *) &(union_size.char_size[data_ptr]), data_to_read, 100);
            if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
                close();
                set_should_stop();
                break;
            }
            if (ret > 0) {
                data_ptr     += ret;
                data_to_read -= ret;
            }
        }

        if (should_stop())
            break;

        data_ptr = 0;
        all_data = data_to_read = ntohl(union_size.int_size);
        if (all_data == 0) {
            continue;
        }
        buffer.clear();
        buffer.reserve(data_to_read+1); // reserve storage

        while (!should_stop() && (data_to_read > 0)) {
            ret = timeout_read((char*) buffer.buf()+data_ptr, data_to_read, 100);
            if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
                close();
                set_should_stop();
                break;
            }
            if (ret > 0) {
                buffer.incr_len(ret);
                data_to_read -= ret;
                data_ptr     += ret;
            }
        } 


        if (should_stop())
            break;


        //log_debug("ExternalRouter TCP Connection got %d byte packet", all_data);
        process_data(buffer.buf(),all_data);

        memset(buffer.buf()+all_data, 0, 1);

        parent_->post(new std::string((const char*) buffer.buf()));
    }

    tcp_sender_ = NULL;

    close();

    // inform parent we are exiting
    parent_->client_closed ( this );

}

//----------------------------------------------------------------------
void
ExternalRouter::ModuleServer::TcpConnection::sender_closed ( TcpSender* sender )
{
    if (tcp_sender_ == sender)
    {
      tcp_sender_ = NULL;
    }
}

ExternalRouter::ModuleServer::TcpConnection::TcpSender::TcpSender(TcpConnection* parent)
    : Thread("/router/external/moduleserver/tcpsndr"),
      Logger("ExternalRouter::ModuleServer::TcpConnection::TcpSender", "/router/external/moduleserver/tcpsndr")
{
    set_logpath("/router/external/moduleserver/tcpsndr");

    parent_ = parent;

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< std::string * >(logpath_);
}

ExternalRouter::ModuleServer::TcpConnection::TcpSender::~TcpSender()
{
    // free all pending events
    std::string *event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}

void
ExternalRouter::ModuleServer::TcpConnection::TcpSender::post(std::string* event)
{
    eventq_->push_back(event);
}

void
ExternalRouter::ModuleServer::TcpConnection::TcpSender::run() 
{
     
    log_always("ExternalRouter::ModuleServer::TcpSender::run - started");

    int cc;

    union US { 
       char       chars[4];
       int32_t    int_size;
    };   

    US union_size;


    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

        if (ret == oasys::IOINTR) {
            //log_debug("module server TcpSender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            //log_debug("module server TcpSender error");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                union_size.int_size = htonl(event->size());

                cc = parent_->writeall(union_size.chars, 4);
                if (cc != 4) {
                    log_err("error writing msg: %s", strerror(errno));
                } else {
                    //log_debug("Transmit message of length: %zu", event->size());


                    cc = parent_->writeall( const_cast< char * >(event->c_str()), event->size());
                    if (cc != (int) event->size())
                    {
                        log_err("error writing msg: %s", strerror(errno));
                    }
                }

                delete event;
            }
        }
    }

//    parent_->sender_closed ( this );
}


ExternalRouter::HelloTimer::HelloTimer(ExternalRouter *router)
    : router_(router)
{
}

ExternalRouter::HelloTimer::~HelloTimer()
{
    cancel();
}

// Timeout callback for the hello timer
void
ExternalRouter::HelloTimer::timeout(const struct timeval &)
{
    bpa message;
    message.hello_interval(ExternalRouter::hello_interval);

#ifdef DEBUG_RTR_MSGS
    router_->send(message, "hello");
#else
    router_->send(message, NULL);
#endif

    schedule_in(ExternalRouter::hello_interval * 1000);
}

ExternalRouter::ERRegistration::ERRegistration(ExternalRouter *router)
    : Registration(Registration::EXTERNALROUTER_REGID,
                    EndpointID(BundleDaemon::instance()->local_eid().str() +
                        EXTERNAL_ROUTER_SERVICE_TAG),
                   Registration::DEFER, 0, 0),
      router_(router)
{
    logpathf("/reg/admin");
    
    BundleDaemon::post(new RegistrationAddedEvent(this, EVENTSRC_ADMIN));
}

// deliver a bundle to external routers
void
ExternalRouter::ERRegistration::deliver_bundle(Bundle *bundle)
{
    bundle_delivery_event e(bundle,
                            bundle->bundleid(),
                            bundle->gbofid_str());

    e.bundle().payload_file( bundle->payload().filename() );

    bpa message;
    message.bundle_delivery_event(e);
#ifdef DEBUG_RTR_MSGS
    router_->send(message, "bundle_delivery_event");
#else
    router_->send(message, NULL);
#endif

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

// Global shutdown callback function
void external_rtr_shutdown(void *)
{
    BundleDaemon::instance()->router()->shutdown();
}

// Initialize ExternalRouter parameters
u_int16_t ExternalRouter::server_port          = 8001;
u_int16_t ExternalRouter::hello_interval       = 30;
std::string ExternalRouter::schema             = INSTALL_SYSCONFDIR "/router.xsd";
bool ExternalRouter::server_validation         = true;
bool ExternalRouter::client_validation         = false;
in_addr_t ExternalRouter::multicast_addr_      = htonl(INADDR_ALLRTRS_GROUP);
in_addr_t ExternalRouter::network_interface_   = htonl(INADDR_LOOPBACK);
bool ExternalRouter::use_tcp_interface_        = false;

} // namespace dtn
#endif // XERCES_C_ENABLED && EXTERNAL_DP_ENABLED
