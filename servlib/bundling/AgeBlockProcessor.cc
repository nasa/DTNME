/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

// We're going to use some of the `Time` functionality provided to us by
// `oasys`. This is used under the assumption that the system clock, while
// unsynchronized, can keep time accurately within some delta. If clock drift
// and resets are problems, then we'll need to find a new way of keeping track
// of time.
#include <oasys/util/Time.h>

#include "AgeBlockProcessor.h"

// All the standard includes.
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleTimestamp.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "SDNV.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/aeb";

// `BundleProtocol::AGE_BLOCK` is defined in `BundleProtocol.h` indicating the
// block type `0x00a` or `10`. See the related [Internet-Draft][id].
//
// [id]: http://tools.ietf.org/html/draft-irtf-dtnrg-bundle-age-block-00
AgeBlockProcessor::AgeBlockProcessor()
    : BlockProcessor(BundleProtocol::AGE_BLOCK) 
{
}

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
AgeBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list) 
{
    log_debug_p(LOG, "prepare() begin");

    // Check to see if we're enabled for outbound processing. If not, return
    // BP_FAIL.
    if(BundleProtocol::params_.age_outbound_enabled_ != true) {
        log_debug_p(LOG, "age_outbound_enabled != true, no AEB, returning BP_FAIL");
        log_debug_p(LOG, "prepare() end");
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS;             
    }

    // We can't call `bundle->api_blocks()` as it doesn't return a `const`, so
    // as a temporary measure we've gone ahead and modified the `Bundle` class
    // to provide us with the ability access the `api_blocks()`. If we find that
    // an Age block exists, return `BP_FAIL`, though the validity of the Age block
    // remains questionable...
    //
    // `XXX` Idea: understand how the bundle is processed and handled when it's
    // received from an API call. See if we can validate the bundle and/or
    // delete or fix it if it's malformed, or update it (a bundle might have
    // been in a queue for a while so we need to update the age.
    if( bundle->api_blocks_c()->find_block(BundleProtocol::AGE_BLOCK) != false ) {
        log_debug_p(LOG, "Age block exists in API Blocks, returning BP_FAIL");
        log_debug_p(LOG, "prepare() end");
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS;             
    }

    // Call our parent class `prepare()`. 
    log_debug_p(LOG, "prepare() end");
    return BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

// ``generate`` is called when?
//
// We generate the age field before being sent out on the wire
// but what happens if the connection is dropped and the bundle is
// requeued? Do the block processors get called again to update the
// blocks?
int
AgeBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    // This is only done to suppress warnings. Anything unused typecast
    // to `(void)`.
    /* (void)bundle; */
    (void)link;
    (void)xmit_blocks;

    log_debug_p(LOG, "generate() begin");

    //`XXX` If the creation timestamp isn't 0, this shouldn't exist.
    /* ASSERT(bundle->creation_ts().seconds_ == 0); */

    //`XXX` What flags do we need to set
    u_int64_t flags = BundleProtocol::BLOCK_FLAG_REPLICATE |
                      BundleProtocol::BLOCK_FLAG_DISCARD_BLOCK_ONERROR |
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0);

    //`XXX` Simple `oasys::Time` tests. To be removed.
    //oasys::Time startTime = oasys::Time::now();
    //log_debug_p(LOG, "oasys::Time start: %u", startTime.in_milliseconds());
    //log_debug_p(LOG, "oasys::Time elapsed (ms): %u", startTime.elapsed_ms());
    //log_debug_p(LOG, "elapsed time (ms) [pre sleep]: %u", bundle->time_aeb().elapsed_ms());

    //`XXX` remove; this is to make sure we Age gracefully.
    //log_debug_p(LOG, "sleping for 5s...");
    //sleep(5);    
 
    // The generic formula is the following: 
    //
    //      Bundle Age = Bundle Age + Elapsed Time
    //
    // If we have access to UTC, we calculate Elapsed Time by taking the
    // difference between the current time and the creation timestamp time.

    /*u_int64_t age = BundleTimestamp::get_current_time() - 
                      bundle->creation_ts().seconds_ */
    u_int64_t age = (bundle->time_aeb().elapsed_ms() / 1000) +
            bundle->age();

    // We can't modify the bundle timestamp here, so we'll need to code it in
    // the daemon somewhere.
    /* bundle->set_creation_ts(BundleTimestamp(0, bundle->creation_ts().seqno_)); */

    // Debugging statements
    log_debug_p(LOG, "bundle age (s): %" PRIu64, age);
    log_debug_p(LOG, "elapsed time (ms) [post sleep, age calculated]: %u", bundle->time_aeb().elapsed_ms());

    // Calculate the length of the SDNV-encoded Age for accurate generation of
    // the preamble. We don't want to deal with any segfaults.
    size_t length = SDNV::encoding_len(age);

    // Let the parent class do the hard lifting for us. This let's us focus
    // specifically on the Block content; any other formatting issues we're on
    // our own.
    BlockProcessor::generate_preamble(xmit_blocks,
                                      block,
                                      BundleProtocol::AGE_BLOCK,
                                      flags,
                                      length); 

    // The process of storing our value into the block. We'll create a
    // `DataBuffer` object and `reserve` the length of our encoded Age and
    // update the length of the `DataBuffer`.
    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    // Set our pointer to the right offset.
    u_int8_t* bp = contents->buf() + block->data_offset();

    // The following seems to be the way to copy data in if we're not encoding
    // to SDNVs:
    //
    //     memcpy(contents->buf() + block->data_offset(), bp, length);
    //
    // Since we're not doing that, we'll use `SDNV::encode()`

    // Copy the encoded Age into the buffer. The number of bytes encoded and
    // copies is returned.
    int len = SDNV::encode(age, bp, length);

    // If assertion fails, we ran out of buffer space. 
    ASSERT(len > 0);    

    // Increment our pointer to the end; but this probably isn't necessary
    // since only one value is being written.
    bp += len;

    // Logging for some interesting statistics.
    log_debug_p(LOG, "elapsed time (ms) [finished AEB generation]: %u", bundle->time_aeb().elapsed_ms());
    //log_debug_p(LOG, "oasys::Time elapsed (ms): %u", startTime.elapsed_ms());
    log_debug_p(LOG, "generate() end");

    return BP_SUCCESS;
}
    
