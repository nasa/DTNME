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

#ifndef _BUNDLE_PROTOCOL_H_
#define _BUNDLE_PROTOCOL_H_

#include <sys/types.h>
#include "BlockInfo.h"
#include "contacts/Link.h"

namespace dtn {

class BlockProcessor;
class Bundle;
struct BundleTimestamp;
class EndpointID;

/**
 * Centralized class used to convert a Bundle to / from the bundle
 * protocol specification for the "on-the-wire" representation.
 *
 * The actual implementation of this is mostly encapsulated in the
 * BlockProcessor class hierarchy.
 */
class BundleProtocol {
public:
    /**
     * Initialize the default set of block processors.
     */
    static void init_default_processors();

    /**
     * Give the processors a chance to chew on the bundle after
     * reloading from disk.
     */
    static void reload_post_process(Bundle* bundle);

    /**
     * Generate a BlockInfoVec for the outgoing link and put it into
     * xmit_blocks_.
     *
     * @return a pointer to the new block list
     */
    static SPtr_BlockInfoVec prepare_blocks(Bundle* bundle, const LinkRef& link);
    
    /**
     * Generate contents for the given BlockInfoVec on the given Link.
     *
     * @return the total length of the formatted blocks for this bundle.
     */
    static size_t generate_blocks(Bundle*        bundle,
                                  SPtr_BlockInfoVec&  blocks,
                                  const LinkRef& link);

    static void delete_blocks(Bundle* bundle, const LinkRef& link);

    /**
     * Return the total length of the formatted bundle block data.
     */
    static size_t total_length(Bundle* bundle, const SPtr_BlockInfoVec& blocks);

    /**
     * Temporary helper function to find the offset of the first byte
     * of the payload in a block list.
     */
    static size_t payload_offset(const Bundle* bundle, const SPtr_BlockInfoVec& blocks);
    
    /**
     * Copies out a chunk of formatted bundle data at a specified
     * offset from the provided BlockList.
     *
     * @return the length of the chunk produced (up to the supplied
     * length) and sets *last to true if the bundle is complete.
     */
    static size_t produce(const Bundle* bundle, const SPtr_BlockInfoVec& blocks,
                          u_char* data, size_t offset, size_t len, bool* last);
    
    /**
     * Parse the supplied chunk of arriving data and append it to the
     * rcvd_blocks_ list in the given bundle, finding the appropriate
     * BlockProcessor element and calling its receive() handler.
     *
     * When called repeatedly for arriving chunks of data, this
     * properly fills in the entire bundle, including the in_blocks_
     * record of the arriving blocks and the payload (which is stored
     * externally).
     *
     * @return the length of data consumed or -1 on protocol error,
     * plus sets *last to true if the bundle is complete.
     */
    static ssize_t consume(Bundle* bundle, u_char* data, size_t len, bool* last);

    /**
     * Bundle Protocol Versions
     */
    typedef enum : int64_t {
        BP_VERSION_UNKNOWN                = -1,
        BP_VERSION_6                      = 6,
        BP_VERSION_7                      = 7,
    } bp_versions_t;

    /**
     * Bundle Status Report "Reason Code" flags
     */
    typedef enum {
        REASON_NO_ADDTL_INFO              = 0x00,
        REASON_LIFETIME_EXPIRED           = 0x01,
        REASON_FORWARDED_UNIDIR_LINK      = 0x02,
        REASON_TRANSMISSION_CANCELLED     = 0x03,
        REASON_DEPLETED_STORAGE           = 0x04,
        REASON_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
        REASON_NO_ROUTE_TO_DEST           = 0x06,
        REASON_NO_TIMELY_CONTACT          = 0x07,
        REASON_BLOCK_UNINTELLIGIBLE       = 0x08,
        REASON_HOP_LIMIT_EXCEEDED         = 0x09,
        REASON_TRAFFIC_PARED              = 0x0A,
        REASON_BLOCK_UNSUPPORTED          = 0x0B,

        REASON_SECURITY_FAILED            = 0x0F,
    } status_report_reason_t;

    /**
     * Loop through the bundle's received block list to validate each
     * entry.
     *
     * @return true if the bundle is valid, false if it should be
     * deleted.
     */
    static bool validate(Bundle* bundle,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason);

    /**
     * Bundle Status Report Status Flags
     */
    typedef enum {
        STATUS_RECEIVED         = 1 << 14,
        STATUS_CUSTODY_ACCEPTED = 1 << 15,
        STATUS_FORWARDED        = 1 << 16,
        STATUS_DELIVERED        = 1 << 17,
        STATUS_DELETED          = 1 << 18,
        STATUS_ACKED_BY_APP     = 1 << 19,
        STATUS_UNUSED2          = 1 << 20,
    } status_report_flag_t;

    /**
     * Custody Signal Reason Codes
     */
    typedef enum {
        CUSTODY_NO_ADDTL_INFO              = 0x00,
        CUSTODY_REDUNDANT_RECEPTION        = 0x03,
        CUSTODY_DEPLETED_STORAGE           = 0x04,
        CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
        CUSTODY_NO_ROUTE_TO_DEST           = 0x06,
        CUSTODY_NO_TIMELY_CONTACT          = 0x07,
        CUSTODY_BLOCK_UNINTELLIGIBLE       = 0x08
    } custody_signal_reason_t;


    struct Params {
        Params();
        bool status_rpts_enabled_;
        bool use_bundle_age_block_;
        uint8_t default_hop_limit_;
    };

    static Params params_;

};

} // namespace dtn

#endif /* _BUNDLE_PROTOCOL_H_ */
