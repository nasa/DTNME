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

#include "BP7_BundleAgeBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"


//#include <oasys/util/HexDumpBuffer.h>


namespace dtn {

//----------------------------------------------------------------------
BP7_BundleAgeBlockProcessor::BP7_BundleAgeBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::BUNDLE_AGE_BLOCK)
{
    log_path_ = "/dtn/bp7/bundleageblock";
    block_name_ = "BP7_BundleAge";
}


//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_BundleAgeBlockProcessor::decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle)
{
    CborParser parser;
    CborValue cvElement;
    CborError err;

    std::string fld_name = "BP7_BundleAgeBlockProcessor::decode_block_data";

    err = cbor_parser_init(data_buf, data_len, 0, &parser, &cvElement);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("Bundle Age (millis)");
    uint64_t age_millis = 0;
    int status = decode_uint(cvElement, age_millis, fld_name);
    CHECK_BP7_STATUS_RETURN

    bundle->set_prev_bundle_age_millis(age_millis);

    return BP_SUCCESS;
    
}

//----------------------------------------------------------------------
ssize_t
BP7_BundleAgeBlockProcessor::consume(Bundle*    bundle,
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
        block_len = decode_canonical_block(block, (uint8_t*) buf, len);

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
        block_len = decode_canonical_block(block, (uint8_t*) temp_buf, combined_size);

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
            log_warn_p(log_path(), "Error parsing Bundle Age Block Data - status = %" PRIi64, 
                       status);
        }
    }


            //log_always_p(log_path(), "BP7_BundleAgeBlockProcessor::consume - processed %zu bytes - block complete = %s",
                                     //consumed, block->complete()?"true":"false");

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_BundleAgeBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    if (bundle->has_bundle_age_block()) {
        // this adds a block to the xmit_blocks list
        return BP7_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
    } else {
        // nothing to do here
        return BP_SUCCESS;
    }
}

//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_BundleAgeBlockProcessor::encode_block_data(uint8_t* buf, uint64_t buflen, 
                                               uint64_t age, const Bundle* bundle, int64_t& encoded_len)
{
    CborEncoder encoder;
    CborError err;

    encoded_len = 0;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    std::string fld_name = "BP7_BundleAgeBlockProcessor::encode_block_data";

    err = cbor_encode_uint(&encoder, age);
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
BP7_BundleAgeBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) link;
    (void) xmit_blocks;
    (void) last;
    
        //log_always_p(log_path(), "generate");


    int64_t status;
    int64_t data_len;
    int64_t encoded_len;

    // first run through the encode process with a null buffer to determine the needed size of buffer
    // the bytes needed to encode the age can change between the two passes if we calculate 
    // it on the fly each time so we get the age now and pass it in as a fixed value
    uint64_t age = bundle->current_bundle_age_millis();
    data_len = encode_block_data(nullptr, 0, age, bundle, encoded_len);
    if (BP_FAIL == data_len)
    {
        log_err_p(log_path(), "Error encoding the Bundle Age Data in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // non-zero status value is the bytes needed to encode the EID
    BlockInfo::DataBuffer block_data_buf;
    block_data_buf.reserve(data_len);
    block_data_buf.set_len(data_len);

    uint8_t* data_buf = (uint8_t*) block_data_buf.buf();

    status = encode_block_data(data_buf, data_len, age, bundle, encoded_len);
    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding the Bundle Age Block Data in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(data_len == encoded_len);


        //log_always_p(log_path(), "encoding Bundle Age Block - data len = %" PRIi64, data_len);


    // Now encode the block itself with the above as its data

    if (block->source() == nullptr) {
        // Block is being added by this node so set up the default processing flags
        block->set_flag(BundleProtocolVersion7::BLOCK_FLAG_REPLICATE);
    }

    int64_t block_len;

    // first run through the encode process with a null buffer to determine the needed size of buffer
    block_len = encode_canonical_block(nullptr, 0, block, bundle, data_buf, data_len, encoded_len);

    if (BP_FAIL == block_len)
    {
        log_err_p(log_path(), "Error encoding Bundle Age Block in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // Allocate the memory for the encoding
    block->writable_contents()->reserve(block_len);
    block->writable_contents()->set_len(block_len);
    block->set_data_length(block_len);



    // now do the real encoding
    uint8_t* buf = (uint8_t*) block->writable_contents()->buf();
    
    status = encode_canonical_block(buf, block_len, block, bundle, data_buf, data_len, encoded_len);

    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding Bundle Age Block in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(block_len == encoded_len);

        //log_always_p(log_path(), "generate encoded len = %" PRIi64 "  first byte = %2.2x", 
        //             encoded_len, *buf);


            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());


    return BP_SUCCESS;
}


} // namespace dtn
