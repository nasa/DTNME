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

#include "BP7_PayloadBlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"
#include "PayloadBlockProcessorHelper.h"


#include <third_party/oasys/util/HexDumpBuffer.h>

namespace dtn {

//----------------------------------------------------------------------
BP7_PayloadBlockProcessor::BP7_PayloadBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::PAYLOAD_BLOCK)
{
    log_path_ = "/dtn/bp7/payloadblock";
}



//----------------------------------------------------------------------
// return vals:
//     -1 = protocol or CBOR error
//      0 = not enough data to parse yet
//     >0 = length of data parsed
int64_t
BP7_PayloadBlockProcessor::decode_payload_header(BlockInfo* block, 
                                                      uint8_t*  buf, size_t buflen, size_t& payload_len)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    std::string fld_name = "BP7_PayloadBlockProcessor::decode_payload_header";

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

    if (block_type != BundleProtocolVersion7::PAYLOAD_BLOCK)
    {
        log_err_p(log_path(), "BP7 decode error: %s -  Incorrect Block Type for Payload: %" PRIu64, 
                  fld_name.c_str(), block_type);
        return BP_FAIL;
    }


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
    // block processing flags are ignored for the payload block - they should all be zero
    block->set_flag(0);


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

    // determine the payload size but don't decode it
    SET_FLDNAMES("Payload Data");
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

    // try to read the length of the payload data
    err = cbor_value_get_string_length(&cvBlockElements, &payload_len);
    CHECK_CBOR_DECODE_ERR_RETURN


    // calculate and return length of data that was parsed
    const uint8_t* ptr_to_next_byte = cbor_value_get_next_byte(&cvBlockElements);
    int64_t consumed = ptr_to_next_byte - buf;

    // now need to add in the bytes holding the Payload CBOR type and lengh 
    // which were not actually parsed as far as tinycbor is concerned
    consumed += uint64_encoding_len(payload_len);


    //dzdebug
    //log_always_p(log_path(), "Payload Block - consumed %" PRIi64 " bytes; payload_len = %zu", consumed, payload_len);

    return consumed;
}