// Any special processing that we need to do while consuming the
// block we'll put here. It doesn't seem like the logic for what we
// do after we consume is handled here; perhaps we should extract
// the Age field and modify a new data structure inserted inside a
// Bundle?
//
//     bundle->age()->assign((int)block->data);
//
// Hence we call the parent class consume, which will place the data
// inside `block->data`. (Look at `BlockInfo` class for more
// information?)
//
// For now, consume the block and do nothing.
int64_t 
AgeBlockProcessor::consume(Bundle*    bundle,
                           BlockInfo* block,
                           u_char*    buf,
                           size_t     len)
{
    // Typical debug statements. Changed LOGLEVEL from `ERROR` to `INFO` (but
    // not `DEBUG` as you'll get oversaturated with information).
    log_debug_p(LOG, "consume() begin");

    // Currently just some info/debug statements for testing out `oasys::Time`.
    //oasys::Time startTime = oasys::Time::now();
    //log_debug_p(LOG, "oasys::Time now(): %u", startTime.in_milliseconds());

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

    // This is where we really begin processing our Age block.

    // Initialize some important variables. We'll set pointers to the raw data
    // portion of our block and grab the length as defined.
    //
    // Then, create some variables for the decoded (and human readable) data.
    u_char *  block_data = block->data();
    u_int32_t block_length = block->data_length();

    u_int64_t age_value  = 0;
    int       age_length = 0;

    // Decode the SDNV-encoded Age. Note that we need to know the length of the
    // encoded value and provide some pointers to the encoded value along with
    // where we want the decoded value (in this case, Age).
    if((age_length = SDNV::decode(block_data, block_length, &age_value)) < 0) {
        log_err_p(LOG, "something went wrong here");
    }

    // If we were processing more data or had more fields in our block, this
    // adjusting of our pointers would be necessary.
    block_data += age_length;
    block_length -= age_length;

    // Set some bundle metadata: `age` and the current `Time`, to keep track of
    // how long we held the bundle for. See the `NOTE` on limitations.
    log_debug_p(LOG, "Bundle Age: %" PRIu64, age_value);
    bundle->set_age(age_value);
    bundle->set_time_aeb(oasys::Time::now());

    // `TODO:` We need a way to configure the zeroing out of the Creation
    // Timestamp Time. Currently this will override anything the daemon does
    // with the Creation Timestamp Time but we could get into some interesting
    // contention issues if we don't explore this more carefully.
    if(BundleProtocol::params_.age_zero_creation_ts_time_ == true) {
        log_debug_p(LOG, "Zero-ing Creation Timestamp Time");
        bundle->set_creation_ts(BundleTimestamp(0, bundle->creation_ts().seqno_)); 
    }

    log_debug_p(LOG, "consume() end");

    // Finish and return the number of bytes consumed.
    return cc;
}

//----------------------------------------------------------------------
int
AgeBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Age Block");
}

} // namespace dtn
