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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include "contacts/Link.h"

namespace dtn {

class BlockInfo;
class BlockInfoVec;
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
     * Register a new BlockProcessor handler to handle the given block
     * type code when received off the wire.
     */
    static void register_processor(BlockProcessor* bp);

    static void delete_block_processors();

    /**
     * Find the appropriate BlockProcessor for the given block type
     * code.
     */
    static BlockProcessor* find_processor(u_int8_t type);

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
    static BlockInfoVec* prepare_blocks(Bundle* bundle, const LinkRef& link);
    
    /**
     * Generate contents for the given BlockInfoVec on the given Link.
     *
     * @return the total length of the formatted blocks for this bundle.
     */
    static size_t generate_blocks(Bundle*        bundle,
                                  BlockInfoVec*  blocks,
                                  const LinkRef& link);

    static void delete_blocks(Bundle* bundle, const LinkRef& link);

    /**
     * Return the total length of the formatted bundle block data.
     */
    static size_t total_length(const BlockInfoVec* blocks);
    
    /**
     * Temporary helper function to find the offset of the first byte
     * of the payload in a block list.
     */
    static size_t payload_offset(const BlockInfoVec* blocks);
    
    /**
     * Copies out a chunk of formatted bundle data at a specified
     * offset from the provided BlockList.
     *
     * @return the length of the chunk produced (up to the supplied
     * length) and sets *last to true if the bundle is complete.
     */
    static size_t produce(const Bundle* bundle, const BlockInfoVec* blocks,
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
    static int64_t consume(Bundle* bundle, u_char* data, size_t len, bool* last);

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
        REASON_SECURITY_FAILED            = 0x09,
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
     * Store a DTN timestamp into a 64-bit value suitable for
     * transmission over the network.
     */
    static int set_timestamp(u_char* bp, size_t len, const BundleTimestamp& tv);

    /**
     * Retrieve a DTN timestamp from a 64-bit value that was
     * transmitted over the network. This does not require the
     * timestamp to be word-aligned.
     */
    static u_int64_t get_timestamp(BundleTimestamp* tv, const u_char* bp, size_t len);

    /**
     * Return the length required to encode the timestamp as two SDNVs
     */
    static size_t ts_encoding_len(const BundleTimestamp& tv);

    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x06;
    
    static const unsigned PREAMBLE_FIXED_LENGTH = 1;

    /**
     * Valid type codes for bundle blocks.
     * (See http://www.dtnrg.org/wiki/AssignedNamesAndNumbers)
     */
    typedef enum {
        PRIMARY_BLOCK               = 0x000, ///< INTERNAL ONLY -- NOT IN SPEC
        PAYLOAD_BLOCK               = 0x001, ///< Defined in RFC5050
        BUNDLE_AUTHENTICATION_BLOCK = 0x002, ///< Defined in RFC6257
        PAYLOAD_SECURITY_BLOCK      = 0x003, ///< Defined in RFC6257
        CONFIDENTIALITY_BLOCK       = 0x004, ///< Defined in RFC6257
        PREVIOUS_HOP_BLOCK          = 0x005, ///< Defined in RFC6259
        METADATA_BLOCK              = 0x008, ///< Defined in RFC6258
        EXTENSION_SECURITY_BLOCK    = 0x009, /// Defined in RFC6257

        // XXX/dz In order to be compatible with the ION implementation
        // the CTEB must be 0x00a and it is believed that it will be
        // officially assigned. As a result, I had to move the 
        // Age Block to 0x00d - sorry for any inconvenience!!
        // XXX/dz not conditionally compiling values in this header so 
        // that they will be placeholders even if not enabled.
        CUSTODY_TRANSFER_ENHANCEMENT_BLOCK    = 0x00a,  ///< draft-jenkins-custody-transfer-enhancement-block-01

        QUERY_EXTENSION_BLOCK       = 0x00b, ///< draft-farrell-dtnrg-bpq-00
        SESSION_BLOCK               = 0x00c, ///< NOT IN SPEC YET
        AGE_BLOCK                   = 0x00d, ///< draft-irtf-dtnrg-bundle-age-block-01
        SEQUENCE_ID_BLOCK           = 0x010, ///< NOT IN SPEC YET
        OBSOLETES_ID_BLOCK          = 0x011, ///< NOT IN SPEC YET

        // XXX/dz Adding for compatibility with ION ACS in use by 
        // University of Colorado at Boulder and the CGBA experiments
        // NOT IN SPEC YET
        EXTENDED_CLASS_OF_SERVICE_BLOCK        = 0x013,  ///< draft-irtf-dtnrg-ecos-02

        API_EXTENSION_BLOCK         = 0x100, ///< INTERNAL ONLY -- NOT IN SPEC
        UNKNOWN_BLOCK               = 0x101, ///< INTERNAL ONLY -- NOT IN SPEC
    } bundle_block_type_t;

    /**
     * Values for block processing flags that appear in all blocks
     * except the primary block.
     */
    typedef enum {
        BLOCK_FLAG_REPLICATE               = 1 << 0,
        BLOCK_FLAG_REPORT_ONERROR          = 1 << 1,
        BLOCK_FLAG_DISCARD_BUNDLE_ONERROR  = 1 << 2,
        BLOCK_FLAG_LAST_BLOCK              = 1 << 3,
        BLOCK_FLAG_DISCARD_BLOCK_ONERROR   = 1 << 4,
        BLOCK_FLAG_FORWARDED_UNPROCESSED   = 1 << 5,
        BLOCK_FLAG_EID_REFS                = 1 << 6
    } block_flag_t;

    /**
     * The basic block preamble that's common to all blocks
     * (including the payload block but not the primary block).
     */
    struct BlockPreamble {
        u_int8_t type;
        u_int8_t flags;
        u_char   length[0]; // SDNV
    } __attribute__((packed));

    /**
     * Administrative Record Type Codes
     */
    typedef enum {
        ADMIN_STATUS_REPORT            = 0x01,
        ADMIN_CUSTODY_SIGNAL           = 0x02,
        ADMIN_AGGREGATE_CUSTODY_SIGNAL = 0x04,
        ADMIN_ANNOUNCE                 = 0x05,   // NOT IN BUNDLE SPEC
    } admin_record_type_t;

    /**
     * Administrative Record Flags.
     */
    typedef enum {
        ADMIN_IS_FRAGMENT       = 0x01,
    } admin_record_flags_t;

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

    /**
     * Assuming the given bundle is an administrative bundle, extract
     * the admin bundle type code from the bundle's payload.
     *
     * @return true if successful
     */
    static bool get_admin_type(const Bundle* bundle,
                               admin_record_type_t* type);

    struct Params {
        Params();
        bool age_outbound_enabled_;
        bool age_inbound_processing_;
        bool age_zero_creation_ts_time_;
    };

    static Params params_;

private:
    /**
     * Array of registered BlockProcessor handlers -- fixed size since
     * there can be at most one handler per protocol type
     */
    static BlockProcessor* processors_[256];

};

} // namespace dtn

#endif /* _BUNDLE_PROTOCOL_H_ */
