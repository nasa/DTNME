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

#ifndef _BP7_BLOCKPROCESSOR_H_
#define _BP7_BLOCKPROCESSOR_H_

#include <third_party/oasys/compat/inttypes.h>

#include "Bundle.h"
#include "BundleProtocolVersion7.h"
#include "BundleTimestamp.h"
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "BP_Local.h"
#include "naming/EndpointID.h"

namespace dtn {

class BlockInfo;
class BlockInfoVec;
class Bundle;
class Link;
class OpaqueContext;

#define BP7_UNEXPECTED_EOF (int)(1)


/**
 * Base class for the protocol handling of bundle blocks, including
 * the core primary and payload handling, security, and other
 * extension blocks.
 */
class BP7_BlockProcessor : public BlockProcessor {
public:
    
    /**
     * Constructor that takes the block typecode. Generally, typecodes
     * should be defined in BundleProtocolVersion7::bundle_block_type_t, but
     * the field is defined as an int so that handlers for
     * non-specified blocks can be defined.
     */
    BP7_BlockProcessor(int block_type);
                   
    /**
     * Virtual destructor.
     */
    virtual ~BP7_BlockProcessor();
    
    /**
     * First callback for parsing blocks that is expected to append a
     * chunk of the given data to the given block. When the block is
     * completely received, this should also parse the block into any
     * fields in the bundle class.
     *
     * The base class implementation parses the block preamble fields
     * to find the length of the block and copies the preamble and the
     * data in the block's contents buffer.
     *
     * This and all derived implementations must be able to handle a
     * block that is received in chunks, including cases where the
     * preamble is split into multiple chunks.
     *
     * @return the amount of data consumed or -1 on error
     */
    virtual ssize_t consume(Bundle*    bundle,
                        SPtr_BlockInfo& block,
                        u_char*    buf,
                        size_t     len) override;

    /**
     * Perform any needed action in the case where a block/bundle
     * has been reloaded from store
     */
    virtual int reload_post_process(Bundle*       bundle,
                                    SPtr_BlockInfoVec& block_list,
                                    SPtr_BlockInfo&    block) override;


    /**
     * Validate the block. This is called after all blocks in the
     * bundle have been fully received.
     *
     * @return true if the block passes validation
     */
    virtual bool validate(const Bundle*           bundle,
                          SPtr_BlockInfoVec&           block_list,
                          SPtr_BlockInfo&              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason) override;


    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual int prepare(const Bundle*    bundle,
                        SPtr_BlockInfoVec&    xmit_blocks,
                        const SPtr_BlockInfo& source,
                        const LinkRef&   link,
                        list_owner_t     list) override;
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.  It MUST add any EID references it needs by
     * calling block->add_eid(), then call generate_preamble(), which
     * will add the EIDs to the primary block's dictionary and write
     * their offsets to this block's preamble.
     */
    virtual int generate(const Bundle*  bundle,
                         SPtr_BlockInfoVec&  xmit_blocks,
                         SPtr_BlockInfo&     block,
                         const LinkRef& link,
                         bool           last) = 0;
    
    /**
     * Third callback for transmitting a bundle. This pass should
     * generate any data (such as security signatures) for the block
     * that may depend on other blocks' contents.
     *
     * The base class implementation does nothing. 
     * 
     * We pass xmit_blocks explicitly to indicate that ALL blocks
     * might be changed by finalize, typically by being encrypted.
     * Parameters such as length might also change due to padding
     * and encapsulation.
     */
    virtual int finalize(const Bundle*  bundle, 
                         SPtr_BlockInfoVec&  xmit_blocks, 
                         SPtr_BlockInfo&     block, 
                         const LinkRef& link) override;

