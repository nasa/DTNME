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

#include <third_party/oasys/util/Time.h>

#include "BP6_IMCStateBlockProcessor.h"

// All the standard includes.
#include "Bundle.h"
#include "BundleProtocol.h"
#include "SDNV.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/imc";

BP6_IMCStateBlockProcessor::BP6_IMCStateBlockProcessor()
    : BP6_BlockProcessor(BundleProtocolVersion6::IMC_STATE_BLOCK) 
{
	block_name_ = "BP6_IMCState";
}

//Useful notes from the AgeBlockProcessor:
// Where does `prepare` fit in the bundle processing? It doesn't seem like we
// need to do much here aside from calling the parent class function.
//
// Would functionality such as checking timestamps happen here? 
//
// The parent class function will make room for the outgoing bundle and place
// this block after the primary block and before the payload. Unsure (w.r.t.
// `prepare()`) where or how you'd place the block *after* the payload (see
// BSP?)
int
BP6_IMCStateBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const SPtr_BlockInfo source,
                           const LinkRef&   link,
                           list_owner_t     list) 
{
    // If not an IMC destination then no further processing is needed
    if (!bundle->dest()->is_imc_scheme()) {
        return BP_SUCCESS;
    }

    // Call our parent class `prepare()`. 
    return BP6_BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//
int
BP6_IMCStateBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    (void)link;
    (void)last;

    oasys::ScopeLock l(bundle->lock(), __func__);

    // Flags Requirements:  bit 0 MUST be SET    (replicate in every fragment)
    //                      bit 2 SHOULD be CLEAR  (delete bundle if block can't be processed)
    //                      bit 4 SHOULD be CLEAR  (discard block if it can't be processed)
    //                      bit 6 MUST be CLEAR    (block contains an EID-reference field)
    u_int64_t flags = 1;

    // Need to pass in the length of the data contents to reserve space in the transmit buffer
    // calc the length of encoded data:
    //     SDNV length of num destination nodes
    //     SDNV length of each dest node number
    //     SDNV length of num processed regions
    //     SDNV lenght of each region number
    size_t dest_encoded_len = bundle->imc_size_of_sdnv_dest_nodes_array();
    size_t regions_encoded_len = bundle->imc_size_of_sdnv_processed_regions_array();
    size_t data_len = dest_encoded_len + regions_encoded_len;

    //log_always_p(LOG, "generate - sdnv lens - dest nodes: %zu  regions: %zu  total: %zu", 
    //                  dest_encoded_len, regions_encoded_len, data_len);

    // Generate the generic block preamble and reserve
    // buffer space for the block-specific data.
    BP6_BlockProcessor::generate_preamble(xmit_blocks,
                                          block,
                                          block_type(),
                                          flags,
                                          data_len);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->full_length());

    size_t buf_len = data_len;
    u_char* buf_ptr = contents->buf() + block->data_offset();

    ssize_t result = bundle->imc_sdnv_encode_dest_nodes_array(buf_ptr, buf_len);
    if (result < 0) {
        log_err_p(LOG, "Error SDNV encoding IMC Destinations Node Array");
        return BP_FAIL;
    }
    ASSERT((size_t)result == dest_encoded_len);
    buf_ptr += result;
    buf_len -= result;

    result = bundle->imc_sdnv_encode_processed_regions_array(buf_ptr, buf_len);
    if (result < 0) {
        log_err_p(LOG, "Error SDNV encoding IMC Processed Regions Array");
        return BP_FAIL;
    }
    ASSERT((size_t)result == regions_encoded_len);
    buf_len -= result;
    ASSERT(buf_len == 0);

    contents->set_len(block->full_length());

    return BP_SUCCESS;
}
    
//Useful notes from the AgeBlockProcessor:
// Any special processing that we need to do while consuming the
// block we'll put here. 
//
// Hence we call the parent class consume, which will place the data
// inside `block->data`. (Look at `BlockInfo` class for more
// information?)
//
// For now, consume the block and do nothing.

ssize_t
BP6_IMCStateBlockProcessor::consume(Bundle*    bundle,
                           BlockInfo* block,
                           u_char*    buf,
                           size_t     len)
{

    // Calling the generic `consume` to fill in some of the blanks. Returns the
    // number of bytes consumed.
    ssize_t cc = BP6_BlockProcessor::consume(bundle, block, buf, len);

    // If this is `-1`, there's some (undefined?) protocol error. A more
    // descriptive error would probably be better.
    if (cc == -1) {
        return -1; // protocol error
    }
   
    // If we don't finish processing the block, return the number of bytes
    // consumed. (Error checking done in the calling function?)
    if (! block->complete()) {
        ASSERT(cc == (ssize_t)len);
        return cc;
    }

    // Initialize some important variables. We'll set pointers to the raw data
    // portion of our block and grab the length as defined.
    //
    // Then, create some variables for the decoded (and human readable) data.
    u_char *  block_data = block->data();
    size_t    block_len = block->data_length();

        // function to SDNV decode fields
        #define IMCSTATEBP_READ_SDNV(var) { \
            int sdnv_len = SDNV::decode(block_data, block_len, var); \
            if (sdnv_len < 0) { \
                log_err_p(LOG, "Error decoding SDNV in consume ptr: %p (%2.2x) len = %zu", block_data, *block_data, block_len); \
                return -1; \
            } \
            block_data += sdnv_len; \
            block_len -= sdnv_len; }
    

    // first field is an SDNV of number of destination nodes
    size_t num_dest_nodes = 0;
    IMCSTATEBP_READ_SDNV(&num_dest_nodes);

    log_always_p(LOG, "consume - num dest nodes: %zu", num_dest_nodes);

    // decode the array of SDNV dest nodes
    size_t node_num;
    for (size_t idx=0; idx<num_dest_nodes; ++idx) {
        IMCSTATEBP_READ_SDNV(&node_num);
        bundle->add_imc_orig_dest_node(node_num);
        log_always_p(LOG, "consume -     dest node: %zu", node_num);
    }

    // next field is the number of processed regions
    size_t num_regions = 0;
    IMCSTATEBP_READ_SDNV(&num_regions);

    log_always_p(LOG, "consume - num regions: %zu", num_regions);

    // decode the array of SDNV regions
    size_t region_num;
    for (size_t idx=0; idx<num_regions; ++idx) {
        IMCSTATEBP_READ_SDNV(&region_num);
        bundle->add_imc_region_processed(region_num);
        log_always_p(LOG, "consume -     region: %zu", region_num);
    }

    // Should not be anything else in the block
    ASSERT(0 == block_len);

    // Finish and return the number of bytes consumed.
    return cc;
}

} // namespace dtn

