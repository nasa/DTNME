/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "BundleProtocolVersion7.h"
#include "ExpirationTimer.h"

#include "bundling/BundleDaemonStorage.h"
#include "storage/GlobalStore.h"
#include "storage/BundleStore.h"

namespace dtn {

//----------------------------------------------------------------------
void
Bundle::init(bundleid_t id)
{
    // set the received time as quickly as possible
    received_time_.get_time();

    bundleid_                   = id;
    primary_block_crc_type_     = 2; 
    highest_rcvd_block_number_  = 1;  // every bundle must have a Primary (0) and Payload (1) Block
    is_admin_                   = false;
    do_not_fragment_            = false;
    refcount_                   = 0;
    in_datastore_               = false;
    custody_requested_          = false;
    local_custody_              = false;
    bibe_custody_               = false;
    singleton_dest_             = true;
    priority_                   = COS_NORMAL;
    receive_rcpt_               = false;
    custody_rcpt_               = false;
    forward_rcpt_               = false;
    delivery_rcpt_              = false;
    deletion_rcpt_              = false;
    app_acked_rcpt_             = false;
    orig_length_                = 0;
    expiration_millis_          = 0;
    owner_                      = "";
    fragmented_incoming_        = false;
    freed_                      = false;
    payload_space_reserved_     = false;
    queued_for_datastore_       = false;
    in_storage_queue_           = false;
    deleting_                   = false;
    manually_deleting_          = false;
    hop_count_                  = 0;
    hop_limit_                  = 0;
    prev_bundle_age_millis_            = 0;
    has_bundle_age_block_       = false;
    req_time_in_status_rpt_     = false;
    frag_created_from_bundleid_ = 0;


    // as per the spec, the creation timestamp should be calculated as
    // seconds since 1/1/2000, and since the bundle id should be
    // monotonically increasing, it's safe to use that for the seqno

    if ((bp_version_ == BundleProtocol::BP_VERSION_7) && 
        (BundleProtocol::params_.use_bundle_age_block_)) {

        // creating new bundle sourced locally
        set_creation_ts(0, bundleid_);

        // new bundle so previous age is zero and also sets the has_age_block flag
        set_prev_bundle_age_millis(0);
    } else {
        set_creation_ts(BundleTimestamp::get_current_time(), bundleid_);
    }

    // Agregate Custody parameters
    custodyid_          = 0;
    custody_dest_       = "bpv6"; // default to BPv6 compatibility for custody ID assignment
    cteb_valid_         = false;
    cteb_custodyid_     = 0;

#ifdef ECOS_ENABLED
    ecos_enabled_ = false;
    ecos_flags_ = 0;
    ecos_ordinal_ = 0;
    ecos_flowlabel_ = 0;
#endif

    // This identifier provides information about when a local Bundle
    // object was created so that bundles with the same GBOF-ID can be
    // distinguished. We have to keep a copy separate from creation_ts_
    // because that will be set to the actual BP creation time if this
    // bundle was received from a peer, or is the result of
    // fragmentation, etc.

    // XXX/ hence, let's not break functionality by setting the internal
    // timestamp to 0. 
    extended_id_ = creation_ts();

    expiration_timer_ = nullptr;
    //log_debug_p("/dtn/bundle", "Bundle::init bundle id %" PRIbid, id);

    recv_blocks_ = std::make_shared<BlockInfoVec>();
    api_blocks_ = std::make_shared<BlockInfoVec>();
}

//----------------------------------------------------------------------
Bundle::Bundle(int32_t bp_version, BundlePayload::location_t location)
    : Logger("Bundle", "/dtn/bundle/bundle"),
      payload_(&lock_), fwdlog_(&lock_, this), xmit_blocks_(&lock_),
      recv_metadata_("recv_metadata")
{
    bundleid_t id = GlobalStore::instance()->next_bundleid();
    bp_version_ = bp_version;
    init(id);
    payload_.init(id, location);
}

//----------------------------------------------------------------------
Bundle::Bundle(const oasys::Builder&)
    : Logger("Bundle", "/dtn/bundle/bundle"),
      payload_(&lock_), fwdlog_(&lock_, this), xmit_blocks_(&lock_),
      recv_metadata_("recv_metadata")
{
    // don't do anything here except set the id to a bogus default
    // value and make sure the expiration timer is NULL, since the
    // fields are set and the payload initialized when loading from
    // the database
    bp_version_ = BundleProtocol::BP_VERSION_UNKNOWN;
    init(0xffffffff);
}

//----------------------------------------------------------------------
Bundle::~Bundle()
{
    //log_debug_p("/dtn/bundle/free", "destroying bundle id %" PRIbid, bundleid_);

    ASSERT(mappings_.size() == 0);
    bundleid_ = 0xdeadf00d;

    if (!BundleDaemon::shutting_down()) {
        ASSERTF(expiration_timer_ == nullptr,
                "bundle deleted with pending expiration timer");
    }

}

//----------------------------------------------------------------------
int
Bundle::format(char* buf, size_t sz) const
{
    if (is_admin()) {
        return snprintf(buf, sz, "bundle id %" PRIbid " [%s -> %s %zu byte payload, is_admin]",
                        bundleid_, source().c_str(), dest_.c_str(),
                        payload_.length());
    } else if (is_fragment()) {
        return snprintf(buf, sz, "bundle id %" PRIbid " [%s -> %s %zu byte payload, fragment @%zu/%zu]",
                        bundleid_, source().c_str(), dest_.c_str(),
                        payload_.length(), frag_offset(), orig_length_);
    } else {
        return snprintf(buf, sz, "bundle id %" PRIbid " [%s -> %s %zu byte payload]",
                        bundleid_, source().c_str(), dest_.c_str(),
                        payload_.length());
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose(oasys::StringBuffer* buf)
{

#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("bundle id %" PRIbid ":\n", bundleid_);
    buf->appendf("  Protocol Version: %" PRIi32 "\n", bp_version_);
    buf->appendf("         Global ID: %s\n", gbofid_str().c_str());
    buf->appendf("            source: %s\n", source().c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf("           replyto: %s\n", replyto_.c_str());
    buf->appendf("           prevhop: %s\n", prevhop_.c_str());
    buf->appendf("         hop count: %" PRIu64 "\n", hop_count_);
    buf->appendf("         hop limit: %" PRIu64 "\n", hop_limit_);
    buf->appendf("    payload_length: %zu\n", payload_.length());
    buf->appendf("          priority: %d\n", priority_);
    buf->appendf(" custody_requested: %s\n", bool_to_str(custody_requested_));
    buf->appendf("     local_custody: %s\n", bool_to_str(local_custody_));
    buf->appendf("      bibe_custody: %s\n", bool_to_str(bibe_custody_));
    buf->appendf("    singleton_dest: %s\n", bool_to_str(singleton_dest_));
    buf->appendf("      receive_rcpt: %s\n", bool_to_str(receive_rcpt_));
    buf->appendf("      custody_rcpt: %s\n", bool_to_str(custody_rcpt_));
    buf->appendf("      forward_rcpt: %s\n", bool_to_str(forward_rcpt_));
    buf->appendf("     delivery_rcpt: %s\n", bool_to_str(delivery_rcpt_));
    buf->appendf("     deletion_rcpt: %s\n", bool_to_str(deletion_rcpt_));
    buf->appendf("    app_acked_rcpt: %s\n", bool_to_str(app_acked_rcpt_));
    buf->appendf("       creation_ts: %" PRIu64 ".%" PRIu64 "\n",
                 creation_ts().seconds_, creation_ts().seqno_);

    if (time_to_expiration_secs() < 0) {
        buf->appendf("        expiration: %" PRIu64 " secs (expired)\n", expiration_secs());
    } else {
        buf->appendf("        expiration: %" PRIu64 " secs (%" PRIi64 " left)\n", expiration_secs(),
                     time_to_expiration_secs());
    }

    buf->appendf("  bundle age block: %s\n", bool_to_str(has_bundle_age_block_));
    buf->appendf("      received age: %" PRIu64 " (secs)\n", prev_bundle_age_secs());



    const char *fmtstr="%Y-%m-%d %H:%M:%S";
    const time_t rcv_time = (const time_t) received_time_.sec_;
    struct tm tmval_buf;
    struct tm *tmval;
    tmval = gmtime_r(&rcv_time, &tmval_buf);
    char rcv_date[64];
    strftime(rcv_date, 64, fmtstr, tmval);

    buf->appendf("     received time: %s GMT\n", rcv_date);


    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment()));
    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
    buf->appendf("   do_not_fragment: %s\n", bool_to_str(do_not_fragment_));
    buf->appendf("       orig_length: %zu\n", orig_length_);
    buf->appendf("       frag_offset: %zu\n", frag_offset());

    buf->appendf("    cteb was valid: %s\n", bool_to_str(cteb_valid()));
    buf->appendf("   cteb custody id: %" PRIu64 "\n", cteb_custodyid());
    buf->appendf("  local custody id: %" PRIu64 "\n", custodyid());

#ifdef ECOS_ENABLED
    if (ecos_enabled()) {
        buf->appendf("        ECOS flags: %u\n", ecos_flags_);
        buf->appendf("      ECOS ordinal: %u\n", ecos_ordinal_);
        buf->appendf("   ECOS flow label: %" PRIu64 "\n", ecos_flowlabel_);
    }
#endif

    buf->appendf("          refcount: %d\n", refcount_);

    buf->append("\n");

    buf->appendf("forwarding log:\n");
    fwdlog_.dump(buf);
    buf->append("\n");

    oasys::ScopeLock l(&lock_, "Bundle::format_verbose");
    buf->appendf("queued on %zu lists:\n", mappings_.size());
    for (BundleMappings::iterator i = mappings_.begin();
         i != mappings_.end(); ++i) {
        SPBMapping bmap ( *i );
        buf->appendf("\t%s\n", bmap->list()->name().c_str());
    }

    if (recv_blocks_->size() > 0) {
        buf->append("\nrecv blocks:");
        for (BlockInfoVec::iterator iter = recv_blocks_->begin();
             iter != recv_blocks_->end();
             ++iter)
        {
            SPtr_BlockInfo blkptr = *iter;

            buf->appendf("\n type: 0x%02x ", blkptr->type());
            BlockProcessor *owner;
            if (bp_version() == BundleProtocol::BP_VERSION_7) {
                owner = BundleProtocolVersion7::find_processor(blkptr->type());
            } else {
                owner = BundleProtocolVersion6::find_processor(blkptr->type());
            }

            if (blkptr->type()!=owner->block_type()){
                blkptr->set_owner(owner);
            }
            if (blkptr->data_offset() == 0)
                buf->append("(runt)");
            else {
                if (!blkptr->complete())
                    buf->append("(incomplete) ");
                buf->appendf("data length: %" PRIu64, blkptr->full_length());
                buf->appendf(" -%d- ", owner->block_type());
                owner->format(buf, blkptr.get());
            }
        }
    } else {
        buf->append("\nno recv_blocks");
    }

    if (api_blocks_->size() > 0) {
        buf->append("\napi_blocks:");
        for (BlockInfoVec::iterator iter = api_blocks_->begin();
             iter != api_blocks_->end();
             ++iter)
        {
            SPtr_BlockInfo blkptr = *iter;

            buf->appendf("\n type: 0x%02x data length: %" PRIu64,
                         blkptr->type(), blkptr->full_length());
            buf->append(" -- ");
            blkptr->owner()->format(buf, blkptr.get());
       }
    } else {
        buf->append("\nno api_blocks");
    }
    buf->append("\n");
}

//----------------------------------------------------------------------
void
Bundle::serialize(oasys::SerializeAction* a)
{
    EndpointID src_id(source());
    uint64_t creation_ts_secs = creation_ts().seconds_;
    uint64_t creation_ts_seqno = creation_ts().seqno_;
    bool is_frag = is_fragment();
    uint64_t frag_offset = gbofid_.frag_offset();

    a->process("bundleid", &bundleid_);
    a->process("bp_version", &bp_version_);
    a->process("primary_crc_type", &primary_block_crc_type_);
    a->process("highest_rcvd_block_num", &highest_rcvd_block_number_);
    a->process("is_fragment", &is_frag);
    a->process("is_admin", &is_admin_);
    a->process("do_not_fragment", &do_not_fragment_);
    a->process("source",  &src_id);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("prevhop", &prevhop_);    
    a->process("hop_count", &hop_count_);
    a->process("hop_limit", &hop_limit_);
    a->process("priority", &priority_);
    a->process("custody_requested", &custody_requested_);
    a->process("local_custody", &local_custody_);
    a->process("bibe_custody", &bibe_custody_);
    a->process("singleton_dest", &singleton_dest_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("receive_rcpt", &receive_rcpt_);
    a->process("forward_rcpt", &forward_rcpt_);
    a->process("delivery_rcpt", &delivery_rcpt_);
    a->process("deletion_rcpt", &deletion_rcpt_);
    a->process("app_acked_rcpt", &app_acked_rcpt_);
    a->process("req_time_in_status_rpt", &req_time_in_status_rpt_);
    a->process("creation_ts_seconds", &creation_ts_secs);
    a->process("creation_ts_seqno", &creation_ts_seqno);
    a->process("expiration", &expiration_millis_);
    a->process("payload", &payload_);
    a->process("orig_length", &orig_length_);
    a->process("frag_offset", &frag_offset);
    a->process("owner", &owner_);
    a->process("extended_id_seconds", &extended_id_.seconds_);
    a->process("extended_id_seqno", &extended_id_.seqno_);
    a->process("recv_blocks", recv_blocks_.get());
    a->process("api_blocks", api_blocks_.get());

    a->process("has_bundle_age_block", &has_bundle_age_block_);
    a->process("prev_bundle_age", &prev_bundle_age_millis_);
    a->process("received_time_secs", &received_time_.sec_);
    a->process("received_time_usecs", &received_time_.usec_);


    // a->process("metadata", &recv_metadata_); // XXX/kscott

    a->process("custodyid", &custodyid_);
    a->process("custody_dest", &custody_dest_);
    a->process("cteb_valid", &cteb_valid_);
    a->process("cteb_custodyid", &cteb_custodyid_);


#ifdef ECOS_ENABLED
    a->process("ecos_enabled", &ecos_enabled_);
    a->process("ecos_flags", &ecos_flags_);
    a->process("ecos_ordinal", &ecos_ordinal_);
    a->process("ecos_flowlabel", &ecos_flowlabel_);
#endif

    // serialize the forwarding log
    // Changes to the forwarding log result in the bundle being
    // updated on disk.
    if (BundleDaemon::params_.persistent_fwd_logs_) {
        a->process("forwarding_log", &fwdlog_);
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        in_datastore_ = true;
        payload_.init_from_store(bundleid_);
        set_source(src_id);
        set_creation_ts(creation_ts_secs, creation_ts_seqno);
        set_fragment(is_frag, frag_offset, payload_.length());
    }

    // Call consume() on each of the blocks?
}
    
//----------------------------------------------------------------------
void
Bundle::set_highest_rcvd_block_number(uint64_t block_num)
{
    if (block_num > highest_rcvd_block_number_) {
        highest_rcvd_block_number_ = block_num;
    }
}

//----------------------------------------------------------------------
void
Bundle::copy_metadata(Bundle* new_bundle) const
{
    new_bundle->bp_version_             = bp_version_;
    new_bundle->is_admin_               = is_admin_;
    new_bundle->set_is_fragment(gbofid_.is_fragment());
    new_bundle->do_not_fragment_        = do_not_fragment_;
    new_bundle->set_source(source());
    new_bundle->dest_                   = dest_;
    new_bundle->custodian_              = custodian_;
    new_bundle->replyto_                = replyto_;
    new_bundle->priority_               = priority_;
    new_bundle->custody_requested_      = custody_requested_;
    new_bundle->local_custody_          = false;
    new_bundle->bibe_custody_           = false;
    new_bundle->singleton_dest_         = singleton_dest_;
    new_bundle->custody_rcpt_           = custody_rcpt_;
    new_bundle->receive_rcpt_           = receive_rcpt_;
    new_bundle->forward_rcpt_           = forward_rcpt_;
    new_bundle->delivery_rcpt_          = delivery_rcpt_;
    new_bundle->deletion_rcpt_          = deletion_rcpt_;
    new_bundle->app_acked_rcpt_         = app_acked_rcpt_;
    new_bundle->set_creation_ts(creation_ts());
    new_bundle->expiration_millis_      = expiration_millis_;

    new_bundle->prev_bundle_age_millis_ = prev_bundle_age_millis_;
    new_bundle->has_bundle_age_block_   = has_bundle_age_block_;
    new_bundle->received_time_          = received_time_;

    new_bundle->mutable_prevhop()->assign(prevhop());
    new_bundle->primary_block_crc_type_ = primary_block_crc_type_;
    new_bundle->highest_rcvd_block_number_ = highest_rcvd_block_number_;
    new_bundle->hop_limit_              = hop_limit_;
    new_bundle->hop_count_              = hop_count_;

#ifdef ECOS_ENABLED
    new_bundle->ecos_enabled_           = ecos_enabled_;
    new_bundle->ecos_flags_             = ecos_flags_;
    new_bundle->ecos_ordinal_           = ecos_ordinal_;
    new_bundle->ecos_flowlabel_         = ecos_flowlabel_;
#endif
}

//----------------------------------------------------------------------
int
Bundle::add_ref(const char* what1, const char* what2)
{
    (void)what1;
    (void)what2;
   
    char refstr[32];
    snprintf(refstr, sizeof(refstr), "Bundle::add_ref: %" PRIbid, bundleid_);
    oasys::ScopeLock l(&lock_, refstr);

    ASSERTF(freed_ == false, "Bundle::add_ref on bundle %" PRIbid " (%p)"
            "called when bundle is already being freed!", bundleid_, this);

    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    //log_debug_p("/dtn/bundle/refs",
    //            "bundle id %" PRIbid " (%p): refcount %d -> %d (%zu mappings) add %s %s",
    //            bundleid_, this, refcount_ - 1, refcount_,
    //            mappings_.size(), what1, what2);

    // if this is the first time we're adding a reference, then put it
    // on the all_bundles, which itself adds another reference to it.
    // note that we need to be careful to drop the scope lock before
    // calling push_back.
    if (ret == 1) {
        l.unlock(); // release scope lock
        BundleDaemon::instance()->all_bundles()->push_back(this);
    }
    
    return ret;
}

//----------------------------------------------------------------------
int
Bundle::del_ref(const char* what1, const char* what2)
{
    (void)what1;
    (void)what2;
    
    oasys::ScopeLock l(&lock_, "Bundle::del_ref");

    int ret = --refcount_;
    //log_debug_p("/dtn/bundle/refs2",
    //            "bundle id %" PRIbid " (%p): freed_(%d) refcount %d -> %d (%zu mappings) del %s:%s",
    //            bundleid_, this, freed_, refcount_ + 1, refcount_,
    //            mappings_.size(), what1, what2);
#if 0
    log_debug_p("/dtn/bundle/refs2",
                "queued on %zu lists:\n", mappings_.size());
    for (BundleMappings::iterator i = mappings_.begin();
         i != mappings_.end(); ++i) {
        SPBMapping bmap ( *i );
        log_debug_p("/dtn/bundle/refs2", "\t%s\n", bmap->list()->name().c_str());
    }
#endif


    if (refcount_ == 1) {
        ASSERTF(freed_ == false,  "Bundle::del_ref on bundle %" PRIbid " (%p)"
                "called when bundle is freed but has %d references",
                bundleid_, this, refcount_);
        
        //log_debug("bundle id %" PRIbid " (%p): one reference remaining, posting free event",
        //          bundleid_, this);

        // One final check to see if delete from database is needed
        if (queued_for_datastore_) {
            BundleDaemon::instance()->bundle_delete_from_storage(this);
            queued_for_datastore_ = false; // prevent BundleDaemon from posting delete also
        }

        freed_ = true;

        BundleDaemon::instance()->post(new BundleFreeEvent(this));

    } else if (refcount_ > 0) {
        ASSERTF(freed_ == false,  "Bundle::del_ref on bundle %" PRIbid " (%p)"
                "called when bundle is freed but has %d references",
                bundleid_, this, refcount_);
    
        return ret;

    } else if (refcount_ == 0) {
        //log_debug_p("/dtn/bundle",
        //            "bundle id %" PRIbid " (%p): last reference removed",
        //            bundleid_, this);
        ASSERTF(freed_ == true,
                "Bundle %" PRIbid " (%p) refcount is zero but bundle wasn't properly freed",
                bundleid_, this);
    } else if (refcount_ < 0) {
        log_always_p("/dtn/bundle",
                    "bundle id %" PRIbid " (%p): refcount_(%d) < 0!",
                     bundleid_, this, refcount_);
   }
    
    return 0;
}


//----------------------------------------------------------------------
uint64_t
Bundle::current_bundle_age_secs() const
{
    uint64_t cur_age_secs = 0;
    if (has_bundle_age_block_)
    {
        cur_age_secs = current_bundle_age_millis() / 1000;
    }
    else
    {
        cur_age_secs = BundleTimestamp::get_current_time();
        cur_age_secs -= creation_ts().seconds_;
    }
    return cur_age_secs;
}

//----------------------------------------------------------------------
uint64_t
Bundle::current_bundle_age_millis() const
{
    uint64_t cur_age_millis = 0;
    if (has_bundle_age_block_)
    {
        cur_age_millis = prev_bundle_age_millis_ + received_time_.elapsed_ms();
    }
    else
    {
        cur_age_millis = BundleTimestamp::get_current_time();
        cur_age_millis -= creation_ts().seconds_;
        cur_age_millis *= 1000;
    }
    return cur_age_millis;
}

//----------------------------------------------------------------------
int64_t
Bundle::time_to_expiration_secs()
{
    int64_t result = time_to_expiration_millis();
    if (result > 0) {
        result /= 1000;
    }
    return result;
}

//----------------------------------------------------------------------
int64_t
Bundle::time_to_expiration_millis()
{
    int64_t result = expiration_millis() - current_bundle_age_millis();
    if (result < 0)
    {
        result = -1;
    }
    return result;
}

//----------------------------------------------------------------------
size_t
Bundle::num_mappings()
{
    oasys::ScopeLock l(&lock_, "Bundle::num_mappings");
    return mappings_.size();
}

//----------------------------------------------------------------------
BundleMappings*
Bundle::mappings()
{
    ASSERTF(lock_.is_locked_by_me(),
            "Must lock Bundle before using mappings iterator");
    
    return &mappings_;
}

//----------------------------------------------------------------------
bool
Bundle::is_queued_on(const BundleListBase* bundle_list)
{
    oasys::ScopeLock l(&lock_, "Bundle::is_queued_on");
    return mappings_.contains(bundle_list);
}

//----------------------------------------------------------------------
bool
Bundle::validate(oasys::StringBuffer* errbuf)
{
    if (!source().valid()) {
        errbuf->appendf("invalid source eid [%s]", source().c_str());
        return false;
    }
    
    if (!dest_.valid()) {
        errbuf->appendf("invalid dest eid [%s]", dest_.c_str());
        return false;
    }

    if (!replyto_.valid()) {
        errbuf->appendf("invalid replyto eid [%s]", replyto_.c_str());
        return false;
    }

    if (!custodian_.valid()) {
        errbuf->appendf("invalid custodian eid [%s]", custodian_.c_str());
        return false;
    }

    return true;
    
}

//----------------------------------------------------------------------
SPtr_ExpirationTimer
Bundle::expiration_timer()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    return expiration_timer_;
}



//----------------------------------------------------------------------
void
Bundle::set_expiration_timer(SPtr_ExpirationTimer& e)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    expiration_timer_ = e;
}


//----------------------------------------------------------------------
void
Bundle::clear_expiration_timer()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    expiration_timer_ = nullptr;
}

} // namespace dtn