//----------------------------------------------------------------------
ssize_t
BP7_PayloadBlockProcessor::consume(Bundle*    bundle,
                               BlockInfo* block,
                               u_char*    buf,
                               size_t     len)
{
    ssize_t consumed = 0;
    int64_t block_hdr_len = 0;
    size_t payload_len = 0;

            //log_always_p(log_path(), "consume - first CBOR charater: %02x", buf[0]);

    if (block->data_offset() == 0)
    {
        size_t prev_consumed = block->contents().len();

        if (prev_consumed == 0)
        {

            //log_always_p(log_path(), "BP7_PayloadBlockProcessor::consume - first CBOR charater: %02x", buf[0]);

            // try to decode the header info from the received data
            block_hdr_len = decode_payload_header(block, (uint8_t*) buf, len, payload_len);

            if (block_hdr_len == BP_FAIL)
            {
                // protocol error - abort
                return BP_FAIL;
            }
            else if (block_hdr_len == BP7_UNEXPECTED_EOF)
            {
                // not enogh data received yet
                // - store this chunk in the block contents and wait for more
                BlockInfo::DataBuffer* contents = block->writable_contents();
                contents->reserve(contents->len() + len);
                memcpy(contents->end(), buf, len);
                contents->set_len(contents->len() + len);
                consumed = len;

                //dzdebug
                //log_always_p(log_path(), "BP7_Payload.consume (Bundle %s) - not enough data (len = %zu) - buf: %2.2x contents: %2.2x",
                //                         bundle->gbofid_str().c_str(), len, buf[0], block->contents().buf()[0]);
            }
            else
            {
                // got the entire header info in this first chunk of data
                BlockInfo::DataBuffer* contents = block->writable_contents();
                contents->reserve(block_hdr_len);
                memcpy(contents->end(), buf, block_hdr_len);
                contents->set_len(block_hdr_len);

                block->set_data_offset(block_hdr_len);
                block->set_data_length(payload_len);

                block->set_crc_offset(block_hdr_len + payload_len);
                switch (block->crc_type()) {
                    case 0: block->set_crc_length(0);
                       break;
                    case 1: block->set_crc_length(3); // CRC16 = 2 bytes + 1 CBOR header byte
                       break;
                    case 2: block->set_crc_length(5); // CRC32c = 4 bytes + 1 CBOR header byte
                       break;
                }

                // Payload block is always last block of bundle 
                // so we will let it consume the extra CBOR break character that must follow it
                block->set_bpv7_EOB_offset(block_hdr_len + payload_len + block->crc_length());

                consumed = block_hdr_len;


                //oasys::HexDumpBuffer hex;
                //hex.append(contents->buf(), contents->len());

                //log_always_p(log_path(), "consume - processed %zu bytes of header",
                //                         consumed);
                //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());
            }
        }
        else
        {
            // combine the previous data with the new data and 
            // try again to decode the header info

            // instead of adding all of the new data to the contents
            // using a temp buffer and then only adding the needed
            // bybtes to the contents

            size_t combined_size = prev_consumed + len;
            u_char* temp_buf = (u_char*) malloc(combined_size);

            memcpy(temp_buf, block->contents().buf(), prev_consumed);
            memcpy(temp_buf + prev_consumed, buf, len);

                //dzdebug
                //log_always_p(log_path(), "BP7_Payload.consume (resuming Bundle %s) - (prev_len = %zu) - temp_buf: %2.2x contents: %2.2x",
                //                         bundle->gbofid_str().c_str(), prev_consumed, temp_buf[0], block->contents().buf()[0]);

            // try to decode the block 
            block_hdr_len = decode_payload_header(block, (uint8_t*) temp_buf, combined_size, payload_len);

            free(temp_buf);

            if (block_hdr_len == BP_FAIL)
            {
                //dzdebug
                //log_always_p(log_path(), "BP7_Payload.consume (Bundle %s) - (prev_len = %zu) - first byte = %2.2x",
                //                         bundle->gbofid_str().c_str(), prev_consumed, temp_buf[0]);
                // protocol error - abort
                return BP_FAIL;
            }
            else if (block_hdr_len == BP7_UNEXPECTED_EOF)
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
                // we now have the entire header info
                BlockInfo::DataBuffer* contents = block->writable_contents();
                contents->reserve(block_hdr_len);
                memcpy(contents->end(), buf, block_hdr_len - prev_consumed);
                contents->set_len(block_hdr_len);

                block->set_data_offset(block_hdr_len);
                block->set_data_length(payload_len);

                block->set_crc_offset(block_hdr_len + payload_len);
                switch (block->crc_type()) {
                    case 0: block->set_crc_length(0);
                       break;
                    case 1: block->set_crc_length(3); // CRC16 = 2 bytes + 1 CBOR header byte
                       break;
                    case 2: block->set_crc_length(5); // CRC32c = 4 bytes + 1 CBOR header byte
                       break;
                }

                // Payload block is always last block of bundle 
                // so we will let it consume the extra CBOR break character that must follow it
                block->set_bpv7_EOB_offset(block_hdr_len + payload_len + block->crc_length());

                consumed = block_hdr_len - prev_consumed;



                //oasys::HexDumpBuffer hex;
                //hex.append(contents->buf(), contents->len());

                //log_always_p(log_path(), "consume - processed %zu bytes of header",
                //                         consumed);
                //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());
            }
        }

        buf += consumed;
        len -= consumed;

        ASSERT(bundle->payload().length() == 0);
    }
   


 
    // If we still don't know the data offset, we must have consumed
    // the whole buffer
    if (block->data_offset() == 0) {
        ASSERT(len == 0);
        return consumed;
    }

    // Special case for the simulator -- if the payload location is
    // NODATA, then we're done.
    // XXX/dz - hopefully the simulator uses CRC type 0
    // How to handle the CBOR break charater terminating the bundle
    if (bundle->payload().location() == BundlePayload::NODATA) {
        block->set_complete(true);
        return consumed;
    }

    // Otherwise, the buffer should always hold just the preamble
    // since we store the rest in the payload file
    ASSERT(block->contents().len() == block->data_offset());

    // bail if there's nothing left to do
    if (len == 0) {
        return consumed;
    }


    size_t payload_bytes_rcvd = bundle->payload().length();

    //dzdebug
    //log_always_p(log_path(), "consume - payload bytes prev consumed: %zu ; expected total: %zu ; new bytes: %zu ; need: %zu",
    //             payload_bytes_rcvd, block->data_length(), len, (block->data_length() - payload_bytes_rcvd + 1));

    if (block->data_length() != payload_bytes_rcvd)
    {
        // accumulate the payload bytes
        ASSERT(block->data_length() > bundle->payload().length());

        size_t remainder = block->data_length() - payload_bytes_rcvd;
        size_t tocopy;

        if (len >= remainder) {
            tocopy = remainder;
        } else {
            tocopy = len;
        }

        bundle->mutable_payload()->set_length(payload_bytes_rcvd + tocopy);
        bundle->mutable_payload()->write_data(buf, payload_bytes_rcvd, tocopy);

        consumed += tocopy;
        buf += tocopy;
        len -= tocopy;

        // update the payload bytes received
        payload_bytes_rcvd      = bundle->payload().length();

        // set the frag_length if appropriate
        if (bundle->is_fragment()) {
            bundle->set_frag_length(payload_bytes_rcvd);
        }
    }
   
    // bail if there's nothing left to do
    if (len == 0) {
        return consumed;
    }

    // accumulate the CRC bytes
    // NOTE - not checking the CRC of the payload block for now
    //        >> current CRC functions require the block to be in contiguous memory
    if (block->crc_bytes_received() != block->crc_length())
    {
        size_t remainder = block->crc_length() - block->crc_bytes_received();
        size_t tocopy;

        if (len >= remainder) {
            tocopy = remainder;
        } else {
            tocopy = len;
        }

        block->add_crc_cbor_bytes(buf, tocopy);

        consumed += tocopy;
        buf += tocopy;
        len -= tocopy;
    }

    // bail if there's nothing left to do
    if (len == 0) {
        return consumed;
    }

    // should be nothing left to do but consume and discard the CBOR break character [end of bundle]
    ASSERT(!block->complete());

    // verify that the next byte in the buff is the CBOR break character
    if (buf[0] != 0xff)
    {
        log_err_p(log_path(), "BP7 error: CBOR break character (0xff) expected after Payload Block - got: %2.2x", buf[0]);
        return BP_FAIL;
    }
    // consume the CBOR break character
    consumed += 1;
    buf += 1;
    len -= 1;

    block->set_complete(true);

            //log_always_p(log_path(), "BP7_PayloadBlockProcessor::consume - Bundle: *%p - payload len = %zu, block data len = %zu",
            //                         bundle, bundle->payload().length(), block->data_length());
    return consumed;
}

