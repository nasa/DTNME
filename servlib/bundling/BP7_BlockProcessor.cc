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

#include "BP7_BlockProcessor.h"
#include "BlockInfo.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "naming/DTNScheme.h"
#include "naming/IPNScheme.h"
#include "naming/IMCScheme.h"

namespace dtn {

//----------------------------------------------------------------------
BP7_BlockProcessor::BP7_BlockProcessor(int block_type)
    : BlockProcessor(block_type)
{
    log_path_ = "/dtn/bp7";
    block_name_ = "BP7BlockProcessor";

    set_bp_version(BundleProtocol::BP_VERSION_7);
}

//----------------------------------------------------------------------
BP7_BlockProcessor::~BP7_BlockProcessor()
{
}

//----------------------------------------------------------------------
ssize_t
BP7_BlockProcessor::consume_preamble(BlockInfoVec* recv_blocks,
                                 BlockInfo*    block,
                                 u_char*       buf,
                                 size_t        len,
                                 u_int64_t*    flagp)
{
    (void) recv_blocks;
    (void) block;
    (void) buf;
    (void) len;
    (void) flagp;

    return BP_FAIL;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) bundle;
    (void) link;
    (void) xmit_blocks;
    (void) block;
    (void) last;
    
    // This can only be called if there was a corresponding block in
    // the input path
    const SPtr_BlockInfo source = block->source();
    ASSERT(source != nullptr);
    ASSERT(source->owner() == this);

    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
   
