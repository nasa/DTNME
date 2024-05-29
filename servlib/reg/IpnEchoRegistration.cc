/*
 *    Copyright 2015 United States Government as represented by NASA
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

#include <third_party/oasys/util/ScratchBuffer.h>

#include "IpnEchoRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

IpnEchoRegistration::IpnEchoRegistration(const SPtr_EID& sptr_eid)
    : Registration(IPN_ECHO_REGID, sptr_eid, Registration::DEFER, Registration::NONE, 0, 0) 
{
    logpathf("/dtn/reg/ipnecho");
    set_active(true);

    reg_list_type_str_ = "IpnEchoReg";
}

int
IpnEchoRegistration::deliver_bundle(Bundle* bundle, SPtr_Registration& sptr_reg)
{
    (void) sptr_reg;

    size_t payload_len = bundle->payload().length();
    log_debug("%zu byte ping from %s",
              payload_len, bundle->source().c_str());
    
    size_t ipn_echo_max_length = BundleDaemon::params_.ipn_echo_max_return_length_;

    Bundle* reply;

    if ( bundle->is_bpv6() ) {
        reply = new Bundle(BundleProtocol::BP_VERSION_6);
    } else {
        reply = new Bundle(BundleProtocol::BP_VERSION_7);
    }

    reply->set_source(sptr_endpoint_->str());
    reply->mutable_dest() = bundle->source();
    reply->mutable_replyto() = BD_MAKE_EID_NULL();
    reply->mutable_custodian() = BD_MAKE_EID_NULL();
    reply->set_expiration_millis(bundle->expiration_millis());

    if ((ipn_echo_max_length > 0) && (payload_len > ipn_echo_max_length)) {
        reply->mutable_payload()->set_length(ipn_echo_max_length);
        reply->mutable_payload()->write_data(bundle->payload(), 0, ipn_echo_max_length, 0);
    } else {
        reply->mutable_payload()->set_length(payload_len);
        reply->mutable_payload()->write_data(bundle->payload(), 0, payload_len, 0);
    }

    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(reply, EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
    // mark bundle as delivered
    bundle->fwdlog()->update(this, ForwardingInfo::DELIVERED);

    return REG_DELIVER_BUNDLE_SUCCESS;
}


} // namespace dtn
