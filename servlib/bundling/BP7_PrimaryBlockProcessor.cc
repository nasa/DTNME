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

#include <string>
#include <sys/types.h>
#include <netinet/in.h>



#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"
#include "BP7_PrimaryBlockProcessor.h"
#include "naming/EndpointID.h"
#include "naming/IPNScheme.h"
#include "naming/DTNScheme.h"
#include "SDNV.h"


//#include <oasys/util/HexDumpBuffer.h>


namespace dtn {


//----------------------------------------------------------------------
BP7_PrimaryBlockProcessor::BP7_PrimaryBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::PRIMARY_BLOCK)
{
    log_path_ = "/dtn/bp7/primaryblock";
}

//----------------------------------------------------------------------
u_int64_t
BP7_PrimaryBlockProcessor::format_bundle_flags(const Bundle* bundle)
{
    u_int64_t flags = 0;

    if (bundle->is_fragment()) {
        flags |= BUNDLE_IS_FRAGMENT;
    }

    if (bundle->is_admin()) {
        flags |= BUNDLE_IS_ADMIN;
    }

    if (bundle->do_not_fragment()) {
        flags |= BUNDLE_DO_NOT_FRAGMENT;
    }

    if (bundle->req_time_in_status_rpt()) {
        flags |= REQ_TIME_IN_STATUS_REPORTS;
    }

    if (bundle->app_acked_rcpt()) {
        flags |= BUNDLE_ACK_BY_APP;
    }
    
    if (bundle->receive_rcpt()) {
        flags |= REQ_REPORT_OF_RECEPTION;
    }

    if (bundle->forward_rcpt()) {
        flags |= REQ_REPORT_OF_FORWARDING;
    }

    if (bundle->delivery_rcpt()) {
        flags |= REQ_REPORT_OF_DELIVERY;
    }

    if (bundle->deletion_rcpt()) {
        flags |= REQ_REPORT_OF_DELETION;
    }

    return flags;
}

//----------------------------------------------------------------------
void
BP7_PrimaryBlockProcessor::parse_bundle_flags(Bundle* bundle, u_int64_t flags)
{
    if (flags & BUNDLE_IS_FRAGMENT) {
        bundle->set_is_fragment(true);
    } else {
        bundle->set_is_fragment(false);
    }

    if (flags & BUNDLE_IS_ADMIN) {
        bundle->set_is_admin(true);
    } else {
        bundle->set_is_admin(false);
    }

    if (flags & BUNDLE_DO_NOT_FRAGMENT) {
        bundle->set_do_not_fragment(true);
    } else {
        bundle->set_do_not_fragment(false);
    }

    if (flags & REQ_TIME_IN_STATUS_REPORTS) {
        bundle->set_req_time_in_status_rpt(true);
    } else {
        bundle->set_req_time_in_status_rpt(false);
    }
    
    if (flags & BUNDLE_ACK_BY_APP) {
        bundle->set_app_acked_rcpt(true);
    } else {
        bundle->set_app_acked_rcpt(false);
    }

    if (flags & REQ_REPORT_OF_RECEPTION) {
        bundle->set_receive_rcpt(true);
    } else {
        bundle->set_receive_rcpt(false);
    }

    if (flags & REQ_REPORT_OF_FORWARDING) {
        bundle->set_forward_rcpt(true);
    } else {
        bundle->set_forward_rcpt(false);
    }

    if (flags & REQ_REPORT_OF_DELIVERY) {
        bundle->set_delivery_rcpt(true);
    } else {
        bundle->set_delivery_rcpt(false);
    }

    if (flags & REQ_REPORT_OF_DELETION) {
        bundle->set_deletion_rcpt(true);
    } else {
        bundle->set_deletion_rcpt(false);
    }

}

