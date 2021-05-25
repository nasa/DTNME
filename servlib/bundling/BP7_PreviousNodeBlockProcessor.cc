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

#include "BP7_PreviousNodeBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"


//#include <oasys/util/HexDumpBuffer.h>


namespace dtn {

//----------------------------------------------------------------------
BP7_PreviousNodeBlockProcessor::BP7_PreviousNodeBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::PREVIOUS_NODE_BLOCK)
{
    log_path_ = "/dtn/bp7/prevnodeblock";
}


//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_PreviousNodeBlockProcessor::decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle)
{
    CborParser parser;
    CborValue cvElement;
    CborError err;

    std::string fld_name = "BP7_PreviousNodeBlockProcessor::decode_block_data";
    err = cbor_parser_init(data_buf, data_len, 0, &parser, &cvElement);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("Previous Node EID");
    EndpointID eid;
    int64_t status = decode_eid(cvElement, eid, fld_name);
    CHECK_BP7_STATUS_RETURN

    bundle->mutable_prevhop()->assign(eid);
        //log_always_p(log_path(), "%s = %s", fld_name.c_str(), eid.c_str());

    return BP_SUCCESS;
    
}

//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_PreviousNodeBlockProcessor::decode_cbor(BlockInfo* block, uint8_t*  buf, size_t buflen)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    std::string fld_name = "BP7_PreviousNodeBlockProcessor::decode_cbor";

    err = cbor_parser_init(buf, buflen, 0, &parser, &cvBlockArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("CBOR Block Array");
    uint64_t block_elements;
    int status = validate_cbor_fixed_array_length(cvBlockArray, 5, 6, block_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvBlockElements);
    CHECK_CBOR_DECODE_ERR_RETURN


    // bundle protocol version field
    SET_FLDNAMES("Block Type");
    uint64_t block_type = 0;
    status = decode_uint(cvBlockElements, block_type, fld_name);
    CHECK_BP7_STATUS_RETURN


    // bundle protocol version field
    SET_FLDNAMES("Block Number");
    uint64_t block_number = 0;
    status = decode_uint(cvBlockElements, block_number, fld_name);
    CHECK_BP7_STATUS_RETURN

    block->set_block_number(block_number);


    // block proessing control flags
    SET_FLDNAMES("Processing Control Flags");
    uint64_t block_processing_flags = 0;
    status = decode_uint(cvBlockElements, block_processing_flags, fld_name);
    CHECK_BP7_STATUS_RETURN
    block->set_flag(block_processing_flags);


    // CRC Type
    SET_FLDNAMES("CRC Type");
    uint64_t crc_type = 0;
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
    uint64_t data_len = 0;
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
    //log_always_p(log_path(), "Unknown Block - consumed %" PRIi64 " bytes; data_len = %zu", consumed, data_len);

    return consumed;
}


//----------------------------------------------------------------------
ssize_t
BP7_PreviousNodeBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    (void) bundle;

    ssize_t consumed = 0;

            //log_always_p(log_path(), "consume - first CBOR charater: %02x", buf[0]);

    ASSERT(! block->complete());

    int64_t block_len;
    size_t prev_consumed = block->contents().len();

    if (prev_consumed == 0)
    {
        // try to decode the block from the received data
        block_len = decode_cbor(block, (uint8_t*) buf, len);

        if (block_len == BP_FAIL)
        {
            // protocol error - abort
            consumed = BP_FAIL;
        }
        else if (block_len == BP7_UNEXPECTED_EOF)
        {
            // not enogh data received yet
            // - store this chunk in the block contents and wait for more
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(contents->len() + len);
            memcpy(contents->end(), buf, len);
            contents->set_len(contents->len() + len);
            consumed = len;
        }
        else
        {
            // got the entire block in this first chunk of data
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(block_len);
            memcpy(contents->end(), buf, block_len);
            contents->set_len(block_len);

            block->set_complete(true);
            bundle->set_highest_rcvd_block_number(block->block_number());

            consumed = block_len;

            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "consumed %zu CBOR bytes:",
            //                         block->contents().len());
            ////log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

        }
    }
    else
    {
        // combine the previous data with the new data and 
        // try again to decode the block

        // instead of adding all of the new data to the contents
        // using a temp buffer and then only adding the needed
        // bybtes to the contents

        BlockInfo::DataBuffer* contents = block->writable_contents();

        size_t combined_size = prev_consumed + len;
        u_char* temp_buf = (u_char*) malloc(combined_size);

        memcpy(temp_buf, contents->buf(), prev_consumed);
        memcpy(temp_buf + prev_consumed, buf, len);

        // try to decode the block 
        block_len = decode_cbor(block, (uint8_t*) temp_buf, combined_size);

        free(temp_buf);

        if (block_len == BP_FAIL)
        {
            // protocol error - abort
            consumed = BP_FAIL;
        }
        else if (block_len == BP7_UNEXPECTED_EOF)
        {
            // not enogh data received yet
            // - store this chunk in the block contents and wait for more
            contents->reserve(contents->len() + len);
            memcpy(contents->end(), buf, len);
            contents->set_len(contents->len() + len);
            consumed = len;
        }
        else
        {
            // we now have the entire block
            contents->reserve(block_len);
            memcpy(contents->end(), buf, block_len - prev_consumed);
            contents->set_len(block_len);

            block->set_complete(true);
            bundle->set_highest_rcvd_block_number(block->block_number());

            consumed = block_len - prev_consumed;

            //oasys::HexDumpBuffer hex;
            //hex.append(contents->buf(), block_len);

            //log_always_p(log_path(), "consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());
        }

    }

    if (block->complete())
    {
        // decode the EID from the data
        const BlockInfo::DataBuffer& contents = block->contents();
        uint8_t* data_buf = (uint8_t*) (contents.buf() + block->data_offset());

        int64_t status = decode_block_data(data_buf, block->data_length(), bundle);
        if (status != BP_SUCCESS)
        {
            // What to do here???
            log_warn_p(log_path(), "Error parsing Previous Node Block Data - status = %" PRIi64, 
                       status);
        }
    }


            //log_always_p(log_path(), "BP7_UnknwonBlockProcessor::consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_PreviousNodeBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    if (list == BlockInfo::LIST_RECEIVED) {
        // do not pass through the received block - we may add our own later
        // skip base class call and return success
        return BP_SUCCESS;
    }

    if (link == nullptr || !link->params().prevhop_hdr_) {
        return BP_SUCCESS;  // no block to include - not a failure
    }
    
        //log_always_p(log_path(), "prepare - adding to xmit_blocks");

    // this adds a block to the xmit_blocks list
    return BP7_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_PreviousNodeBlockProcessor::encode_block_data(uint8_t* buf, uint64_t buflen, 
                                                  const EndpointID& eidref, 
                                                  const Bundle* bundle, int64_t& encoded_len)
{
    CborEncoder encoder;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    std::string fld_name = "BP7_PreviousNodeBlockProcessor::encode_block_data";

    int64_t status = encode_eid(encoder, eidref, bundle, fld_name);
    CHECK_BP7_STATUS_RETURN

    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    if (0 == need_bytes)
    {
        encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    }

    return need_bytes;
}


