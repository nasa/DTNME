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

#ifndef _EXTERNAL_ROUTER_IF_H_
#define _EXTERNAL_ROUTER_IF_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <memory>
#include <vector>


namespace dtn {

/**
 * ExternalRouter Interface class is a base class that defines
 * data types common to both server and client.
 */
class ExternalRouterIF {
public:

    typedef std::vector<std::string> LinkParametersVector;


    typedef enum : uint64_t {
        // server sourced messages
        EXTRTR_MSG_HELLO = 1,
        EXTRTR_MSG_ALERT,

        EXTRTR_MSG_LINK_REPORT,
        EXTRTR_MSG_LINK_OPENED,
        EXTRTR_MSG_LINK_CLOSED,
        EXTRTR_MSG_LINK_AVAILABLE,
        EXTRTR_MSG_LINK_UNAVAILABLE,

        EXTRTR_MSG_BUNDLE_REPORT,
        EXTRTR_MSG_BUNDLE_RECEIVED,
        EXTRTR_MSG_BUNDLE_TRANSMITTED,
        EXTRTR_MSG_BUNDLE_DELIVERED,
        EXTRTR_MSG_BUNDLE_EXPIRED,
        EXTRTR_MSG_BUNDLE_CANCELLED,
        EXTRTR_MSG_CUSTODY_TIMEOUT,
        EXTRTR_MSG_CUSTODY_ACCEPTED,
        EXTRTR_MSG_CUSTODY_SIGNAL,
        EXTRTR_MSG_AGG_CUSTODY_SIGNAL,

        // client sourced messages
        EXTRTR_MSG_LINK_QUERY = 100,
        EXTRTR_MSG_BUNDLE_QUERY,
        EXTRTR_MSG_TRANSMIT_BUNDLE_REQ,
        EXTRTR_MSG_LINK_RECONFIGURE_REQ,
        EXTRTR_MSG_LINK_CLOSE_REQ,
        EXTRTR_MSG_LINK_ADD_REQ,
        EXTRTR_MSG_LINK_DEL_REQ,
        EXTRTR_MSG_TAKE_CUSTODY_REQ,
        EXTRTR_MSG_DELETE_BUNDLE_REQ,
        EXTRTR_MSG_DELETE_ALL_BUNDLES_REQ,

        EXTRTR_MSG_SHUTDOWN_REQ,
        
    } extrtr_msg_type_t;


    const char* msg_type_to_str(uint64_t msg_type) {
        switch (msg_type) {
            // server sourced messages
            case EXTRTR_MSG_HELLO: return "Hello";
            case EXTRTR_MSG_ALERT: return "Alert";

            case EXTRTR_MSG_LINK_REPORT: return "Link Report";
            case EXTRTR_MSG_LINK_OPENED: return "Link Opened Event";
            case EXTRTR_MSG_LINK_CLOSED: return "Link Closed Event";
            case EXTRTR_MSG_LINK_AVAILABLE: return "Link Available Event";
            case EXTRTR_MSG_LINK_UNAVAILABLE: return "Link Unavailable Event";

            case EXTRTR_MSG_BUNDLE_REPORT: return "Bundle Report";
            case EXTRTR_MSG_BUNDLE_RECEIVED: return "Bundle Received Event";
            case EXTRTR_MSG_BUNDLE_TRANSMITTED: return "Bundle Transmitted Event";
            case EXTRTR_MSG_BUNDLE_DELIVERED: return "Bundle Delivered Event";
            case EXTRTR_MSG_BUNDLE_EXPIRED: return "Bundle Expired Event";
            case EXTRTR_MSG_BUNDLE_CANCELLED: return "Bundle Cancelled Event";
            case EXTRTR_MSG_CUSTODY_TIMEOUT: return "Custody Timeout Event";
            case EXTRTR_MSG_CUSTODY_ACCEPTED: return "Custody Accepted Event";
            case EXTRTR_MSG_CUSTODY_SIGNAL: return "Custody Signal Event";
            case EXTRTR_MSG_AGG_CUSTODY_SIGNAL: return "Aggregate Custody Signal Event";



            // client sourced messages
            case EXTRTR_MSG_LINK_QUERY: return "Link Query";
            case EXTRTR_MSG_BUNDLE_QUERY: return "Bundle Querey";
            case EXTRTR_MSG_TRANSMIT_BUNDLE_REQ: return "Transmit Bundle Request";
            case EXTRTR_MSG_LINK_RECONFIGURE_REQ: return "Link Reconfigure Request";
            case EXTRTR_MSG_LINK_CLOSE_REQ: return "Link Close Request";
            case EXTRTR_MSG_LINK_ADD_REQ: return "Link Add Request";
            case EXTRTR_MSG_LINK_DEL_REQ: return "Link Delete Request";
            case EXTRTR_MSG_TAKE_CUSTODY_REQ: return "Take Custody Request";
            case EXTRTR_MSG_DELETE_BUNDLE_REQ: return "Delete Bundle Request";
            case EXTRTR_MSG_DELETE_ALL_BUNDLES_REQ: return "Delete All Bundles Request";


            case EXTRTR_MSG_SHUTDOWN_REQ: return "Shutdown Request";

            default: return "Unknown";
        }
    }

