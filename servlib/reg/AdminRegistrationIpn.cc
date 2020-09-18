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

#include <oasys/util/ScratchBuffer.h>

#include "AdminRegistrationIpn.h"
#include "RegistrationTable.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleStatusReport.h"
#include "bundling/CustodySignal.h"
#include "routing/BundleRouter.h"

namespace dtn {

AdminRegistrationIpn::AdminRegistrationIpn()
    : Registration(ADMIN_REGID_IPN,
                   BundleDaemon::instance()->local_eid_ipn(),
                   Registration::DEFER,
                   Registration::NEW, 0, 0)
{
    logpathf("/dtn/reg/adminipn");
    set_active(true);
}

void
AdminRegistrationIpn::deliver_bundle(Bundle* bundle)
{
    u_char typecode;

    size_t payload_len = bundle->payload().length();
    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const u_char* payload_buf = 
        bundle->payload().read_data(0, payload_len, scratch.buf(payload_len));
    
    log_debug("got %zu byte bundle", payload_len);
        
    bool is_delivered = true;

    if (payload_len == 0) {
        log_err("admin registration got 0 byte *%p", bundle);
        is_delivered = false;
        goto done;
    }

    if (!bundle->is_admin()) {
        if (NULL != strstr((const char*)payload_buf, "ping"))  {
            Bundle* reply = new Bundle();
            reply->mutable_source()->assign(endpoint_);
            reply->mutable_dest()->assign(bundle->source());
            reply->mutable_replyto()->assign(EndpointID::NULL_EID());
            reply->mutable_custodian()->assign(EndpointID::NULL_EID());
            reply->set_expiration(bundle->expiration());

            reply->mutable_payload()->set_length(payload_len);
            reply->mutable_payload()->write_data(bundle->payload(), 0, payload_len, 0);

            BundleDaemon::post(new BundleReceivedEvent(reply, EVENTSRC_ADMIN));
        } else {
            log_warn("non-admin *%p sent to local eid", bundle);
            is_delivered = false;
        } 
        goto done;
    }


    /*
     * As outlined in the bundle specification, the first four bits of
     * all administrative bundles hold the type code, with the
     * following values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - echo request
     * 0x4     - aggregate custodial signal
     * 0x5     - announce
     * (other) - reserved
     */
    typecode = payload_buf[0] >> 4;
    
    switch(typecode) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
    {
        BundleStatusReport::data_t sr_data;
        if (BundleStatusReport::parse_status_report(&sr_data, bundle))
        {
           GbofId source_gbofid(sr_data.orig_source_eid_,
                                sr_data.orig_creation_tv_,
                                (sr_data.orig_frag_length_ > 0),
                                sr_data.orig_frag_length_,
                                sr_data.orig_frag_offset_);

            char tmptxt[32];
            std::string rpt_text;
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_RECEIVED)
            {
                rpt_text.append("RECEIVED at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.receipt_tv_.seconds_);
                rpt_text.append(tmptxt);
            }
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_CUSTODY_ACCEPTED)
            {
                if (rpt_text.length() > 0) rpt_text.append(" & ");
                rpt_text.append("CUSTODY_ACCEPTED at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.custody_tv_.seconds_);
                rpt_text.append(tmptxt);
            }
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_FORWARDED)
            {
                if (rpt_text.length() > 0) rpt_text.append(" & ");
                rpt_text.append("FORWARDED at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.forwarding_tv_.seconds_);
                rpt_text.append(tmptxt);
            }
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_DELIVERED)
            {
                if (rpt_text.length() > 0) rpt_text.append(" & ");
                rpt_text.append("DELIVERED at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.delivery_tv_.seconds_);
                rpt_text.append(tmptxt);
            }
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_ACKED_BY_APP)
            {
                if (rpt_text.length() > 0) rpt_text.append(" & ");
                rpt_text.append("ACKED_BY_APP at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.receipt_tv_.seconds_);
                rpt_text.append(tmptxt);
            }
            if (sr_data.status_flags_ & BundleStatusReport::STATUS_DELETED)
            {
                if (rpt_text.length() > 0) rpt_text.append(" & ");
                rpt_text.append("DELETED at ");
                snprintf(tmptxt, sizeof(tmptxt), "%" PRIu64, sr_data.deletion_tv_.seconds_);
                rpt_text.append(tmptxt);
            }

            log_info_p("/statusrpt", "Report from %s: Bundle %s status(%d): %s : %s",
                       bundle->source().c_str(),
                       source_gbofid.str().c_str(),
                       sr_data.status_flags_,
                       rpt_text.c_str(),
                       BundleStatusReport::reason_to_str(sr_data.reason_code_));
            
        } else {
            log_err("Error parsing Status Report bundle: *%p", bundle);
            is_delivered = false;
        }           
        break;
    }
    
    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
    {
        log_info("ADMIN_CUSTODY_SIGNAL *%p received", bundle);
        CustodySignal::data_t data;
        
        bool ok = CustodySignal::parse_custody_signal(&data, payload_buf, payload_len);
        if (!ok) {
            log_err("malformed custody signal *%p", bundle);
            break;
        }

        BundleDaemon::post(new CustodySignalEvent(data, 0)); // Bundle ID will be filled in later

        break;
    }
#ifdef ACS_ENABLED
    case BundleProtocol::ADMIN_AGGREGATE_CUSTODY_SIGNAL:
    {
        log_info("ADMIN_AGGREGATE_CUSTODY_SIGNAL *%p received", bundle);
        AggregateCustodySignal::data_t data;
        
        bool ok = AggregateCustodySignal::parse_aggregate_custody_signal(&data, payload_buf, payload_len);
        if (!ok) {
            log_err("malformed aggregate custody signal *%p", bundle);

            // delete the entry map which was allocated in 
            // AggregateCustodySignal::parse_aggregate_custody_signal()
            delete data.acs_entry_map_;

            break;
        }
        BundleDaemon::post(new AggregateCustodySignalEvent(data));

        //XXX/dz try to determine if the External Router is active?
        BundleDaemon::post(new ExternalRouterAcsEvent((const char*)payload_buf, payload_len));

        break;
    }
#endif // ACS_ENABLED
    case BundleProtocol::ADMIN_ANNOUNCE:
    {
        log_info("ADMIN_ANNOUNCE from %s", bundle->source().c_str());
        break;
    }
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
        is_delivered = false;
    }    


 done:
    // Flag Admin bundles as delivered
    if (is_delivered) {
        bundle->fwdlog()->update(this, ForwardingInfo::DELIVERED);
    }

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}


} // namespace dtn
