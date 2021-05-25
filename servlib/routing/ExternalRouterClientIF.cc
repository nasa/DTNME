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

#include "ExternalRouterClientIF.h"

namespace dtn {

//----------------------------------------------------------------------
ExternalRouterClientIF::ExternalRouterClientIF(const char* usage)
    : ExternalRouterIF(),
      CborUtil(usage)
{
}

//----------------------------------------------------------------------
ExternalRouterClientIF::~ExternalRouterClientIF()
{
}

//----------------------------------------------------------------------
void
ExternalRouterClientIF::serialize_send_msg(uint8_t* buf, int64_t buflen)
{
    oasys::ScopeLock l(&lock_, __func__);

    send_msg(new std::string((const char*)buf, buflen));
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_server_msg_header(CborValue& cvElement, uint64_t& msg_type, uint64_t& msg_version, 
                                                 std::string& server_eid)
{
    // msg type
    int status = decode_uint(cvElement, msg_type);
    CHECK_CBORUTIL_STATUS_RETURN

    // msg version
    status = decode_uint(cvElement, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // server_eid
    status = decode_text_string(cvElement, server_eid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_hello_msg_v0(CborValue& cvElement, 
                                            uint64_t& bundles_received, uint64_t& bundles_pending)
{
    // msg header already decoded
    //
    // hello msg data fields must be in this order:
    // 
    //     bundles received
    int status = decode_uint(cvElement, bundles_received);
    CHECK_CBORUTIL_STATUS_RETURN

    //     bundles pending
    status = decode_uint(cvElement, bundles_pending);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_alert_msg_v0(CborValue& cvElement, 
                                            std::string& alert_txt)
{
    // msg header already decoded

    //     alert text
    int status = decode_text_string(cvElement, alert_txt);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_link_report_msg_v0(CborValue& cvElement, 
                                                  extrtr_link_vector_t& link_vec)
{
    int status;

    while (!cbor_value_at_end(&cvElement)) {

        status = decode_link_v0(cvElement, link_vec);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_link_v0(CborValue& rptElement, 
                                       extrtr_link_vector_t& link_vec)
{
    CborValue linkElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &linkElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    extrtr_link_ptr_t linkptr = std::make_shared<extrtr_link_t>();

    // link_id
    status = decode_text_string(linkElement, linkptr->link_id_);
    CHECK_CBORUTIL_STATUS_RETURN

    // remote_eid_
    status = decode_text_string(linkElement, linkptr->remote_eid_);
    CHECK_CBORUTIL_STATUS_RETURN

    // conv_layer
    status = decode_text_string(linkElement, linkptr->conv_layer_);
    CHECK_CBORUTIL_STATUS_RETURN

    // link_state
    status = decode_text_string(linkElement, linkptr->link_state_);
    CHECK_CBORUTIL_STATUS_RETURN

    // next_hop
    status = decode_text_string(linkElement, linkptr->next_hop_);
    CHECK_CBORUTIL_STATUS_RETURN

    // remote_addr
    status = decode_text_string(linkElement, linkptr->remote_addr_);
    CHECK_CBORUTIL_STATUS_RETURN

    // remote port
    status = decode_uint(linkElement, linkptr->remote_port_);
    CHECK_CBORUTIL_STATUS_RETURN

    // rate
    status = decode_uint(linkElement, linkptr->rate_);
    CHECK_CBORUTIL_STATUS_RETURN

    err = cbor_value_leave_container(&rptElement, &linkElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    link_vec.push_back(linkptr);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_link_opened_msg_v0(CborValue& cvElement, 
                                                  extrtr_link_vector_t& link_vec)
{
    int status;

    while (!cbor_value_at_end(&cvElement)) {

        status = decode_link_v0(cvElement, link_vec);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_link_closed_msg_v0(CborValue& cvElement, 
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
ExternalRouterClientIF::decode_link_available_msg_v0(CborValue& cvElement, 
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
ExternalRouterClientIF::decode_link_unavailable_msg_v0(CborValue& cvElement, 
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
ExternalRouterClientIF::decode_bundle_report_msg_v0(CborValue& cvElement, 
                                                    extrtr_bundle_vector_t& bundle_vec)
{
    int status;

    while (!cbor_value_at_end(&cvElement)) {

        status = decode_bundle_v0(cvElement, bundle_vec);
        CHECK_CBORUTIL_STATUS_RETURN

    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_v0(CborValue& rptElement, 
                                       extrtr_bundle_vector_t& bundle_vec)
{
    CborValue bundleElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &bundleElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    extrtr_bundle_ptr_t bundleptr = std::make_shared<extrtr_bundle_t>();


    // bundle id
    status = decode_uint(bundleElement, bundleptr->bundleid_);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle protocol version
    status = decode_uint(bundleElement, bundleptr->bp_version_);
    CHECK_CBORUTIL_STATUS_RETURN

    // custody id
    status = decode_uint(bundleElement, bundleptr->custodyid_);
    CHECK_CBORUTIL_STATUS_RETURN

    // source EID
    status = decode_text_string(bundleElement, bundleptr->source_);
    CHECK_CBORUTIL_STATUS_RETURN

    // destination EID
    status = decode_text_string(bundleElement, bundleptr->dest_);
    CHECK_CBORUTIL_STATUS_RETURN

    // custodian EID
//    status = decode_text_string(bundleElement, bundleptr->custodian_);
//    CHECK_CBORUTIL_STATUS_RETURN

    // replyto EID
//    status = decode_text_string(bundleElement, bundleptr->replyto_);
//    CHECK_CBORUTIL_STATUS_RETURN

    // gbofid_str EID
    status = decode_text_string(bundleElement, bundleptr->gbofid_str_);
    CHECK_CBORUTIL_STATUS_RETURN

    // prev_hop EID
    status = decode_text_string(bundleElement, bundleptr->prev_hop_);
    CHECK_CBORUTIL_STATUS_RETURN

    // length
    status = decode_uint(bundleElement, bundleptr->length_);
    CHECK_CBORUTIL_STATUS_RETURN

    // is_fragment
//    status = decode_boolean(bundleElement, bundleptr->is_fragment_);
//    CHECK_CBORUTIL_STATUS_RETURN

    // is_admin
//    status = decode_boolean(bundleElement, bundleptr->is_admin_);
//    CHECK_CBORUTIL_STATUS_RETURN

    // priority
    status = decode_uint(bundleElement, bundleptr->priority_);
    CHECK_CBORUTIL_STATUS_RETURN

    // ecos_flags
    status = decode_uint(bundleElement, bundleptr->ecos_flags_);
    CHECK_CBORUTIL_STATUS_RETURN

    // ecos_ordinal
    status = decode_uint(bundleElement, bundleptr->ecos_ordinal_);
    CHECK_CBORUTIL_STATUS_RETURN

    // ecos_flowlabel
    status = decode_uint(bundleElement, bundleptr->ecos_flowlabel_);
    CHECK_CBORUTIL_STATUS_RETURN

    // custody_requested
    status = decode_boolean(bundleElement, bundleptr->custody_requested_);
    CHECK_CBORUTIL_STATUS_RETURN

    // local_custody
    status = decode_boolean(bundleElement, bundleptr->local_custody_);
    CHECK_CBORUTIL_STATUS_RETURN

    // singleton_dest
    status = decode_boolean(bundleElement, bundleptr->singleton_dest_);
    CHECK_CBORUTIL_STATUS_RETURN

    // expired_in_transit
    status = decode_boolean(bundleElement, bundleptr->expired_in_transit_);
    CHECK_CBORUTIL_STATUS_RETURN


    err = cbor_value_leave_container(&rptElement, &bundleElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    bundle_vec.push_back(bundleptr);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_received_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, extrtr_bundle_vector_t& bundle_vec)
{
    int status;


    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN


    while (!cbor_value_at_end(&cvElement)) {

        status = decode_bundle_v0(cvElement, bundle_vec);
        CHECK_CBORUTIL_STATUS_RETURN

    }

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_transmitted_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, uint64_t& bundleid, uint64_t& bytes_sent)
{
    int status;

    // Link ID
    status = decode_text_string(cvElement, link_id);
    CHECK_CBORUTIL_STATUS_RETURN

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    // bytes sent
    status = decode_uint(cvElement, bytes_sent);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_delivered_msg_v0(CborValue& cvElement, uint64_t& bundleid)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_expired_msg_v0(CborValue& cvElement, uint64_t& bundleid)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bundle_cancelled_msg_v0(CborValue& cvElement, uint64_t& bundleid)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_custody_timeout_msg_v0(CborValue& cvElement, uint64_t& bundleid)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_custody_accepted_msg_v0(CborValue& cvElement, uint64_t& bundleid, uint64_t& custody_id)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    // Custody ID
    status = decode_uint(cvElement, custody_id);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_custody_signal_msg_v0(CborValue& cvElement, uint64_t& bundleid, 
                                                     bool& success, uint64_t& reason)
{
    int status;

    // Bundle ID
    status = decode_uint(cvElement, bundleid);
    CHECK_CBORUTIL_STATUS_RETURN

    // success
    status = decode_boolean(cvElement, success);
    CHECK_CBORUTIL_STATUS_RETURN

    // reason
    status = decode_uint(cvElement, reason);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_agg_custody_signal_msg_v0(CborValue& cvElement, std::string& acs_data)
{
    int status;

    // ACS data
    status = decode_text_string(cvElement, acs_data);
    CHECK_CBORUTIL_STATUS_RETURN

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::encode_client_msg_header(CborEncoder& msgEncoder, uint64_t msg_type, uint64_t msg_version)
{
    // msg type
    CborError err = cbor_encode_uint(&msgEncoder, msg_type);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // msg version
    err = cbor_encode_uint(&msgEncoder, msg_version);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_link_query_msg()
{
    uint64_t msg_type = EXTRTR_MSG_LINK_QUERY;
    uint64_t msg_version = 0;

    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_client_msg_with_no_data(nullptr, 0, encoded_len, msg_type, msg_version);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Query msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_client_msg_with_no_data(buf, msg_len, encoded_len, msg_type, msg_version);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Query msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_client_msg_with_no_data(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                       uint64_t msg_type, uint64_t msg_version)
{

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

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
int
ExternalRouterClientIF::client_send_bundle_query_msg()
{
    uint64_t msg_type = EXTRTR_MSG_BUNDLE_QUERY;
    uint64_t msg_version = 0;

    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_client_msg_with_no_data(nullptr, 0, encoded_len, msg_type, msg_version);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Query msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_client_msg_with_no_data(buf, msg_len, encoded_len, msg_type, msg_version);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Bundle Query msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_transmit_bundle_req_msg(uint64_t bundleid, std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_transmit_bundle_req_msg_v0(nullptr, 0, encoded_len, bundleid, link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Transmit Bundle Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_transmit_bundle_req_msg_v0(buf, msg_len, encoded_len, bundleid, link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Transmit Bundle Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_transmit_bundle_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  uint64_t bundleid, std::string& link_id)
{
    uint64_t msg_type = EXTRTR_MSG_TRANSMIT_BUNDLE_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // bundle ID
    err = cbor_encode_uint(&msgEncoder, bundleid);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

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
int
ExternalRouterClientIF::client_send_link_reconfigure_req_msg(std::string& link_id, extrtr_key_value_vector_t& kv_vec)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_reconfigure_req_msg_v0(nullptr, 0, encoded_len, 
                                                 link_id, kv_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Reconfigure Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_reconfigure_req_msg_v0(buf, msg_len, encoded_len, 
                                                        link_id, kv_vec);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Reconfigure Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_link_reconfigure_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                        std::string& link_id, extrtr_key_value_vector_t& kv_vec)

{
    uint64_t msg_type = EXTRTR_MSG_LINK_RECONFIGURE_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder kvEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // add a CBOR array for each key_value in the list    
    extrtr_key_value_vector_iter_t iter = kv_vec.begin();
    while (iter != kv_vec.end()) {
        extrtr_key_value_ptr_t kvptr = *iter;

        // open an array for this key_value
        err = cbor_encoder_create_array(&msgEncoder, &kvEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_key_value_v0(kvEncoder, kvptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the key_value array
        err = cbor_encoder_close_container(&msgEncoder, &kvEncoder);
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
ExternalRouterClientIF::client_send_link_add_req_msg(std::string& link_id, 
                                                     std::string& next_hop,
                                                     std::string& link_mode,
                                                     std::string& cl_name,
                                                     LinkParametersVector& params)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_add_req_msg_v0(nullptr, 0, encoded_len, 
                                         link_id, next_hop, link_mode, cl_name, params);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Add Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_add_req_msg_v0(buf, msg_len, encoded_len, 
                                                link_id, next_hop, link_mode, cl_name, params);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Add Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_link_add_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                   std::string& link_id, 
                                                   std::string& next_hop,
                                                   std::string& link_mode,
                                                   std::string& cl_name,
                                                   LinkParametersVector& params)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_ADD_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder paramsEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Link ID
    err = cbor_encode_text_string(&msgEncoder, link_id.c_str(), link_id.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // Next Hop
    err = cbor_encode_text_string(&msgEncoder, next_hop.c_str(), next_hop.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // Link Mode (ALWAYSON, OPPORTUNISTIC. etc)
    err = cbor_encode_text_string(&msgEncoder, link_mode.c_str(), link_mode.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // Convergence Layer Name
    err = cbor_encode_text_string(&msgEncoder, cl_name.c_str(), cl_name.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN



    // add a CBOR array for the list of parameters
    err = cbor_encoder_create_array(&msgEncoder, &paramsEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    LinkParametersVector::iterator piter = params.begin();
    while (piter != params.end()) {
        err = cbor_encode_text_string(&paramsEncoder, (*piter).c_str(), (*piter).length());
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++piter;
    }
        
    // close the parameters array
    err = cbor_encoder_close_container(&msgEncoder, &paramsEncoder);
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
ExternalRouterClientIF::client_send_link_del_req_msg(std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_del_req_msg_v0(nullptr, 0, encoded_len, 
                                         link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Delete Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_del_req_msg_v0(buf, msg_len, encoded_len, 
                                                link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Delete Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_link_del_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                   std::string& link_id)
{
    uint64_t msg_type = EXTRTR_MSG_LINK_DEL_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
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
int
ExternalRouterClientIF::encode_key_value_v0(CborEncoder& kvEncoder, extrtr_key_value_ptr_t& kvptr)
{
    CborError err;

    // Key
    err = cbor_encode_text_string(&kvEncoder, kvptr->key_.c_str(), kvptr->key_.length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // Value Type
    err = cbor_encode_uint(&kvEncoder, kvptr->value_type_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    if (kvptr->value_type_ == KEY_VAL_BOOL) {
        err = cbor_encode_boolean(&kvEncoder, kvptr->value_bool_);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_UINT) {
        err = cbor_encode_uint(&kvEncoder, kvptr->value_uint_);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_INT) {
        err = cbor_encode_int(&kvEncoder, kvptr->value_int_);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
    } else if (kvptr->value_type_ == KEY_VAL_STRING) {
        err = cbor_encode_text_string(&kvEncoder, kvptr->value_string_.c_str(), kvptr->value_string_.length());
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
    } else {
        log_crit_p(cborutil_logpath(), "Error encoding the Key-Value pair - unknown value type");
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_link_close_req_msg(std::string& link_id)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_link_close_req_msg_v0(nullptr, 0, encoded_len, 
                                           link_id);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Close Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_link_close_req_msg_v0(buf, msg_len, encoded_len, 
                                                  link_id);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Close Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_link_close_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                        std::string& link_id)

{
    uint64_t msg_type = EXTRTR_MSG_LINK_CLOSE_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
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
int
ExternalRouterClientIF::client_send_shutdown_req_msg()
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_shutdown_req_msg_v0(nullptr, 0, encoded_len);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Shutdown Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_shutdown_req_msg_v0(buf, msg_len, encoded_len);

    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Shutdown Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_shutdown_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len)

{
    uint64_t msg_type = EXTRTR_MSG_SHUTDOWN_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

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
ExternalRouterClientIF::client_send_take_custody_req_msg(uint64_t bundleid)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_take_custody_req_msg_v0(nullptr, 0, encoded_len, 
                                                 bundleid);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Take Custody msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_take_custody_req_msg_v0(buf, msg_len, encoded_len, 
                                                  bundleid);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Take Custody msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_take_custody_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                       uint64_t bundleid)

{
    uint64_t msg_type = EXTRTR_MSG_TAKE_CUSTODY_REQ;
    uint64_t msg_version = 0;

    CborEncoder encoder;
    CborEncoder msgEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN

    // Bundle ID
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
int
ExternalRouterClientIF::client_send_delete_bundle_req_msg(extrtr_bundle_id_vector_t& bid_vec)
{
    uint64_t msg_type = EXTRTR_MSG_DELETE_BUNDLE_REQ;
    uint64_t msg_version = 0;

    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bundle_id_list_msg_v0(nullptr, 0, encoded_len, msg_type, msg_version, bid_vec);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Link Report msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bundle_id_list_msg_v0(buf, msg_len, encoded_len, msg_type, msg_version, bid_vec);
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
ExternalRouterClientIF::client_send_delete_all_bundles_req_msg()
{
    uint64_t msg_type = EXTRTR_MSG_DELETE_ALL_BUNDLES_REQ;
    uint64_t msg_version = 0;

    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_client_msg_with_no_data(nullptr, 0, encoded_len, msg_type, msg_version);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Delete All Bundles Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_client_msg_with_no_data(buf, msg_len, encoded_len, msg_type, msg_version);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the Delete All Bundles Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_delete_bundle_req_msg(uint64_t bundleid)
{
    extrtr_bundle_id_ptr_t bidptr = std::make_shared<extrtr_bundle_id_t>();
    extrtr_bundle_id_vector_t bid_vec;

    bidptr->bundleid_ = bundleid;
    bid_vec.push_back(bidptr);

    return client_send_delete_bundle_req_msg(bid_vec);
}

//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_bundle_id_list_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     uint64_t msg_type, uint64_t msg_version, 
                                                     extrtr_bundle_id_vector_t& bid_vec)
{
    CborEncoder encoder;
    CborEncoder msgEncoder;
    CborEncoder bidEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    err = cbor_encoder_create_array(&encoder, &msgEncoder, CborIndefiniteLength);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    int status = encode_client_msg_header(msgEncoder, msg_type, msg_version);
    CHECK_CBORUTIL_STATUS_RETURN


    // add a CBOR array for each bundle ID in the list    
    extrtr_bundle_id_vector_iter_t iter = bid_vec.begin();
    while (iter != bid_vec.end()) {
        extrtr_bundle_id_ptr_t bidptr = *iter;

        // open an array for this Bundle ID
        err = cbor_encoder_create_array(&msgEncoder, &bidEncoder, CborIndefiniteLength);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        status = encode_bundle_id_v0(bidEncoder, bidptr);
        CHECK_CBORUTIL_STATUS_RETURN

        // close the Bundle ID array
        err = cbor_encoder_close_container(&msgEncoder, &bidEncoder);
        CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

        ++iter;
    }

    // close the message array
    err = cbor_encoder_close_container(&encoder, &msgEncoder);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

        //dzdebug
        //log_always_p(cborutil_logpath(), "bundle_id_list msg (%s)  need_bytes = %" PRIu64, 
        //                                 msg_type_to_str(msg_type), need_bytes);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::encode_bundle_id_v0(CborEncoder& bidEncoder, extrtr_bundle_id_ptr_t& bidptr)
{
    CborError err;


    // Bunbdle ID
    err = cbor_encode_uint(&bidEncoder, bidptr->bundleid_);
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    return CBORUTIL_SUCCESS;
}


} // namespace dtn
