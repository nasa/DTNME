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

#ifndef _BUNDLE_PROTOCOL_VERSION_7_H_
#define _BUNDLE_PROTOCOL_VERSION_7_H_

#include <sys/types.h>

#include <cbor.h>

#include "BlockInfo.h"
#include "BundleProtocol.h"
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
class BundleProtocolVersion7 {
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
    static SPtr_BlockInfoVec prepare_blocks(Bundle* bundle, const LinkRef& link);
    
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
    static size_t total_length(Bundle* bundle, const BlockInfoVec* blocks);
    
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
    static ssize_t consume(Bundle* bundle, u_char* data, size_t len, bool* last);

    /**
     * Loop through the bundle's received block list to validate each
     * entry.
     *
     * @return true if the bundle is valid, false if it should be
     * deleted.
     */
    static bool validate(Bundle* bundle,
                         BundleProtocol::status_report_reason_t* reception_reason,
                         BundleProtocol::status_report_reason_t* deletion_reason);

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
    static const int CURRENT_VERSION = 0x07;
    
    static const unsigned PREAMBLE_FIXED_LENGTH = 1;

    /**
     * Valid type codes for bundle blocks.
     * (See http://www.dtnrg.org/wiki/AssignedNamesAndNumbers)
     */
    typedef enum : uint16_t {
        PRIMARY_BLOCK               = 0x0000, ///< Defined in BPv7
        PAYLOAD_BLOCK               = 0x0001, ///< Defined in BPv7
        PREVIOUS_NODE_BLOCK         = 0x0006, ///< Defined in BPv7
        BUNDLE_AGE_BLOCK            = 0x0007, ///< Defined in BPv7
        HOP_COUNT_BLOCK             = 0x000A, ///< Defined in BPv7

        // these two values are here in case they are ever assigned
        // as valid block types - then BlockInfo::type() needs to be updated
        BP7_CBOR_BLOCK_ARRAY_NO_CRC   = 0x0085, ///< NTERNAL ONLY -- NOT IN SPEC
        BP7_CBOR_BLOCK_ARRAY_WITH_CRC = 0x0086, ///< NTERNAL ONLY -- NOT IN ;SPEC

        API_EXTENSION_BLOCK         = 0x0100, ///< INTERNAL ONLY -- NOT IN SPEC
        UNKNOWN_BLOCK               = 0x0101, ///< INTERNAL ONLY -- NOT IN SPEC
    } bundle_block_type_t;

    /**
     * Values for block processing flags that appear in all blocks
     * except the primary block.
     */
    typedef enum : uint8_t {
        BLOCK_FLAG_REPLICATE               = 1 << 0,
        BLOCK_FLAG_REPORT_ONERROR          = 1 << 1,
        BLOCK_FLAG_DISCARD_BUNDLE_ONERROR  = 1 << 2,
        BLOCK_FLAG_RESERVED                = 1 << 3,
        BLOCK_FLAG_DISCARD_BLOCK_ONERROR   = 1 << 4,
        BLOCK_FLAG_RESERVED2               = 1 << 5,
        BLOCK_FLAG_RESERVED3               = 1 << 6
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
    typedef enum : uint8_t {
        ADMIN_STATUS_REPORT            = 0x01,
        ADMIN_BUNDLE_IN_BUNDLE_ENCAP   = 0x03,
        ADMIN_CUSTODY_SIGNAL           = 0x04,
    } admin_record_type_t;

    /**
     * Administrative Record Flags.
     */
    typedef enum : uint8_t {
        ADMIN_IS_FRAGMENT       = 0x01,
    } admin_record_flags_t;

    /**
     * Assuming the given bundle is an administrative bundle, extract
     * the admin bundle type code from the bundle's payload.
     *
     * @return true if successful
     */
    static bool get_admin_type(const Bundle* bundle,
                               admin_record_type_t* type);

    /**
     * Values for Custody Transfer Disposition Code
     */
    typedef enum : uint32_t {
        CUSTODY_TRANSFER_DISPOSITION_ACCEPTED = 0,
        CUSTODY_TRANSFER_DISPOSITION_NO_INFO,
        CUSTODY_TRANSFER_DISPOSITION_REDUNDANT,
        CUSTODY_TRANSFER_DISPOSITION_STORAGE_DEPLETED,
        CUSTODY_TRANSFER_DISPOSITION_DEST_UNINTELLIGIBLE,
        CUSTODY_TRANSFER_DISPOSITION_NO_ROUTE,
        CUSTODY_TRANSFER_DISPOSITION_NO_TIMELY_ROUTE,
        CUSTODY_TRANSFER_DISPOSITION_BLOCK_UNINTELLIGIBLE,
    } custody_transfer_disposition_t;

private:
    /**
     * Array of registered BlockProcessor handlers -- fixed size since
     * there can be at most one handler per protocol type
     */
    static BlockProcessor* processors_[256];

    static int peek_into_cbor_for_block_type(u_char* buf, size_t buflen, uint8_t& block_type);
};

} // namespace dtn

#endif /* _BUNDLE_PROTOCOL_VERSION_7_H_ */
