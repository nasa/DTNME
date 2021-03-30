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

#include "BP7_UnknownBlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"


//#include <oasys/util/HexDumpBuffer.h>

namespace oasys {
    template <> dtn::BP7_UnknownBlockProcessor* oasys::Singleton<dtn::BP7_UnknownBlockProcessor>::instance_ = nullptr;
}

namespace dtn {

//----------------------------------------------------------------------
BP7_UnknownBlockProcessor::BP7_UnknownBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::UNKNOWN_BLOCK)
{
    log_path_ = "/dtn/bp7/unknownblock";
}




//----------------------------------------------------------------------
// return vals:
//     -1 = protocol or CBOR error
//      0 = not enough data to parse yet
//     >0 = length of data parsed
int64_t
BP7_UnknownBlockProcessor::decode_cbor(SPtr_BlockInfo& block, uint8_t*  buf, size_t buflen)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    std::string fld_name = "BP7_UnknownBlockProcessor::decode_cbor";
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
BP7_UnknownBlockProcessor::consume(Bundle*    bundle,
                                   SPtr_BlockInfo& block,
                                   u_char*    buf,
                                   size_t     len)
{
    (void) bundle;

    ssize_t consumed = 0;

            //log_always_p(log_path(), "BP7_PrimaryBlockProcessor::consume - first CBOR charater: %02x", buf[0]);

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
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

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

            //log_always_p(log_path(), "BP7_UnknwonBlockProcessor::consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_UnknownBlockProcessor::prepare(const Bundle*    bundle,
                               SPtr_BlockInfoVec&    xmit_blocks,
                               const SPtr_BlockInfo& source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    ASSERT(source != nullptr);
    ASSERT(source->owner() == this);

    if (source->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
        return BP_SUCCESS;  // Now must return SUCCESS if block is not to be forwarded
    }

    // If we're called for this type then security is not enabled
    // and we should NEVER forward BAB
    // WRONG: non-security aware nodes can forward BAB blocks.  BAB is
    // explicitly hop-by-hop, but the hops are between security aware
    // nodes.
    //if (source->type() == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
    //    return BP_FAIL;
    //}

    return BP7_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}


//----------------------------------------------------------------------
int
BP7_UnknownBlockProcessor::generate(const Bundle*  bundle,
                                SPtr_BlockInfoVec&  xmit_blocks,
                                SPtr_BlockInfo&     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)last;
    
    // This can only be called if there was a corresponding block in
    // the input path
    const SPtr_BlockInfo source = block->source();
    ASSERT(source != nullptr);
    ASSERT(source->owner() == this);

    // We shouldn't be here if the block has the following flags set
    ASSERT((source->block_flags() &
            BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) == 0);

    // A change to BundleProtocol results in outgoing discard taking place here
    if (source->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) {
        return BP_SUCCESS;
    }
    
    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
  
    // just copy the block's data from the source
    *block = *source;


            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes (full_length=%zu) :",
            //                         block->contents().len(), block->full_length());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

    return BP_SUCCESS;
}


//----------------------------------------------------------------------
bool
BP7_UnknownBlockProcessor::validate(const Bundle*           bundle,
                                    SPtr_BlockInfoVec&           block_list,
                                    SPtr_BlockInfo&              block,
                                    status_report_reason_t* reception_reason,
                                    status_report_reason_t* deletion_reason)
{
    // check for generic block errors
    if (!BP7_BlockProcessor::validate(bundle, block_list, block,
                                      reception_reason, deletion_reason)) {
        return false;
    }

    // extension blocks of unknown type are considered to be "unsupported"
    if (block->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_REPORT_ONERROR) {
        *reception_reason = BundleProtocol::REASON_BLOCK_UNSUPPORTED;
    }

    if (block->block_flags() & BundleProtocolVersion7::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) {
        log_warn_p(log_path(), "Discard Bundle on Error: *%p  Block type: 0x%2x", 
                   bundle, block->type());
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNSUPPORTED;
        return false;
    }

    return true;
}


//----------------------------------------------------------------------
int
BP7_UnknownBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Unknown");
}

} // namespace dtn
