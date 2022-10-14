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
                                                    extrtr_bundle_vector_t& bundle_vec,
                                                    bool& last_msg)
{
    int status;

    // first field is flag indicating last message of the report
    status = decode_boolean(cvElement, last_msg);
    CHECK_CBORUTIL_STATUS_RETURN


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


//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_bard_usage_req_msg()
{
#ifdef BARD_ENABLED
    uint64_t msg_type = EXTRTR_MSG_BARD_USAGE_REQ;
    uint64_t msg_version = 0;

    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_client_msg_with_no_data(nullptr, 0, encoded_len, msg_type, msg_version);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Usage Request msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_client_msg_with_no_data(buf, msg_len, encoded_len, msg_type, msg_version);
    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Usage Request msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;

#else
    log_warn_p(cborutil_logpath(), "BARD Usage req message not generated because BARD is not enabled");
    return CBORUTIL_FAIL;
#endif // BARD_ENABLED

}

#ifdef BARD_ENABLED
//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_bard_add_quota_req_msg(BARDNodeStorageUsage& quota)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bard_add_quota_req_msg_v0(nullptr, 0, encoded_len, quota);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Add Quota msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bard_add_quota_req_msg_v0(buf, msg_len, encoded_len, quota);

    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Add Quota msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;

}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::client_send_bard_del_quota_req_msg(BARDNodeStorageUsage& quota)
{
    int64_t msg_len;
    int64_t encoded_len = 0;
    
    // first run through the encode process with a null buffer to determine the needed size of buffer
    msg_len = encode_bard_del_quota_req_msg_v0(nullptr, 0, encoded_len, quota);

    if (CBORUTIL_FAIL == msg_len)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Delete Quota msg in first pass");
        return CBORUTIL_FAIL;
    }

    oasys::ScratchBuffer<int8_t*,0> scratch;
    uint8_t* buf = (uint8_t*) scratch.buf(msg_len);

    int64_t status = encode_bard_del_quota_req_msg_v0(buf, msg_len, encoded_len, quota);

    if (CBORUTIL_SUCCESS != status)
    {
        log_crit_p(cborutil_logpath(), "Error encoding the BARD Delete Quota msg in 2nd pass");
        return CBORUTIL_FAIL;
    }

    serialize_send_msg(buf, encoded_len);

    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int64_t
