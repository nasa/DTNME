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

#ifndef _BUNDLE_STATUS_REPORT_H_
#define _BUNDLE_STATUS_REPORT_H_

#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleTimestamp.h"

namespace dtn {

/**
 * Utility class to create and parse status reports.
 */
class BundleStatusReport {
public:
    typedef enum {
        STATUS_RECEIVED         = 0x01,
        STATUS_CUSTODY_ACCEPTED = 0x02,
        STATUS_FORWARDED        = 0x04,
        STATUS_DELIVERED        = 0x08,
        STATUS_DELETED          = 0x10,
        STATUS_ACKED_BY_APP     = 0x20,
        STATUS_UNUSED           = 0x40,
        STATUS_UNUSED2          = 0x80,
    } flag_t;
    
    /**
     * The reason codes are defined in the BundleProtocol class.
     */
    typedef BundleProtocol::status_report_reason_t reason_t;

    /**
     * Specification of the contents of a Bundle Status Report
     */
    struct data_t {
        u_int8_t        admin_type_;
        u_int8_t        admin_flags_;
        u_int8_t        status_flags_;
        u_int8_t        reason_code_;
        u_int64_t       orig_frag_offset_;
        u_int64_t       orig_frag_length_;
        BundleTimestamp receipt_tv_;
        BundleTimestamp custody_tv_;
        BundleTimestamp forwarding_tv_;
        BundleTimestamp delivery_tv_;
        BundleTimestamp deletion_tv_;
        BundleTimestamp ack_by_app_tv_;
        BundleTimestamp orig_creation_tv_;
        EndpointID      orig_source_eid_;
    };

    /**
     * Constructor-like function that fills in the bundle payload
     * buffer with the appropriate status report format.
     *
     * Although the spec allows for multiple timestamps to be set in a
     * single status report, this implementation only supports
     * creating a single timestamp per report, hence there is only
     * support for a single flag to be passed in the parameters.
     */
    static void create_status_report(Bundle*           bundle,
                                     const Bundle*     orig_bundle,
                                     const EndpointID& source,
                                     flag_t            status_flag,
                                     reason_t          reason);
    
    /**
     * Parse a byte stream containing a Status Report Payload and
     * store the fields in the given struct. Returns false if parsing
     * failed.
     */
    static bool parse_status_report(data_t* data,
                                    const u_char* bp, u_int len);

    /**
     * Parse the payload of the given bundle into the given struct.
     * Returns false if the bundle is not a well formed status report.
     */
    static bool parse_status_report(data_t* data,
                                    const Bundle* bundle);

    /**
     * Return a string version of the reason code.
     */
    static const char* reason_to_str(u_int8_t reason);
};

} // namespace dtn

#endif /* _BUNDLE_STATUS_REPORT_H_ */
