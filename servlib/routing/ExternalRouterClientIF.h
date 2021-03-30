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
/*
    typedef enum : uint64_t {
        EXTRTR_MSG_HELLO = 1,
        EXTRTR_MSG_ALERT,
        EXTRTR_MSG_LINK_QUERY,
        EXTRTR_MSG_BUNDLE_QUERY,
        EXTRTR_MSG_LINK_REPORT,
        EXTRTR_MSG_BUNDLE_REPORT,
        
    } extrtr_msg_type_t;


    const char* msg_type_to_str(uint64_t msg_type) {
        switch (msg_type) {
            case EXTRTR_MSG_HELLO: return "Hello";
            case EXTRTR_MSG_ALERT: return "Alert";
            case EXTRTR_MSG_LINK_QUERY: return "Link Query";
            case EXTRTR_MSG_BUNDLE_QUERY: return "Bundle Querey";
            case EXTRTR_MSG_LINK_REPORT: return "Link Report";
            case EXTRTR_MSG_BUNDLE_REPORT: return "Bundle Report";
            default: return "Unknown";
        }
    }


    typedef struct extrtr_link_t {
        std::string link_id_ { "x" };
        std::string remote_eid_ { "x" };
        std::string conv_layer_ { "x" };
        std::string link_state_ { "x" };
        std::string next_hop_ { "x" };
        std::string remote_addr_ { "x" };
        uint64_t    remote_port_ = 0;
        uint64_t    rate_ = 0;
    } extrtr_link_t;
    typedef std::shared_ptr<extrtr_link_t> extrtr_link_ptr_t;
    typedef std::vector<extrtr_link_ptr_t> extrtr_link_vector_t;
    typedef extrtr_link_vector_t::iterator extrtr_link_vector_iter_t;

    typedef struct extrtr_bundle_t {
        uint64_t bundleid_ = 0;
        uint64_t custodyid_ = 0;
        std::string source_ { "x" };
        std::string dest_ { "x" };

        std::string custodian_;
        std::string replyto_;
        std::string gbofid_str_;
        std::string prev_hop_;
        uint64_t length_ = 0;
        bool is_fragment_ = false;
        bool is_admin_ = false;
        uint64_t priority_ = 0;
        uint64_t ecos_flags_ = 0;
        uint64_t ecos_ordinal_ = 0;
        uint64_t ecos_flowlabel_ = 0;
        bool custody_requested_ = false;
        bool local_custody_ = false;
        bool singleton_dest_ = false;
    } extrtr_bundle_t;
    typedef std::shared_ptr<extrtr_bundle_t> extrtr_bundle_ptr_t;
    typedef std::vector<extrtr_bundle_ptr_t> extrtr_bundle_vector_t;
    typedef extrtr_bundle_vector_t::iterator extrtr_bundle_vector_iter_t;
*/


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
    virtual int client_send_delete_bundle_req_msg(uint64_t bundleid);
    virtual int client_send_delete_bundle_req_msg(extrtr_bundle_id_vector_t& bid_vec);

    virtual int client_send_link_reconfigure_req_msg(std::string& link_id_, extrtr_key_value_vector_t& kv_vec);
    virtual int client_send_link_close_req_msg(std::string& link_id_);

    virtual int client_send_take_custody_req_msg(uint64_t bundleid);

    virtual int client_send_shutdown_req_msg();




    // decoders
    virtual int decode_server_msg_header(CborValue& cvElement, uint64_t& msg_type, uint64_t& msg_version,
                                         std::string& server_eid);
    
    virtual int decode_hello_msg_v0(CborValue& cvElement, 
                                    uint64_t& bundles_received, uint64_t& bundles_pending);

    virtual int decode_alert_msg_v0(CborValue& cvElement, std::string& alert_txt);

    virtual int decode_link_report_msg_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);
    virtual int decode_link_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);

    virtual int decode_link_opened_msg_v0(CborValue& cvElement, extrtr_link_vector_t& link_vec);

    virtual int decode_link_closed_msg_v0(CborValue& cvElement, std::string& link_id);

    virtual int decode_link_available_msg_v0(CborValue& cvElement, std::string& link_id);

    virtual int decode_link_unavailable_msg_v0(CborValue& cvElement, std::string& link_id);




    virtual int decode_bundle_report_msg_v0(CborValue& cvElement, extrtr_bundle_vector_t& bundle_vec);
    virtual int decode_bundle_v0(CborValue& cvElement, extrtr_bundle_vector_t& bundle_vec);

    virtual int decode_bundle_received_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, extrtr_bundle_vector_t& bundle_vec);
    
    virtual int decode_bundle_transmitted_msg_v0(CborValue& cvElement, 
                                          std::string& link_id, uint64_t& bundleid, uint64_t& bytes_sent);

    virtual int decode_bundle_delivered_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    virtual int decode_bundle_expired_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    virtual int decode_bundle_cancelled_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    virtual int decode_custody_timeout_msg_v0(CborValue& cvElement, uint64_t& bundleid);

    virtual int decode_custody_accepted_msg_v0(CborValue& cvElement, uint64_t& bundleid, uint64_t& custody_id);

    virtual int decode_custody_signal_msg_v0(CborValue& cvElement, uint64_t& bundleid, 
                                               bool& success, uint64_t& reason);

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
    virtual int encode_client_msg_header(CborEncoder& msgEncoder, uint64_t msg_type, uint64_t msg_version);

    virtual int64_t encode_client_msg_with_no_data(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                   uint64_t msg_type, uint64_t msg_version);

    virtual int64_t encode_transmit_bundle_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  uint64_t bundleid, std::string& link_id);

    virtual int64_t encode_link_reconfigure_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id, extrtr_key_value_vector_t& kv_vec);

    virtual int encode_key_value_v0(CborEncoder& kvEncoder, extrtr_key_value_ptr_t& kvptr);

    virtual int64_t encode_link_close_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  std::string& link_id);

    virtual int64_t encode_take_custody_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                  uint64_t bundleid);

    // generic encoding for a message that is just a list of bundle ids (used by Delete Bundle request)
    virtual int64_t encode_bundle_id_list_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len,
                                                 uint64_t msg_type, uint64_t msg_version, 
                                                 extrtr_bundle_id_vector_t& bid_vec);

    virtual int encode_bundle_id_v0(CborEncoder& bidEncoder, extrtr_bundle_id_ptr_t& bidptr);

    virtual int64_t encode_shutdown_req_msg_v0(uint8_t* buf, uint64_t buflen, int64_t& encoded_len);

protected:
    oasys::SpinLock lock_;


};

} // namespace dtn

#endif /* _EXTERNAL_ROUTER_CLIENT_IF_H_ */