ExternalRouterClientIF::encode_bard_add_quota_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                        BARDNodeStorageUsage& quota)
{
    uint64_t msg_type = EXTRTR_MSG_BARD_ADD_QUOTA_REQ;
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

    // quota type
    err = cbor_encode_text_string(&msgEncoder, quota.quota_type_cstr(), 
                                                 strlen(quota.quota_type_cstr()));
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // scheme type
    err = cbor_encode_text_string(&msgEncoder, quota.naming_scheme_cstr(), 
                                                 strlen(quota.naming_scheme_cstr()));
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // node name/number
    err = cbor_encode_text_string(&msgEncoder, quota.nodename().c_str(), 
                                                 quota.nodename().length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN


    // quota info
    err = cbor_encode_uint(&msgEncoder, quota.quota_internal_bundles());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encode_uint(&msgEncoder, quota.quota_internal_bytes());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encode_uint(&msgEncoder, quota.quota_external_bundles());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encode_uint(&msgEncoder, quota.quota_external_bytes());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // can determine whether to refuse bundle if link name is blank
    err = cbor_encode_text_string(&msgEncoder, quota.quota_restage_link_name().c_str(), 
                                                 quota.quota_restage_link_name().length());
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encode_boolean(&msgEncoder, quota.quota_auto_reload());
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
ExternalRouterClientIF::encode_bard_del_quota_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                        BARDNodeStorageUsage& quota)
{
    uint64_t msg_type = EXTRTR_MSG_BARD_DEL_QUOTA_REQ;
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

    // quota type
    err = cbor_encode_text_string(&msgEncoder, quota.quota_type_cstr(), 
                                                 strlen(quota.quota_type_cstr()));
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // scheme type
    err = cbor_encode_text_string(&msgEncoder, quota.naming_scheme_cstr(), 
                                                 strlen(quota.naming_scheme_cstr()));
    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

    // node name/number
    err = cbor_encode_text_string(&msgEncoder, quota.nodename().c_str(), 
                                                 quota.nodename().length());
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
ExternalRouterClientIF::decode_bard_usage_report_msg_v0(CborValue& cvElement, 
                                                       BARDNodeStorageUsageMap& bard_usage_map,
                                                       RestageCLMap& restage_cl_map)
{
    int status;

    if (!cbor_value_at_end(&cvElement)) {
        status = decode_bard_usage_map_v0(cvElement, bard_usage_map);
        CHECK_CBORUTIL_STATUS_RETURN
    } else {
        return CBORUTIL_FAIL;
    }

    if (!cbor_value_at_end(&cvElement)) {
        status = decode_restage_cl_map_v0(cvElement, restage_cl_map);
        CHECK_CBORUTIL_STATUS_RETURN
    } else {
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bard_usage_map_v0(CborValue& rptElement, 
                                                BARDNodeStorageUsageMap& bard_usage_map)
{
    CborValue mapElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &mapElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN


    while (!cbor_value_at_end(&mapElement)) {
        status = decode_bard_usage_entry_v0(mapElement, bard_usage_map);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    err = cbor_value_leave_container(&rptElement, &mapElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_bard_usage_entry_v0(CborValue& mapElement, 
                                       BARDNodeStorageUsageMap& bard_usage_map)
{
    CborValue entryElement;
    CborError err;
    int status;

    std::string tmp_str;
    uint64_t    tmp_u64;
    bool        tmp_bool;

    SPtr_BARDNodeStorageUsage sptr_usage;

    err = cbor_value_enter_container(&mapElement, &entryElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    sptr_usage = std::make_shared<BARDNodeStorageUsage>();


    // quota type
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_type(str_to_bard_quota_type(tmp_str.c_str()));

    // scheme type
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_naming_scheme(str_to_bard_naming_scheme(tmp_str.c_str()));

    // node name/number
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_nodename(tmp_str);

    // quota info
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_internal_bundles(tmp_u64);

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_internal_bytes(tmp_u64);

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_external_bundles(tmp_u64);

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_external_bytes(tmp_u64);


    // can determine whether to refuse bundle if link name is blank
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_restage_link_name(tmp_str);
    sptr_usage->set_quota_refuse_bundle(tmp_str.empty());

    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->set_quota_auto_reload(tmp_bool);
    


    // in-use uinfo
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->inuse_internal_bundles_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->inuse_internal_bytes_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->inuse_external_bundles_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->inuse_external_bytes_ = tmp_u64;


    // reserved info
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->reserved_internal_bundles_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->reserved_internal_bytes_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->reserved_external_bundles_ = tmp_u64;

    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_usage->reserved_external_bytes_ = tmp_u64;



    err = cbor_value_leave_container(&mapElement, &entryElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    bard_usage_map[sptr_usage->key()] = sptr_usage;


    return CBORUTIL_SUCCESS;
}


//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_restage_cl_map_v0(CborValue& rptElement, 
                                                 RestageCLMap& restage_cl_map)
{
    CborValue mapElement;
    CborError err;
    int status;

    err = cbor_value_enter_container(&rptElement, &mapElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN


    while (!cbor_value_at_end(&mapElement)) {
        status = decode_restage_cl_entry_v0(mapElement, restage_cl_map);
        CHECK_CBORUTIL_STATUS_RETURN
    }

    err = cbor_value_leave_container(&rptElement, &mapElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int
ExternalRouterClientIF::decode_restage_cl_entry_v0(CborValue& mapElement, 
                                                   RestageCLMap& restage_cl_map)
{
    CborValue entryElement;
    CborError err;
    int status;

    std::string tmp_str;
    uint64_t    tmp_u64;
    bool        tmp_bool;

    SPtr_RestageCLStatus sptr_clstatus;

    err = cbor_value_enter_container(&mapElement, &entryElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    sptr_clstatus = std::make_shared<RestageCLStatus>();


    // link name
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->restage_link_name_ = tmp_str;

    // storage path
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->storage_path_ = tmp_str;

    // mount point flag
    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->mount_point_ = tmp_bool;

    //validated mount point 
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->validated_mount_pt_ = tmp_str;

    // mount point validated flag
    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->mount_pt_validated_ = tmp_bool;

    // storage path exists flag
    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->storage_path_exists_= tmp_bool;

    // part of pool flag
    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->part_of_pool_ = tmp_bool;

    // Total volume space
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->vol_total_space_ = tmp_u64;

    // Available volume space
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->vol_space_available_ = tmp_u64;

    // Max disk quota
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->disk_quota_ = tmp_u64;

    // Disk quota in use
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->disk_quota_in_use_ = tmp_u64;

    // Num files restaged
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->disk_num_files_ = tmp_u64;

    // Num days retention
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->days_retention_ = tmp_u64;

    // Expire bundles flag
    status = decode_boolean(entryElement, tmp_bool);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->expire_bundles_ = tmp_bool;

    // Time to live override when reloading bundles
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->ttl_override_ = tmp_u64;

    // Auto-reload interval (seconds)
    status = decode_uint(entryElement, tmp_u64);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->auto_reload_interval_ = tmp_u64;

    // current state of the restage CL
    status = decode_text_string(entryElement, tmp_str);
    CHECK_CBORUTIL_STATUS_RETURN
    sptr_clstatus->cl_state_ = str_to_restage_cl_state(tmp_str.c_str());


    err = cbor_value_leave_container(&mapElement, &entryElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    restage_cl_map[sptr_clstatus->restage_link_name_] = sptr_clstatus;


    return CBORUTIL_SUCCESS;
}



#endif // BARD_ENABLED


} // namespace dtn