//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len
//     >0 = bytes needed to encode
//
int64_t
BP7_PreviousNodeBlockProcessor::encode_cbor(uint8_t* buf, uint64_t buflen, const BlockInfo* block, const Bundle* bundle,
                                            uint8_t* data_buf, int64_t data_len, int64_t& encoded_len)
{
    CborEncoder encoder;
    CborEncoder blockArrayEncoder;

    CborError err;

    encoded_len = 0;

    std::string fld_name = "BP7_PreviousNodeBlockProcessor::encode_cbor";

    cbor_encoder_init(&encoder, buf, buflen, 0);

    //XXX/dz Using CRC type to zero for this block

    // Array sizes:  5 = Canonical Block without CRC
    //               6 = Canonical Block with CRC
    uint8_t array_size = 5;
    uint8_t expected_val = 0x80 | array_size;

    // Create the CBOR array fro the block
    err = cbor_encoder_create_array(&encoder, &blockArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    uint8_t* block_first_cbor_byte = buf;
    if (block_first_cbor_byte != nullptr) {
        if (*block_first_cbor_byte != expected_val)
        {
            log_crit_p(log_path(), "Error - First byte of Previous Node Block CBOR does not equal expected value (%2.2x): %2.2x", 
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
    uint64_t block_flags = block->block_flags();
    err = cbor_encode_uint(&blockArrayEncoder, block_flags);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // CRC type
    uint64_t crc_type = 0;
    err = cbor_encode_uint(&blockArrayEncoder, crc_type);

    // Block data (a CBOR encoded EID)
    err = cbor_encode_byte_string(&blockArrayEncoder, data_buf, data_len);
    CHECK_CBOR_ENCODE_ERROR_RETURN


    // close the Primary Block
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
BP7_PreviousNodeBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) link;
    (void) xmit_blocks;
    (void) last;
    

    // the block data is a byte string which is a CBOR encoded EID
    // - generate the data for the byte string
    BundleDaemon* bd = BundleDaemon::instance();
    EndpointID eid;
    if (bd->params_.announce_ipn_) {
        eid = bd->local_eid_ipn();
    } else {
        eid = bd->local_eid();
    }
    int64_t status;
    int64_t data_len;
    int64_t encoded_len;

    // first run through the encode process with a null buffer to determine the needed size of buffer
    data_len = encode_block_data(nullptr, 0, eid, bundle, encoded_len);
    if (BP_FAIL == data_len)
    {
        log_err_p(log_path(), "Error encoding the Previous Node Block EID in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // non-zero status value is the bytes needed to encode the EID
    BlockInfo::DataBuffer block_data_buf;
    block_data_buf.reserve(data_len);
    block_data_buf.set_len(data_len);

    uint8_t* data_buf = (uint8_t*) block_data_buf.buf();

    status = encode_block_data(data_buf, data_len, eid, bundle, encoded_len);
    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding the Previous Node Block Data in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(data_len == encoded_len);



    // Now encode the block itself with the above as its data

    if (block->source() == nullptr) {
        // Block is being added by this node so set up the default processing flags
        block->set_flag(0);
    }


    int64_t block_len;

    // first run through the encode process with a null buffer to determine the needed size of buffer
    //dzdebug block_len = encode_cbor(nullptr, 0, block, bundle, data_buf, data_len, encoded_len);
    block_len = encode_canonical_block(nullptr, 0, block, bundle, data_buf, data_len, encoded_len);

    if (BP_FAIL == block_len)
    {
        log_err_p(log_path(), "Error encoding Previous Node Block in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // Allocate the memory for the encoding
    block->writable_contents()->reserve(block_len);
    block->writable_contents()->set_len(block_len);
    block->set_data_length(block_len);



    // now do the real encoding
    uint8_t* buf = (uint8_t*) block->writable_contents()->buf();
    
    //dzdebug status = encode_cbor(buf, block_len, block, bundle, data_buf, data_len, encoded_len);
    status = encode_canonical_block(buf, block_len, block, bundle, data_buf, data_len, encoded_len);

    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding Previous Node Block in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(block_len == encoded_len);

        //log_always_p(log_path(), "generate encoded len = %" PRIi64, encoded_len);


            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());


    return BP_SUCCESS;
}


//----------------------------------------------------------------------
int
BP7_PreviousNodeBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("PreviousNode");
}

} // namespace dtn