    // Structures for the Link Report
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



    // Structures for the Bundle Report
    typedef struct extrtr_bundle_t {
        uint64_t bundleid_ = 0;
        uint64_t bp_version_ = 0;
        uint64_t custodyid_ = 0;
        std::string source_ { "x" };
        std::string dest_ { "x" };
        std::string gbofid_str_;
        std::string prev_hop_;
        uint64_t length_ = 0;
        uint64_t priority_ = 0;
        uint64_t ecos_flags_ = 0;
        uint64_t ecos_ordinal_ = 0;
        uint64_t ecos_flowlabel_ = 0;
        bool custody_requested_ = false;
        bool local_custody_ = false;
        bool singleton_dest_ = false;
        bool expired_in_transit_ = false; // flag to indicate it should not be routed
    } extrtr_bundle_t;
    typedef std::shared_ptr<extrtr_bundle_t> extrtr_bundle_ptr_t;
    typedef std::vector<extrtr_bundle_ptr_t> extrtr_bundle_vector_t;
    typedef extrtr_bundle_vector_t::iterator extrtr_bundle_vector_iter_t;


    // Structures for Key-Value pairs list
    typedef enum : uint64_t {
        KEY_VAL_BOOL = 1,
        KEY_VAL_UINT,
        KEY_VAL_INT,
        KEY_VAL_STRING
    } key_value_type_t;

    typedef struct extrtr_key_value_t {
        std::string key_;
        key_value_type_t value_type_ = KEY_VAL_INT;
        // only specified value type is set:
        bool value_bool_ = false;
        uint64_t value_uint_ = 0;
        int64_t value_int_ = 0;
        std::string value_string_;
    } extrtr_key_value_t;
    typedef std::shared_ptr<extrtr_key_value_t> extrtr_key_value_ptr_t;
    typedef std::vector<extrtr_key_value_ptr_t> extrtr_key_value_vector_t;
    typedef extrtr_key_value_vector_t::iterator extrtr_key_value_vector_iter_t;

    // Structures for the Delete Bundle Request and possibly others
    // - may need to add the GbofID eventually so following the
    //   pattern and using a structure
    typedef struct extrtr_bundle_id_t {
        uint64_t bundleid_ = 0;
    } extrtr_bundle_id_t;
    typedef std::shared_ptr<extrtr_bundle_id_t> extrtr_bundle_id_ptr_t;
    typedef std::vector<extrtr_bundle_id_ptr_t> extrtr_bundle_id_vector_t;
    typedef extrtr_bundle_id_vector_t::iterator extrtr_bundle_id_vector_iter_t;


    /**
     * Constructor 
     */
    ExternalRouterIF();
                   
    /**
     * Virtual destructor.
     */
    virtual ~ExternalRouterIF();

};

} // namespace dtn

#endif /* _EXTERNAL_ROUTER_IF_H_ */

