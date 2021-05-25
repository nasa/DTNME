
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleProtocolVersion7.h"
#include "BundleStatusReport.h"
#include "BP6_BundleStatusReport.h"
#include "SDNV.h"
#include <netinet/in.h>
#include <third_party/oasys/util/ScratchBuffer.h>

#define SET_FLDNAMES(fld) \
    cborutil.set_fld_name(fld);

#define CHECK_CBOR_ENCODE_ERR_RETURN \
    if (err && (err != CborErrorOutOfMemory)) \
    { \
      return CBORUTIL_FAIL; \
    }

#define CHECK_ENCODE_STATUS \
    if (status != CBORUTIL_SUCCESS) \
    { \
      return CBORUTIL_FAIL; \
    }


#define CHECK_CBOR_DECODE_ERR \
    if (err != CborNoError) \
    { \
      return false; \
    }

#define CHECK_CBOR_STATUS \
    if (status != CBORUTIL_SUCCESS) \
    { \
      return false; \
    }

namespace dtn {

//----------------------------------------------------------------------
void
BundleStatusReport::create_status_report(Bundle*           bundle,
                                         const Bundle*     orig_bundle,
                                         const EndpointID& source,
                                         flag_t            status_flags,
                                         reason_t          reason)
{
    (void) bundle;
    (void) orig_bundle;
    (void) source;
    (void) status_flags;
    (void) reason;


    if (bundle->is_bpv6()) {
        BP6_BundleStatusReport::create_status_report(bundle, orig_bundle, source, status_flags, reason);
        return;
    }


    bundle->set_source(source);
    if (orig_bundle->replyto().equals(EndpointID::NULL_EID())){
        bundle->mutable_dest()->assign(orig_bundle->source());
    } else {
        bundle->mutable_dest()->assign(orig_bundle->replyto());
    }
    bundle->mutable_replyto()->assign(EndpointID::NULL_EID());
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    
    bundle->set_is_admin(true);

    // use the expiration time from the original bundle
    bundle->set_expiration_millis(orig_bundle->expiration_millis());


    CborUtilBP7 cborutil("statusrpt");
    int64_t rpt_len;

    cborutil.set_bundle_id(orig_bundle->bundleid());

    rpt_len = encode_report(nullptr, 0, cborutil, orig_bundle, status_flags, reason);

    if (BP_FAIL == rpt_len) {
        log_err_p("/dtn/bundlestatusrpt", "Error encoding Bundle Status Report *%p", orig_bundle);
        return;
    }

    // we generally don't expect the Status Peport length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;
    u_char* bp = scratch.buf(rpt_len);

    int64_t status = encode_report(bp, rpt_len, cborutil, orig_bundle, status_flags, reason);

    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/dtn/bundlestatusrpt", "Error encoding Bundle Status Report *%p", orig_bundle);
        return;
    }

    // 
    // Finished generating the payload
    //
    bundle->mutable_payload()->set_data(scratch.buf(), rpt_len);
}

