
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <cbor.h>

#include <third_party/oasys/debug/Log.h>

#include "CborUtilBP7.h"
#include "naming/DTNScheme.h"
#include "naming/IPNScheme.h"



namespace dtn {

//----------------------------------------------------------------------
CborUtilBP7::CborUtilBP7(const char* usage)
{
    set_cborutil_logpath(usage);
    fld_name_= "";
}

//----------------------------------------------------------------------
CborUtilBP7::~CborUtilBP7()
{
}

//----------------------------------------------------------------------
int
CborUtilBP7::encode_bundle_timestamp(CborEncoder& blockArrayEncoder, const BundleTimestamp& bts)
{
    CborError err;
    CborEncoder timeArrayEncoder;

    // creation timestamp - an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &timeArrayEncoder, 2);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // dtn time
    err = cbor_encode_uint(&timeArrayEncoder, bts.secs_or_millisecs_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // sequence ID
    err = cbor_encode_uint(&timeArrayEncoder, bts.seqno_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &timeArrayEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
CborUtilBP7::encode_eid_dtn_none(CborEncoder& blockArrayEncoder)
{
    CborError err;
    CborEncoder eidArrayEncoder;

    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    uint64_t scheme_code = 1;  // 1 = dtn:   2 = ipn:
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // special for dtn:none - 2nd element is the unsigned integer zero
    err = cbor_encode_uint(&eidArrayEncoder, 0);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
CborUtilBP7::encode_eid_dtn(CborEncoder& blockArrayEncoder, const EndpointID& eidref)
{
    CborError err;
    CborEncoder eidArrayEncoder;

    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    uint64_t scheme_code = 1;  // 1 = dtn:   2 = ipn:
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // text string
    err = cbor_encode_text_string(&eidArrayEncoder, eidref.ssp().c_str(), strlen(eidref.ssp().c_str()));
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
CborUtilBP7::encode_eid_ipn(CborEncoder& blockArrayEncoder, const EndpointID& eidref)
{
    CborError err;
    CborEncoder eidArrayEncoder, sspArrayEncoder;

    uint64_t node;
    uint64_t service;

    if (!IPNScheme::parse(eidref, &node, &service))
    {
        log_crit_p(cborutil_logpath(), "Error parsing IPN scheme to node and service components: %s", eidref.c_str());
        return CBORUTIL_FAIL;
    }


    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // scheme code is the first element
    uint64_t scheme_code = 2;  // 1 = dtn:   2 = ipn:
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // for IPN scheme, second element is also an array with 2 elements
    err = cbor_encoder_create_array(&eidArrayEncoder, &sspArrayEncoder, 2);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    //     node number is first element     
    err = cbor_encode_uint(&sspArrayEncoder, node);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    //     service number is second element     
    err = cbor_encode_uint(&sspArrayEncoder, service);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&eidArrayEncoder, &sspArrayEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the EID array
    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
CborUtilBP7::encode_eid(CborEncoder& blockArrayEncoder, const EndpointID& eidref)
{
    if (eidref == EndpointID::NULL_EID())
    {
        return encode_eid_dtn_none(blockArrayEncoder);
    }
    else if (eidref.scheme() == IPNScheme::instance())
    {
        return encode_eid_ipn(blockArrayEncoder, eidref);
    }
    else if (eidref.scheme() == DTNScheme::instance())
    {
        return encode_eid_dtn(blockArrayEncoder, eidref);
    }
    else
    {
        log_crit_p(cborutil_logpath(), "Unrecognized EndpointID Scheme: %s", eidref.c_str());
        return CBORUTIL_FAIL;
    }
}



//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtilBP7::decode_eid(CborValue& cvElement, EndpointID& eid)
{
    // default to dtn:none (NULL_EID)
    eid.assign(EndpointID::NULL_EID());

    uint64_t num_elements;
    int status = validate_cbor_fixed_array_length(cvElement, 2, 2, num_elements);
    CHECK_CBORUTIL_STATUS_RETURN


    CborValue cvEidElements;
    CborError err = cbor_value_enter_container(&cvElement, &cvEidElements);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    uint64_t schema_type = 0;
    uint64_t node_id = 0;
    uint64_t service_num = 0;

    // first element is the schema type
    status = decode_uint(cvEidElements, schema_type);
    CHECK_CBORUTIL_STATUS_RETURN

    if (schema_type == 1)
    {
        if (!data_available_to_parse(cvEidElements, 1))
        {
            return CBORUTIL_UNEXPECTED_EOF;
        }

        // dtn schema
        //
        // check for special dtn:none encoding
        if (cbor_value_is_unsigned_integer(&cvEidElements))
        {
            status = decode_uint(cvEidElements, node_id);
            CHECK_CBORUTIL_STATUS_RETURN

            if (0 != node_id)
            {
                log_err_p(cborutil_logpath(), "CBOR decode error: %s - EID with DTN schema and CBOR unsigned value must be zero for dtn:none", 
                          fld_name());
                return CBORUTIL_FAIL;
            }
            else
            {
                //eid is specifically dtn:none
            }
        }
        else if (cbor_value_is_text_string(&cvEidElements))
        {
            std::string ssp;
            status = decode_text_string(cvEidElements, ssp);
            CHECK_CBORUTIL_STATUS_RETURN
  
            eid.assign("dtn:", ssp);
        }
    }
    else if (2 == schema_type)
    {
        // ipn schema is a two element array (unsigned integers)
        status = validate_cbor_fixed_array_length(cvEidElements, 2, 2, num_elements);
        CHECK_CBORUTIL_STATUS_RETURN

        CborValue cvSspElements;
        err = cbor_value_enter_container(&cvEidElements, &cvSspElements);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN
  
        status = decode_uint(cvSspElements, node_id);
        CHECK_CBORUTIL_STATUS_RETURN

        status = decode_uint(cvSspElements, service_num);
        CHECK_CBORUTIL_STATUS_RETURN
  
        err = cbor_value_leave_container(&cvEidElements, &cvSspElements);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        IPNScheme::format(&eid, node_id, service_num);
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - unable to decode EID with unknown scheme type", 
                  fld_name());
        return CBORUTIL_FAIL;
    }

    err = cbor_value_leave_container(&cvElement, &cvEidElements);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtilBP7::decode_bundle_timestamp(CborValue& cvElement, BundleTimestamp& timestamp)
{
    // default to dtn:none (NULL_EID)
    timestamp.secs_or_millisecs_ = 0;
    timestamp.seqno_ = 0;
    

    uint64_t num_elements;
    int status = validate_cbor_fixed_array_length(cvElement, 2, 2, num_elements);
    CHECK_CBORUTIL_STATUS_RETURN


    CborValue cvTsElements;
    CborError err = cbor_value_enter_container(&cvElement, &cvTsElements);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    // first element of the array is the seconds
    status = decode_uint(cvTsElements, timestamp.secs_or_millisecs_);
    CHECK_CBORUTIL_STATUS_RETURN

    // sedond element of the array is the sequence number
    status = decode_uint(cvTsElements, timestamp.seqno_);
    CHECK_CBORUTIL_STATUS_RETURN

    err = cbor_value_leave_container(&cvElement, &cvTsElements);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
} // namespace dtn