//----------------------------------------------------------------------
int
BP7_PrimaryBlockProcessor::prepare(const Bundle*    bundle,
                               SPtr_BlockInfoVec&    xmit_blocks,
                               const SPtr_BlockInfo& source,
                               const LinkRef&   link,
                               list_owner_t     list)
{


    (void)bundle;
    (void)link;
    (void)list;

    // There shouldn't already be anything in the xmit_blocks
    ASSERT(xmit_blocks->size() == 0);

    // make sure to add the primary to the front
    xmit_blocks->append_block(this, source);

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int
BP7_PrimaryBlockProcessor::validate_bundle_array_size(uint64_t block_elements, bool is_fragment, uint64_t crc_type,std::string& fld_name)
{
    if (is_fragment)
    {
        if (0 == crc_type)
        {
            if (10 != block_elements)
            {
                log_err_p(log_path(), "BP7 decode error: %s - must have 10 elements for fragment and no CRC - actual: %" PRIu64,
                          fld_name.c_str(), block_elements);
                return BP_FAIL;
            }
        }
        else
        {
            if (11 != block_elements)
            {
                log_err_p(log_path(), "BP7 decode error: %s - must have 11 elements for fragment with CRC - actual: %" PRIu64,
                          fld_name.c_str(), block_elements);
                return BP_FAIL;
            }
        }
    }
    else
    {
        if (0 == crc_type)
        {
            if (8 != block_elements)
            {
                log_err_p(log_path(), "BP7 decode error: %s - must have 8 elements for non-fragment and no CRC - actual: %" PRIu64,
                          fld_name.c_str(), block_elements);
                return BP_FAIL;
            }
        }
        else
        {
            if (9 != block_elements)
            {
                log_err_p(log_path(), "BP7 decode error: %s - must have 9 elements for non-fragment with CRC - actual: %" PRIu64,
                          fld_name.c_str(), block_elements);
                return BP_FAIL;
            }
        }
    }

   return BP_SUCCESS;
}  


//----------------------------------------------------------------------
// return vals:
//     -1 = protocol or CBOR error
//      0 = not enough data to parse yet
//     >0 = length of data parsed
int64_t
BP7_PrimaryBlockProcessor::decode_cbor(Bundle* bundle, SPtr_BlockInfo& block, uint8_t*  buf, size_t buflen)
{
    CborError err;
    CborParser parser;
    CborValue cvBlockArray;
    CborValue cvBlockElements;

    std::string fld_name = "BP7_PrimaryBlockProcessor::decode_cbor";

    err = cbor_parser_init(buf, buflen, 0, &parser, &cvBlockArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("CBOR Block Array");
    uint64_t block_elements;
    int status = validate_cbor_fixed_array_length(cvBlockArray, 8, 11, block_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvBlockElements);
    CHECK_CBOR_DECODE_ERR_RETURN


    // bundle protocol version field
    SET_FLDNAMES("CBOR Protocol Version");
    uint64_t bp_version = 0;
    status = decode_uint(cvBlockElements, bp_version, fld_name);
    CHECK_BP7_STATUS_RETURN

    if (bp_version == BundleProtocolVersion7::CURRENT_VERSION)
    {
        bundle->set_bp_version(bp_version);
    }
    else
    {
        log_err_p(log_path(), "BP7 decode error: %s -  Invalid protcol version: %" PRIu64, 
                  fld_name.c_str(), bp_version);
        return BP_FAIL;
    }


    // bundle proessing control flags
    SET_FLDNAMES("Processing Control Flags");
    uint64_t bundle_processing_flags = 0;
    status = decode_uint(cvBlockElements, bundle_processing_flags, fld_name);
    CHECK_BP7_STATUS_RETURN
    parse_bundle_flags(bundle, bundle_processing_flags);
    // NOTE that bundle processing flags are not stored as BlockInfo.block_flags_
    block->set_flag(0);

        //log_always_p(log_path(), "decoded %s = %" PRIu64, 
        //          fld_name.c_str(), bundle_processing_flags);

    // CRC Type
    SET_FLDNAMES("CRC Type");
    uint64_t crc_type = 0;
    status = decode_uint(cvBlockElements, crc_type, fld_name);
    CHECK_BP7_STATUS_RETURN

    if (crc_type > 2)
    {
        log_err_p(log_path(), "BP7 decode error: %s -  Invalid CRC type: %" PRIu64, 
                  fld_name.c_str(), crc_type);
        return BP_FAIL;
    }

        //log_always_p(log_path(), "decoded %s = %" PRIu64, 
        //          fld_name.c_str(), crc_type);


    // we now have enough info to verify the number of elements required for the block
    SET_FLDNAMES("CBOR Block Array");
    status = validate_bundle_array_size(block_elements, bundle->is_fragment(), crc_type, fld_name);
    CHECK_BP7_STATUS_RETURN



    SET_FLDNAMES("Destination EID");
    EndpointID eid;
    status = decode_eid(cvBlockElements, eid, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->mutable_dest()->assign(eid);
        //log_always_p(log_path(), "%s = %s", fld_name.c_str(), eid.c_str());

    SET_FLDNAMES("Source EID");
    status = decode_eid(cvBlockElements, eid, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_source(eid);
        //log_always_p(log_path(), "%s = %s", fld_name.c_str(), eid.c_str());

    SET_FLDNAMES("Report-To EID");
    status = decode_eid(cvBlockElements, eid, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->mutable_replyto()->assign(eid);
        //log_always_p(log_path(), "%s = %s", fld_name.c_str(), eid.c_str());

    // no custodian as used in BP6
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());


    SET_FLDNAMES("Creation Timestamp");
    BundleTimestamp timestamp;
    status = decode_bundle_timestamp(cvBlockElements, timestamp, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_creation_ts(timestamp);


    SET_FLDNAMES("Bundle Lifetime");
    uint64_t lifetime = 0;
    status = decode_uint(cvBlockElements, lifetime, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_expiration_millis(lifetime);

    if (bundle->is_fragment())
    {
        uint64_t fragment_offset = 0;
        uint64_t total_adu_length = 0;

        SET_FLDNAMES("Fragment Offset");
        status = decode_uint(cvBlockElements, fragment_offset, fld_name);
        CHECK_BP7_STATUS_RETURN
        bundle->set_frag_offset(fragment_offset);


        SET_FLDNAMES("Total ADU Length");
        status = decode_uint(cvBlockElements, total_adu_length, fld_name);
        CHECK_BP7_STATUS_RETURN
        bundle->set_orig_length(total_adu_length);
    }

    // force the gbofid string to be filled in
    bundle->gbofid_str();


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
    // else there must be a BPSec Block Integrity Block that needs
    // to set the crc_validated flag appropriately

    err = cbor_value_leave_container(&cvBlockArray, &cvBlockElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    if (!cbor_value_at_end(&cvBlockArray))
    {
        log_err_p(log_path(), "BP7 decode error: %s - CBOR array not at end after parsing all fields",
                  fld_name.c_str());
        return BP_FAIL;
    }

    // calculate and return length of data that was parsed
    const uint8_t* ptr_to_next_byte = cbor_value_get_next_byte(&cvBlockArray);
    int64_t consumed = ptr_to_next_byte - buf;

    //dzdebug
    //log_always_p(log_path(), "Primary Block - consumed %" PRIi64 " bytes", consumed);

    return consumed;
}

//----------------------------------------------------------------------
ssize_t
BP7_PrimaryBlockProcessor::consume(Bundle*    bundle,
                                   SPtr_BlockInfo& block,
                                   u_char*    buf,
                                   size_t     len)
{
    ssize_t consumed = 0;

            //log_always_p(log_path(), "BP7_PrimaryBlockProcessor::consume - first CBOR charater: %02x", buf[0]);

    ASSERT(! block->complete());

    int64_t block_len;
    size_t prev_consumed = block->contents().len();

    if (prev_consumed == 0)
    {
        // try to decode the block from the received data
        block_len = decode_cbor(bundle, block, (uint8_t*) buf, len);

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
            block->set_data_offset(block_len);
            block->set_data_length(0);

            consumed = block_len;

            //oasys::HexDumpBuffer hex;
            ////hex.append(block->contents().buf(), block->contents().len());

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

        size_t combined_size = prev_consumed + len;
        u_char* temp_buf = (u_char*) malloc(combined_size);

        memcpy(temp_buf, block->contents().buf(), prev_consumed);
        memcpy(temp_buf+prev_consumed, buf, len);

        // try to decode the block 
        block_len = decode_cbor(bundle, block, (uint8_t*) temp_buf, combined_size);

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
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(contents->len() + len);
            memcpy(contents->end(), buf, len);
            contents->set_len(contents->len() + len);
            consumed = len;
        }
        else
        {
            // we now have the entire block
            BlockInfo::DataBuffer* contents = block->writable_contents();
            contents->reserve(block_len);
            memcpy(contents->end(), buf, block_len - prev_consumed);
            contents->set_len(block_len);

            block->set_complete(true);
            block->set_data_offset(block_len);
            block->set_data_length(0);

            consumed = block_len - prev_consumed;


            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "consumed %zu CBOR bytes:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());


        }

    }

            //log_always_p(log_path(), "BP7_PrimaryBlockProcessor::consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");

    return consumed;
}

//----------------------------------------------------------------------
bool
BP7_PrimaryBlockProcessor::validate(const Bundle*           bundle,
                                SPtr_BlockInfoVec&           block_list,
                                SPtr_BlockInfo&              block,
                                status_report_reason_t* reception_reason,
                                status_report_reason_t* deletion_reason)
{
    (void)block_list;
    (void)block;
    (void)reception_reason;

    if (!block->crc_validated()) {
        log_err_p(log_path(), "block with invalid CRC");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // Make sure all four EIDs are valid.
    bool eids_valid = true;
    eids_valid &= bundle->source().valid();
    eids_valid &= bundle->dest().valid();
    eids_valid &= bundle->replyto().valid();
    
    if (!eids_valid) {
        log_err_p(log_path(), "bad value for one or more EIDs");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
    
    // According to BP section 3.3, there are certain things that a bundle
    // with a null source EID should not try to do. Check for these cases
    // and reject the bundle if any is true.
    if (bundle->source() == EndpointID::NULL_EID()) {
        if (bundle->receipt_requested() || bundle->app_acked_rcpt()) { 
            log_err_p(log_path(),
                      "bundle with null source eid has requested a "
                      "report; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    
        if (!bundle->do_not_fragment()) {
            log_err_p(log_path(),
                      "bundle with null source eid has not set "
                      "'do-not-fragment' flag; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }
    
    // Admin bundles cannot request custody transfer.
    if (bundle->is_admin()) {
        if ( bundle->receive_rcpt() ||
             bundle->custody_rcpt() ||
             bundle->forward_rcpt() ||
             bundle->delivery_rcpt() ||
             bundle->deletion_rcpt() ||
             bundle->app_acked_rcpt() )
        {
            log_err_p(log_path(), "admin bundle has requested a report; reject it");
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
//
int64_t
BP7_PrimaryBlockProcessor::encode_cbor(uint8_t* buf, uint64_t buflen, const Bundle* bundle, int64_t& encoded_len)
{
    CborEncoder encoder;
    CborEncoder blockArrayEncoder;

    CborError err;
    int status;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);


    // Array sizes:  8 = not a fragment and no CRC
    //               9 = not a fragment with a CRC
    //              10 = is a fragment and no CRC
    //              11 = is a fragment with a CRC
    uint8_t array_size = 8;
    if (bundle->is_fragment())
    {
        array_size += 2;
    }
    if (bundle->primary_block_crc_type() > 0)
    {
        array_size += 1;
    }
    uint8_t expected_val = 0x80 | array_size;

    std::string fld_name = "BP7_PrimaryBlockProcessor::encode_cbor";
    // primary block
    err = cbor_encoder_create_array(&encoder, &blockArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    uint8_t* block_first_cbor_byte = buf;
    if (block_first_cbor_byte != nullptr) {
        if (*block_first_cbor_byte != expected_val)
        {
            log_crit_p(log_path(), "Error - First byte of Primary Block CBOR does not equal expected value (%2.2x): %2.2x", 
                     expected_val, *block_first_cbor_byte);
        }
    }

    // version
    err = cbor_encode_uint(&blockArrayEncoder, bundle->bp_version());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // control flags
    uint64_t flags = format_bundle_flags(bundle);
    err = cbor_encode_uint(&blockArrayEncoder, flags);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // CRC Type
    err = cbor_encode_uint(&blockArrayEncoder, bundle->primary_block_crc_type());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // Destination EID
    status = encode_eid(blockArrayEncoder, bundle->dest(), bundle, fld_name);
    CHECK_BP7_STATUS_RETURN

    // Source EID
    status = encode_eid(blockArrayEncoder, bundle->source(), bundle, fld_name);
    CHECK_BP7_STATUS_RETURN

    // Report-to EID: dtn:none
    status = encode_eid(blockArrayEncoder, bundle->replyto(), bundle, fld_name);
    CHECK_BP7_STATUS_RETURN


    // creation timestamp - an array with two elements
    status = encode_bundle_timestamp(blockArrayEncoder, bundle, fld_name);
    CHECK_BP7_STATUS_RETURN

    // lifetime
    err = cbor_encode_uint(&blockArrayEncoder, bundle->expiration_millis());
    CHECK_CBOR_ENCODE_ERROR_RETURN

    if (bundle->is_fragment())
    {
        // fragment offset
        err = cbor_encode_uint(&blockArrayEncoder, bundle->frag_offset());
        CHECK_CBOR_ENCODE_ERROR_RETURN

        // Total Applicatoin Data Unit Length
        err = cbor_encode_uint(&blockArrayEncoder, bundle->orig_length());
        CHECK_CBOR_ENCODE_ERROR_RETURN
    }

    if (bundle->primary_block_crc_type() > 0)
    {
        status = encode_crc(blockArrayEncoder, bundle->primary_block_crc_type(), 
                                 block_first_cbor_byte, bundle, fld_name);
        CHECK_BP7_STATUS_RETURN
    }

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
BP7_PrimaryBlockProcessor::generate(const Bundle*  bundle,
                                SPtr_BlockInfoVec&  xmit_blocks,
                                SPtr_BlockInfo&     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)link;
    (void)xmit_blocks;
    (void) last;

    /*
     * The primary can't be last since there must be a payload block
     * and always regenerating it in case fragmentation occurred locally
     */

    int64_t encoded_len;
    int64_t primary_len;
    int64_t status;


    // first run through the encode process with a null buffer to determine the needed size of buffer
    primary_len = encode_cbor(nullptr, 0, bundle, encoded_len);

    if (-1 == primary_len)
    {
        log_err_p(log_path(), "Error encoding Primary Block: *%p", bundle);
        return BP_FAIL;
    }

    // Allocate the memory for the encoding
    block->writable_contents()->reserve(primary_len);
    block->writable_contents()->set_len(primary_len);
    block->set_data_length(primary_len);

    // now do the real encoding
    uint8_t* buf = (uint8_t*) block->writable_contents()->buf();
    
    status = encode_cbor(buf, primary_len, bundle, encoded_len);

    if (status != BP_SUCCESS)
    {
        log_err_p(log_path(), "Error encoding Primary Block in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(primary_len == encoded_len);

    //dzdebug
    //log_always_p(log_path(), "Bundle: *%p - Primary Block CBOR encoded length = %" PRIi64, bundle, encoded_len);

            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), primary_len);

            //log_always_p(log_path(), "generated %zu CBOR bytes:",
            //                         primary_len);
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
BP7_PrimaryBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Primary");
}

} // namespace dtn
