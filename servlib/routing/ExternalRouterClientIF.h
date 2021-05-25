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

#ifndef _EXTERNAL_ROUTER_CLIENT_IF_H_
#define _EXTERNAL_ROUTER_CLIENT_IF_H_

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
class ExternalRouterClientIF : public ExternalRouterIF,
                               public CborUtil {
public:
    /**
     * Constructor 
     */
    ExternalRouterClientIF(const char* usage);
                   
    /**
     * Virtual destructor.
     */
    virtual ~ExternalRouterClientIF();


    // encode and send message per type
    virtual int client_send_link_query_msg();
    virtual int client_send_bundle_query_msg();

    virtual int client_send_transmit_bundle_req_msg(uint64_t bundleid, std::string& link_id);

    /**
     * Issue command to delete a single Bundle IDs
     * @param bundleid The Bundle ID to be deleted
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_delete_bundle_req_msg(uint64_t bundleid);

    /**
     * Issue command to delete all bundles
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_delete_all_bundles_req_msg();

    /**
     * Issue command to delete a list of Bundle IDs
     * @param bid_vec A vector Bundle IDs to be deleted
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_delete_bundle_req_msg(extrtr_bundle_id_vector_t& bid_vec);

    /**
     * Issue command to reconfigure a link
     * @param link_id The name of the link to reconfigure
     * @param kv_vec A vector of key and value objects for the convergence layer specific parameters to be updated
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_link_reconfigure_req_msg(std::string& link_id_, extrtr_key_value_vector_t& kv_vec);

    /**
     * Issue command to close a link
     * @param link_id The name of the link to close
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_link_close_req_msg(std::string& link_id_);

    /**
     * Issue command to take custody of a bundle
     * @param bundleid The Bundle ID of th ebundle to take custody of
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_take_custody_req_msg(uint64_t bundleid);

    /**
     * Issue command to shutdown the DTN server
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_shutdown_req_msg();

    /**
     * Issue command to add a new link
     * @param link_id The name of the link to add
     * @param next_hop The convergence layer specific next hop string (<ip_addr>:<port> for most CLs;
     * @param link_mode The link mode or type ("ALWAYSON", "ONDEMAND", "SCHEDULED" or "OPPORTUNISTIC")
     * @param cl_name The name of the convergence layer the link is an instance of
     * @param params A vector of "key=value" strings as would be entered through the console or in the config file
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_link_add_req_msg(std::string& link_id, 
                                             std::string& next_hop, std::string& link_mode,
                                             std::string& cl_name, LinkParametersVector& params);

    /**
     * Issue command to delete a link
     * @param link_id The name of the link to delete
     * @return 0 on success or -1 on failure of building and sending the message (not the result of the command itself)
     */
    virtual int client_send_link_del_req_msg(std::string& link_id_);



    // decoders
    /**
     * Decode the Message Header portion of the CBOR string
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param msg_type The message type identifier (extrtr_msg_type_t defined in ExternalRouterIF.h as an int64 value)
     * @param msg_version The version of the message type
     * @param server_eid The EndpointID string of the DTN server that sent the message
     * @return 0 on success or -1 on failure
     */
    virtual int decode_server_msg_header(CborValue& cvElement, uint64_t& msg_type, uint64_t& msg_version,
                                         std::string& server_eid);
    
    /**
     * Decode the Hello Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundles_received Total number of bundles received by the DTN server
     * @param bundles_pending The number of bundle currently pending
     * @return 0 on success or -1 on failure
     */
    virtual int decode_hello_msg_v0(CborValue& cvElement, 
                                    uint64_t& bundles_received, uint64_t& bundles_pending);

    /**
     * Decode the Hello Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundles_received Total number of bundles received by the DTN server
     * @param bundles_pending The number of bundle currently pending
     * @return 0 on success or -1 on failure
     */
    virtual int decode_alert_msg_v0(CborValue& cvElement, std::string& alert_txt);

    /**
     * Decode the Link Report Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_vec A vector used to return the details of the DTN server links
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_report_msg_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);
    /**
     * Decode a single Link's data from a Link Report Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_vec A vector used to return the details of the DTN server links
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);

    /**
     * Decode the Link Opened Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_vec A vector used to return the details of the DTN server links (only one link will be in the vector)
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_opened_msg_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);

    /**
     * Decode the Link Closed Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_id The name of the link that was closed
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_closed_msg_v0(CborValue& cvElement, std::string& link_id);

    /**
     * Decode the Link Availabe Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_id The name of the link that transitioned to the avaliable state
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_available_msg_v0(CborValue& cvElement, std::string& link_id);

    /**
     * Decode the Link Unavailable Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_id The name of the link that transitioned to the unavaliable state
     * @return 0 on success or -1 on failure
     */
    virtual int decode_link_unavailable_msg_v0(CborValue& cvElement, std::string& link_id);





    /**
     * Decode the Bundle Report Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundle_vec A vector used to return the details of the bundles currently in the DTN server
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_report_msg_v0(CborValue& cvElement, extrtr_bundle_vector_t& bundle_vec);
    /**
     * Decode the data for a single bundle witthin the Bundle Report Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundle_vec A vector used to return the details of the bundles currently in the DTN server
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_v0(CborValue& cvElement, extrtr_bundle_vector_t& bundle_vec);

    /**
     * Decode the Bundle Received Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundle_vec A vector used to return the details of the [single] bundle that was receied
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_received_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, extrtr_bundle_vector_t& bundle_vec);
    
    /**
     * Decode the Bundle Transmitted Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param link_id The name of the link that the bundle was transmitted (or attempted to transmit) on
     * @param bundleid The Bundle ID of the bundle that was transmitted (or failed to be transmitted)
     * @param byes_sent The number of bytes transmitted (zero indicates a failed transmission)
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_transmitted_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, uint64_t& bundleid, uint64_t& bytes_sent);

    /**
     * Decode the Bundle Delivered Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle that was delivered
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_delivered_msg_v0(CborValue& cvElement, uint64_t& bundleid);
    
    /**
     * Decode the Bundle Expired Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle that expired
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_expired_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    /**
     * Decode the Bundle Cancelled Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle for which the transmission was cancelled
     * @return 0 on success or -1 on failure
     */
    virtual int decode_bundle_cancelled_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    /**
     * Decode the Bundle Custody Accepted Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle for which local custody was accepted
     * @param custody_id The local Custody ID assigned to the bundle
     * @return 0 on success or -1 on failure
     */
    virtual int decode_custody_accepted_msg_v0(CborValue& cvElement, uint64_t& bundleid, uint64_t& custody_id);

    /**
     * Decode the Bundle Custody Timeout Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle that was transmitted but for which a Custody Signal was not received within the expected time period and needed to be resent
     * @return 0 on success or -1 on failure
     */
    virtual int decode_custody_timeout_msg_v0(CborValue& cvElement, uint64_t& bundleid);


    /**
     * Decode the Bundle Custody Signal Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param bundleid The Bundle ID of the bundle for which a Custody Signal was received
     * @param success Boolean flag indicating whether or not the bundle was accepted by the remote node 
     * @param reason The reason code provided in the Custody Signal
     * @return 0 on success or -1 on failure
     */
    virtual int decode_custody_signal_msg_v0(CborValue& cvElement, uint64_t& bundleid, 
                                               bool& success, uint64_t& reason);

    /**
     * Decode the Bundle Aggregate Custody Signal Message
     * @param cvElement The CBOR iterator that keep track of the current location and context within a CBOR string that is being parsed
     * @param acs_data The data from the Aggregate Custody Signal that will need to be parsed
     * @return 0 on success or -1 on failure
     */
    virtual int decode_agg_custody_signal_msg_v0(CborValue& cvElement, std::string& acs_data);

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


    // encoders - client    
    /**
     * Encode the Message Header portion of the CBOR string
     * @param msgEncoder The CBOR encoder that keep track of the current location and context within a CBOR string that is being generated
     * @param msg_type The message type identifier (extrtr_msg_type_t defined in ExternalRouterIF.h as an int64 value)
     * @param msg_version The version of the message type
     * @return 0 on success or -1 on failure
     */
    virtual int encode_client_msg_header(CborEncoder& msgEncoder, uint64_t msg_type, uint64_t msg_version);

    /**
     * Encode a Message Type that does not require any message specific data
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param msg_type The message type identifier (extrtr_msg_type_t defined in ExternalRouterIF.h as an int64 value)
     * @param msg_version The version of the message type
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_client_msg_with_no_data(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                   uint64_t msg_type, uint64_t msg_version);

    /**
     * Encode a Transmit Bundle Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param bundleid The Bundle ID of the bundle to transmit
     * @param link_id The name of the link to use for the transmission of the bundle
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_transmit_bundle_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  uint64_t bundleid, std::string& link_id);

    /**
     * Encode a Link Reconfigure Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param link_id The name of the link to use for the transmission of the bundle
     * @param The list of Key-Value parameters to update for the link
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_link_reconfigure_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, extrtr_key_value_vector_t& kv_vec);

    /**
     * Encode the Key-Value pair list for a message
     * @param kvEncoder The CBOR encoder that keep track of the current location and context within a CBOR string that is being generated
     * @param The list of Key-Value parameters to update for the link
     * @return 0 on success or -1 on failure
     */
    virtual int encode_key_value_v0(CborEncoder& kvEncoder, extrtr_key_value_ptr_t& kvptr);

    /**
     * Encode a Link Close Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param link_id The name of the link to close
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_link_close_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id);

    /**
     * Encode a Bundle Take Custody Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param bundleid The Bundle ID of the bundle to take local suctody of
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_take_custody_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  uint64_t bundleid);

    // generic encoding for a message that is just a list of bundle ids (used by Delete Bundle request)
    /**
     * Encode a message that only sends a list of bundle IDs (Delete Bundle Request Message)
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param msg_type The message type identifier (extrtr_msg_type_t defined in ExternalRouterIF.h as an int64 value)
     * @param msg_version The version of the message type
     * @param bid_vec A vector of bundle IDs to which the request message applies
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_bundle_id_list_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t msg_type, uint64_t msg_version, 
                                                 extrtr_bundle_id_vector_t& bid_vec);

    /**
     * Encode a bundle ID for a message
     * @param bidEncoder The CBOR encoder that keep track of the current location and context within a CBOR string that is being generated
     * @param bidptr The pointer to the Bundle ID to encode
     * @return 0 on success or -1 on failure
     */
    virtual int encode_bundle_id_v0(CborEncoder& bidEncoder, extrtr_bundle_id_ptr_t& bidptr);

    /**
     * Encode a Shutdown Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_shutdown_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len);

    /**
     * Encode a Link Add Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param link_id The name of the link to be added
     * @param next_hop The next_hop string for the link to be added
     * @param link_mode The link mode (ALWAYSON, OPPORTUNISTIC, etc.) for the link to be added
     * @param cl_name The Convergence Layer name for the link to be added
     * @param params A vector of the paramter=value strings for the link to be added
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_link_add_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                               std::string& link_id,
                                               std::string& next_hop, std::string& link_mode,
                                               std::string& cl_name, LinkParametersVector& params);

    /**
     * Encode a Link Delete Request Message
     * @param buf A pointer to a buffer to place the encoded message in or nullptr to calculate the size needed to hold the encoded message
     * @param buflen The length of the passed in buffer os zero to calculate the size needed to hold the encoded message
     * @param encoded_len Returns the length of the encoded message if it was successful
     * @param link_id The name of the link to be deleted
     * @return CBORUTIL_SUCCESS (0) on success or CBORUTIL_FAIL (-1) else the length of the [additional] space needed to encode the message if buflen was too small
     */
    virtual int64_t encode_link_del_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                               std::string& link_id);


protected:
    oasys::SpinLock lock_;


};

} // namespace dtn

#endif /* _EXTERNAL_ROUTER_CLIENT_IF_H_ */

