/*
 *    Copyright 2022 United States Government as represented by NASA
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

#include "BP7_IMCDestinationsBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"
#include "routing/IMCRouter.h"


//#include <oasys/util/HexDumpBuffer.h>


namespace dtn {

//----------------------------------------------------------------------
BP7_IMCDestinationsBlockProcessor::BP7_IMCDestinationsBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::IMC_DESTINATIONS_BLOCK)
{
    log_path_ = "/dtn/bp7/imcdestsblock";
    block_name_ = "BP7_IMCDestinations";
}


//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_IMCDestinationsBlockProcessor::decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle)
{
    CborParser parser;
    CborValue cvDataArray;
    CborValue cvElements;
    CborError err;

    std::string fld_name = "BP7_IMCDestinationsBlockProcessor::decode_block_data";

    err = cbor_parser_init(data_buf, data_len, 0, &parser, &cvDataArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    SET_FLDNAMES("Block Data Contents");
    uint64_t num_elements;
    int64_t status = validate_cbor_fixed_array_length(cvDataArray, 0, UINT64_MAX, num_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvDataArray, &cvElements);
    CHECK_CBOR_DECODE_ERR_RETURN

    uint64_t dest = 0;

    for (uint64_t ix=0; ix<num_elements; ++ix) {
        status = decode_uint(cvElements, dest, fld_name);
        CHECK_BP7_STATUS_RETURN

        bundle->add_imc_orig_dest_node(dest);
    }

    return BP_SUCCESS;
    
}

//----------------------------------------------------------------------
ssize_t
BP7_IMCDestinationsBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    (void) bundle;

    ssize_t consumed = 0;

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
            // not enough data received yet
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
            log_warn_p(log_path(), "Error parsing IMC Destinations Block Data - status = %" PRIi64, 
                       status);
        }
    }

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_IMCDestinationsBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    if (!bundle->dest()->is_imc_scheme()) {
        // nothing to do here
        return BP_SUCCESS;
    } else {
        // adds a new block to the xmit_blocks
        return BP7_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
    }
}

//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_IMCDestinationsBlockProcessor::encode_block_data(uint8_t* buf, uint64_t buflen, 
                                                  std::string& link_name, bool is_home_region,
                                                  const Bundle* bundle, int64_t& encoded_len)
{
    CborEncoder encoder;
    CborEncoder destListEncoder;

    CborError err;

    encoded_len = 0;

    // get the count and map of nodes known to be accessible via this link
    // (or the original link in the case of a redirection to a BIBE CL)
    std::string orig_link_name;
    if (!bundle->get_redirect_orig_link(link_name, orig_link_name)) {
        orig_link_name = link_name;
    }

    size_t array_size = 0;
    auto sptr_map = bundle->imc_dest_map_for_link(orig_link_name);

    if (!sptr_map || bundle->bard_requested_restage()) {
        // no link specific list so use the full list
        //    (restaging link should not return a specific list but 
        //     also checking bundle indication just in case))
        sptr_map = bundle->imc_dest_map();
        if (sptr_map) {
            array_size = bundle->num_imc_nodes_not_handled();
        }
    } else {
        array_size = sptr_map->size();
    }

    // get the count and map of unrouteable nodes to also send via this link
    if (is_home_region) {
        array_size += bundle->num_imc_home_region_unrouteable();
    } else {
        array_size += bundle->num_imc_outer_regions_unrouteable();
    }
    auto sptr_unrouteable_map = bundle->imc_unrouteable_dest_map();

    // do the encoding
    cbor_encoder_init(&encoder, buf, buflen, 0);

    std::string fld_name = "BP7_IMCDestinationsBlockProcessor::encode_block_data";

    err = cbor_encoder_create_array(&encoder, &destListEncoder, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // first encode the known accessible nodes
    if (sptr_map) {
        auto iter = sptr_map->begin();

        while (iter != sptr_map->end()) {
            if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                err = cbor_encode_uint(&destListEncoder, iter->first);
                CHECK_CBOR_ENCODE_ERROR_RETURN
            }

            ++iter;
        }
    }

    // then include and encode the unrouteable nodes if any
    if (sptr_unrouteable_map) {
        auto iter = sptr_unrouteable_map->begin();

        while (iter != sptr_unrouteable_map->end()) {
            if (iter->second == is_home_region) {
                err = cbor_encode_uint(&destListEncoder, iter->first);
                CHECK_CBOR_ENCODE_ERROR_RETURN
            }

            ++iter;
        }
    }

    err = cbor_encoder_close_container(&encoder, &destListEncoder);
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
BP7_IMCDestinationsBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) xmit_blocks;
    (void) last;

    // the block data is a byte string which is a CBOR encoded 
    // array of [IPN] destination node numbers
    // - generate the data for the byte string
    int64_t status;
    int64_t data_len;
    int64_t encoded_len;

    bool is_home_region = false;
    std::string link_name;
    if (link != nullptr) {
        link_name = link->name_str();
        if (link->remote_eid()->is_ipn_scheme()) {
            size_t remote_ipn_num = link->remote_eid()->node_num();
            is_home_region = IMCRouter::imc_config_.is_node_in_home_region(remote_ipn_num);
        }
    }

    // first run through the encode process with a null buffer to determine the needed size of buffer
    data_len = encode_block_data(nullptr, 0, link_name, is_home_region, bundle, encoded_len);
    if (BP_FAIL == data_len)
    {
        log_err_p(log_path(), "Error encoding the IMC Destinations List in 1st pass: *%p", bundle);
        return BP_FAIL;
    }

    // non-zero status value is the bytes needed to encode the EID
    BlockInfo::DataBuffer block_data_buf;
    block_data_buf.reserve(data_len);
    block_data_buf.set_len(data_len);

    uint8_t* data_buf = (uint8_t*) block_data_buf.buf();

    status = encode_block_data(data_buf, data_len, link_name, is_home_region, bundle, encoded_len);
    if (BP_SUCCESS != status)
    {
        log_err_p(log_path(), "Error encoding the IMC Destinations List in 2nd pass: *%p", bundle);
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
    block_len = encode_canonical_block(nullptr, 0, block, bundle, data_buf, data_len, encoded_len);

    if (BP_FAIL == block_len)
    {
        log_err_p(log_path(), "Error encoding IMC Destinations Block in 1st pass: *%p", bundle);
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
        log_err_p(log_path(), "Error encoding IMC Destinations Block in 2nd pass: *%p", bundle);
        return BP_FAIL;
    }

    ASSERT(block_len == encoded_len);

    return BP_SUCCESS;
}


} // namespace dtn
