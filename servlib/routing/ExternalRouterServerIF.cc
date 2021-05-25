/*
 *    Copyright 2006 Intel Corporation
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

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/util/ScratchBuffer.h>

#include "ExternalRouterServerIF.h"

namespace dtn {

//----------------------------------------------------------------------
ExternalRouterServerIF::ExternalRouterServerIF(const char* usage)
    : ExternalRouterIF(),
      CborUtil(usage)
{
}

//----------------------------------------------------------------------
ExternalRouterServerIF::~ExternalRouterServerIF()
{
}

//----------------------------------------------------------------------
void
ExternalRouterServerIF::serialize_send_msg(uint8_t* buf, int64_t buflen)
{
    oasys::ScopeLock l(&lock_, __func__);

    send_msg(new std::string((const char*)buf, buflen));
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::encode_server_msg_header(CborEncoder& msgEncoder, uint64_t msg_type, uint64_t msg_version)
{
    if (!server_eid_set_) {
        log_crit_p(cborutil_logpath(), "Aborting - server EID not set in ExternalRouterSererIF");
        return CBORUTIL_FAIL;
    }

    // msg type
    CborError err = cbor_encode_uint(&msgEncoder, msg_type);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // msg version
    err = cbor_encode_uint(&msgEncoder, msg_version);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // server_eid
    err = cbor_encode_text_string(&msgEncoder, server_eid_.c_str(), server_eid_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_hello_msg(uint64_t bundles_received, uint64_t bundles_pending)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_hello_msg_v0(nullptr, 0, encoded_len, bundles_received, bundles_pending);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Hello msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_hello_msg_v0(buf, msg_len, encoded_len, bundles_received, bundles_pending);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Hello msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_alert_msg(std::string& alert_txt)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_alert_msg_v0(nullptr, 0, encoded_len, alert_txt);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Alert msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_alert_msg_v0(buf, msg_len, encoded_len, alert_txt);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Alert msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}




//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_link_report_msg(extrtr_link_vector_t& link_vec)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_report_msg_v0(nullptr, 0, encoded_len, link_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Report msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_report_msg_v0(buf, msg_len, encoded_len, link_vec);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Report msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_link_opened_msg(extrtr_link_vector_t& link_vec)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_opened_msg_v0(nullptr, 0, encoded_len, link_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Opened msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_opened_msg_v0(buf, msg_len, encoded_len, link_vec);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Opened msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_link_closed_msg(std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_closed_msg_v0(nullptr, 0, encoded_len, link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Closed msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_closed_msg_v0(buf, msg_len, encoded_len, link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Closed msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_link_available_msg(std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_available_msg_v0(nullptr, 0, encoded_len, link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Available msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_available_msg_v0(buf, msg_len, encoded_len, link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Available msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_link_unavailable_msg(std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_unavailable_msg_v0(nullptr, 0, encoded_len, link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Unavailable msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_unavailable_msg_v0(buf, msg_len, encoded_len, link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Unavailable msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}









//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_report_msg(extrtr_bundle_vector_t& bundle_vec)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_report_msg_v0(nullptr, 0, encoded_len, bundle_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Report msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_report_msg_v0(buf, msg_len, encoded_len, bundle_vec);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Report msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_received_msg(std::string& link_id, extrtr_bundle_vector_t& bundle_vec)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_received_msg_v0(nullptr, 0, encoded_len, link_id, bundle_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Received msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_received_msg_v0(buf, msg_len, encoded_len, link_id, bundle_vec);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Report msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_transmitted_msg(std::string& link_id, uint64_t bundleid, uint64_t bytes_sent)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_transmitted_msg_v0(nullptr, 0, encoded_len, link_id, bundleid, bytes_sent);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Transmitted msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_transmitted_msg_v0(buf, msg_len, encoded_len, link_id, bundleid, bytes_sent);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Transmitted msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_delivered_msg(uint64_t bundleid)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_delivered_msg_v0(nullptr, 0, encoded_len, bundleid);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Delivered msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_delivered_msg_v0(buf, msg_len, encoded_len, bundleid);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Delivered msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_expired_msg(uint64_t bundleid)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_expired_msg_v0(nullptr, 0, encoded_len, bundleid);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Expired msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_expired_msg_v0(buf, msg_len, encoded_len, bundleid);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Expired msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_bundle_cancelled_msg(uint64_t bundleid)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_cancelled_msg_v0(nullptr, 0, encoded_len, bundleid);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Cancelled msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_cancelled_msg_v0(buf, msg_len, encoded_len, bundleid);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Cancelled msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_custody_timeout_msg(uint64_t bundleid)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_custody_timeout_msg_v0(nullptr, 0, encoded_len, bundleid);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Timeout msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_custody_timeout_msg_v0(buf, msg_len, encoded_len, bundleid);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Timeout msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_custody_accepted_msg(uint64_t bundleid, uint64_t custody_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_custody_accepted_msg_v0(nullptr, 0, encoded_len, bundleid, custody_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Accepted msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_custody_accepted_msg_v0(buf, msg_len, encoded_len, bundleid, custody_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Accepted msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_custody_signal_msg(uint64_t bundleid, bool success, uint64_t reason)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_custody_signal_msg_v0(nullptr, 0, encoded_len, 
                                           bundleid, success, reason);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Signal msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_custody_signal_msg_v0(buf, msg_len, encoded_len, 
                                           bundleid, success, reason);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Custody Signal msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::server_send_agg_custody_signal_msg(std::string& acs_data)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_agg_custody_signal_msg_v0(nullptr, 0, encoded_len,
                                           acs_data);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Aggregate Aggregate Custody Signal msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_agg_custody_signal_msg_v0(buf, msg_len, encoded_len, 
                                           acs_data);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Aggregate Custody Signal msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}






//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_hello_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                            uint64_t bundles_received, uint64_t bundles_pending)
{
    uint64_t msg_type = EXTRTR_MSG_HELLO;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // hello msg data fields must be in this order:

    //     bundles received
    err = cbor_encode_uint(&msgEncoder, bundles_received);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    //     bundles pending
    err = cbor_encode_uint(&msgEncoder, bundles_pending);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    
    // close the array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_alert_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                            std::string& alert_txt)
{
    uint64_t msg_type = EXTRTR_MSG_ALERT;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    err = cbor_encode_text_string(&msgEncoder, alert_txt.c_str(), alert_txt.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    
    // close the array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_link_report_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  extrtr_link_vector_t& link_vec)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_REPORT;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder linkEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN


    // add a CBOR array for each link in the list    
    extrtr_link_vector_iter_t iter = link_vec.begin();
    while (iter != link_vec.end()) {
        extrtr_link_ptr_t linkptr = *iter;

        // open an array for this link
        err = cbor_encoder_create_array(&msgEncoder, &linkEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_link_v0(linkEncoder, linkptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the link array
        err = cbor_encoder_close_container(&msgEncoder, &linkEncoder);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++iter;
    }

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

        //dzdebug
        //log_always_p(cborutil_logpath(), "link_report need_bytes = %" PRIu64, need_bytes);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_link_opened_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  extrtr_link_vector_t& link_vec)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_OPENED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder linkEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN


    // add a CBOR array for each link in the list    
    extrtr_link_vector_iter_t iter = link_vec.begin();
    while (iter != link_vec.end()) {
        extrtr_link_ptr_t linkptr = *iter;

        // open an array for this link
        err = cbor_encoder_create_array(&msgEncoder, &linkEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_link_v0(linkEncoder, linkptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the link array
        err = cbor_encoder_close_container(&msgEncoder, &linkEncoder);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++iter;
    }

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::encode_link_v0(CborEncoder& linkEncoder, extrtr_link_ptr_t& linkptr)
{
    CborError err;

    // link_id
    err = cbor_encode_text_string(&linkEncoder, linkptr->link_id_.c_str(), linkptr->link_id_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // remote_eid_
    err = cbor_encode_text_string(&linkEncoder, linkptr->remote_eid_.c_str(), linkptr->remote_eid_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // conv_layer
    err = cbor_encode_text_string(&linkEncoder, linkptr->conv_layer_.c_str(), linkptr->conv_layer_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // link_state
    err = cbor_encode_text_string(&linkEncoder, linkptr->link_state_.c_str(), linkptr->link_state_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // next_hop
    err = cbor_encode_text_string(&linkEncoder, linkptr->next_hop_.c_str(), linkptr->next_hop_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // remote_addr
    err = cbor_encode_text_string(&linkEncoder, linkptr->remote_addr_.c_str(), linkptr->remote_addr_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // remote port
    err = cbor_encode_uint(&linkEncoder, linkptr->remote_port_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // rate
    err = cbor_encode_uint(&linkEncoder, linkptr->rate_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_link_closed_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_CLOSED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_link_available_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_AVAILABLE;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_link_unavailable_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_UNAVAILABLE;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}








//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_report_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  extrtr_bundle_vector_t& bundle_vec)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_REPORT;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder bundleEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // add a CBOR array for each bundle in the list    
    extrtr_bundle_vector_iter_t iter = bundle_vec.begin();
    while (iter != bundle_vec.end()) {
        extrtr_bundle_ptr_t bundleptr = *iter;

        // open an array for this bundle
        err = cbor_encoder_create_array(&msgEncoder, &bundleEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_bundle_v0(bundleEncoder, bundleptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the bundle array
        err = cbor_encoder_close_container(&msgEncoder, &bundleEncoder);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++iter;
    }

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

        //dzdebug
        //log_always_p(cborutil_logpath(), "bundle_report need_bytes = %" PRIu64, need_bytes);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::encode_bundle_v0(CborEncoder& bundleEncoder, extrtr_bundle_ptr_t& bundleptr)
{
    CborError err;

    // bundle id
    err = cbor_encode_uint(&bundleEncoder, bundleptr->bundleid_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // bundle protocol version
    err = cbor_encode_uint(&bundleEncoder, bundleptr->bp_version_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // custody id
    err = cbor_encode_uint(&bundleEncoder, bundleptr->custodyid_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // source EID
    err = cbor_encode_text_string(&bundleEncoder, bundleptr->source_.c_str(), bundleptr->source_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // destination EID
    err = cbor_encode_text_string(&bundleEncoder, bundleptr->dest_.c_str(), bundleptr->dest_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // custodian EID
//    err = cbor_encode_text_string(&bundleEncoder, bundleptr->custodian_.c_str(), bundleptr->custodian_.length());
//    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // replyto EID
//    err = cbor_encode_text_string(&bundleEncoder, bundleptr->replyto_.c_str(), bundleptr->replyto_.length());
//    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // gbofid_str EID
    err = cbor_encode_text_string(&bundleEncoder, bundleptr->gbofid_str_.c_str(), bundleptr->gbofid_str_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // prev_hop EID
    err = cbor_encode_text_string(&bundleEncoder, bundleptr->prev_hop_.c_str(), bundleptr->prev_hop_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // length
    err = cbor_encode_uint(&bundleEncoder, bundleptr->length_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // is_fragment
//    err = cbor_encode_boolean(&bundleEncoder, bundleptr->is_fragment_);
//    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // is_admin
//    err = cbor_encode_boolean(&bundleEncoder, bundleptr->is_admin_);
//    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // priority
    err = cbor_encode_uint(&bundleEncoder, bundleptr->priority_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // ecos_flags
    err = cbor_encode_uint(&bundleEncoder, bundleptr->ecos_flags_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // ecos_ordinal
    err = cbor_encode_uint(&bundleEncoder, bundleptr->ecos_ordinal_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // ecos_flowlabel
    err = cbor_encode_uint(&bundleEncoder, bundleptr->ecos_flowlabel_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // custody_requested
    err = cbor_encode_boolean(&bundleEncoder, bundleptr->custody_requested_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // local_custody
    err = cbor_encode_boolean(&bundleEncoder, bundleptr->local_custody_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // singleton_dest
    err = cbor_encode_boolean(&bundleEncoder, bundleptr->singleton_dest_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // expired_in_transit
    err = cbor_encode_boolean(&bundleEncoder, bundleptr->expired_in_transit_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}




//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_received_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, extrtr_bundle_vector_t& bundle_vec)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_RECEIVED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder bundleEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // add a CBOR array for each bundle in the list    
    extrtr_bundle_vector_iter_t iter = bundle_vec.begin();
    while (iter != bundle_vec.end()) {
        extrtr_bundle_ptr_t bundleptr = *iter;

        // open an array for this bundle
        err = cbor_encoder_create_array(&msgEncoder, &bundleEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_bundle_v0(bundleEncoder, bundleptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the bundle array
        err = cbor_encoder_close_container(&msgEncoder, &bundleEncoder);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++iter;
    }

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}



//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_transmitted_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, uint64_t bundleid, uint64_t bytes_sent)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_TRANSMITTED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // bytes sent
    err = cbor_encode_uint(&msgEncoder, bytes_sent);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_delivered_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                       uint64_t bundleid)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_DELIVERED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_expired_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t bundleid)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_EXPIRED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_bundle_cancelled_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t bundleid)
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_CANCELLED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_custody_timeout_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t bundleid)
{
    uint64_t msg_type = EXTRTR_MSG_CUSTODY_TIMEOUT;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

 
//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_custody_accepted_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t bundleid, uint64_t custody_id)
{
    uint64_t msg_type = EXTRTR_MSG_CUSTODY_ACCEPTED;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // custody ID
    err = cbor_encode_uint(&msgEncoder, custody_id);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

 
//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_custody_signal_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t bundleid, bool success, uint64_t reason)
{
    uint64_t msg_type = EXTRTR_MSG_CUSTODY_SIGNAL;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // custody ID
    err = cbor_encode_boolean(&msgEncoder, success);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // reason
    err = cbor_encode_uint(&msgEncoder, reason);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterServerIF::encode_agg_custody_signal_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     std::string& acs_data)
{
    uint64_t msg_type = EXTRTR_MSG_AGG_CUSTODY_SIGNAL;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_server_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // ACS data
    err = cbor_encode_text_string(&msgEncoder, acs_data.c_str(), acs_data.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}

 
//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_client_msg_header(CborValue& cvElement, uint64_t& msg_type, uint64_t& msg_version)
{
    // msg type
    int status = decode_uint(cvElement, msg_type);
    CHECK_CBORUTIL_STATUS_RETURN

    // msg version
    status = decode_uint(cvElement, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_transmit_bundle_req_msg_v0(CborValue& cvElement, 
                                          uint64_t& bundleid, std::string& link_id)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_link_reconfigure_req_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, extrtr_key_value_vector_t& kv_vec)
{
    int status;

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    while (!cbor_value_at_end(&cvElement)) {

        status = decode_key_value_v0(cvElement, kv_vec);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_key_value_v0(CborValue& rptElement, 
                                       extrtr_key_value_vector_t& kv_vec)
{
    CborValue kvElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &kvElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    extrtr_key_value_ptr_t kvptr (new extrtr_key_value_t());

    // key
    status = decode_text_string(kvElement, kvptr->key_);
    CHECK_CBORUTIL_STATUS_RETURN

    // value type
    uint64_t val_type = 0;
    status = decode_uint(kvElement, val_type);
    CHECK_CBORUTIL_STATUS_RETURN

    kvptr->value_type_ = (key_value_type_t) val_type;

    // value
    if (kvptr->value_type_ == KEY_VAL_BOOL) {
        status = decode_boolean(kvElement, kvptr->value_bool_);
        CHECK_CBORUTIL_STATUS_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_UINT) {
        status = decode_uint(kvElement, kvptr->value_uint_);
        CHECK_CBORUTIL_STATUS_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_INT) {
        status = decode_int(kvElement, kvptr->value_int_);
        CHECK_CBORUTIL_STATUS_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_STRING) {
        status = decode_text_string(kvElement, kvptr->value_string_);
        CHECK_CBORUTIL_STATUS_RETURN
    } else {
        // unknown value type
        log_err_p(cborutil_logpath(), "Unknown Key Value data type");
        return CBORUTIL_FAIL;
    }
    
    err = cbor_value_leave_container(&rptElement, &kvElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    kv_vec.push_back(kvptr);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_link_close_req_msg_v0(CborValue& cvElement, 
                                          std::string& link_id)
{
    int status;

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_link_add_req_msg_v0(CborValue& cvElement, 
                                                   std::string& link_id, std::string& next_hop, 
                                                   std::string& link_mode, std::string& cl_name, 
                                                   LinkParametersVector& params)
{
    int status;

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    // Next Hop
    status = decode_text_string(cvElement, next_hop);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link Mode
    status = decode_text_string(cvElement, link_mode);
    CHECK_CBORUTIL_STATUS_RETURN

    // Convergence Layer Name
    status = decode_text_string(cvElement, cl_name);
    CHECK_CBORUTIL_STATUS_RETURN

    return decode_link_params_v0(cvElement, params);
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_link_params_v0(CborValue& cvElement, 
                                              LinkParametersVector& params)
{
    CborValue paramsElement;
    CborError err;
    int status;
    std::string param_str;

    err = cbor_value_enter_container(&cvElement, &paramsElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    while (!cbor_value_at_end(&paramsElement)) {

        status = decode_text_string(paramsElement, param_str);
        CHECK_CBORUTIL_STATUS_RETURN

        params.push_back(param_str);
    }

    err = cbor_value_leave_container(&cvElement, &paramsElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    return CBORUTIL_SUCCESS;
}
//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_link_del_req_msg_v0(CborValue& cvElement, 
                                                   std::string& link_id)
{
    int status;

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}



//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_take_custody_req_msg_v0(CborValue& cvElement, 
                                          uint64_t& bundleid)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_delete_bundle_req_msg_v0(CborValue& cvElement, 
                                       extrtr_bundle_id_vector_t& bid_vec)
{
    int status;

    while (!cbor_value_at_end(&cvElement)) {

        status = decode_bundle_id_v0(cvElement, bid_vec);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterServerIF::decode_bundle_id_v0(CborValue& rptElement, 
                                       extrtr_bundle_id_vector_t& bid_vec)
{
    CborValue bidElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &bidElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    extrtr_bundle_id_ptr_t bidptr = std::make_shared<extrtr_bundle_id_t>();

    // bundle id
    status = decode_uint(bidElement, bidptr->bundleid_);
    CHECK_CBORUTIL_STATUS_RETURN

    err = cbor_value_leave_container(&rptElement, &bidElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    bid_vec.push_back(bidptr);

    return CBORUTIL_SUCCESS;
}


} // namespace dtn