    /**
     * Accessor to virtualize read-only processing contents of the
     * block in various ways. This is overloaded by the payload since
     * the contents are not actually stored in the BlockInfo contents_
     * buffer but rather are on-disk.
     *
     * Processing can be anything the calling routine wishes, such as
     * digest of the block, encryption, decryption etc. This routine
     * is permitted to process the data in several calls to the target
     * "func" routine as long as the data is processed in order and
     * exactly once.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void process(process_func*    func,
                         const Bundle*    bundle,
                         const SPtr_BlockInfo& caller_block,
                         const SPtr_BlockInfo& target_block,
                         size_t           offset,            
                         size_t           len,
                         OpaqueContext*   context) override;
    
    /**
     * Similar to process() but for potentially mutating processing
     * functions.
     *
     * The function returns true iff it modified the target_block.
     */
    virtual bool mutate(mutate_func*     func,
                        Bundle*          bundle,
                        const SPtr_BlockInfo& caller_block,
                        SPtr_BlockInfo&       target_block,
                        size_t           offset,
                        size_t           len,
                        OpaqueContext*   context) override;

    /**
     * Accessor to virtualize copying contents out from the block
     * info. This is overloaded by the payload since the contents are
     * not actually stored in the BlockInfo contents_ buffer but
     * rather are on-disk.
     *
     * The base class implementation simply does a memcpy from the
     * contents into the supplied buffer.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void produce(const Bundle*    bundle,
                         const SPtr_BlockInfo& block,
                         u_char*          buf,
                         size_t           offset,
                         size_t           len) override;

    /**
     * General hook to set up a block with the given contents. Used
     * for creating generic extension blocks coming from the API.
     * Can be specialized for specific block types.
     * Also used when testing bundle protocol routines.
     * If the block has a list of EIDs, the list should be attached
     * to the block before calling this routine. The EIDs will be
     * incorporated in the dictionary in the block_list object.
     */
    virtual void init_block(SPtr_BlockInfo&    block,
                            SPtr_BlockInfoVec& block_list,
                            Bundle*		  bundle,
                            u_int8_t      type,
                            u_int8_t      flags,
                            const u_char* bp,
                            size_t        len) override;
    
    /**
     * Return a one-line representation of the block.
     */
    virtual int format(oasys::StringBuffer* buf, BlockInfo *b = nullptr) override;

    static int64_t uint64_encoding_len(uint64_t val);

protected:
    #define SET_FLDNAMES(fld) \
      fld_name = block_name_ + " - " + fld;

    #define CHECK_CBOR_ENCODE_ERROR_RETURN \
        if (err && (err != CborErrorOutOfMemory)) \
        { \
          log_err_p(log_path(), "BP7 CBOR encoding error (bundle ID: %" PRIbid "): %s - %s", \
                    bundle->bundleid(), fld_name.c_str(), cbor_error_string(err)); \
          return BP_FAIL; \
        }

    #define CHECK_CBOR_DECODE_ERR_RETURN \
        if ((err == CborErrorAdvancePastEOF) || (err == CborErrorUnexpectedEOF)) \
        { \
          return BP7_UNEXPECTED_EOF; \
        } \
        else if (err != CborNoError) \
        { \
          log_err_p(log_path(), "BP7 CBOR parsing error: %s -  %s", fld_name.c_str(), cbor_error_string(err)); \
          return BP_FAIL; \
        }

    #define CHECK_BP7_STATUS_RETURN \
        if (status != BP_SUCCESS) \
        { \
          return status; \
        }


    friend class BundleProtocolVersion7;
    friend class BlockInfo;
    friend class Ciphersuite;
    
    /**
     * Consume a block preamble consisting of type, flags(SDNV),
     * EID-list (composite field of SDNVs) and length(SDNV).
     * This method does not apply to the primary block, but is
     * suitable for payload and all extensions.
     */
    virtual ssize_t consume_preamble(SPtr_BlockInfoVec& recv_blocks,
                         SPtr_BlockInfo&    block,
                         u_char*       buf,
                         size_t        len,
                         u_int64_t*    flagp = nullptr) override;
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.
     */
    virtual void generate_preamble(SPtr_BlockInfoVec& xmit_blocks,
                           SPtr_BlockInfo&    block,
                           u_int8_t      type,
                           u_int64_t     flags,
                           u_int64_t     data_length) override;


