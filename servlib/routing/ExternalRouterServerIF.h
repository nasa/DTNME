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

#ifndef _EXTERNAL_ROUTER_SERVER_IF_H_
#define _EXTERNAL_ROUTER_SERVER_IF_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <memory>

#include <third_party/oasys/thread/SpinLock.h>

#include "bundling/CborUtil.h"
#include "routing/ExternalRouterIF.h"

namespace dtn {

/**
 * ExternalRouter Interface class to handle encoding and decoding
 * messages. It is a base class with a pure virtual send_msg()
 * method that must be provided by the derived class.
 */
class ExternalRouterServerIF : public ExternalRouterIF,
                               public CborUtil {
public:
    /**
     * Constructor 
     */
    ExternalRouterServerIF(const char* usage);
                   
    /**
     * Virtual destructor.
     */
    virtual ~ExternalRouterServerIF();

    /**
     * Set the Server EID to use when sending External Router messages
     * @param eid The Endpoint ID of the DTN server the External Router is controlling
     */
    virtual void set_server_eid(const std::string& eid) {
        server_eid_ = eid; 
        server_eid_set_ = true;
    }


    /**
     * Encode and send a Hello Message
     * @param bundles_received The total number of bundles received by the DTN server
     * @param bundles_pending The number of bundles currently pending in the DTN server
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_hello_msg(uint64_t bundles_received, uint64_t bundles_pending);

    /**
     * Encode and send an Alert Message
     * @param alert_txt The text of the Alert message
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_alert_msg(std::string& alert_txt);


    /**
     * Encode and send a Link Report Message
     * @param link_vec A vector of the link details to send
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_link_report_msg(extrtr_link_vector_t& link_vec);

    /**
     * Encode and send a Link Opened Message
     * @param link_vec A vector with one entry with the details of the link that opened
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_link_opened_msg(extrtr_link_vector_t& link_vec);

    /**
     * Encode and send a Link Closed Message
     * @param link_id The name of the link that closed
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_link_closed_msg(std::string& link_id);

    /**
     * Encode and send a Link Available Message
     * @param link_id The name of the link that transitioned to the available state
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_link_available_msg(std::string& link_id);

    /**
     * Encode and send a Link Unavailable Message
     * @param link_id The name of the link that transitioned to the unavailable state
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_link_unavailable_msg(std::string& link_id);



    /**
     * Encode and send a Bundle Report Message
     * @param bundle_vec A vector of bundle details 
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_report_msg(extrtr_bundle_vector_t& bundle_vec);

    /**
     * Encode and send a Bundle Received Message
     * @param bundle_vec A vector with a single entry containing the details of the received bundle
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_received_msg(std::string& link_id, extrtr_bundle_vector_t& bundle_vec);

    /**
     * Encode and send Bundle Transmitted Message
     * @param bundleid The bundle ID of the bundle that was transmitted (or was attempted to be sent)
     * @param bytes_sent The number of bytes transmitted or zero if the transmit failed
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_transmitted_msg(std::string& link_id, uint64_t bundleid, uint64_t bytes_sent);

    /**
     * Encode and send Bundle Delivered Message
     * @param bundleid The bundle ID of the bundle that was delivered
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_delivered_msg(uint64_t bundleid);

    /**
     * Encode and send Bundle Expired Message
     * @param bundleid The bundle ID of the bundle that expired
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_expired_msg(uint64_t bundleid);

    /**
     * Encode and send Bundle Cancelled Message
     * @param bundleid The bundle ID of the bundle for which a transmission was cancelleed
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_bundle_cancelled_msg(uint64_t bundleid);

    /**
     * Encode and send a Bundle Custody Timeout Message
     * @param bundleid The bundle ID of the bundle that was transmitted but a Custody Signal was not received within the expected time
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_custody_timeout_msg(uint64_t bundleid);

    /**
     * Encode and send a Bundle Custody Accepted Message
     * @param alert_txt The text of the Alert message
     * @param bundleid The bundle ID of the bundle that was accepted into local custody
     * @param custody_id The Custody ID assigned to the bundle
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_custody_accepted_msg(uint64_t bundleid, uint64_t custody_id);

    /**
     * Encode and send a Custody Signal Message
     * @param bundleid The bundle ID of the bundle for which a Custody Signal was received
     * @param success A boolean flag indicating whether or not the bundle was accepted by the remote node
     * @param read The reason code provided in the Cusstody Signal
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_custody_signal_msg(uint64_t bundleid, bool success, uint64_t reason);

    /**
     * Encode and send an Aggregate Custody Signal Message
     * @param acs_data The data from the Aggregate Custody Signal that will need to be parsed
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1)
     */
    virtual int server_send_agg_custody_signal_msg(std::string& acs_data);



    // decoders
    virtual int decode_client_msg_header(CborValue& cvElement, 
                                       uint64_t& msg_type, uint64_t& msg_version);
    
    virtual int decode_transmit_bundle_req_msg_v0(CborValue& cvElement, 
                                        uint64_t& bundleid, std::string& link_id);

    virtual int decode_link_reconfigure_req_msg_v0(CborValue& cvElement, 
                                       std::string& link_id, extrtr_key_value_vector_t& kv_vec);

    virtual int decode_key_value_v0(CborValue& rptElement, 
                                       extrtr_key_value_vector_t& kv_vec);
    
    virtual int decode_link_close_req_msg_v0(CborValue& cvElement, 
                                       std::string& link_id);

    virtual int decode_link_add_req_msg_v0(CborValue& cvElement, 
                                           std::string& link_id, std::string& next_hop, 
                                           std::string& link_mode, std::string& cl_name, 
                                           LinkParametersVector& params);

    virtual int decode_link_params_v0(CborValue& cvElement, LinkParametersVector& params);

    virtual int decode_link_del_req_msg_v0(CborValue& cvElement, 
                                           std::string& link_id);

    virtual int decode_take_custody_req_msg_v0(CborValue& cvElement, 
                                       uint64_t& bundleid);

    virtual int decode_delete_bundle_req_msg_v0(CborValue& cvElement, 
                                       extrtr_bundle_id_vector_t& bid_vec);

    virtual int decode_bundle_id_v0(CborValue& rptElement, 
                                       extrtr_bundle_id_vector_t& bid_vec);
    
protected:
    /**
     * send_msg method must be defined by the derived class
     * -- usuaully queue the string to be transmitted on a different thread
     */
    virtual void send_msg(std::string* msg) = 0;

    /**
     * serializes multiple threads sending messages. 
     * Reports will lock out other threads until they complete.
     */
    virtual void serialize_send_msg(uint8_t* buf, int64_t buflen);


    // encoders - server
    virtual int encode_server_msg_header(CborEncoder& msgEncoder, uint64_t msg_type, uint64_t msg_version);

    virtual int64_t encode_hello_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                        uint64_t bundles_received, uint64_t bundles_pending);

