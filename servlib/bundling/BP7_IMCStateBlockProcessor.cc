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

#include "BP7_IMCStateBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion7.h"


//#include <oasys/util/HexDumpBuffer.h>


namespace dtn {

//----------------------------------------------------------------------
BP7_IMCStateBlockProcessor::BP7_IMCStateBlockProcessor()
    : BP7_BlockProcessor(BundleProtocolVersion7::IMC_STATE_BLOCK)
{
    log_path_ = "/dtn/bp7/imcstate";
    block_name_ = "BP7_IMCState";
}


//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_IMCStateBlockProcessor::decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle)
{
    CborParser parser;
    CborValue cvBlockParser;
    CborValue cvBlockArray;
    CborError err;

    // Received bundle with an IMC State block so this bundle is automatically DTNME compatible
    // (this info could be combined with the IMC Destinationc block but is separate for now
    //  to retain compatibility with the current ION 4.1.1 implementation)
    bundle->set_imc_is_dtnme_node(true);


    // Determine which block format version to encode
    // - Currently supporting 3 formats to minimize the block size on the various bundle types
    // 
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version
    //   block array element 2: array of elements specific to each version

    //  format version 0:
    //        Used on "regular" IMC bundles with a destination other than imc::0.0
    //    block array element 2: array of size 2
    //            v0 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v0 array element 2: array of processeed proxy nodes to prevent circular forwarding
    //
    //  format version 1:
    //        Used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
    //    block array element 2: array of size 4
    //            v1 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v1 array element 2: bool - is IMC sync requested
    //            v1 array element 3: bool - is proxy petition
    //            v1 array element 4: array of nodes that have processed the proxy petition to 
    //                                prevent continuous circular proxies
    //
    //  format version 2:
    //        Used on IMC Briefing bundles 
    //        (these bundles have an IPN administrative destination and setting the imc_briefing
    //         flag on the bundle triggers adding this block to the outgoing serialized bundle)
    //    block array element 2: array of size 3
    //            v2 array element 1: bool - is IMC sync requested
    //            v2 array element 2: bool - is IMC sync response
    //            v2 array element 3: bool - is DTNME IMC Router node
    //

    std::string fld_name = "BP7_IMCState.parser_init";

    err = cbor_parser_init(data_buf, data_len, 0, &parser, &cvBlockParser);
    CHECK_CBOR_DECODE_ERR_RETURN

    fld_name = "BP7_IMCState.block_array";
    size_t num_elements;
    int64_t status = validate_cbor_fixed_array_length(cvBlockParser, 2, 2, num_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockParser, &cvBlockArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    fld_name = "BP7_IMCState.block_array.format_version";
    size_t format_version = 0;
    status = decode_uint(cvBlockArray, format_version, fld_name);
    CHECK_BP7_STATUS_RETURN

    if (format_version == 0) {
        return decode_block_data_format_0(cvBlockArray, bundle);
    } else if (format_version == 1) {
        return decode_block_data_format_1(cvBlockArray, bundle);
    } else if (format_version == 2) {
        return decode_block_data_format_2(cvBlockArray, bundle);
    } else {
        log_err_p(log_path(), 
                  "Error parsing IMC State Block Data - unsupported format version: %" PRIu64,
                  format_version);
        return BP_FAIL;
    }
}

//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_IMCStateBlockProcessor::decode_block_data_format_0(CborValue& cvBlockArray, Bundle* bundle)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version ---- already decoded
    //   block array element 2: array of elements specific to each version

    //  format version 0:
    //        Used on "regular" IMC bundles with a destination other than imc::0.0
    //    block array element 2: array of size 2
    //            v0 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v0 array element 2: array of processeed proxy nodes to prevent circular forwarding

    CborValue cvFormatArray;
    CborValue cvProcessedRegionsArray;
    CborValue cvProcessedNodesArray;
    CborError err;

    std::string fld_name;

    // block array element 2: v0 format array of size 2
    fld_name = "BP7_IMCState.v0.format_array";
    size_t num_fa_elements = 0;
    int64_t status = validate_cbor_fixed_array_length(cvBlockArray, 2, 2, num_fa_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    // v0 format array element 1:
    fld_name = "BP7_IMCState.v0.processed_region_list";
    {
        size_t num_region_elements = 0;
        int64_t status = validate_cbor_fixed_array_length(cvFormatArray, 0, UINT64_MAX, num_region_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        err = cbor_value_enter_container(&cvFormatArray, &cvProcessedRegionsArray);
        CHECK_CBOR_DECODE_ERR_RETURN

        fld_name = "BP7_IMCState.v0.processed_region_list.region";
        size_t region = 0;

        for (size_t ix=0; ix<num_region_elements; ++ix) {
            status = decode_uint(cvProcessedRegionsArray, region, fld_name);
            CHECK_BP7_STATUS_RETURN

            bundle->add_imc_region_processed(region);
        }

        fld_name = "BP7_IMCState.v0.processed_region_list";
        err = cbor_value_leave_container(&cvFormatArray, &cvProcessedRegionsArray);
        CHECK_CBOR_DECODE_ERR_RETURN
    }

    // v0 format array element 2:
    fld_name = "BP7_IMCState.v0.processed_by_node_list";
    {
        size_t num_node_elements = 0;
        status = validate_cbor_fixed_array_length(cvFormatArray, 0, UINT64_MAX, num_node_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        err = cbor_value_enter_container(&cvFormatArray, &cvProcessedNodesArray);
        CHECK_CBOR_DECODE_ERR_RETURN

        fld_name = "BP7_IMCState.v0.processed_by_node_list.node_num";
        size_t node_num = 0;

        for (size_t ix=0; ix<num_node_elements; ++ix) {
            status = decode_uint(cvProcessedNodesArray, node_num, fld_name);
            CHECK_BP7_STATUS_RETURN

            bundle->add_imc_processed_by_node(node_num);
            bundle->imc_dest_node_handled(node_num);
        }

        fld_name = "BP7_IMCState.v0.processed_by_node_list";
        err = cbor_value_leave_container(&cvFormatArray, &cvProcessedNodesArray);
        CHECK_CBOR_DECODE_ERR_RETURN
    }

    // close the format array
    fld_name = "BP7_IMCState.v0.format_array";
    err = cbor_value_leave_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_IMCStateBlockProcessor::decode_block_data_format_1(CborValue& cvBlockArray, Bundle* bundle)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version ---- already decoded
    //   block array element 2: array of elements specific to each version

    //  format version 1:
    //        Used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
    //    block array element 2: array of size 4
    //            v1 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v1 array element 2: bool - is IMC sync requested
    //            v1 array element 3: bool - is proxy petition
    //            v1 array element 4: array of nodes that have processed the proxy petition to 
    //                                prevent continuous circular proxies

    CborValue cvFormatArray;
    CborValue cvProcessedRegionsArray;
    CborValue cvProcessedNodesArray;
    CborError err;

    std::string fld_name;

    // block array element 2: v1 format array of size 4
    fld_name = "BP7_IMCState.v1.format_array";
    size_t num_fa_elements = 0;
    int64_t status = validate_cbor_fixed_array_length(cvBlockArray, 4, 4, num_fa_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    // v1 format array element 1:
    fld_name = "BP7_IMCState.v1.processed_region_list";
    {
        size_t num_region_elements = 0;
        int64_t status = validate_cbor_fixed_array_length(cvFormatArray, 0, UINT64_MAX, num_region_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        err = cbor_value_enter_container(&cvFormatArray, &cvProcessedRegionsArray);
        CHECK_CBOR_DECODE_ERR_RETURN

        fld_name = "BP7_IMCState.v1.processed_region_list.region";
        size_t region = 0;

        for (size_t ix=0; ix<num_region_elements; ++ix) {
            status = decode_uint(cvProcessedRegionsArray, region, fld_name);
            CHECK_BP7_STATUS_RETURN

            bundle->add_imc_region_processed(region);
        }

        fld_name = "BP7_IMCState.v1.processed_region_list";
        err = cbor_value_leave_container(&cvFormatArray, &cvProcessedRegionsArray);
        CHECK_CBOR_DECODE_ERR_RETURN
    }

    // v1 format array element 2:
    fld_name = "BP7_IMCState.v1.imc_sync_requested";
    bool bval = false;
    status = decode_boolean(cvFormatArray, bval, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_imc_sync_request(bval);

    // v1 format array element 3:
    fld_name = "BP7_IMCState.v1.is_proxy";
    bval = false;
    status = decode_boolean(cvFormatArray, bval, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_imc_is_proxy_petition(bval);


    // v1 format array element 4:
    fld_name = "BP7_IMCState.v1.processed_by_node_list";
    {
        size_t num_node_elements = 0;
        status = validate_cbor_fixed_array_length(cvFormatArray, 0, UINT64_MAX, num_node_elements, fld_name);
        CHECK_BP7_STATUS_RETURN

        err = cbor_value_enter_container(&cvFormatArray, &cvProcessedNodesArray);
        CHECK_CBOR_DECODE_ERR_RETURN

        size_t node_num = 0;

        for (size_t ix=0; ix<num_node_elements; ++ix) {
            status = decode_uint(cvProcessedNodesArray, node_num, fld_name);
            CHECK_BP7_STATUS_RETURN

            bundle->add_imc_processed_by_node(node_num);
            bundle->imc_dest_node_handled(node_num);
        }

        err = cbor_value_leave_container(&cvFormatArray, &cvProcessedNodesArray);
        CHECK_CBOR_DECODE_ERR_RETURN
    }


    // close the format array
    fld_name = "BP7_IMCState.v1.format_array";
    err = cbor_value_leave_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    return BP_SUCCESS;
}




//----------------------------------------------------------------------
// return vals:
//     BP_FAIL (-1) = CBOR or Protocol Error
//     BP_SUCCESS (0) = success
//     BP7_UNEXPECTED_EOF (1) = need more data to parse
int64_t
BP7_IMCStateBlockProcessor::decode_block_data_format_2(CborValue& cvBlockArray, Bundle* bundle)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version ---- already decoded
    //   block array element 2: array of elements specific to each version

    //  format version 2:
    //        Used on IMC Briefing bundles 
    //        (these bundles have an IPN administrative destination and setting the imc_briefing
    //         flag on the bundle triggers adding this block to the outgoing serialized bundle)
    //    block array element 2: array of size 3
    //            v2 array element 1: bool - is IMC sync requested
    //            v2 array element 2: bool - is IMC sync response
    //            v2 array element 3: bool - is DTNME IMC Router node
    //


    CborValue cvFormatArray;
    CborError err;

    std::string fld_name;

    // block array element 2: v2 format array of size 3
    fld_name = "BP7_IMCState.v2.format_array";
    size_t num_fa_elements = 0;
    int64_t status = validate_cbor_fixed_array_length(cvBlockArray, 3, 3, num_fa_elements, fld_name);
    CHECK_BP7_STATUS_RETURN

    err = cbor_value_enter_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    // v2 format array element 1:
    fld_name = "BP7_IMCState.v2.imc_sync_requested";
    bool bval = false;
    status = decode_boolean(cvFormatArray, bval, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_imc_sync_request(bval);

    // v2 format array element 2:
    fld_name = "BP7_IMCState.v2.sync_reply";
    bval = false;
    status = decode_boolean(cvFormatArray, bval, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_imc_sync_reply(bval);

    // v2 format array element 3:
    fld_name = "BP7_IMCState.v2.imc_router";
    bval = false;
    status = decode_boolean(cvFormatArray, bval, fld_name);
    CHECK_BP7_STATUS_RETURN
    bundle->set_imc_is_router_node(bval);

    // close the format array
    fld_name = "BP7_IMCState.v2.format_array";
    err = cbor_value_leave_container(&cvBlockArray, &cvFormatArray);
    CHECK_CBOR_DECODE_ERR_RETURN

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
ssize_t
BP7_IMCStateBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    (void) bundle;

    ssize_t consumed = 0;

    ASSERT(!block->complete() );

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
            log_warn_p(log_path(), "Error parsing IMC Destinations Block Data - status = %" PRIi64, 
                       status);
        }
    }


            //log_always_p(log_path(), "BP7_IMCStateBlockProcessor::consume - processed %zu bytes - block complete = %s",
            //                         consumed, block->complete()?"true":"false");

    return consumed;
}


//----------------------------------------------------------------------
int
BP7_IMCStateBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const SPtr_BlockInfo source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    if (!bundle->dest()->is_imc_scheme() && !bundle->imc_briefing()) {
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
BP7_IMCStateBlockProcessor::encode_block_data(uint8_t* buf, uint64_t buflen, 
                                              const Bundle* bundle, int64_t& encoded_len)
{
    // Determine which block format version to encode
    // - Currently supporting 3 formats to minimize the block size on the various bundle types
    // 
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version
    //   block array element 2: array of elements specific to each version

    //  format version 0:
    //        Used on "regular" IMC bundles with a destination other than imc::0.0
    //    block array element 2: array of size 2
    //            v0 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v0 array element 2: array of processeed proxy nodes to prevent circular forwarding
    //
    //  format version 1:
    //        Used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
    //    block array element 2: array of size 4
    //            v1 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v1 array element 2: bool - is IMC sync requested
    //            v1 array element 3: bool - is proxy petition
    //            v1 array element 4: array of nodes that have processed the proxy petition to 
    //                                prevent continuous circular proxies
    //
    //  format version 2:
    //        Used on IMC Briefing bundles 
    //        (these bundles have an IPN administrative destination and setting the imc_briefing
    //         flag on the bundle triggers adding this block to the outgoing serialized bundle)
    //    block array element 2: array of size 3
    //            v2 array element 1: bool - is IMC sync requested
    //            v2 array element 2: bool - is IMC sync response
    //            v2 array element 3: bool - is DTNME IMC Router node
    //

    if (bundle->imc_briefing()) {
        return encode_block_data_format_2(buf, buflen, bundle, encoded_len);
    } else if (bundle->dest()->node_num() == 0) {
        return encode_block_data_format_1(buf, buflen, bundle, encoded_len);
    } else {
        return encode_block_data_format_0(buf, buflen, bundle, encoded_len);
    }
}

//----------------------------------------------------------------------
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_IMCStateBlockProcessor::encode_block_data_format_0(uint8_t* buf, uint64_t buflen, 
                                                       const Bundle* bundle, int64_t& encoded_len)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version
    //   block array element 2: array of elements specific to each version

    //  format version 0:
    //        Used on "regular" IMC bundles with a destination other than imc::0.0
    //    block array element 2: array of size 2
    //            v0 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v0 array element 2: array of processeed proxy nodes to prevent circular forwarding

    encoded_len = 0;

    CborEncoder encoder;
    CborEncoder encoderBlockArray;
    CborEncoder encoderFormatArray;
    CborEncoder encoderRegionList;
    CborEncoder encoderProcessedByNodeList;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    // create the block array of size 2
    size_t array_size = 2;

    std::string fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_create_array(&encoder, &encoderBlockArray, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block array element 1: format version
    fld_name = "BP7_IMCState.v0.format_version";
    size_t format_version = 0;
    err = cbor_encode_uint(&encoderBlockArray, format_version);
    CHECK_CBOR_ENCODE_ERROR_RETURN
    

    // block array element 2: format v0 array of size 2
    {
        fld_name = "BP7_IMCState.v0.format_array";
        array_size = 2;
        err = cbor_encoder_create_array(&encoderBlockArray, &encoderFormatArray, array_size);
        CHECK_CBOR_ENCODE_ERROR_RETURN


        // foramt array element 1: array of processed regions
        {
            fld_name = "BP7_IMCState.v0.processed_regiom_list";
            size_t array_size = 0;
            auto sptr_map = bundle->imc_processed_regions_map();

            if (sptr_map) {
                array_size = sptr_map->size();
            }

            err = cbor_encoder_create_array(&encoderFormatArray, &encoderRegionList, array_size);
            CHECK_CBOR_ENCODE_ERROR_RETURN

            if (sptr_map) {
                fld_name = "BP7_IMCState.v0.processed_region_list.region";
                auto iter = sptr_map->begin();

                while (iter != sptr_map->end()) {
                    err = cbor_encode_uint(&encoderRegionList, iter->first);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    ++iter;
                }
            }

            err = cbor_encoder_close_container(&encoderFormatArray, &encoderRegionList);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }

        // foramt array element 2: array of processed proxy nodes
        {
            size_t array_size = 0;
            auto sptr_map = bundle->imc_processed_by_nodes_map();

            if (sptr_map) {
                array_size = sptr_map->size();
            }

            err = cbor_encoder_create_array(&encoderFormatArray, &encoderProcessedByNodeList, array_size);
            CHECK_CBOR_ENCODE_ERROR_RETURN

            if (sptr_map) {
                auto iter = sptr_map->begin();

                while (iter != sptr_map->end()) {
                    err = cbor_encode_uint(&encoderProcessedByNodeList, iter->first);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    ++iter;
                }
            }

            err = cbor_encoder_close_container(&encoderFormatArray, &encoderProcessedByNodeList);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }

        // close the format array
        fld_name = "BP7_IMCState.v0.format_array";
        err = cbor_encoder_close_container(&encoderBlockArray, &encoderFormatArray);
        CHECK_CBOR_ENCODE_ERROR_RETURN
    }

    // close the block array
    fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_close_container(&encoder, &encoderBlockArray);
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
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_IMCStateBlockProcessor::encode_block_data_format_1(uint8_t* buf, uint64_t buflen, 
                                                       const Bundle* bundle, int64_t& encoded_len)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version
    //   block array element 2: array of elements specific to each version

    //  format version 1:
    //        Used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
    //    block array element 2: array of size 4
    //            v1 array element 1: array of processeed regions to prevent multiple routers 
    //                                in a region from adding dest nodes
    //            v1 array element 2: bool - is IMC sync requested
    //            v1 array element 3: bool - is proxy petition
    //            v1 array element 4: array of nodes that have processed the proxy petition to 
    //                                prevent continuous circular proxies

    encoded_len = 0;

    CborEncoder encoder;
    CborEncoder encoderBlockArray;
    CborEncoder encoderFormatArray;
    CborEncoder encoderRegionList;
    CborEncoder encoderProcessedByNodeList;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    // create the block array of size 2
    size_t array_size = 2;

    std::string fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_create_array(&encoder, &encoderBlockArray, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block array element 1: format version
    fld_name = "BP7_IMCState.v1.format_version";
    size_t format_version = 1;
    err = cbor_encode_uint(&encoderBlockArray, format_version);
    CHECK_CBOR_ENCODE_ERROR_RETURN
    

    // block array element 2: format v1 array of size 4
    {
        fld_name = "BP7_IMCState.v1.format_array";
        array_size = 4;
        err = cbor_encoder_create_array(&encoderBlockArray, &encoderFormatArray, array_size);
        CHECK_CBOR_ENCODE_ERROR_RETURN


        // foramt array element 1: array of processed regions
        {
            fld_name = "BP7_IMCState.v1.processed_regiom_list";
            array_size = 0;
            auto sptr_map = bundle->imc_processed_regions_map();

            if (sptr_map) {
                array_size = sptr_map->size();
            }

            err = cbor_encoder_create_array(&encoderFormatArray, &encoderRegionList, array_size);
            CHECK_CBOR_ENCODE_ERROR_RETURN

            if (sptr_map) {
                fld_name = "BP7_IMCState.v1.processed_regiom_list.region";
                auto iter = sptr_map->begin();

                while (iter != sptr_map->end()) {
                    err = cbor_encode_uint(&encoderRegionList, iter->first);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    ++iter;
                }
            }

            fld_name = "BP7_IMCState.v1.processed_regiom_list";
            err = cbor_encoder_close_container(&encoderFormatArray, &encoderRegionList);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }

        // foramt array element 2: IMC Sync Request flag
        fld_name = "BP7_IMCState.v1.imc_sync_request";
        err = cbor_encode_boolean(&encoderFormatArray, bundle->imc_sync_request());
        CHECK_CBOR_ENCODE_ERROR_RETURN

        // foramt array element 2: IMC proxy petition flag
        fld_name = "BP7_IMCState.v1.imc_proxy_petition";
        err = cbor_encode_boolean(&encoderFormatArray, bundle->imc_is_proxy_petition());
        CHECK_CBOR_ENCODE_ERROR_RETURN

        // foramt array element 4: proxy processed by node list
        fld_name = "BP7_IMCState.v1.processed_by_node_list";
        {
            size_t array_size = 0;
            auto sptr_map = bundle->imc_processed_by_nodes_map();

            if (sptr_map) {
                array_size = sptr_map->size();
            }

            err = cbor_encoder_create_array(&encoderFormatArray, &encoderProcessedByNodeList, array_size);
            CHECK_CBOR_ENCODE_ERROR_RETURN

            if (sptr_map) {
                auto iter = sptr_map->begin();

                while (iter != sptr_map->end()) {
                    err = cbor_encode_uint(&encoderProcessedByNodeList, iter->first);
                    CHECK_CBOR_ENCODE_ERROR_RETURN

                    ++iter;
                }
            }

            err = cbor_encoder_close_container(&encoderFormatArray, &encoderProcessedByNodeList);
            CHECK_CBOR_ENCODE_ERROR_RETURN
        }

        // close the format array
        fld_name = "BP7_IMCState.v1.format_array";
        err = cbor_encoder_close_container(&encoderBlockArray, &encoderFormatArray);
        CHECK_CBOR_ENCODE_ERROR_RETURN
    }

    // close the block array
    fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_close_container(&encoder, &encoderBlockArray);
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
// return values:
//     BP_FAIL = error
//     BP_SUCCESS = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
//
int64_t
BP7_IMCStateBlockProcessor::encode_block_data_format_2(uint8_t* buf, uint64_t buflen, 
                                                       const Bundle* bundle, int64_t& encoded_len)
{
    // Block is an array of 2 elements
    //   block array element 1: uint64 - block format version
    //   block array element 2: array of elements specific to each version

    //  format version 2:
    //        Used on IMC Briefing bundles 
    //        (these bundles have an IPN administrative destination and setting the imc_briefing
    //         flag on the bundle triggers adding this block to the outgoing serialized bundle)
    //    block array element 2: array of size 3
    //            v2 array element 1: bool - is IMC sync requested
    //            v2 array element 2: bool - is IMC sync response
    //            v2 array element 3: bool - is DTNME IMC Router node
    //
    // 

    encoded_len = 0;

    CborEncoder encoder;
    CborEncoder encoderBlockArray;
    CborEncoder encoderFormatArray;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);

    // create the block array of size 2
    size_t array_size = 2;

    std::string fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_create_array(&encoder, &encoderBlockArray, array_size);
    CHECK_CBOR_ENCODE_ERROR_RETURN

    // block array element 1: format version
    fld_name = "BP7_IMCState.v2.format_version";
    size_t format_version = 2;
    err = cbor_encode_uint(&encoderBlockArray, format_version);
    CHECK_CBOR_ENCODE_ERROR_RETURN
    

    // block array element 2: format v2 array of size 3
    {
        fld_name = "BP7_IMCState.v2.format_array";
        array_size = 3;
        err = cbor_encoder_create_array(&encoderBlockArray, &encoderFormatArray, array_size);
        CHECK_CBOR_ENCODE_ERROR_RETURN


        // foramt array element 1: IMC sync request
        // element 2:
        fld_name = "BP7_IMCState.v2.imc_sync_request";
        err = cbor_encode_boolean(&encoderFormatArray, bundle->imc_sync_request());
        CHECK_CBOR_ENCODE_ERROR_RETURN
    
        // foramt array element 2: IMC sync response
        fld_name = "BP7_IMCState.v2.imc_sync_reply";
        err = cbor_encode_boolean(&encoderFormatArray, bundle->imc_sync_reply());
        CHECK_CBOR_ENCODE_ERROR_RETURN

        // foramt array element 1: IMC router node flag
        fld_name = "BP7_IMCState.v2.imc_is_router_node";
        err = cbor_encode_boolean(&encoderFormatArray, bundle->imc_is_router_node());
        CHECK_CBOR_ENCODE_ERROR_RETURN

        // close the format array
        fld_name = "BP7_IMCState.v2.format_array";
        err = cbor_encoder_close_container(&encoderBlockArray, &encoderFormatArray);
        CHECK_CBOR_ENCODE_ERROR_RETURN
    }

    // close the block array
    fld_name = "BP7_IMCState.block_array";
    err = cbor_encoder_close_container(&encoder, &encoderBlockArray);
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
BP7_IMCStateBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void) xmit_blocks;
    (void) link;
    (void) last;

    // the block data is a byte string which is a CBOR encoded 
    // array of [IPN] destination node numbers
    // - generate the data for the byte string
    int64_t status;
    int64_t data_len;
    int64_t encoded_len;

    // first run through the encode process with a null buffer to determine the needed size of buffer
    data_len = encode_block_data(nullptr, 0, bundle, encoded_len);
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

    status = encode_block_data(data_buf, data_len, bundle, encoded_len);
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

            //oasys::HexDumpBuffer hex;
            //hex.append(block->contents().buf(), block->contents().len());

            //log_always_p(log_path(), "generated %zu CBOR bytes:",
            //                         block->contents().len());
            //log_multiline(log_path(), oasys::LOG_ALWAYS, hex.hexify().c_str());


    return BP_SUCCESS;
}


} // namespace dtn