    //XXX/dz  TODO - give credit and location (stackexchange) where CRC calc routines were gotten
    /**
     * CRC calculations
     * Credit to Mark Adler...
     **/
    static uint32_t crc32c_table[8][256];
    static bool crc_initialized_;
    static void crc32c_init_sw(void);

    virtual uint32_t crc32c_sw(uint32_t crci, const unsigned char* buf, size_t len);
    virtual uint16_t crc16(const unsigned char* data, size_t size, uint16_t crc_in=0);


    //----------------------------------------------------------------------
    // return values:
    //     BP_FAIL = error
    //     BP_SUCCESS = success encoding (actual len returned in encoded_len
    //     >0 = bytes needed to encode
    //
    virtual int64_t encode_canonical_block(uint8_t* buf, uint64_t buflen, 
                                                const SPtr_BlockInfo& block, const Bundle* bundle,
                                                uint8_t* data_buf, int64_t data_len, int64_t& encoded_len);

    /**
     * Calculate and CBOR encode values
     **/
    virtual int encode_eid_dtn_none(CborEncoder& blockArrayEncoder, const Bundle* bundle, std::string& fld_name);
    virtual int encode_eid_dtn(CborEncoder& blockArrayEncoder, const EndpointID& eidref, const Bundle* bundle, std::string& fld_name);
    virtual int encode_eid_ipn(CborEncoder& blockArrayEncoder, const EndpointID& eidref, const Bundle* bundle, std::string& fld_name);
    virtual int encode_eid(CborEncoder& blockArrayEncoder, const EndpointID& eidref, const Bundle* bundle, std::string& fld_name);
    virtual int encode_bundle_timestamp(CborEncoder& blockArrayEncoder, const Bundle* bundle, std::string& fld_name);
    virtual int encode_crc(CborEncoder& blockArrayEncoder, uint32_t crc_type, 
                                uint8_t* block_first_cbor_byte, const Bundle* bundle, std::string& fld_name);
    /**
     * Check the CBOR parser to verify there is len data available to process
     */
    virtual bool  data_available_to_parse(CborValue& cvElement, uint64_t len);


    virtual int validate_cbor_fixed_array_length(CborValue& cvElement, 
                    uint64_t min_len, uint64_t max_len, uint64_t& num_elements, std::string& fld_name);

    //----------------------------------------------------------------------
    // this is a wrapper around cbor_value_get_string_length() which provide an
    // error indication if there is not enough data available to extract the length!
    // <> found it was aborting on cbor_parer_init() when there were not enough bytes
    //    to fully decode the length so that may mitigate the issue for normal processing
    // -- probably needs something similar for array length if we have to worry about
    //    lengths greater than 23
    //
    // returns: 
    //     BP_FAIL (-1) = CBOR or Protocol Error
    //     BP_SUCCESS (0) = success
    //     BP7_UNEXPECTED_EOF (1) = need more data to parse
    virtual int decode_length_of_fixed_string(CborValue& cvElement, uint64_t& ret_val, 
                                              std::string& fld_name, uint64_t& decoded_byes);

    virtual int decode_byte_string(CborValue& cvElement, std::string& ret_val, std::string& fld_name);
    virtual int decode_text_string(CborValue& cvElement, std::string& ret_val, std::string& fld_name);
    virtual int decode_uint(CborValue& cvElement, uint64_t& ret_val, std::string& fld_name);
    virtual int decode_eid(CborValue& cvElement, EndpointID& eid, std::string& fld_name);
    virtual int decode_crc_and_validate(CborValue& cvElement, uint64_t crc_type, 
                                             const uint8_t* block_first_cbor_byte, bool& validated, std::string& fld_name);
    virtual int decode_bundle_timestamp(CborValue& cvElement, BundleTimestamp& timestamp, std::string& fld_name);

};

} // namespace dtn

#endif /* _BP7_BLOCKPROCESSOR_H_ */
