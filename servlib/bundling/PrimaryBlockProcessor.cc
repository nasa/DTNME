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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <string>
#include <sys/types.h>
#include <netinet/in.h>

#include "Bundle.h"
#include "BundleProtocol.h"
#include "PrimaryBlockProcessor.h"
#include "naming/EndpointID.h"
#include "naming/IPNScheme.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
PrimaryBlockProcessor::PrimaryBlockProcessor()
    : BlockProcessor(BundleProtocol::PRIMARY_BLOCK)
{
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_bundle_flags(const Bundle* bundle)
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

    if (bundle->custody_requested()) {
        flags |= BUNDLE_CUSTODY_XFER_REQUESTED;
    }

    if (bundle->singleton_dest()) {
        flags |= BUNDLE_SINGLETON_DESTINATION;
    }
    
    if (bundle->app_acked_rcpt()) {
        flags |= BUNDLE_ACK_BY_APP;
    }
    
    return flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_bundle_flags(Bundle* bundle, u_int64_t flags)
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

    if (flags & BUNDLE_CUSTODY_XFER_REQUESTED) {
        bundle->set_custody_requested(true);
    } else {
        bundle->set_custody_requested(false);
    }

    if (flags & BUNDLE_SINGLETON_DESTINATION) {
        bundle->set_singleton_dest(true);
    } else {
        bundle->set_singleton_dest(false);
    }
    
    if (flags & BUNDLE_ACK_BY_APP) {
        bundle->set_app_acked_rcpt(true);
    } else {
        bundle->set_app_acked_rcpt(false);
    }
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_cos_flags(const Bundle* b)
{
    u_int64_t cos_flags = 0;

    cos_flags = ((b->priority() & 0x3) << 7);

    return cos_flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_cos_flags(Bundle* b, u_int64_t cos_flags)
{
    b->set_priority((cos_flags >> 7) & 0x3);
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_srr_flags(const Bundle* b)
{
    u_int64_t srr_flags = 0;
    
    if (b->receive_rcpt())
        srr_flags |= BundleProtocol::STATUS_RECEIVED;

    if (b->custody_rcpt())
        srr_flags |= BundleProtocol::STATUS_CUSTODY_ACCEPTED;

    if (b->forward_rcpt())
        srr_flags |= BundleProtocol::STATUS_FORWARDED;

    if (b->delivery_rcpt())
        srr_flags |= BundleProtocol::STATUS_DELIVERED;

    if (b->deletion_rcpt())
        srr_flags |= BundleProtocol::STATUS_DELETED;

    return srr_flags;
}
    
//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_srr_flags(Bundle* b, u_int64_t srr_flags)
{
    if (srr_flags & BundleProtocol::STATUS_RECEIVED)
        b->set_receive_rcpt(true);

    if (srr_flags & BundleProtocol::STATUS_CUSTODY_ACCEPTED)
        b->set_custody_rcpt(true);

    if (srr_flags & BundleProtocol::STATUS_FORWARDED)
        b->set_forward_rcpt(true);

    if (srr_flags & BundleProtocol::STATUS_DELIVERED)
        b->set_delivery_rcpt(true);

    if (srr_flags & BundleProtocol::STATUS_DELETED)
        b->set_deletion_rcpt(true);
}

//----------------------------------------------------------------------
size_t
PrimaryBlockProcessor::get_primary_len(const Bundle*  bundle,
                                       Dictionary*    dict,
                                       PrimaryBlock*  primary)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t primary_len = 0;
    primary->dictionary_length = 0;
    primary->block_length = 0;
    
    /*
     * We need to figure out the total length of the primary block,
     * except for the SDNVs used to encode flags and the length itself and
     * the one byte version field.
     *
     * First, we determine the size of the dictionary by first
     * figuring out all the unique strings, and in the process,
     * remembering their offsets and summing up their lengths
     * (including the null terminator for each).
     */

    // CBHE can only be performed if all four EIDs are ipn compatible.
    // I believe that the custodian is most likely to be non-ipn so testing
    // it first to short circuit this if statement as quick as possible. 
    if ( (IPNScheme::parse(bundle->custodian(), 
                           &primary->custodian_scheme_offset, 
                           &primary->custodian_ssp_offset)) &&
         (IPNScheme::parse(bundle->dest(), 
                           &primary->dest_scheme_offset, 
                           &primary->dest_ssp_offset)) &&
         (IPNScheme::parse(bundle->source(), 
                           &primary->source_scheme_offset, 
                           &primary->source_ssp_offset)) &&
         (IPNScheme::parse(bundle->replyto(), 
                           &primary->replyto_scheme_offset, 
                           &primary->replyto_ssp_offset)) )
    {
        // the offsets are now all set so just set the dictionary length
        primary->dictionary_length = 0;
    }
    else
    {
        dict->get_offsets(bundle->dest(), 
                          &primary->dest_scheme_offset,
                          &primary->dest_ssp_offset);
    
        dict->get_offsets(bundle->source(), 
                          &primary->source_scheme_offset,
                          &primary->source_ssp_offset);

        dict->get_offsets(bundle->replyto(),
                          &primary->replyto_scheme_offset,
                          &primary->replyto_ssp_offset);

        dict->get_offsets(bundle->custodian(), 
                          &primary->custodian_scheme_offset,
                          &primary->custodian_ssp_offset);
    
        primary->dictionary_length = dict->length();
    }

    primary->block_length += SDNV::encoding_len(primary->dest_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->dest_ssp_offset);

    primary->block_length += SDNV::encoding_len(primary->source_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->source_ssp_offset);

    primary->block_length += SDNV::encoding_len(primary->replyto_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->replyto_ssp_offset);

    primary->block_length += SDNV::encoding_len(primary->custodian_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->custodian_ssp_offset);

    (void)log; // in case NDEBUG is defined
    //log_debug_p(log, "generated dictionary length %llu",
    //            U64FMT(primary->dictionary_length));
    
    primary->block_length += SDNV::encoding_len(bundle->creation_ts().seconds_);
    primary->block_length += SDNV::encoding_len(bundle->creation_ts().seqno_);
    primary->block_length += SDNV::encoding_len(bundle->expiration());

    primary->block_length += SDNV::encoding_len(primary->dictionary_length);
    primary->block_length += primary->dictionary_length;
    
    /*
     * If the bundle is a fragment, we need to include space for the
     * fragment offset and the original payload length.
     *
     * Note: Any changes to this protocol must be reflected into the
     * FragmentManager since it depends on this length when
     * calculating fragment sizes.
     */
    if (bundle->is_fragment()) {
        primary->block_length += SDNV::encoding_len(bundle->frag_offset());
        primary->block_length += SDNV::encoding_len(bundle->orig_length());
    }
    
    // Format the processing flags.
    primary->processing_flags = format_bundle_flags(bundle);
    primary->processing_flags |= format_cos_flags(bundle);
    primary->processing_flags |= format_srr_flags(bundle);

    /*
     * Finally, add up the initial preamble and the variable
     * length part.
     */
    primary_len = 1 + SDNV::encoding_len(primary->processing_flags) +
                  SDNV::encoding_len(primary->block_length) +
                  primary->block_length;
    
    //log_debug_p(log, "get_primary_len(bundle %" PRIbid "): %zu",
    //            bundle->bundleid(), primary_len);
    
    // Fill in the remaining values of 'primary' just for the sake of returning
    // a complete data structure.
    primary->version = BundleProtocol::CURRENT_VERSION;
    primary->creation_time = bundle->creation_ts().seconds_;
    primary->creation_sequence = bundle->creation_ts().seqno_;
    primary->lifetime = bundle->expiration();
    
    return primary_len;
}

//----------------------------------------------------------------------
int
PrimaryBlockProcessor::prepare(const Bundle*    bundle,
                               BlockInfoVec*    xmit_blocks,
                               const BlockInfo* source,
                               const LinkRef&   link,
                               list_owner_t     list)
{
    static const char* log = "/dtn/bundle/protocol";
    (void) log; // prevent unused warning if debug logging disables


    (void)bundle;
    (void)link;
    (void)list;

    // There shouldn't already be anything in the xmit_blocks
    ASSERT(xmit_blocks->size() == 0);

    // Add EIDs to start off the dictionary (unless we're all in the
    // ipn scheme)
    if (bundle->dest().scheme() == IPNScheme::instance() &&
        bundle->source().scheme() == IPNScheme::instance() &&
        
        (bundle->replyto() == EndpointID::NULL_EID() ||
         bundle->replyto().scheme() == IPNScheme::instance()) &&
        
        (bundle->custodian() == EndpointID::NULL_EID() ||
         bundle->custodian().scheme() == IPNScheme::instance()))
    {
        //log_debug_p(log,
        //            "omitting dictionary entries since all eids in ipn scheme");
    }
    else
    {
        xmit_blocks->dict()->add_eid(bundle->dest());
        xmit_blocks->dict()->add_eid(bundle->source());
        xmit_blocks->dict()->add_eid(bundle->replyto());
        xmit_blocks->dict()->add_eid(bundle->custodian());
    }

    // make sure to add the primary to the front
    xmit_blocks->insert(xmit_blocks->begin(), BlockInfo(this, source));

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
PrimaryBlockProcessor::consume(Bundle*    bundle,
                               BlockInfo* block,
                               u_char*    buf,
                               size_t     len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t consumed = 0;
    PrimaryBlock primary;
    
    ASSERT(! block->complete());
    
    Dictionary* dict = bundle->mutable_recv_blocks()->dict();
    memset(&primary, 0, sizeof(primary));
    
    /*
     * Now see if this completes the primary by calling into
     * BlockProcessor::consume() to get the default behavior (now that
     * we've found the data length above).
     */
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return consumed + cc;
    }

    /*
     * Ok, now the block is complete so we can parse it. We'll return
     * the total amount consumed (or -1 for protocol error) when
     * finished.
     */
    consumed += cc;

    size_t primary_len = block->full_length();

    buf = block->writable_contents()->buf();
    len = block->writable_contents()->len();

    ASSERT(primary_len == len);

    primary.version = *(u_int8_t*)buf;
    buf += 1;
    len -= 1;
    
    if (primary.version != BundleProtocol::CURRENT_VERSION) {
        log_warn_p(log, "protocol version mismatch %d != %d",
                   primary.version, BundleProtocol::CURRENT_VERSION);
        return -1;
    }
    
#define PBP_READ_SDNV(location) { \
    int sdnv_len = SDNV::decode(buf, len, location); \
    if (sdnv_len < 0) \
        goto tooshort; \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Grab the SDNVs representing the flags and the block length.
    PBP_READ_SDNV(&primary.processing_flags);
    PBP_READ_SDNV(&primary.block_length);

    //log_debug_p(log, "parsed primary block: version %d length %u",
    //            primary.version, block->data_length());    
    
    // Parse the flags.
    parse_bundle_flags(bundle, primary.processing_flags);
    parse_cos_flags(bundle, primary.processing_flags);
    parse_srr_flags(bundle, primary.processing_flags);
    
    // What remains in the buffer should now be equal to what the block-length
    // field advertised.
    ASSERT(len == block->data_length());
    
    // Read the various SDNVs up to the start of the dictionary.
    PBP_READ_SDNV(&primary.dest_scheme_offset);
    PBP_READ_SDNV(&primary.dest_ssp_offset);
    PBP_READ_SDNV(&primary.source_scheme_offset);
    PBP_READ_SDNV(&primary.source_ssp_offset);
    PBP_READ_SDNV(&primary.replyto_scheme_offset);
    PBP_READ_SDNV(&primary.replyto_ssp_offset);
    PBP_READ_SDNV(&primary.custodian_scheme_offset);
    PBP_READ_SDNV(&primary.custodian_ssp_offset);
    PBP_READ_SDNV(&primary.creation_time);
    PBP_READ_SDNV(&primary.creation_sequence);
    PBP_READ_SDNV(&primary.lifetime);
    PBP_READ_SDNV(&primary.dictionary_length);
    
    bundle->set_creation_ts(BundleTimestamp(primary.creation_time,
                                            primary.creation_sequence));
    bundle->set_expiration(primary.lifetime);

    /*
     * Verify that we have the whole dictionary.
     */
    if (len < primary.dictionary_length) {
 tooshort:
        log_err_p(log, "primary block advertised incorrect length %u",
                  block->data_length());
        return -1;
    }

    if (primary.dictionary_length != 0)
    {
        /*
         * Make sure that the dictionary ends with a null byte.
         */
        if (buf[primary.dictionary_length - 1] != '\0') {
            log_err_p(log, "dictionary does not end with a NULL character!");
            return -1;
        }

        /*
         * Now use the dictionary buffer to parse out the various endpoint
         * identifiers, making sure that none of them peeks past the end
         * of the dictionary block.
         */
        u_char* dictionary = buf;
        buf += primary.dictionary_length;
        len -= primary.dictionary_length;
    
        dict->set_dict(dictionary, primary.dictionary_length);

        dict->extract_eid(bundle->mutable_source(),
                          primary.source_scheme_offset,
                          primary.source_ssp_offset);
    
        dict->extract_eid(bundle->mutable_dest(),
                          primary.dest_scheme_offset,
                          primary.dest_ssp_offset);
    
        dict->extract_eid(bundle->mutable_replyto(),
                          primary.replyto_scheme_offset,
                          primary.replyto_ssp_offset);
    
        dict->extract_eid(bundle->mutable_custodian(),
                          primary.custodian_scheme_offset,
                          primary.custodian_ssp_offset);
    }
    else
    {
        //log_debug_p(log, "decoding CBHE bundle");

        IPNScheme::format(bundle->mutable_dest(),
                          primary.dest_scheme_offset,
                          primary.dest_ssp_offset);

        IPNScheme::format(bundle->mutable_source(),
                          primary.source_scheme_offset,
                          primary.source_ssp_offset);

        IPNScheme::format(bundle->mutable_replyto(),
                          primary.replyto_scheme_offset,
                          primary.replyto_ssp_offset);

        IPNScheme::format(bundle->mutable_custodian(),
                          primary.custodian_scheme_offset,
                          primary.custodian_ssp_offset);
    }
    

    // If the bundle is a fragment, grab the fragment offset and original
    // bundle size (and make sure they fit in a 32 bit integer).
    if (bundle->is_fragment()) {
        u_int64_t sdnv_buf = 0;
        PBP_READ_SDNV(&sdnv_buf);
        if (sdnv_buf > 0xffffffff) {
            log_err_p(log, "fragment offset is too large: %llu",
                      U64FMT(sdnv_buf));
            return -1;
        }
        
        bundle->set_frag_offset(sdnv_buf);
        //log_debug_p(log, "PrimaryBlockProcessor::consume: setting frag_offset to %" PRIu64, sdnv_buf);
        // XXX/dz frag_length will be set in the PayloadBlockProcessor

        sdnv_buf = 0;
        
        PBP_READ_SDNV(&sdnv_buf);
        if (sdnv_buf > 0xffffffff) {
            log_err_p(log, "fragment original length is too large: %llu",
                      U64FMT(sdnv_buf));
            return -1;
        }
        
        bundle->set_orig_length(sdnv_buf);
        //log_debug_p(log, "PrimaryBlockProcessor::consume: setting orig_len to %" PRIu64, sdnv_buf);

        //log_debug_p(log, "parsed fragmentation info: offset %u orig_len %u",
        //            bundle->frag_offset(), bundle->orig_length());
    }
    
#undef PBP_READ_SDNV
    
    return consumed;
}

//----------------------------------------------------------------------
bool
PrimaryBlockProcessor::validate(const Bundle*           bundle,
                                BlockInfoVec*           block_list,
                                BlockInfo*              block,
                                status_report_reason_t* reception_reason,
                                status_report_reason_t* deletion_reason)
{
    (void)block_list;
    (void)block;
    (void)reception_reason;
    static const char* log = "/dtn/bundle/protocol";
    
    // Make sure all four EIDs are valid.
    bool eids_valid = true;
    eids_valid &= bundle->source().valid();
    eids_valid &= bundle->dest().valid();
    eids_valid &= bundle->custodian().valid();
    eids_valid &= bundle->replyto().valid();
    
    if (!eids_valid) {
        log_err_p(log, "bad value for one or more EIDs");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
    
    // According to BP section 3.3, there are certain things that a bundle
    // with a null source EID should not try to do. Check for these cases
    // and reject the bundle if any is true.
    if (bundle->source() == EndpointID::NULL_EID()) {
        if (bundle->receipt_requested() || bundle->app_acked_rcpt()) { 
            log_err_p(log,
                      "bundle with null source eid has requested a "
                      "report; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    
        if (bundle->custody_requested()) {
            log_err_p(log,
                      "bundle with null source eid has requested custody "
                      "transfer; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }

        if (!bundle->do_not_fragment()) {
            log_err_p(log,
                      "bundle with null source eid has not set "
                      "'do-not-fragment' flag; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }
    
    // Admin bundles cannot request custody transfer.
    if (bundle->is_admin()) {
        if (bundle->custody_requested()) {
            log_err_p(log,
                      "admin bundle requested custody transfer; "
                      "reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }

        if ( bundle->receive_rcpt() ||
             bundle->custody_rcpt() ||
             bundle->forward_rcpt() ||
             bundle->delivery_rcpt() ||
             bundle->deletion_rcpt() ||
             bundle->app_acked_rcpt() )
        {
            log_err_p(log, "admin bundle has requested a report; reject it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }
    
    return true;
}

//----------------------------------------------------------------------
int
PrimaryBlockProcessor::generate(const Bundle*  bundle,
                                BlockInfoVec*  xmit_blocks,
                                BlockInfo*     block,
                                const LinkRef& link,
                                bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)block;

    /*
     * The primary can't be last since there must be a payload block
     */
    ASSERT(!last);

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::generate_primary(const Bundle* bundle,
                                        BlockInfoVec* xmit_blocks,
                                        BlockInfo*    block)
{
    static const char* log = "/dtn/bundle/protocol";
    // point at the local dictionary
    Dictionary* dict = xmit_blocks->dict();
    size_t primary_len = 0;     // total length of the primary block
    PrimaryBlock primary;
    
    memset(&primary, 0, sizeof(primary));

    /*
     * Calculate the primary block length and initialize the buffer.
     */
    primary_len = get_primary_len(bundle, dict, &primary);
    block->writable_contents()->reserve(primary_len);
    block->writable_contents()->set_len(primary_len);
    block->set_data_length(primary_len);
    
    /*
     * Advance buf and decrement len as we go through the process.
     */
    u_char* buf = block->writable_contents()->buf();
    int     len = primary_len;
    
    (void)log; // in case NDEBUG is defined
    //log_debug_p(log, "generating primary: length %zu", primary_len);
    
    // Stick the version number in the first byte.
    *buf = BundleProtocol::CURRENT_VERSION;
    ++buf;
    --len;
    
#define PBP_WRITE_SDNV(value) { \
    int sdnv_len = SDNV::encode(value, buf, len); \
    ASSERT(sdnv_len > 0); \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Write out all of the SDNVs
    PBP_WRITE_SDNV(primary.processing_flags);
    PBP_WRITE_SDNV(primary.block_length);
    PBP_WRITE_SDNV(primary.dest_scheme_offset);
    PBP_WRITE_SDNV(primary.dest_ssp_offset);
    PBP_WRITE_SDNV(primary.source_scheme_offset);
    PBP_WRITE_SDNV(primary.source_ssp_offset);
    PBP_WRITE_SDNV(primary.replyto_scheme_offset);
    PBP_WRITE_SDNV(primary.replyto_ssp_offset);
    PBP_WRITE_SDNV(primary.custodian_scheme_offset);
    PBP_WRITE_SDNV(primary.custodian_ssp_offset);
    PBP_WRITE_SDNV(bundle->creation_ts().seconds_);
    PBP_WRITE_SDNV(bundle->creation_ts().seqno_);
    PBP_WRITE_SDNV(bundle->expiration());
    PBP_WRITE_SDNV(primary.dictionary_length);
    
    // Add the dictionary.
    if (primary.dictionary_length > 0) {
        memcpy(buf, dict->dict(), dict->length());
        buf += dict->length();
        len -= dict->length();
    }
 
    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (bundle->is_fragment()) {
        //log_debug_p(log,"PrimaryBlockProcessor::generate_primary: bundle is a fragment with frag_offset=%d orig_length=%d", bundle->frag_offset(), bundle->orig_length());
        PBP_WRITE_SDNV(bundle->frag_offset());
        PBP_WRITE_SDNV(bundle->orig_length());
    }
    
#undef PBP_WRITE_SDNV
    
    /*
     * Assuming that get_primary_len is written correctly, len should
     * now be zero since we initialized it to primary_len at the
     * beginning of the function.
     */
    ASSERT(len == 0);
}

//----------------------------------------------------------------------
int
PrimaryBlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *b)
{
    (void) b;
    return buf->append("Primary");
}

} // namespace dtn