    virtual int64_t encode_alert_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                        std::string& alert_txt);

    virtual int64_t encode_link_report_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                              extrtr_link_vector_t& link_vec);
    virtual int encode_link_v0(CborEncoder& linkEncoder, extrtr_link_ptr_t& linkptr);

    virtual int64_t encode_link_opened_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                              extrtr_link_vector_t& link_vec);

    virtual int64_t encode_link_closed_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                              std::string& link_id);

    virtual int64_t encode_link_available_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                              std::string& link_id);

    virtual int64_t encode_link_unavailable_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                              std::string& link_id);



    virtual int64_t encode_bundle_report_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                extrtr_bundle_vector_t& bundle_vec);
    virtual int encode_bundle_v0(CborEncoder& bundleEncoder, extrtr_bundle_ptr_t& bundleptr);

    virtual int64_t encode_bundle_received_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, extrtr_bundle_vector_t& bundle_vec);

    virtual int64_t encode_bundle_transmitted_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, uint64_t bundleid, uint64_t bytes_sent);

    virtual int64_t encode_bundle_delivered_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid);

    virtual int64_t encode_bundle_expired_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid);

    virtual int64_t encode_bundle_cancelled_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid);

    virtual int64_t encode_custody_timeout_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid);

    virtual int64_t encode_custody_accepted_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid, uint64_t custody_id);

    virtual int64_t encode_custody_signal_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t bundleid, bool success, uint64_t reason);

    virtual int64_t encode_agg_custody_signal_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                     std::string& acs_data);

protected:
    oasys::SpinLock lock_;

    bool server_eid_set_ = false;
    std::string server_eid_;


};

} // namespace dtn

#endif /* _EXTERNAL_ROUTER_SERVER_IF_H_ */