//----------------------------------------------------------------------
bool
BP7_PayloadBlockProcessor::validate(const Bundle*           bundle,
                                BlockInfoVec*           block_list,
                                BlockInfo*              block,
                                status_report_reason_t* reception_reason,
                                status_report_reason_t* deletion_reason)
{
    // check for generic block errors
    if (!BP7_BlockProcessor::validate(bundle, block_list, block,
                                  reception_reason, deletion_reason)) {
        return false;
    }

    if (!block->complete()) {
        // We do not need the block to be complete because we may be
        // able to reactively fragment it, but we must have at least
        // the full preamble to do so.
        if (block->data_offset() == 0
            
            // There is not much value in a 0-byte payload fragment so
            // discard those as well.
            || (block->data_length() != 0 &&
                bundle->payload().length() == 0)
            
            // If the bundle should not be fragmented and the payload
            // block is not complete, we must discard the bundle.
            || bundle->do_not_fragment())
        {
            log_err_p(log_path(), "payload incomplete and cannot be fragmented");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
int64_t
BP7_PayloadBlockProcessor::encode_payload_len(uint8_t* buf, uint64_t buflen, uint64_t in_use, size_t payload_len, int64_t& encoded_len)
{
    int64_t need_bytes = 0;


    // calculate number of bytes need to encode the CBOR header bytes
    // and set the CBOR Major Type byte
    uint8_t major_type_byte = 0x40;
    if (payload_len < 24U) {
        // length encoded in the major type byte
        need_bytes = 1;
        major_type_byte |= payload_len & 0xffU;
    } else if (payload_len <= UINT8_MAX) {
        // major type byte + 1 size byte
        need_bytes = 2;
        major_type_byte += 24U;
    } else if (payload_len <= UINT16_MAX) {
        // major type byte + 2 size bytes
        need_bytes = 3;
        major_type_byte += 25U;
    } else if (payload_len <= UINT32_MAX) {
        // major type byte + 4 size bytes
        need_bytes = 5;
        major_type_byte += 26U;
    } else {
        // major type byte + 8 size bytes
        need_bytes = 9;
        major_type_byte += 27U;
    }

    int64_t remaining = need_bytes;

    // adjust need_bytes to be the total needed for the block header
    need_bytes += in_use;

    if (buf != nullptr) {
        // verify buf has enough room for the encoding of the CBOR header bytes
        if ((int64_t)(buflen - in_use) < remaining) {
            return -1;
        } else {
            int bits_to_shift;
            uint64_t offset = in_use;
            buf[offset++] = major_type_byte;
            --remaining;

            // insert size bytes in big-endian order
            while (remaining > 0) {
                bits_to_shift = 8 * (remaining - 1);
                buf[offset++] = (payload_len >> bits_to_shift) & 0xff;
                --remaining;
            }

            encoded_len = need_bytes;

            // encoding successful - return 0 since no additional bytes needed
            need_bytes = 0;
        }
    }

    return need_bytes;
}


//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_PayloadBlockProcessor::encode_cbor(uint8_t* buf, uint64_t buflen, const Bundle* bundle, uint64_t block_num, uint64_t block_flags, int64_t& encoded_len)
{
    CborEncoder encoder;
    CborEncoder blockArrayEncoder;

    CborError err;

    std::string fld_name = "BP7_PayloadBlockProcessor::encode_cbor";

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    //XXX/dz Forcing CRC type to zero - 
    // payload is stored on disk and read in piecemeal
    // would be difficult and slow for really large payloads


    // Array sizes:  5 = Canonical Block without CRC
    //               6 = Canonical Block with CRC
    uint8_t array_size = 5;
    uint8_t expected_val = 0x80 | array_size;

    // Create the CBOR array for the block
    err = cbor_encoder_create_array(&encoder, &blockArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    uint8_t* block_first_cbor_byte = buf;
    if (block_first_cbor_byte != nullptr) {
        if (*block_first_cbor_byte != expected_val)
        {
            log_crit_p(log_path(), "Error - First byte of Payload Block CBOR does not equal expected value (%2.2x): %2.2x", 
                     expected_val, *block_first_cbor_byte);
        }
    }

    // block type
    err = cbor_encode_uint(&blockArrayEncoder, block_type());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block number
    err = cbor_encode_uint(&blockArrayEncoder, block_num);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block processing control flags
    err = cbor_encode_uint(&blockArrayEncoder, block_flags);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // CRC type
    uint64_t crc_type = 0;
    err = cbor_encode_uint(&blockArrayEncoder, crc_type);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // get the number of bytes in use so far
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&blockArrayEncoder);
    if (need_bytes == 0)
    {
        need_bytes = cbor_encoder_get_buffer_size(&blockArrayEncoder, buf);
    }


    // block data -- only encoding the CBOR header bytes 
    // - actual data stays on disk and read in during the "produce" phase
    size_t payload_len = bundle->payload().length();

    return encode_payload_len(buf, buflen, need_bytes, payload_len, encoded_len);
}



//----------------------------------------------------------------------
int
BP7_PayloadBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) link;
    (void) xmit_blocks;
    (void) last;
    
    
        //log_always_p(log_path(), "generate");


    // in the ::generate pass, we just need to set up the preamble,
    // since the payload stays on disk


    //XXX/dz Forcing CRC type to zero - 
    // payload is stored on disk and read in piecemeal
    // would be difficult and slow for really large payloads

    // Payload is always last block in BPv7

    int64_t encoded_len;
    int64_t block_hdr_len;
    int64_t status;

    // Payload Block always has a block_number of 1
    if (block->block_number() != 1)
    {
        block->set_block_number(1);
    }

    if (block->source() == nullptr) {
        // Block is being added by this node so set up the default processing flags
        block->set_flag(0);
    }

    // first run through the encode process with a null buffer to determine the needed size of buffer
    block_hdr_len = encode_cbor(nullptr, 0, bundle, block->block_number(), block->block_flags(), encoded_len);

    if (BP_FAIL == block_hdr_len)
    {
        log_err_p(log_path(), "Error encoding Payload Block in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // Allocate the memory for the encoding
    block->writable_contents()->reserve(block_hdr_len);
    block->writable_contents()->set_len(block_hdr_len);



    // now do the real encoding
    uint8_t* buf = (uint8_t*) block->writable_contents()->buf();
    
    status = encode_cbor(buf, block_hdr_len, bundle, block->block_number(), block->block_flags(), encoded_len);

    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding Payload Block in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(block_hdr_len == encoded_len);

    block->set_data_offset(block_hdr_len);
    block->set_data_length(bundle->payload().length());



            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes of header:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());


    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
BP7_PayloadBlockProcessor::produce(const Bundle*    bundle,
                               const BlockInfo* block,
                               u_char*          buf,
                               size_t           offset,
                               size_t           len)
{
    PayloadBlockProcessorHelper* helper = nullptr;

    // check to see if a working buffer would improve disk I/O performance
    if ((offset == 0) && (block->locals() == nullptr)) {
        if (bundle->payload().length() > len) {
            size_t alloc_len = bundle->payload().length();
            if (alloc_len > 10000000) {
                alloc_len = 10000000;
            }

            helper = new PayloadBlockProcessorHelper(alloc_len);

            // save a reference to the working buffer on the blocks so we can
            // keep using it each time this method is called
            (const_cast<BlockInfo*>(block))->set_locals(helper);
        }
    } else if (block->locals() != nullptr) {
        helper = dynamic_cast<PayloadBlockProcessorHelper*>(block->locals());  // could be nullptr
    }

    // First copy out the specified range of the preamble
    if (offset < block->data_offset()) {
        size_t tocopy = std::min(len, block->data_offset() - offset);
        memcpy(buf, block->contents().buf() + offset, tocopy);
        buf    += tocopy;
        offset += tocopy;
        len    -= tocopy;
    }

    if (len == 0)
        return;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - block->data_offset();

    size_t tocopy = std::min(len, bundle->payload().length() - payload_offset);

    if (helper != nullptr) {
        // already have enough data in the work buf?
        if (helper->work_buf_.fullbytes() >= tocopy) {
            //copy the requested amount of data
            memcpy(buf, helper->work_buf_.start(), tocopy);
            helper->work_buf_.consume(tocopy);

            payload_offset += tocopy;

            if (payload_offset == bundle->payload().length()) {
                // finished reading in the payload so the helper can be deleted
                (const_cast<BlockInfo*>(block))->set_locals(nullptr);
            }
        } else {
            // not enough data so copy what we have and read in some more
            tocopy = helper->work_buf_.fullbytes();
            if (tocopy > 0) {
                memcpy(buf, helper->work_buf_.start(), helper->work_buf_.fullbytes());
                helper->work_buf_.consume(helper->work_buf_.fullbytes());

                buf            += tocopy;
                payload_offset += tocopy;
                offset         += tocopy;
                len            -= tocopy;
            }
            ASSERT(helper->work_buf_.tailbytes() == helper->work_buf_.size());

            // read in buffers until we satisy the len request
            while (len > 0) {
                size_t toread = std::min(helper->work_buf_.tailbytes(), bundle->payload().length() - payload_offset);
                bundle->payload().read_data(payload_offset, toread, (u_char*)helper->work_buf_.start());
                helper->work_buf_.fill(toread);
              
                tocopy = std::min(len, helper->work_buf_.fullbytes());

                // copy the data from the work buffer
                memcpy(buf, helper->work_buf_.start(), tocopy);
                helper->work_buf_.consume(tocopy);

                buf            += tocopy;
                payload_offset += tocopy;
                offset         += tocopy;
                len            -= tocopy;
            }

            if (payload_offset == bundle->payload().length()) {
                // finished reading in the payload so the helper can be deleted
                (const_cast<BlockInfo*>(block))->set_locals(nullptr);
            }
        }
    } else {
        bundle->payload().read_data(payload_offset, tocopy, buf);
    }

    return;
}

//----------------------------------------------------------------------
void
BP7_PayloadBlockProcessor::process(process_func*    func,
                               const Bundle*    bundle,
                               const BlockInfo* caller_block,
                               const BlockInfo* target_block,
                               size_t           offset,            
                               size_t           len,
                               OpaqueContext*   context)
{
    const u_char* buf;
    u_char  work[1024]; // XXX/pl TODO rework buffer usage
                        // and look at type for the payload data
    size_t  len_to_do = 0;
    
    // re-do these appropriately for the payload
    //ASSERT(offset < target_block->contents().len());
    //ASSERT(target_block->contents().len() >= offset + len);
    
    // First work on specified range of the preamble
    if (offset < target_block->data_offset()) {
        len_to_do = std::min(len, target_block->data_offset() - offset);

        // convert the offset to a pointer in the target block
        buf = target_block->contents().buf() + offset;

        // call the processing function to do the work
        (*func)(bundle, caller_block, target_block, buf, len_to_do, context);
        buf    += len_to_do;
        offset += len_to_do;
        len    -= len_to_do;
    }

    if (len == 0)
        return;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - target_block->data_offset();
    size_t  remaining = std::min(len, bundle->payload().length() - payload_offset);

    buf = work;     // use local buffer

    while ( remaining > 0 ) {        
        len_to_do = std::min(remaining, sizeof(work));   
        buf = bundle->payload().read_data(payload_offset, len_to_do, work);
        
        // call the processing function to do the work
        (*func)(bundle, caller_block, target_block, buf, len_to_do, context);
        
        payload_offset  += len_to_do;
        remaining       -= len_to_do;
    }
}

//----------------------------------------------------------------------
bool
BP7_PayloadBlockProcessor::mutate(mutate_func*     func,
                              Bundle*          bundle,
                              const BlockInfo* caller_block,
                              BlockInfo*       target_block,
                              size_t           offset,
                              size_t           len,
                              OpaqueContext*   r)
{
    bool changed = false;
    u_char* buf;
    u_char  work[1024];         // XXX/pl TODO rework buffer usage
    //    and look at type for the payload data
    size_t  len_to_do = 0;
    
    // re-do these appropriately for the payload
    //ASSERT(offset < target_block->contents().len());
    //ASSERT(target_block->contents().len() >= offset + len);
    
    // First work on specified range of the preamble
    if (offset < target_block->data_offset()) {
        len_to_do = std::min(len, target_block->data_offset() - offset);
        // convert the offset to a pointer in the target block
        buf = target_block->contents().buf() + offset;
        // call the processing function to do the work
        changed = (*func)(bundle, caller_block, target_block, buf, len_to_do, r);
        buf    += len_to_do;
        offset += len_to_do;
        len    -= len_to_do;
    }

    if (len == 0)
        return changed;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - target_block->data_offset();
    size_t remaining = std::min(len, bundle->payload().length() - payload_offset);

    buf = work;     // use local buffer

    while ( remaining > 0 ) {        
        len_to_do = std::min(remaining, sizeof(work));   
        bundle->payload().read_data(payload_offset, len_to_do, buf);
        
        // call the processing function to do the work
        bool chunk_changed =
            (*func)(bundle, caller_block, target_block, buf, len_to_do, r);
        
        // if we need to flush changed content back to disk, do it 
        if ( chunk_changed )            
            bundle->mutable_payload()->
                write_data(buf, payload_offset, len_to_do);
        
        changed |= chunk_changed;
                   
        payload_offset  += len_to_do;
        remaining       -= len_to_do;
    }

    return changed;
}

//----------------------------------------------------------------------
int
BP7_PayloadBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Payload");
}

} // namespace dtn
