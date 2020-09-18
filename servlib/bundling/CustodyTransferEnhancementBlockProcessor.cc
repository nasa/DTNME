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


#ifdef ACS_ENABLED

#include "CustodyTransferEnhancementBlockProcessor.h"

// All the standard includes.
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "SDNV.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/cteb";

// `BundleProtocol::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK` is defined 
// in `BundleProtocol.h` indicating the block type `0x00a` or `10`.
//
CustodyTransferEnhancementBlockProcessor::CustodyTransferEnhancementBlockProcessor()
    : BlockProcessor(BundleProtocol::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK) 
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
CustodyTransferEnhancementBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list) 
{
    log_debug_p(LOG, "prepare() begin");

    // If no CTEB block then return fail as no further processing is needed
    if (source == NULL) {
      //return BP_FAIL;  // fail used to be okay but now aborts
      return BP_SUCCESS;
  }

    // Check to see if we're enabled for outbound processing. If not, return
    // BP_FAIL.

    // if enabled and we have custody then we strip received CTEB block(s)
    if ( bundle->local_custody() && (BlockInfo::LIST_RECEIVED == list) ) {
        log_info_p(LOG, "Strip received CTEB from transmitted bundle: *%p", bundle);
        //return BP_FAIL;  // fail used to be okay but now aborts
        return BP_SUCCESS;
    }
    // XXX/dz Local custody bundles will have an API CTEB block if received
    // while ACS was enabled for the destination. I don't think we need to
    // check to see if ACS is still enabled for the dest on the outbound here
    // but this is the place to strip the API CTEB if the need arises.
    //if ( bundle->local_custody() && (BlockInfo::LIST_API == list) ) {
    //    if (BundleDaemonACS::instance()->acs_enabled_for_endpoint(bundle->dest())) {
    //        log_info_p(LOG, Strip API CTEB from transmitted bundle: *%p", bundle);
    //        log_debug_p(LOG, "prepare() end");
    //        return BP_FAIL;
    //    }
    //}

    // Call our parent class `prepare()`. 
    log_debug_p(LOG, "prepare() end");
    return BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//Useful notes from the AgeBlockProcessor:
// ``generate`` is called when?
//
// We generate the age field before being sent out on the wire
// but what happens if the connection is dropped and the bundle is
// requeued? Do the block processors get called again to update the
// blocks?
int
CustodyTransferEnhancementBlockProcessor::generate(const Bundle*  bundle,
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
    
    // Flags Requirements:  bit 0 MUST be CLEAR    (replicate in every fragment)
    //                      bit 2 SHOULD be CLEAR  (delete bundle if block can't be processed)
    //                      bit 4 SHOULD be CLEAR  (discard block if it can't be processed)
    //                      bit 6 MUST be CLEAR    (block contains an EID-reference field)
    u_int64_t flags = 0;

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
CustodyTransferEnhancementBlockProcessor::consume(Bundle*    bundle,
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

    // If a valid CTEB has already been processed then we can skip any other CTEBs.
    // Technically there should never be more than one CTEB per bundle but just in case
    // we will handle it and only use the first one that is valid
    if ( bundle->cteb_valid() ) {
        log_info_p(LOG, "consume() : bundle received with multiple CTE Blocks "
                        "- ignoring invalid blocks");
        return cc;
    }
    
    // Initialize some important variables. We'll set pointers to the raw data
    // portion of our block and grab the length as defined.
    //
    // Then, create some variables for the decoded (and human readable) data.
    u_char *  block_data = block->data();
    u_int32_t block_length = block->data_length();

    u_int64_t custody_id    = 0;
    int       cid_len       = 0;
    char*     cteb_creator  = NULL;

    // Decode the SDNV-encoded Custody ID. Note that we need to know the length of the
    // encoded value and provide some pointers to the encoded value along with
    // where we want the decoded value (in this case, custody_id).
    if((cid_len = SDNV::decode(block_data, block_length, &custody_id)) < 0) {
        log_err_p(LOG, "Error SDNV decoding CTEB Custody ID");
    }

    // move pointers to the CTEB Creator field
    block_data += cid_len;
    block_length -= cid_len;

    // Remainder of block is an non-terminated custodian EID

    cteb_creator = new char[block_length+1];
    strncpy(cteb_creator, (char*)block_data, block_length+1);
    cteb_creator[block_length] = '\0';

    // check to see if this is a valid CTE Block 
    // (custodian listed here must equal the current custodian)
    if ( ! is_cteb_creator_custodian((const char*)bundle->custodian().c_str(), cteb_creator) ) {
        // invalid block - remove it
       log_info_p(LOG, "Invalid CTEB recevied - removing block: "
                       "CTEB creator: %s  Bundle ID: %" PRIbid " Custody ID: %" PRIu64, 
                  cteb_creator, bundle->bundleid(), custody_id );
    } else {
        // good CTEB set up ACS bundle parameters
       bundle->set_cteb_valid(true);
       bundle->set_cteb_custodyid(custody_id);
       log_info_p(LOG, "Received bundle from ACS capable server: %s  "
                       "Bundle ID: %" PRIbid " Custody ID: %" PRIu64, 
                  cteb_creator, bundle->bundleid(), custody_id );
    }

    delete[] cteb_creator;

    log_debug_p(LOG, "consume() end");

    // Finish and return the number of bytes consumed.
    return cc;
}

bool
CustodyTransferEnhancementBlockProcessor::is_cteb_creator_custodian(const char* custodian, const char* creator) {
    bool result = false;

    // hopefully at some point in the future all we need is this simple string compare
    if ( 0 == strcmp(custodian, creator) ) {
        result = true;
    }
//
//    XXX/dz else clause is not needed if the ipn_scheme_updates.patch uploaded to
//    the Sourceforge Bugs tracker has been applied.
//
//    // Compare the CTEB Creator to the Custodian to see if the match. Currently 
//    // there is a disconnect in the implementations for ION and DTNME so we have 
//    // to do a bit more than a simple string compare.
//    } else if ( 0 == strncmp("ipn:", custodian, 3) ) {
//        // Custodian for ipn destinations is of the form ipn://n[/m] where 
//        // /mm is dropped if zero
//        // Creator for ipn destinations is of the from ipn:n.m in the 
//        // current CU-Boulder implementation
//        std::string scustodian(custodian);
//
//        std::string screator("ipn://");
//        bool add_slash = false;
//        for (size_t ix=4; ix<strlen(creator); ++ix) {
//            if ( '.' == creator[ix] )
//                add_slash = true;
//            else if ( add_slash && ('0' == creator[ix]) )
//                break; // do not add a final "/0" 
//            else {
//                if ( add_slash ) {
//                    screator.append("/");
//                    add_slash = false;
//                }
//                screator.append(1, creator[ix]);
//            }
//        }
//
//        result = (0 == scustodian.compare(screator));
//    }

    return result;
}

//----------------------------------------------------------------------
int
CustodyTransferEnhancementBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
	(void) b;

	buf->append("Custody Transfer Enhancement Block");
	return 0;
}

} // namespace dtn

#endif // ACS_ENABLED
