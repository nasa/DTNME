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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#include "BP6_BundleStatusReport.h"
#include "BundleDaemon.h"
#include "SDNV.h"
#include <netinet/in.h>
#include <third_party/oasys/util/ScratchBuffer.h>

namespace dtn {

//----------------------------------------------------------------------
void
BP6_BundleStatusReport::create_status_report(Bundle*           bundle,
                                             const Bundle*     orig_bundle,
                                             const SPtr_EID&   sptr_source,
                                             BundleStatusReport::flag_t  status_flags,
                                             reason_t          reason)
{
    bundle->set_source(sptr_source);
    if (orig_bundle->replyto() == BD_NULL_EID()) {
        bundle->mutable_dest() = orig_bundle->source();
    } else {
        bundle->mutable_dest() = orig_bundle->replyto();
    }
    bundle->mutable_replyto() = BD_MAKE_EID_NULL();
    bundle->mutable_custodian() = BD_MAKE_EID_NULL();
    
    bundle->set_is_admin(true);

    // use the expiration time from the original bundle
    // XXX/demmer maybe something more clever??
    bundle->set_expiration_secs(orig_bundle->expiration_secs());

    // store the original bundle's source eid
    SPtr_EID sptr_orig_source = orig_bundle->source();

    int sdnv_encoding_len = 0;  // use an int to handle -1 return values
    int report_length = 0;
    
    // we generally don't expect the Status Peport length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;

    //
    // Structure of bundle status reports:
    //
    // 1 byte Admin Payload type and flags
    // 1 byte Status Flags
    // 1 byte Reason Code
    // SDNV   [Fragment Offset (if present)]
    // SDNV   [Fragment Length (if present)]
    // SDNVx2 Time of {receipt/forwarding/delivery/deletion/custody/app-ack}
    //        of bundle X
    // SDNVx2 Copy of bundle X's Creation Timestamp
    // SDNV   Length of X's source endpoint ID
    // vari   Source endpoint ID of bundle X

    // note that the spec allows for all 6 of the "Time of..." fields
    // to be present in a single Status Report, but for this
    // implementation we will always have one and only one of the 6
    // timestamp fields
    // XXX/matt we may want to change to allow multiple-timestamps per SR

    // the non-optional, fixed-length fields above:
    report_length = 1 + 1 + 1;

    // the 2 SDNV fragment fields:
    if (orig_bundle->is_fragment()) {
        report_length += SDNV::encoding_len(orig_bundle->frag_offset());
        report_length += SDNV::encoding_len(orig_bundle->frag_length());
    }

    // Time field, set to the current time (with no sub-second
    // accuracy defined at all)
    BundleTimestamp now;
    now.secs_or_millisecs_ = BundleTimestamp::get_current_time_secs();
    now.seqno_   = 0;

    // dz - multiple timestamps
    for (uint32_t ix=(uint32_t)STATUS_RECEIVED; ix<=STATUS_DELETED; ix*=2) {
        if (status_flags & ix) {
            report_length += BundleProtocolVersion6::ts_encoding_len(now);
        }
    }

    // The bundle's creation timestamp:
    report_length += BundleProtocolVersion6::ts_encoding_len(orig_bundle->creation_ts());

    // the Source Endpoint ID:
    report_length += SDNV::encoding_len(sptr_orig_source->length()) +
                     sptr_orig_source->length();

    //
    // Done calculating length, now create the report payload
    //
    u_char* bp = scratch.buf(report_length);
    int len = report_length;
    
    // Admin Payload Type and flags
    *bp = BundleProtocolVersion6::ADMIN_STATUS_REPORT << 4;
    if (orig_bundle->is_fragment()) {
        *bp |= BundleProtocolVersion6::ADMIN_IS_FRAGMENT;
    }
    bp++;
    len--;
    
    // Status Flags
    *bp++ = status_flags;
    len--;

    // Reason Code
    *bp++ = reason;
    len--;
    
    // The 2 Fragment Fields
    if (orig_bundle->is_fragment()) {
        sdnv_encoding_len = SDNV::encode(orig_bundle->frag_offset(), bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
        
        sdnv_encoding_len = SDNV::encode(orig_bundle->frag_length(), bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
    }   

    // dz multiple timestamps
    for (uint32_t ix=(uint32_t)STATUS_RECEIVED; ix<=STATUS_DELETED; ix*=2) {
        if (status_flags & ix) {
            sdnv_encoding_len = BundleProtocolVersion6::set_timestamp(bp, len, now);
            ASSERT(sdnv_encoding_len > 0);
            bp  += sdnv_encoding_len;
            len -= sdnv_encoding_len;
        }
    }

    // Copy of bundle X's Creation Timestamp
    sdnv_encoding_len = 
        BundleProtocolVersion6::set_timestamp(bp, len, orig_bundle->creation_ts());
    ASSERT(sdnv_encoding_len > 0);
    bp  += sdnv_encoding_len;
    len -= sdnv_encoding_len;
    
    // The 2 Endpoint ID fields:
    sdnv_encoding_len = SDNV::encode(sptr_orig_source->length(), bp, len);
    ASSERT(sdnv_encoding_len > 0);
    len -= sdnv_encoding_len;
    bp  += sdnv_encoding_len;
    
    ASSERT((u_int)len == sptr_orig_source->length());
    memcpy(bp, sptr_orig_source->c_str(), sptr_orig_source->length());
    
    // 
    // Finished generating the payload
    //
    bundle->mutable_payload()->set_data(scratch.buf(), report_length);
}

//----------------------------------------------------------------------
bool BP6_BundleStatusReport::parse_status_report(data_t* data,
                                             const u_char* bp, u_int len)
{
    // 1 byte Admin Payload Type + Flags:
    if (len < 1) { return false; }
    data->admin_type_  = (*bp >> 4);
    data->admin_flags_ = *bp & 0xf;
    bp++;
    len--;

    // validate the admin type
    if (data->admin_type_ != BundleProtocolVersion6::ADMIN_STATUS_REPORT) {
        return false;
    }

    // 1 byte Status Flags:
    if (len < 1) { return false; }
    data->status_flags_ = *bp++;
    len--;
    
    // 1 byte Reason Code:
    if (len < 1) { return false; }
    data->reason_code_ = *bp++;
    len--;
    
    // Fragment SDNV Fields (offset & length), if present:
    if (data->admin_flags_ & BundleProtocolVersion6::ADMIN_IS_FRAGMENT) {
        int sdnv_bytes = SDNV::decode(bp, len, &data->orig_frag_offset_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
        sdnv_bytes = SDNV::decode(bp, len, &data->orig_frag_length_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
    } else {
        data->orig_frag_offset_ = 0;
        data->orig_frag_length_ = 0;
    }

    // The 6 Optional ACK Timestamps:
    
    int ts_len;
    
    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_RECEIVED) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->receipt_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->receipt_tv_.secs_or_millisecs_ = 0;
        data->receipt_tv_.seqno_   = 0;
    }

    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_CUSTODY_ACCEPTED) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->custody_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->custody_tv_.secs_or_millisecs_ = 0;
        data->custody_tv_.seqno_   = 0;
    }

    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_FORWARDED) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->forwarding_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->forwarding_tv_.secs_or_millisecs_ = 0;
        data->forwarding_tv_.seqno_   = 0;
    }

    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_DELIVERED) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->delivery_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->delivery_tv_.secs_or_millisecs_ = 0;
        data->delivery_tv_.seqno_   = 0;
    }

    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_DELETED) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->deletion_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->deletion_tv_.secs_or_millisecs_ = 0;
        data->deletion_tv_.seqno_   = 0;
    }

    if (data->status_flags_ & BP6_BundleStatusReport::STATUS_ACKED_BY_APP) {
        ts_len = BundleProtocolVersion6::get_timestamp(&data->ack_by_app_tv_, bp, len);
        if (ts_len < 0) { return false; }
        bp  += ts_len;
        len -= ts_len;
    } else {
        data->ack_by_app_tv_.secs_or_millisecs_ = 0;
        data->ack_by_app_tv_.seqno_   = 0;
    }
    
    // Bundle Creation Timestamp
    ts_len = BundleProtocolVersion6::get_timestamp(&data->orig_creation_tv_, bp, len);
    if (ts_len < 0) { return false; }
    bp  += ts_len;
    len -= ts_len;
    
    // EID of Bundle
    u_int64_t EID_len;
    int num_bytes = SDNV::decode(bp, len, &EID_len);
    if (num_bytes == -1) { return false; }
    bp  += num_bytes;
    len -= num_bytes;

    if (len != EID_len) { return false; }
    data->sptr_orig_source_eid_ = BD_MAKE_EID(std::string((const char*)bp, len));
    bool ok = data->sptr_orig_source_eid_->valid();
    if (!ok) {
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
BP6_BundleStatusReport::parse_status_report(data_t* data,
                                        const Bundle* bundle)
{
    BundleProtocolVersion6::admin_record_type_t admin_type;
    if (! BundleProtocolVersion6::get_admin_type(bundle, &admin_type)) {
        return false;
    }

    if (admin_type != BundleProtocolVersion6::ADMIN_STATUS_REPORT) {
        return false;
    }

    size_t payload_len = bundle->payload().length();
    if (payload_len > 16384) {
        log_err_p("/dtn/bundle/protocol",
                  "status report length %zu too big to be parsed!!",
                  payload_len);
        return false;
    }

    oasys::ScratchBuffer<u_char*, 256> buf;
    buf.reserve(payload_len);
    const u_char* bp = bundle->payload().read_data(0, payload_len, buf.buf());
    return parse_status_report(data, bp, payload_len);
}

//----------------------------------------------------------------------
const char*
BP6_BundleStatusReport::reason_to_str(u_int8_t reason)
{
    // covers both BPv6 and BPv7

    switch (reason) {
    case BundleProtocol::REASON_NO_ADDTL_INFO:
        return "no additional information";

    case BundleProtocol::REASON_LIFETIME_EXPIRED:
        return "lifetime expired";

    case BundleProtocol::REASON_FORWARDED_UNIDIR_LINK:
        return "forwarded over unidirectional link";

    case BundleProtocol::REASON_TRANSMISSION_CANCELLED:
        return "transmission cancelled";

    case BundleProtocol::REASON_DEPLETED_STORAGE:
        return "depleted storage";

    case BundleProtocol::REASON_ENDPOINT_ID_UNINTELLIGIBLE:
        return "endpoint id unintelligible";

    case BundleProtocol::REASON_NO_ROUTE_TO_DEST:
        return "no known route to destination";

    case BundleProtocol::REASON_NO_TIMELY_CONTACT:
        return "no timely contact";

    case BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE:
        return "block unintelligible";

    case BundleProtocol::REASON_HOP_LIMIT_EXCEEDED:
        return "hop limit exceeded";

    case BundleProtocol::REASON_TRAFFIC_PARED:
        return "traffic pared";

    default:
        return "(unknown reason)";
    }
}

} // namespace dtn
