/*
 *    Copyright 2015 United States Government as represented by NASA
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

//Useful notes from the AgeBlockProcessor:
// **BlockProcessor**s typically have four procedures. `prepare`,
// `generate`, `finalize`, and `consume`. `consume` is called when a particular
// extension block is being inbound processed, while the first three are called
// for outbound processing. 
//
// Depending on the nature of the extension block, `prepare` and `generate`
// is all that's necessary; `finalize` is required if a field of your extension
// block depends on all the other blocks.
//
// Because of how DTNME works, additional changes are required to various core
// parts of the daemon, to be documented.
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef ECOS_ENABLED

#include <oasys/util/Time.h>

#include "ExtendedClassOfServiceBlockProcessor.h"

// All the standard includes.
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleTimestamp.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "SDNV.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/cteb";

// `BundleProtocol::EXTENDED_CLASS_OF_SERVICE_BLOCK` is defined in `BundleProtocol.h` indicating the
// block type `0x013` or `19`. See the related [Internet-Draft][id].
//
// [id]: draft-jenkins-custody-transfer-enhancement-block-01
ExtendedClassOfServiceBlockProcessor::ExtendedClassOfServiceBlockProcessor()
    : BlockProcessor(BundleProtocol::EXTENDED_CLASS_OF_SERVICE_BLOCK) 
{
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
ExtendedClassOfServiceBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list) 
{
    log_debug_p(LOG, "prepare() begin");

    // If no ECOS block then return fail as no further processing is needed
    if (source == NULL) {
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS;
    }

    // Call our parent class `prepare()`. 
    log_debug_p(LOG, "prepare() end");
    return BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

// ``generate`` is called when?
//
int
ExtendedClassOfServiceBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    // This is only done to suppress warnings. Anything unused typecast
    // to `(void)`.
    (void)bundle;
    (void)link;
    (void)last;

    log_debug_p(LOG, "generate() begin");

    // Test the block source
    const BlockInfo* source = block->source();
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);

    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
    
    // Flags Requirements:  bit 0 MUST be SET    (replicate in every fragment)
    //                      bit 2 SHOULD be CLEAR  (delete bundle if block can't be processed)
    //                      bit 4 SHOULD be CLEAR  (discard block if it can't be processed)
    //                      bit 6 MUST be CLEAR    (block contains an EID-reference field)
    u_int64_t flags = 1;

    // Need to pass in the length of the data contents to reserve space in the transmit buffer

    // Generate the generic block preamble and reserve
    // buffer space for the block-specific data.
    BlockProcessor::generate_preamble(xmit_blocks,
                                      block,
                                      block_type(),
                                      flags,
                                      source->data_length());


    ASSERT(block->data_length() == source->data_length());

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->full_length());
    memcpy(contents->buf()          + block->data_offset(),
           source->contents().buf() + source->data_offset(),
           block->data_length());
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
int64_t
ExtendedClassOfServiceBlockProcessor::consume(Bundle*    bundle,
                           BlockInfo* block,
                           u_char*    buf,
                           size_t     len)
{
    log_debug_p(LOG, "consume() begin");

    // Calling the generic `consume` to fill in some of the blanks. Returns the
    // number of bytes consumed.
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    // If this is `-1`, there's some (undefined?) protocol error. A more
    // descriptive error would probably be better.
    if (cc == -1) {
        return -1; // protocol error
    }
   
    // If we don't finish processing the block, return the number of bytes
    // consumed. (Error checking done in the calling function?)
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    // Initialize some important variables. We'll set pointers to the raw data
    // portion of our block and grab the length as defined.
    //
    // Then, create some variables for the decoded (and human readable) data.
    u_char *  block_data = block->data();
    u_int32_t block_length = block->data_length();

    // must be at least two byes of data (the flags and the ordinal value)
    if (block_length < 2) {
        return -1;
    }

    // flags and ordinal are 1 byte each and not SDNVs
    bundle->set_ecos_enabled(true);
    bundle->set_ecos_flags(*block_data);
    ++block_data;
    bundle->set_ecos_ordinal(*block_data);
    ++block_data;
    block_length -= 2;

    // Is there a flow label also?
    if (bundle->ecos_has_flowlabel()) {
        if (block_length < 1) {
            return -1;   // Flow Label SDNV value is missing
        }

        u_int64_t flow_label    = 0;
        int       fl_len        = 0;

        // Decode the SDNV-encoded Custody ID. Note that we need to know the length of the
        // encoded value and provide some pointers to the encoded value along with
        // where we want the decoded value (in this case, custody_id).
        fl_len = SDNV::decode(block_data, block_length, &flow_label);
        if (fl_len < 0) {
            log_err_p(LOG, "Error SDNV decoding ECOS Flow Label");
            return -1;   // Flow Label SDNV value is missing
        }

        bundle->set_ecos_flowlabel(flow_label);

        block_data += fl_len;
        block_length -= fl_len;
    }

    // Should not be anything else in the block
    // ASSERT(0 == block_length);

    log_debug_p(LOG, "consume() end");

    // Finish and return the number of bytes consumed.
    return cc;
}

//----------------------------------------------------------------------
int
ExtendedClassOfServiceBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
	(void) b;

	buf->append("Extended Class Of Service Block");
	return 0;
}

} // namespace dtn

#endif // ECOS_ENABLED