//----------------------------------------------------------------------
int64_t
BundleStatusReport::encode_report(uint8_t* buf, int64_t buflen, 
                                  CborUtilBP7& cborutil,
                                  const Bundle*     orig_bundle,
                                  flag_t            status_flags,
                                  reason_t          reason)
{
    CborEncoder encoder;
    CborEncoder adminRecArrayEncoder;
    CborEncoder rptArrayEncoder;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);


    err = cbor_encoder_create_array(&encoder, &adminRecArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // admin rec type
    err = cbor_encode_uint(&adminRecArrayEncoder, BundleProtocolVersion7::ADMIN_STATUS_REPORT);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // Array containing Biundle Status Report details
    uint64_t array_size = 4;
    if (orig_bundle->is_fragment()) {
        array_size += 2;
    }

    err = cbor_encoder_create_array(&adminRecArrayEncoder, &rptArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // first up is an array of status indicators
    int status = encode_status_indicators(rptArrayEncoder, status_flags, orig_bundle->req_time_in_status_rpt());
    CHECK_ENCODE_STATUS
    
    // second is the reason code
    err = cbor_encode_uint(&rptArrayEncoder, static_cast<uint64_t>(reason));
    CHECK_CBOR_ENCODE_ERR_RETURN

    // third is the bundle's souuce EID 
    status = cborutil.encode_eid(rptArrayEncoder, orig_bundle->source());
    CHECK_ENCODE_STATUS

    // fourth is the bundle's creation timestamp
    status = cborutil.encode_bundle_timestamp(rptArrayEncoder, orig_bundle->creation_ts());
    CHECK_ENCODE_STATUS

    if (orig_bundle->is_fragment()) {
        // fifth is the fragment offset
        err = cbor_encode_uint(&rptArrayEncoder, orig_bundle->frag_offset());
        CHECK_CBOR_ENCODE_ERR_RETURN

        // sixth is the fragment length
        err = cbor_encode_uint(&rptArrayEncoder, orig_bundle->frag_length());
        CHECK_CBOR_ENCODE_ERR_RETURN
    }

    // close the array
    err = cbor_encoder_close_container(&encoder, &rptArrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    //if (0 == need_bytes)
    //{
    //    encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    //}

    return need_bytes;
}


//----------------------------------------------------------------------
int
BundleStatusReport::encode_status_indicators(CborEncoder& rptArrayEncoder, flag_t status_flags, 
                                             const bool include_timestamp)
{
    CborEncoder indicatorArrayEncoder;

    CborError err;

    uint64_t array_size = 4;
    err = cbor_encoder_create_array(&rptArrayEncoder, &indicatorArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // indicators are in specific order
    // 1) bundle received
    uint64_t indicated = (status_flags & STATUS_RECEIVED) ? 1 : 0;
    int status = encode_indicator(indicatorArrayEncoder, indicated, include_timestamp);
    CHECK_ENCODE_STATUS

    // 2) bundle forwarded
    indicated = (status_flags & STATUS_FORWARDED) ? 1 : 0;
    status = encode_indicator(indicatorArrayEncoder, indicated, include_timestamp);
    CHECK_ENCODE_STATUS

    // 3) bundle delivered
    indicated = (status_flags & STATUS_DELIVERED) ? 1 : 0;
    status = encode_indicator(indicatorArrayEncoder, indicated, include_timestamp);
    CHECK_ENCODE_STATUS

    // 4) bundle deleted
    indicated = (status_flags & STATUS_DELETED) ? 1 : 0;
    status = encode_indicator(indicatorArrayEncoder, indicated, include_timestamp);
    CHECK_ENCODE_STATUS


    err = cbor_encoder_close_container(&rptArrayEncoder, &indicatorArrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN
    
    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
BundleStatusReport::encode_indicator(CborEncoder& indicatorArrayEncoder, uint64_t indicated, 
                                             const bool include_timestamp)
{
    CborError err;
    CborEncoder indicatorEncoder;

    uint64_t timestamp = BundleTimestamp::get_current_time();
    timestamp = timestamp * 1000;  // converted to milliseconds since 1/1/2000

    uint64_t array_size = 1;
    if ((indicated == 1) && include_timestamp) {
        array_size += 1;
    }

    err = cbor_encoder_create_array(&indicatorArrayEncoder, &indicatorEncoder, array_size);
    CHECK_CBOR_ENCODE_ERR_RETURN

    err = cbor_encode_uint(&indicatorEncoder, indicated); // 0 = false; 1 = true
    CHECK_CBOR_ENCODE_ERR_RETURN

    if (array_size == 2) {
        err = cbor_encode_uint(&indicatorEncoder, timestamp);
        CHECK_CBOR_ENCODE_ERR_RETURN
    }

    err = cbor_encoder_close_container(&indicatorArrayEncoder, &indicatorEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN
    
    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
bool 
BundleStatusReport::parse_status_indication_array(CborValue& cvStatusElements, CborUtilBP7& cborutil, data_t& sr_data)
{
    CborError err;

    // Minimum of 4 status indicators in this order:
    //     bundle received
    //     bundle forwarded
    //     bundle delivered
    //     bndle deleted
    //
    // Each indicator is an array of 1 or 2 elements


    SET_FLDNAMES("StatusRpt-IndicatorArray");
    uint64_t array_elements;
    int status = cborutil.validate_cbor_fixed_array_length(cvStatusElements, 4, 64, array_elements);
    CHECK_CBOR_STATUS

    CborValue cvIndicatorElements;
    err = cbor_value_enter_container(&cvStatusElements, &cvIndicatorElements);
    CHECK_CBOR_DECODE_ERR

    std::string tfldname("StatusRpt-Indicator-0");
    char* update_char = &tfldname[20];
    int indicator = 1;
    for (uint64_t ix=0; ix<array_elements; ++ix) {
        // update the indicator number in our fld name
        ++(*update_char);
        SET_FLDNAMES(tfldname);
        uint64_t num_elements;
        int status = cborutil.validate_cbor_fixed_array_length(cvIndicatorElements, 1, 2, num_elements);
        CHECK_CBOR_STATUS

        CborValue cvIndicator;
        err = cbor_value_enter_container(&cvIndicatorElements, &cvIndicator);
        CHECK_CBOR_DECODE_ERR

        // first element is whether or not the status is indicated (0 or 1)
        // second element if present is a timestamp
        uint64_t indicated = 0;
        uint64_t timestamp = 0;

        status = cborutil.decode_uint(cvIndicator, indicated);
        CHECK_CBOR_STATUS

        if (num_elements == 2) {
            status = cborutil.decode_uint(cvIndicator, timestamp);
            CHECK_CBOR_STATUS
        }

        err = cbor_value_leave_container(&cvIndicatorElements, &cvIndicator);
        CHECK_CBOR_DECODE_ERR

        switch (indicator) {
            case 1: sr_data.received_ = (indicated == 1);
                    sr_data.received_timestamp_ = timestamp;
                    break;

            case 2: sr_data.forwarded_ = (indicated == 1);
                    sr_data.forwarded_timestamp_ = timestamp;
                    break;

            case 3: sr_data.delivered_ = (indicated == 1);
                    sr_data.delivered_timestamp_ = timestamp;
                    break;

            case 4: sr_data.deleted_ = (indicated == 1);
                    sr_data.deleted_timestamp_ = timestamp;
                    break;

            default: break; // new indications that we don't support so ignored
        }

        ++indicator;
    }

    err = cbor_value_leave_container(&cvStatusElements, &cvIndicatorElements);
    CHECK_CBOR_DECODE_ERR

    return true;
}



//----------------------------------------------------------------------
bool
BundleStatusReport::parse_status_report(CborValue& cvPayloadElements, CborUtilBP7& cborutil, data_t& sr_data)
{
    CborError err;

    SET_FLDNAMES("StatusRpt-CborArray");
    uint64_t array_elements;
    int status = cborutil.validate_cbor_fixed_array_length(cvPayloadElements, 4, 6, array_elements);
    CHECK_CBOR_STATUS

    if (array_elements == 5) {
        return false;
    }

    CborValue cvStatusElements;
    err = cbor_value_enter_container(&cvPayloadElements, &cvStatusElements);
    CHECK_CBOR_DECODE_ERR


    // first element is an array of indicators
    SET_FLDNAMES("StatusRpt-IndicationArray");
    if (!parse_status_indication_array(cvStatusElements, cborutil, sr_data)) {
        return false;
    }

    // second element is the reason code
    SET_FLDNAMES("StatusRpt-ReasonCode");
    status = cborutil.decode_uint(cvStatusElements, sr_data.reason_code_);
    CHECK_CBOR_STATUS

    // third element is the bundle source EID
    SET_FLDNAMES("StatusRpt-SourceEID");
    status = cborutil.decode_eid(cvStatusElements, sr_data.orig_source_eid_);
    CHECK_CBOR_STATUS

    // fourth element is the bundle creation timestamp
    SET_FLDNAMES("StatusRpt-SourceTimestamp");
    status = cborutil.decode_bundle_timestamp(cvStatusElements, sr_data.orig_creation_ts_);
    CHECK_CBOR_STATUS
    
    if (array_elements == 6) {
        // fifth element is the fragment offset
        SET_FLDNAMES("StatusRpt-SourceFragOffset");
        status = cborutil.decode_uint(cvStatusElements, sr_data.orig_frag_offset_);
        CHECK_CBOR_STATUS

        // fifth element is the fragment length
        SET_FLDNAMES("StatusRpt-SourceFragLength");
        status = cborutil.decode_uint(cvStatusElements, sr_data.orig_frag_length_);
        CHECK_CBOR_STATUS
    }
    
    return true;
}

//----------------------------------------------------------------------
const char*
BundleStatusReport::reason_to_str(u_int8_t reason)
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