    // default is to copy the contents from the source block
    *block = *block->source();

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
ssize_t
BP7_BlockProcessor::consume(Bundle*    bundle,
                        BlockInfo* block,
                        u_char*    buf,
                        size_t     len)
{
    (void) bundle;
    (void) block;
    (void) buf;
    (void) len;

    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
BP7_BlockProcessor::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    (void) bundle;
    (void) block_list;
    (void) reception_reason;
    (void) deletion_reason;

    if (!block->crc_validated()) {
        log_err_p(log_path(), "block with invalid CRC");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    
    block->set_reloaded(true);
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const SPtr_BlockInfo source,
                        const LinkRef&   link,
                        list_owner_t     list)
{
    (void)bundle;
    (void)link;
    (void)list;
    
    // Received blocks are added to the end of the list (which
    // maintains the order they arrived in) but blocks from any other
    // source are added after the primary block (that is, before the
    // payload and the received blocks). This places them "outside"
    // the original blocks.
    if (list == BlockInfo::LIST_RECEIVED) {
        SPtr_BlockInfo new_block = xmit_blocks->append_block(this, source);
        new_block->set_block_number(source->block_number());
    }
    else {
        BlockInfoVec::iterator iter = xmit_blocks->begin();
        SPtr_BlockInfo blkptr = *iter;
        ASSERT(blkptr->type() == BundleProtocolVersion7::PRIMARY_BLOCK);

        // for BPv7 the Payload Block is always the last block of the bundle
        if (block_type() == BundleProtocolVersion7::PAYLOAD_BLOCK)
        {
            SPtr_BlockInfo new_block = xmit_blocks->append_block(this, source);
            new_block->set_block_number(1);
        }
        else
        {
            SPtr_BlockInfo new_block = std::make_shared<BlockInfo>(this, source);
            new_block->set_block_number(bundle->highest_rcvd_block_number() + xmit_blocks->size());

            xmit_blocks->insert(iter + 1, new_block);
        }
    }
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::finalize(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link)
{
    (void)xmit_blocks;
    (void)link;
        
    if (bundle->is_admin() && block->type() != BundleProtocolVersion7::PRIMARY_BLOCK) {
        ASSERT((block->block_flags() &
                BundleProtocolVersion7::BLOCK_FLAG_REPORT_ONERROR) == 0);
    }
    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
BP7_BlockProcessor::process(process_func*    func,
                        const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        size_t           offset,
                        size_t           len,
                        OpaqueContext*   context)
{
    u_char* buf;
    
    ASSERT(offset < target_block->contents().len());
    ASSERT(target_block->contents().len() >= offset + len);
    
    // convert the offset to a pointer in the target block
    buf = target_block->contents().buf() + offset;
    
    // call the processing function to do the work
    (*func)(bundle, caller_block, target_block, buf, len, context);
}

//----------------------------------------------------------------------
bool
BP7_BlockProcessor::mutate(mutate_func*     func,
                       Bundle*          bundle,
                       const BlockInfo* caller_block,
                       BlockInfo*       target_block,
                       size_t           offset,   
                       size_t           len,
                       OpaqueContext*   context)
{
    u_char* buf;
    
    ASSERT(offset < target_block->contents().len());
    ASSERT(target_block->contents().len() >= offset + len);
    
    // convert the offset to a pointer in the target block
    buf = target_block->contents().buf() + offset;
    
    // call the processing function to do the work
    return (*func)(bundle, caller_block, target_block, buf, len, context);
    
    // if we need to flush changed content back to disk, do it here
}

//----------------------------------------------------------------------
void
BP7_BlockProcessor::produce(const Bundle*    bundle,
                        const BlockInfo* block,
                        u_char*          buf,
                        size_t           offset,
                        size_t           len)
{
    (void)bundle;

    //log_always_p(log_path(), "produce - offset= %zu len= %zu", offset, len);

    ASSERT(offset < block->contents().len());
    ASSERT(block->contents().len() >= offset + len);
    memcpy(buf, block->contents().buf() + offset, len);
}

//----------------------------------------------------------------------
void
BP7_BlockProcessor::generate_preamble(BlockInfoVec* xmit_blocks,
                                  BlockInfo*    block,
                                  u_int8_t      type,
                                  u_int64_t     flags,
                                  u_int64_t     data_length)
{
    (void) xmit_blocks;
    (void) block;
    (void) type;
    (void) flags;
    (void) data_length;
}

//----------------------------------------------------------------------
void
BP7_BlockProcessor::init_block(BlockInfo*    block,
                           BlockInfoVec* block_list,
                           Bundle*       bundle,
                           u_int8_t      type,
                           u_int8_t      flags,
                           const u_char* bp,
                           size_t        len)
{
    (void)bundle; // Not used in generic case
    ASSERT(block->owner() != nullptr);

    block->set_flag(flags);

    generate_preamble(block_list, block, type, flags, len);
    ASSERT(block->data_offset() != 0);
    block->writable_contents()->reserve(block->full_length());
    block->writable_contents()->set_len(block->full_length());
    memcpy(block->writable_contents()->buf() + block->data_offset(),
           bp, len);
}

//----------------------------------------------------------------------
uint16_t
BP7_BlockProcessor::crc16(const unsigned char* data, size_t size, uint16_t crc_in)
{
    // CRC_16/ISO-HDLC checksum
    #define CRC16_ISO_HDLC_POLYNOMIAL 0x8408

    // first call should use crc = 0; subsequent calls use previously returned crc
    uint16_t crc = ~crc_in;

    while (size--) 
    { 
        crc ^= *data++; 
        for (unsigned k = 0; k < 8; k++)
        {
            crc = crc & 0x0001 ? (crc >> 1) ^ CRC16_ISO_HDLC_POLYNOMIAL : crc >> 1; 
        }
    }
    
    return ~crc;   // XOR with 0xffff
}


/***********************************************************************************************************/
/* CRC32C implementation adapted from Mark Adler's source code published at:                               */
/*                                                                                                         */
/* https://stackoverflow.com/questions/17645167/implementing-sse-4-2s-crc32c-in-software/17646775#17646775 */
/*                                                                                                         */
/***********************************************************************************************************/

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Table for a quadword-at-a-time software crc. */
//static pthread_once_t crc32c_once_sw = PTHREAD_ONCE_INIT;
uint32_t BP7_BlockProcessor::crc32c_table[8][256];
bool BP7_BlockProcessor::crc_initialized_ = false;

//----------------------------------------------------------------------
/* Construct table for software CRC-32C calculation. */
void
BP7_BlockProcessor::crc32c_init_sw(void)
{
    uint32_t n, crc, k;

    for (n = 0; n < 256; n++) {
        crc = n;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc32c_table[0][n] = crc;
    }
    for (n = 0; n < 256; n++) {
        crc = crc32c_table[0][n];
        for (k = 1; k < 8; k++) {
            crc = crc32c_table[0][crc & 0xff] ^ (crc >> 8);
            crc32c_table[k][n] = crc;
        }
    }
}


//----------------------------------------------------------------------
/* Table-driven software version as a fall-back.  This is about 15 times slower
   than using the hardware instructions.  This assumes little-endian integers,
   as is the case on Intel processors that the assembler code here is for. */
uint32_t
BP7_BlockProcessor::crc32c_sw(uint32_t crci, const unsigned char* buf, size_t len)
{
    const unsigned char *next = buf;
    size_t crc;
    size_t next_qword;

    //pthread_once(&crc32c_once_sw, crc32c_init_sw);
    if ( !crc_initialized_ ) {
        crc32c_init_sw();
        crc_initialized_ = true;
    }

    crc = crci ^ 0xffffffff;
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    while (len >= 8) {
        memcpy(&next_qword, next, 8);
        crc ^= next_qword;
        crc = crc32c_table[7][crc & 0xff] ^
              crc32c_table[6][(crc >> 8) & 0xff] ^
              crc32c_table[5][(crc >> 16) & 0xff] ^
              crc32c_table[4][(crc >> 24) & 0xff] ^
              crc32c_table[3][(crc >> 32) & 0xff] ^
              crc32c_table[2][(crc >> 40) & 0xff] ^
              crc32c_table[1][(crc >> 48) & 0xff] ^
              crc32c_table[0][crc >> 56];
        next += 8;
        len -= 8;
    }
    while (len) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    return (uint32_t)crc ^ 0xffffffff;
}


//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len
//     >0 = bytes needed to encode
//
int64_t
BP7_BlockProcessor::encode_canonical_block(uint8_t* buf, size_t buflen, 
                                                const BlockInfo* block, const Bundle* bundle,
                                                uint8_t* data_buf, int64_t data_len, int64_t& encoded_len)
{
    std::string fld_name = "BP7_BlockProcessor::encode_canonical_block";
    CborEncoder encoder;
    CborEncoder blockArrayEncoder;

    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    // Array sizes:  5 = Canonical Block without CRC
    //               6 = Canonical Block with CRC
    uint8_t array_size = 5;
    if (block->crc_type() > 0) {
        ++array_size;
    }

    uint8_t expected_val = 0x80 | array_size;

    // Create the CBOR array fro the block
    err = cbor_encoder_create_array(&encoder, &blockArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    uint8_t* block_first_cbor_byte = buf;
    if (block_first_cbor_byte != nullptr) {
        if (*block_first_cbor_byte != expected_val)
        {
            log_crit_p(log_path(), "Error - First byte of Canonical Block CBOR does not equal expected value (%2.2x): %2.2x", 
                     expected_val, *block_first_cbor_byte);
        }
    }

    // block type
    err = cbor_encode_uint(&blockArrayEncoder, block_type());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block number
    err = cbor_encode_uint(&blockArrayEncoder, block->block_number());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block processing control flags
    size_t block_flags = block->block_flags();
    err = cbor_encode_uint(&blockArrayEncoder, block_flags);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // CRC type
    size_t crc_type = block->crc_type();
    err = cbor_encode_uint(&blockArrayEncoder, crc_type);

    // Block data 
    err = cbor_encode_byte_string(&blockArrayEncoder, data_buf, data_len);
    CHECK_CBOR_ENCODE_ERROR_RETURN


    if (crc_type > 0)
    {
        int64_t status = encode_crc(blockArrayEncoder, crc_type, 
                                 block_first_cbor_byte, bundle, fld_name);
        CHECK_BP7_STATUS_RETURN
    }

    
    // close the Block
    err = cbor_encoder_close_container(&encoder, &blockArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN


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
BP7_BlockProcessor::encode_bundle_timestamp(CborEncoder& blockArrayEncoder, const Bundle* bundle, std::string& fld_name)
{
    CborError err;
    CborEncoder timeArrayEncoder;

    // creation timestamp - an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &timeArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // dtn time
    err = cbor_encode_uint(&timeArrayEncoder, bundle->creation_ts().secs_or_millisecs_);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // sequence ID
    err = cbor_encode_uint(&timeArrayEncoder, bundle->creation_ts().seqno_);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &timeArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_crc(CborEncoder& blockArrayEncoder, uint32_t crc_type, 
                                    uint8_t* block_first_cbor_byte, const Bundle* bundle, std::string& fld_name)
{
    static const uint8_t zeroes[4] = {0,0,0,0};

    CborError err;

    if (crc_type == 1) 
    {
        // 16 bit CRC
        if (block_first_cbor_byte == nullptr) 
        {
            // encode the CRC byte string to allow determination of encoding length
            err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }
        else
        {
            if (blockArrayEncoder.end > blockArrayEncoder.data.ptr)
            {
                size_t available_bytes = blockArrayEncoder.end - blockArrayEncoder.data.ptr;
                // need 3 bytes of buffer space available - 1 CBOR byte and 2 data bytes
                if (available_bytes >= 3)
                {
                    // there is enough buffer space to hold the CBOR'd CRC16 value
                    // initialize it to zeros
                    CborEncoder dummyEncoder;
                    memcpy(&dummyEncoder, &blockArrayEncoder, sizeof(CborEncoder));

                    err = cbor_encode_byte_string(&dummyEncoder, zeroes, 2);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    // calculate the CRC on the CBOR data
                    size_t block_cbor_length = dummyEncoder.data.ptr - block_first_cbor_byte;
                    uint16_t crc = crc16(block_first_cbor_byte, block_cbor_length);

                    // convert CRC to network byte order
                    crc = htons(crc);

                    // CBOR encode the CRC
                    err = cbor_encode_byte_string(&blockArrayEncoder, (const uint8_t*) &crc, 2);
                    CHECK_CBOR_ENCODE_ERROR_RETURN
                }
                else
                {
                    // encode the CRC byte string to allow determination of encoding length
                    err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
                    CHECK_CBOR_ENCODE_ERROR_RETURN
                }
            }
            else
            {
                // encode the CRC byte string to allow determination of encoding length
                err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
                CHECK_CBOR_ENCODE_ERROR_RETURN
            }
        }
    }
    else if (crc_type == 2)
    {
        // 32 bit CRC
        if (block_first_cbor_byte == nullptr) 
        {
            // encode the CRC byte string to allow determination of encoding length
            err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }
        else
        {
            if (blockArrayEncoder.end > blockArrayEncoder.data.ptr)
            {
                size_t available_bytes = blockArrayEncoder.end - blockArrayEncoder.data.ptr;
                // need 5 bytes of buffer space available - 1 CBOR byte and 4 data bytes
                if (available_bytes >= 5)
                {
                    // there is enough buffer space to hold the CBOR'd CRC32c value
                    // initialize it to zeros
                    CborEncoder dummyEncoder;
                    memcpy(&dummyEncoder, &blockArrayEncoder, sizeof(CborEncoder));

                    err = cbor_encode_byte_string(&dummyEncoder, zeroes, 4);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    // calculate the CRC on the CBOR data
                    size_t block_cbor_length = dummyEncoder.data.ptr - block_first_cbor_byte;
                    uint32_t crc = 0;
                    crc = crc32c_sw(crc, block_first_cbor_byte, block_cbor_length);


                    // convert CRC to network byte order
                    crc = htonl(crc);

                    // CBOR encode the CRC
                    err = cbor_encode_byte_string(&blockArrayEncoder, (const uint8_t*) &crc, 4);
                    CHECK_CBOR_ENCODE_ERROR_RETURN
                }
                else
                {
                    // encode the CRC byte string to allow determination of encoding length
                    err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
                    CHECK_CBOR_ENCODE_ERROR_RETURN
                }
            }
            else
            {
                // encode the CRC byte string to allow determination of encoding length
                err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
                CHECK_CBOR_ENCODE_ERROR_RETURN
            }
        }
    }
    else if (crc_type != 0)
    {
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_eid_dtn_none(CborEncoder& blockArrayEncoder, const Bundle* bundle, std::string& fld_name)
{
    CborError err;
    CborEncoder eidArrayEncoder;

    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    size_t scheme_code = BP7_SCHEMA_TYPE_DTN;
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // special for dtn:none - 2nd element is the unsigned integer zero
    err = cbor_encode_uint(&eidArrayEncoder, 0);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_eid_dtn(CborEncoder& blockArrayEncoder, const SPtr_EID& sptr_eid,
                                   const Bundle* bundle, std::string& fld_name)
{
    CborError err;
    CborEncoder eidArrayEncoder;

    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    size_t scheme_code = BP7_SCHEMA_TYPE_DTN;
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // text string
    err = cbor_encode_text_string(&eidArrayEncoder, sptr_eid->ssp().c_str(), strlen(sptr_eid->ssp().c_str()));
    CHECK_CBOR_ENCODE_ERROR_RETURN

    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_eid_ipn(CborEncoder& blockArrayEncoder, const SPtr_EID& sptr_eid,
                                   const Bundle* bundle, std::string& fld_name)
{
    CborError err;
    CborEncoder eidArrayEncoder, sspArrayEncoder;

    if (!sptr_eid->valid())
    {
        log_crit_p(log_path(), "error encoding invalid IPN EID: %s", sptr_eid->c_str());
        return BP_FAIL;
    }


    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // scheme code is the first element
    size_t scheme_code = BP7_SCHEMA_TYPE_IPN;
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // for IPN scheme, second element is also an array with 2 elements
    err = cbor_encoder_create_array(&eidArrayEncoder, &sspArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN
    //     node number is first element     
    err = cbor_encode_uint(&sspArrayEncoder, sptr_eid->node_num());
    CHECK_CBOR_ENCODE_ERROR_RETURN
    //     service number is second element     
    err = cbor_encode_uint(&sspArrayEncoder, sptr_eid->service_num());
    CHECK_CBOR_ENCODE_ERROR_RETURN
    err = cbor_encoder_close_container(&eidArrayEncoder, &sspArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the EID array
    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_eid_imc(CborEncoder& blockArrayEncoder, const SPtr_EID& sptr_eid,
                                   const Bundle* bundle, std::string& fld_name)
{
    CborError err;
    CborEncoder eidArrayEncoder, sspArrayEncoder;

    if (!sptr_eid->valid())
    {
        log_crit_p(log_path(), "error encoding invalid IMC EID: %s", sptr_eid->c_str());
        return BP_FAIL;
    }


    // EID is an array with two elements
    err = cbor_encoder_create_array(&blockArrayEncoder, &eidArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // scheme code is the first element
    size_t scheme_code = BP7_SCHEMA_TYPE_IMC;
    err = cbor_encode_uint(&eidArrayEncoder, scheme_code);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // for IMC scheme, second element is also an array with 2 elements
    err = cbor_encoder_create_array(&eidArrayEncoder, &sspArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERROR_RETURN
    //     node number is first element     
    err = cbor_encode_uint(&sspArrayEncoder, sptr_eid->node_num());
    CHECK_CBOR_ENCODE_ERROR_RETURN
    //     service number is second element     
    err = cbor_encode_uint(&sspArrayEncoder, sptr_eid->service_num());
    CHECK_CBOR_ENCODE_ERROR_RETURN
    err = cbor_encoder_close_container(&eidArrayEncoder, &sspArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // close the EID array
    err = cbor_encoder_close_container(&blockArrayEncoder, &eidArrayEncoder);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_BlockProcessor::encode_eid(CborEncoder& blockArrayEncoder, const SPtr_EID sptr_eid, const Bundle* bundle, std::string& fld_name)
{
    if (sptr_eid->is_null_eid())
    {
        return encode_eid_dtn_none(blockArrayEncoder, bundle, fld_name);
    }
    else if (sptr_eid->scheme() == IPNScheme::instance())
    {
        return encode_eid_ipn(blockArrayEncoder, sptr_eid, bundle, fld_name);
    }
    else if (sptr_eid->scheme() == IMCScheme::instance())
    {
        return encode_eid_imc(blockArrayEncoder, sptr_eid, bundle, fld_name);
    }
    else if (sptr_eid->scheme() == DTNScheme::instance())
    {
        return encode_eid_dtn(blockArrayEncoder, sptr_eid, bundle, fld_name);
    }
    else
    {
        log_crit_p(log_path(), "Unrecognized EndpointID Scheme: %s", sptr_eid->c_str());
        return BP_FAIL;
    }
}


//----------------------------------------------------------------------
bool
BP7_BlockProcessor::data_available_to_parse(CborValue& cvElement, size_t len)
{
    return (cvElement.ptr <= (cvElement.parser->end - len));
}



//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_BlockProcessor::decode_canonical_block(BlockInfo* block, uint8_t*  buf, size_t buflen)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    std::string fld_name = "BP7_BlockProcessor::decode_canonical_block";

    err = cbor_parser_init(buf, buflen, 0, &parser, &cvBlockArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("CBOR Block Array");
    size_t block_elements;
    int status = validate_cbor_fixed_array_length(cvBlockArray, 5, 6, block_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvBlockElements);
    CHECK_CBOR_DECODE_ERR_RETURN


    // bundle protocol version field
    SET_FLDNAMES("Block Type");
    size_t block_type = 0;
    status = decode_uint(cvBlockElements, block_type, fld_name);
    CHECK_BP7_STATUS_RETURN


    // bundle protocol version field
    SET_FLDNAMES("Block Number");
    size_t block_number = 0;
    status = decode_uint(cvBlockElements, block_number, fld_name);
    CHECK_BP7_STATUS_RETURN

    block->set_block_number(block_number);


    // block proessing control flags
    SET_FLDNAMES("Processing Control Flags");
    size_t block_processing_flags = 0;
    status = decode_uint(cvBlockElements, block_processing_flags, fld_name);
    CHECK_BP7_STATUS_RETURN
    block->set_flag(block_processing_flags);


    // CRC Type
    SET_FLDNAMES("CRC Type");
    size_t crc_type = 0;
    status = decode_uint(cvBlockElements, crc_type, fld_name);
    CHECK_BP7_STATUS_RETURN
    block->set_crc_type(crc_type);

    if (crc_type > 2)
    {
        log_err_p(log_path(), "BP7 decode error: %s -  Invalid CRC type: %" PRIu64, 
                  fld_name.c_str(), crc_type);
        return BP_FAIL;
    }


    // we now have enough info to verify the number of elements required for the block
    SET_FLDNAMES("CBOR Block Array");
    if (crc_type == 0)
    {
        block->set_crc_validated(true);  // no actual CRC to validate

        if (block_elements != 5)
        {
            log_err_p(log_path(), "BP7 decode error: %s -  must have 5 elements for block with no CRC",
                      fld_name.c_str());
            return BP_FAIL;
        }
    }
    else
    {
        if (block_elements != 6)
        {
            log_err_p(log_path(), "BP7 decode error: %s -  must have 6 elements for block with a CRC",
                      fld_name.c_str());
            return BP_FAIL;
        }
    }

    // determine the block data size 
    SET_FLDNAMES("Block Data");
    if (!data_available_to_parse(cvBlockElements, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }
    
    if (!cbor_value_is_byte_string(&cvBlockElements))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    if (!cbor_value_is_length_known(&cvBlockElements))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected fixed length CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    // try to read the length of the block data
    size_t data_len = 0;
    err = cbor_value_get_string_length(&cvBlockElements, &data_len);
    CHECK_CBOR_DECODE_ERR_RETURN

    // attempt to decode the block data to see if enough data has been accumulated
    std::string block_data;
    status = decode_byte_string(cvBlockElements, block_data, fld_name);
    CHECK_BP7_STATUS_RETURN

    // the entire block data field has been consumed so we can set the offset and length    
    const uint8_t* ptr_to_next_byte = cbor_value_get_next_byte(&cvBlockElements); // points to byte after the data
    size_t data_offset = ptr_to_next_byte - buf - data_len;
    block->set_data_offset(data_offset);
    block->set_data_length(data_len);


    // CRC value field
    size_t crc_offset = ptr_to_next_byte - buf - 1;
    block->set_crc_offset(crc_offset);
    switch (block->crc_type()) {
        case 0: block->set_crc_length(0);
           break;
        case 1: block->set_crc_length(3); // CRC16 = 2 bytes + 1 CBOR header byte
           break;
        case 2: block->set_crc_length(5); // CRC32c = 4 bytes + 1 CBOR header byte
           break;
    }
    if (0 != crc_type)
    {
        std::string crc_string;
        SET_FLDNAMES("CRC Value");
        block->set_crc_checked(true);
        bool validated = false;
        status = decode_crc_and_validate(cvBlockElements, crc_type, buf, validated, fld_name);
        CHECK_BP7_STATUS_RETURN

        block->set_crc_validated(validated);
    }

    err = cbor_value_leave_container(&cvBlockArray, &cvBlockElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    if (!cbor_value_at_end(&cvBlockArray))
    {
        log_err_p(log_path(), "BP7 decode error: %s - CBOR array not at end after parsing all fields",
                  fld_name.c_str());
        return BP_FAIL;
    }


    // calculate and return length of data that was parsed
    ptr_to_next_byte = cbor_value_get_next_byte(&cvBlockArray);
    int64_t consumed = ptr_to_next_byte - buf;

    //dzdebug
    //log_always_p(log_path(), "Block(%s) - consumed %" PRIi64 " bytes; data_len = %zu", consumed, data_len);

    return consumed;
}


//----------------------------------------------------------------------
// returns: 
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_byte_string(CborValue& cvElement, std::string& ret_val, std::string& fld_name)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }


    if (cbor_value_is_byte_string(&cvElement))
    {
        uint8_t* buf = nullptr;
        size_t n = 0;
        CborError err = cbor_value_dup_byte_string(&cvElement, &buf, &n, &cvElement);
        CHECK_CBOR_DECODE_ERR_RETURN

        ret_val.clear();
        ret_val.append((const char*) buf, n);

        free(buf);
    }
    else
    {
        log_err_p(log_path(), "BP7 decode Error: %s - expect CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
// returns: 
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_text_string(CborValue& cvElement, std::string& ret_val, std::string& fld_name)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }


    if (cbor_value_is_text_string(&cvElement))
    {
        char *buf = nullptr;
        size_t n = 0;
        CborError err = cbor_value_dup_text_string(&cvElement, &buf, &n, &cvElement);
        CHECK_CBOR_DECODE_ERR_RETURN

        ret_val.clear();
        ret_val.append(buf, n);

        free(buf);
    }
    else
    {
        log_err_p(log_path(), "BP7 decode Error: %s - expect CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

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
int
BP7_BlockProcessor::decode_length_of_fixed_string(CborValue& cvElement, size_t& ret_val, 
                                                  std::string& fld_name, size_t& decoded_bytes)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }

    if (!cbor_value_is_byte_string(&cvElement) && !cbor_value_is_text_string(&cvElement)) {
        log_err_p(log_path(), "CBOR decode error: %s - expect CBOR byte or text string", fld_name.c_str());
        return BP_FAIL;
    }

    if (!cbor_value_is_length_known(&cvElement)) {
        log_err_p(log_path(), "CBOR decode error: %s - expect CBOR fixed length byte or text string", fld_name.c_str());
        return BP_FAIL;
    }


    // do we have enough data bytes to extract the length?
    int64_t need_bytes = 1;
    uint8_t length_bits = *cvElement.ptr & 0x1f;

    switch (length_bits) {
        case 31:   // undefined length indicator
        case 30:   // reserved
        case 29:   // reserved
        case 28:   // reserved 
            log_crit_p(log_path(), "CBOR decode error: %s - expect CBOR fixed length byte or text string"
                                           " - ?? was not caught by cbor_value_is_length_known() ??", fld_name.c_str());
            return BP_FAIL;

        case 27:
            need_bytes = 9; // header byte + 8 bytes for a 64 bit value
            break;

        case 26:
            need_bytes = 5; // header byte + 4 bytes for a 32 bit value
            break;

        case 25:
            need_bytes = 3; // header byte + 2 bytes for a 16 bit value
            break;

        case 24:
            need_bytes = 2; // header byte + 1 bytes for a 8 bit value
            break;

        default:
            // need_bytes = 1;
            // the bits are the actual length
            ret_val = length_bits;
            break;
    }

    if (need_bytes > 1) {
        if (!data_available_to_parse(cvElement, need_bytes))
        {
            return BP7_UNEXPECTED_EOF;
        }

        CborError err = cbor_value_get_string_length(&cvElement, &ret_val);
        CHECK_CBOR_DECODE_ERR_RETURN
    }

    decoded_bytes = need_bytes;

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
// returns: 
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_uint(CborValue& cvElement, size_t& ret_val, std::string& fld_name)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }

    if (cbor_value_is_unsigned_integer(&cvElement))
    {
        size_t uval = 0;
        CborError err = cbor_value_get_uint64(&cvElement, &uval);
        CHECK_CBOR_DECODE_ERR_RETURN

        err = cbor_value_advance_fixed(&cvElement);
        CHECK_CBOR_DECODE_ERR_RETURN
  
        ret_val = uval;
    }
    else
    {
        log_err_p(log_path(), "BP7 decode Error: %s - must be encoded as an unsigned integer", fld_name.c_str());
        return BP_FAIL;
    }

    return BP_SUCCESS;
    
}

//----------------------------------------------------------------------
// returns: 
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_boolean(CborValue& cvElement, bool& ret_val, std::string& fld_name)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }

    if (cbor_value_is_boolean(&cvElement))
    {
        bool bval = false;
        CborError err = cbor_value_get_boolean(&cvElement, &bval);
        CHECK_CBOR_DECODE_ERR_RETURN

        err = cbor_value_advance_fixed(&cvElement);
        CHECK_CBOR_DECODE_ERR_RETURN
  
        ret_val = bval;
    }
    else
    {
        log_err_p(log_path(), "BP7 decode Error: %s - must be encoded as a boolean", fld_name.c_str());
        return BP_FAIL;
    }

    return BP_SUCCESS;
    
}

//----------------------------------------------------------------------
// returns: 
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::validate_cbor_fixed_array_length(CborValue& cvElement, 
                                      size_t min_len, size_t max_len, size_t& num_elements, std::string& fld_name)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }

    CborType type = cbor_value_get_type(&cvElement);
    if (CborArrayType != type)
    {
        log_err_p(log_path(), "BP7 decode Error: %s - must be encoded as an array - CBOR byte = %2.2x - type: %2.2x expected: %2.2x", 
                              fld_name.c_str(), *cvElement.ptr, type, CborArrayType);
        return BP_FAIL;
    }

    if (! cbor_value_is_length_known(&cvElement))
    {
        log_err_p(log_path(), "BP7 decode error: %s - must be encoded as a fixed length array", fld_name.c_str());
        return BP_FAIL;
    }

    CborError err = cbor_value_get_array_length(&cvElement, &num_elements);
    CHECK_CBOR_DECODE_ERR_RETURN

    if (num_elements < min_len)
    {
        log_err_p(log_path(), "BP7 decode error: %s -  array length less than minimum (%" PRIu64 "): %" PRIu64, 
                  fld_name.c_str(), min_len, num_elements);
        return BP_FAIL;
    }
    else if (num_elements > max_len)
    {
        log_err_p(log_path(), "BP7 decode error: %s -  array length greater than maximum (%" PRIu64 "): %" PRIu64, 
                  fld_name.c_str(), max_len, num_elements);
        return BP_FAIL;
    }

    return BP_SUCCESS; // okay to continue parsing the array
}


//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_eid(CborValue& cvElement, SPtr_EID& sptr_eid, std::string& fld_name)
{
    // default to NULL EID  (dtn:none)
    sptr_eid = BD_MAKE_EID_NULL();

    size_t num_elements;
    int status = validate_cbor_fixed_array_length(cvElement, 2, 2, num_elements, fld_name);
    CHECK_BP7_STATUS_RETURN


    CborValue cvEidElements;
    CborError err = cbor_value_enter_container(&cvElement, &cvEidElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    size_t schema_type = 0;
    size_t node_id_or_group_num = 0;
    size_t service_num = 0;

    // first element is the schema type
    status = decode_uint(cvEidElements, schema_type, fld_name);
    CHECK_BP7_STATUS_RETURN

    if (schema_type == BP7_SCHEMA_TYPE_DTN)
    {
        if (!data_available_to_parse(cvEidElements, 1))
        {
            return BP7_UNEXPECTED_EOF;
        }

        // dtn schema
        //
        // check for special dtn:none encoding
        if (cbor_value_is_unsigned_integer(&cvEidElements))
        {
            status = decode_uint(cvEidElements, node_id_or_group_num, fld_name);
            CHECK_BP7_STATUS_RETURN

            if (0 != node_id_or_group_num)
            {
                log_err_p(log_path(), "BP7 decode error: %s - EID with DTN schema and CBOR unsigned value must be zero for dtn:none", 
                          fld_name.c_str());
                return BP_FAIL;
            }
            else
            {
                //eid is specifically dtn:none
            }
        }
        else if (cbor_value_is_text_string(&cvEidElements))
        {
            std::string ssp;
            status = decode_text_string(cvEidElements, ssp, fld_name);
            CHECK_BP7_STATUS_RETURN
 
            sptr_eid = BD_MAKE_EID_DTN(ssp);
        }
    }
    else if (schema_type == BP7_SCHEMA_TYPE_IPN)
    {
        // ipn schema is a two element array (unsigned integers)
        status = validate_cbor_fixed_array_length(cvEidElements, 2, 2, num_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        CborValue cvSspElements;
        err = cbor_value_enter_container(&cvEidElements, &cvSspElements);
        CHECK_CBOR_DECODE_ERR_RETURN
  
        status = decode_uint(cvSspElements, node_id_or_group_num, fld_name);
        CHECK_BP7_STATUS_RETURN

        status = decode_uint(cvSspElements, service_num, fld_name);
        CHECK_BP7_STATUS_RETURN
  
        err = cbor_value_leave_container(&cvEidElements, &cvSspElements);
        CHECK_CBOR_DECODE_ERR_RETURN

        sptr_eid = BD_MAKE_EID_IPN(node_id_or_group_num, service_num);
    }
    else if (schema_type == BP7_SCHEMA_TYPE_IMC)
    {
        // imc schema is a two element array (unsigned integers)
        status = validate_cbor_fixed_array_length(cvEidElements, 2, 2, num_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        CborValue cvSspElements;
        err = cbor_value_enter_container(&cvEidElements, &cvSspElements);
        CHECK_CBOR_DECODE_ERR_RETURN
  
        status = decode_uint(cvSspElements, node_id_or_group_num, fld_name);
        CHECK_BP7_STATUS_RETURN

        status = decode_uint(cvSspElements, service_num, fld_name);
        CHECK_BP7_STATUS_RETURN
  
        err = cbor_value_leave_container(&cvEidElements, &cvSspElements);
        CHECK_CBOR_DECODE_ERR_RETURN

        sptr_eid = BD_MAKE_EID_IMC(node_id_or_group_num, service_num);
    }
    else
    {
        log_err_p(log_path(), "BP7 decode error: %s - unable to decode EID with unknown scheme type", 
                  fld_name.c_str());
        return BP_FAIL;
    }

    err = cbor_value_leave_container(&cvElement, &cvEidElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    return BP_SUCCESS;
}


//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_crc_and_validate(CborValue& cvElement, size_t crc_type, 
                                                 const uint8_t* block_first_cbor_byte, bool& validated, std::string& fld_name)
{
    validated = false;

    if (!data_available_to_parse(cvElement, 1))
    {
        return BP7_UNEXPECTED_EOF;
    }

    if (!cbor_value_is_byte_string(&cvElement))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    if (!cbor_value_is_length_known(&cvElement))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected fixed length CBOR byte string", fld_name.c_str());
        return BP_FAIL;
    }

    // correct length?
    size_t crc_len;
    CborError err = cbor_value_get_string_length(&cvElement, &crc_len);
    CHECK_CBOR_DECODE_ERR_RETURN

    if ((crc_type == 1) && (crc_len != 2))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected 2 bytes for CRC16 instead of %zu", fld_name.c_str(), crc_len);
        return BP_FAIL;
    }
    if ((crc_type == 2) && (crc_len != 4))
    {
        log_err_p(log_path(), "BP7 decode error: %s - expected 4 bytes for CRC32c instead of %zu", fld_name.c_str(), crc_len);
        return BP_FAIL;
    }


    // pointers to manipulate the CBOR data in order to calculate the CRC 
    const uint8_t* crc_first_cbor_byte = cvElement.ptr;
    const uint8_t* crc_first_cbor_data_byte = crc_first_cbor_byte + 1;

            
    std::string crc_string;
    int status = decode_byte_string(cvElement, crc_string, fld_name);
    CHECK_BP7_STATUS_RETURN

    size_t block_cbor_length = crc_first_cbor_byte - block_first_cbor_byte + crc_string.size() + 1; // +1 for CBOR type and len byte

    if (crc_type == 1)
    {
        uint16_t crc;
        memcpy(&crc, crc_string.c_str(), 2);
        crc = ntohs(crc);

        // zero out the CRC CBOR bytes in order to calculate the CRC on the received data
        memset((void*) crc_first_cbor_data_byte, 0, 2);

        uint16_t calc_crc = crc16(block_first_cbor_byte, block_cbor_length);

        validated = (calc_crc == crc);

        // restore the original CRC
        crc = htons(crc);
        memcpy((void*) crc_first_cbor_data_byte, (void*)&crc, 2);
    }
    else if (crc_type == 2)
    {
        uint32_t crc;
        memcpy(&crc, crc_string.c_str(), 4);
        crc = ntohl(crc);

        // zero out the CRC CBOR bytes in order to calculate the CRC on the received data
        memset((void*) crc_first_cbor_data_byte, 0, 4);

        uint32_t calc_crc = 0;
        calc_crc = crc32c_sw(calc_crc, block_first_cbor_byte, block_cbor_length);

        validated = (calc_crc == crc);

        // restore the original CRC
        crc = htonl(crc);
        memcpy((void*) crc_first_cbor_data_byte, (void*)&crc, 4);
    }
    else
    {
        log_err_p(log_path(), "BP7 decode error: %s - invalid CRC type: %" PRIu64,
                  fld_name.c_str(), crc_type);
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_BlockProcessor::decode_bundle_timestamp(CborValue& cvElement, BundleTimestamp& timestamp, std::string& fld_name)
{
    // default to dtn:none (NULL_EID)
    timestamp.secs_or_millisecs_ = 0;
    timestamp.seqno_ = 0;
    

    size_t num_elements;
    int status = validate_cbor_fixed_array_length(cvElement, 2, 2, num_elements, fld_name);
    CHECK_BP7_STATUS_RETURN


    CborValue cvTsElements;
    CborError err = cbor_value_enter_container(&cvElement, &cvTsElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    // first element of the array is the milliseconds
    status = decode_uint(cvTsElements, timestamp.secs_or_millisecs_, fld_name);
    CHECK_BP7_STATUS_RETURN

    // sedond element of the array is the sequence number
    status = decode_uint(cvTsElements, timestamp.seqno_, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_leave_container(&cvElement, &cvTsElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
BP7_BlockProcessor::uint64_encoding_len(size_t val)
{
    int64_t need_bytes = 0;

    // calculate number of bytes need to encode the CBOR header bytes
    // and set the CBOR Major Type byte
    if (val < 24U) {
        // length encoded in the major type byte
        need_bytes = 1;
    } else if (val <= UINT8_MAX) {
        // major type byte + 1 size byte
        need_bytes = 2;
    } else if (val <= UINT16_MAX) {
        // major type byte + 2 size bytes
        need_bytes = 3;
    } else if (val <= UINT32_MAX) {
        // major type byte + 4 size bytes
        need_bytes = 5;
    } else {
        // major type byte + 8 size bytes
        need_bytes = 9;
    }

    return need_bytes;
}


} // namespace dtn
