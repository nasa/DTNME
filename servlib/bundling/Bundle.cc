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
#include "SDNV.h"
#include "ExpirationTimer.h"

#include "bundling/BundleDaemonStorage.h"
#include "naming/IMCScheme.h"
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

    sptr_custodian_ = BD_MAKE_EID_NULL();
    sptr_replyto_ = BD_MAKE_EID_NULL();
    sptr_dest_ = BD_MAKE_EID_NULL();
    sptr_prevhop_ = BD_MAKE_EID_NULL();

    primary_block_crc_type_     = 2; 
    highest_rcvd_block_number_  = 1;  // every bundle must have a Primary (0) and Payload (1) Block
    is_admin_                   = false;
    do_not_fragment_            = false;
    refcount_                   = 0;
    in_datastore_               = false;
    custody_requested_          = false;
    local_custody_              = false;
    bibe_custody_               = false;
    singleton_dest_flag_        = true;
    priority_                   = COS_NORMAL;
    receive_rcpt_               = false;
    custody_rcpt_               = false;
    forward_rcpt_               = false;
    delivery_rcpt_              = false;
    deletion_rcpt_              = false;
    app_acked_rcpt_             = false;
    orig_length_                = 0;
    expiration_millis_          = 0;
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

    if ((bp_version_ != BundleProtocol::BP_VERSION_6) && 
        (BundleProtocol::params_.use_bundle_age_block_)) {

        // creating new bundle sourced locally
        set_creation_ts(0, bundleid_);

        // new bundle so previous age is zero and also sets the has_age_block flag
        set_prev_bundle_age_millis(0);
    } else if (bp_version_ == BundleProtocol::BP_VERSION_6) {
        set_creation_ts(BundleTimestamp::get_current_time_secs(), bundleid_);
    } else {
        set_creation_ts(BundleTimestamp::get_current_time_millis(), bundleid_);
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
    // value and make sure the expiration timer is nullptr, since the
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
std::string
Bundle::gbofid_str() const
{
    return gbofid_.str();
}

//----------------------------------------------------------------------
int
Bundle::format(char* buf, size_t sz) const
{
    std::string bpv_str = "BPv7";
    if (is_bpv6()) {
        bpv_str = "BPv6";
    }

    if (is_admin()) {
        return snprintf(buf, sz, "%s bundle id %" PRIbid " [%s -> %s %zu byte payload, is_admin]",
                        bpv_str.c_str(), bundleid_, source()->c_str(), sptr_dest_->c_str(),
                        payload_.length());
    } else if (is_fragment()) {
        return snprintf(buf, sz, "%s bundle id %" PRIbid " [%s -> %s %zu byte payload, fragment @%zu/%zu]",
                        bpv_str.c_str(), bundleid_, source()->c_str(), sptr_dest_->c_str(),
                        payload_.length(), frag_offset(), orig_length_);
    } else {
        return snprintf(buf, sz, "%s bundle id %" PRIbid " [%s -> %s %zu byte payload]",
                        bpv_str.c_str(), bundleid_, source()->c_str(), sptr_dest_->c_str(),
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
    buf->appendf("            source: %s\n", source()->c_str());
    buf->appendf("              dest: %s\n", sptr_dest_->c_str());
    buf->appendf("         custodian: %s\n", sptr_custodian_->c_str());
    buf->appendf("           replyto: %s\n", sptr_replyto_->c_str());
    buf->appendf("           prevhop: %s\n", sptr_prevhop_->c_str());
    buf->appendf("         hop count: %" PRIu64 "\n", hop_count_);
    buf->appendf("         hop limit: %" PRIu64 "\n", hop_limit_);
    buf->appendf("    payload_length: %zu\n", payload_.length());
    buf->appendf("          priority: %d\n", priority_);
    buf->appendf(" custody_requested: %s\n", bool_to_str(custody_requested_));
    buf->appendf("     local_custody: %s\n", bool_to_str(local_custody_));
    buf->appendf("      bibe_custody: %s\n", bool_to_str(bibe_custody_));
    buf->appendf("    singleton_dest: %s\n", bool_to_str(singleton_dest()));
    buf->appendf("      receive_rcpt: %s\n", bool_to_str(receive_rcpt_));
    buf->appendf("      custody_rcpt: %s\n", bool_to_str(custody_rcpt_));
    buf->appendf("      forward_rcpt: %s\n", bool_to_str(forward_rcpt_));
    buf->appendf("     delivery_rcpt: %s\n", bool_to_str(delivery_rcpt_));
    buf->appendf("     deletion_rcpt: %s\n", bool_to_str(deletion_rcpt_));
    buf->appendf("    app_acked_rcpt: %s\n", bool_to_str(app_acked_rcpt_));


    const char *fmtstr="%Y-%m-%d %H:%M:%S";
    struct tm tmval_buf;
    struct tm *tmval;
    char date_str[64];

    time_t tmp_secs = creation_time_secs() + BundleTimestamp::TIMEVAL_CONVERSION_SECS;

    // convert seconds to a time string
    tmval = gmtime_r(&tmp_secs, &tmval_buf);
    strftime(date_str, 64, fmtstr, tmval);

    if (is_bpv6()) {
        buf->appendf("       creation_ts: %zu.%zu  (%s)\n",
                     creation_ts().secs_or_millisecs_, creation_ts().seqno_, date_str);
    } else {
        size_t millisecs = creation_ts().secs_or_millisecs_ % 1000;
        buf->appendf("       creation_ts: %zu.%zu  (%s.%3.3zu)\n",
                     creation_ts().secs_or_millisecs_, creation_ts().seqno_, date_str, millisecs);
    }

    if (time_to_expiration_secs() < 0) {
        buf->appendf("        expiration: %" PRIu64 " secs (expired)\n", expiration_secs());
    } else {
        buf->appendf("        expiration: %" PRIu64 " secs (%" PRIi64 " left)\n", expiration_secs(),
                     time_to_expiration_secs());
    }

    buf->appendf("  bundle age block: %s\n", bool_to_str(has_bundle_age_block_));
    buf->appendf("      received age: %" PRIu64 " (secs)\n", prev_bundle_age_secs());



    tmp_secs = received_time_.sec_;
    tmval = gmtime_r(&tmp_secs, &tmval_buf);
    strftime(date_str, 64, fmtstr, tmval);

    buf->appendf("     received time: %s GMT\n", date_str);


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


#ifdef BARD_ENABLED
    buf->appendf("\nBARD debug info:\n");
    buf->appendf("src bytes in use  : %zu\n", bard_in_use_by_src_);
    buf->appendf("src quota reserved: %zu\n", bard_quota_reserved_by_src_);
    buf->appendf("src extquota rsrvd: %zu\n", bard_extquota_reserved_by_src_);
    buf->appendf("dst bytes in use  : %zu\n", bard_in_use_by_dst_);
    buf->appendf("dst quota reserved: %zu\n", bard_quota_reserved_by_dst_);
    buf->appendf("dst extquota rsrvd: %zu\n", bard_extquota_reserved_by_dst_);
    buf->appendf("    restage by src: %s\n", bool_to_str(bard_restage_by_src_));
    buf->appendf(" restage link name: %s\n", bard_restage_link_name_.c_str());
    buf->appendf("\n");
#endif // BARD_ENABLED

    oasys::ScopeLock l(&lock_, "Bundle::format_verbose");

    if (sptr_dest_->is_imc_scheme()) {
        format_verbose_imc_orig_dest_map(buf);
        format_verbose_imc_dest_map(buf);
        format_verbose_imc_dest_nodes_per_link(buf);
        format_verbose_imc_state(buf);
        buf->append("\n");
    }
    buf->append("\n");


    buf->appendf("forwarding log:\n");
    fwdlog_.dump(buf);
    buf->append("\n");


    buf->appendf("queued on %zu lists:\n", mappings_.size());
    for (BundleMappings::iterator i = mappings_.begin();
         i != mappings_.end(); ++i) {
        SPBMapping bmap ( *i );
        buf->appendf("\t%s\n", bmap->list()->name().c_str());
    }
    buf->appendf("refcount: %d\n", refcount_);


    if (recv_blocks_->size() > 0) {
        buf->append("\nrecv blocks:");
        for (BlockInfoVec::iterator iter = recv_blocks_->begin();
             iter != recv_blocks_->end();
             ++iter)
        {
            SPtr_BlockInfo blkptr = *iter;

            buf->appendf("\n type: 0x%02x (%3d)  ", blkptr->type(), blkptr->type());
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
                buf->appendf("data length: %" PRIu64 "  ", blkptr->full_length());
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

            buf->appendf("\n type: 0x%02x (%3d)  ", blkptr->type(), blkptr->type());
            buf->appendf(" data length: %" PRIu64 "  ", blkptr->full_length());
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
    std::string tmp_src_eid = source()->str();
    std::string tmp_dest_eid = sptr_dest_->str();
    std::string tmp_custody_eid = sptr_custodian_->str();
    std::string tmp_replyto_eid = sptr_replyto_->str();
    std::string tmp_prevhop_eid = sptr_prevhop_->str();

    uint64_t creation_ts_time = creation_ts().secs_or_millisecs_;
    uint64_t creation_ts_seqno = creation_ts().seqno_;
    bool is_frag = is_fragment();
    size_t frag_offset = gbofid_.frag_offset();



    a->process("bundleid", &bundleid_);
    a->process("bp_version", &bp_version_);
    a->process("primary_crc_type", &primary_block_crc_type_);
    a->process("highest_rcvd_block_num", &highest_rcvd_block_number_);
    a->process("is_fragment", &is_frag);
    a->process("is_admin", &is_admin_);
    a->process("do_not_fragment", &do_not_fragment_);
    a->process("source",  &tmp_src_eid);
    a->process("dest", &tmp_dest_eid);
    a->process("custodian", &tmp_custody_eid);
    a->process("replyto", &tmp_replyto_eid);
    a->process("prevhop", &tmp_prevhop_eid);    
    a->process("hop_count", &hop_count_);
    a->process("hop_limit", &hop_limit_);
    a->process("priority", &priority_);
    a->process("custody_requested", &custody_requested_);
    a->process("local_custody", &local_custody_);
    a->process("bibe_custody", &bibe_custody_);
    a->process("singleton_dest", &singleton_dest_flag_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("receive_rcpt", &receive_rcpt_);
    a->process("forward_rcpt", &forward_rcpt_);
    a->process("delivery_rcpt", &delivery_rcpt_);
    a->process("deletion_rcpt", &deletion_rcpt_);
    a->process("app_acked_rcpt", &app_acked_rcpt_);
    a->process("req_time_in_status_rpt", &req_time_in_status_rpt_);
    a->process("creation_ts_time", &creation_ts_time);
    a->process("creation_ts_seqno", &creation_ts_seqno);
    a->process("expiration", &expiration_millis_);
    a->process("payload", &payload_);
    a->process("orig_length", &orig_length_);
    a->process("frag_offset", &frag_offset);
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


    // serialize the BundleIMCState info
    bool have_imc_state =  (qptr_imc_state_ != nullptr);
    a->process("have_imc_state", &have_imc_state);

    if (have_imc_state && 
        (a->action_code() == oasys::Serialize::UNMARSHAL)) {

        oasys::ScopeLock scoplok(&lock_, __func__);

        if (validate_bundle_imc_state()) {
            qptr_imc_state_->serialize(a);
        }
    } else if (have_imc_state) {
        qptr_imc_state_->serialize(a);
    }


    // serialize the forwarding log
    // Changes to the forwarding log result in the bundle being
    // updated on disk which adds a lot of I/O
    if (BundleDaemon::params_.persistent_fwd_logs_) {
        a->process("forwarding_log", &fwdlog_);
    }


    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        in_datastore_ = true;
        payload_.init_from_store(bundleid_);

        set_source(tmp_src_eid);
        sptr_dest_ = BD_MAKE_EID(tmp_dest_eid);
        sptr_custodian_ = BD_MAKE_EID(tmp_custody_eid);
        sptr_replyto_ = BD_MAKE_EID(tmp_replyto_eid);
        sptr_prevhop_ = BD_MAKE_EID(tmp_prevhop_eid);

        set_creation_ts(creation_ts_time, creation_ts_seqno);
        set_fragment(is_frag, frag_offset, payload_.length());
    }

    // Call consume() on each of the blocks?
}
    
//----------------------------------------------------------------------
void
Bundle::set_highest_rcvd_block_number(size_t block_num)
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
    new_bundle->sptr_dest_              = sptr_dest_;
    new_bundle->sptr_custodian_         = sptr_custodian_;
    new_bundle->sptr_replyto_           = sptr_replyto_;
    new_bundle->priority_               = priority_;
    new_bundle->custody_requested_      = custody_requested_;
    new_bundle->local_custody_          = false;
    new_bundle->bibe_custody_           = false;
    new_bundle->singleton_dest_flag_    = singleton_dest_flag_;
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
    new_bundle->received_time_.sec_      = received_time_.sec_;
    new_bundle->received_time_.usec_     = received_time_.usec_;

    new_bundle->mutable_prevhop()       = sptr_prevhop_;
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
        } else if (payload_space_reserved_) {
           BundleStore::instance()->release_payload_space(durable_size());
           payload_space_reserved_ = false;
        }

        freed_ = true;

#ifdef BARD_ENABLED
        if (!BundleDaemon::shutting_down()) {
            payload_.delete_payload_file ();
            BundleDaemon::instance()->bard_bundle_deleted(this);
        }
#endif // BARD_ENABLED


        BundleFreeEvent* event_to_post;
        event_to_post = new BundleFreeEvent(this);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

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
size_t
Bundle::creation_time_millis()  const
{
    if (is_bpv6()) {
        return creation_ts().secs_or_millisecs_ * 1000;
    } else {
        return creation_ts().secs_or_millisecs_;
    }
}

//----------------------------------------------------------------------
size_t
Bundle::creation_time_secs()  const
{
    if (is_bpv6()) {
        return creation_ts().secs_or_millisecs_;
    } else {
        return creation_ts().secs_or_millisecs_ / 1000;
    }
}


//----------------------------------------------------------------------
uint64_t
Bundle::current_bundle_age_secs() const
{
    size_t cur_age_secs = current_bundle_age_millis() / 1000;
    return cur_age_secs;
}

//----------------------------------------------------------------------
size_t
Bundle::current_bundle_age_millis() const
{
    size_t cur_age_millis = 0;
    if (has_bundle_age_block_)
    {
        cur_age_millis = prev_bundle_age_millis_ + received_time_.elapsed_ms();
    }
    else
    {
        cur_age_millis = BundleTimestamp::get_current_time_millis();
        cur_age_millis -= creation_time_millis();
    }
    return cur_age_millis;
}

//----------------------------------------------------------------------
int64_t
Bundle::time_to_expiration_secs() const
{
    int64_t result = time_to_expiration_millis();
    if (result > 0) {
        result /= 1000;
    }
    return result;
}

//----------------------------------------------------------------------
int64_t
Bundle::time_to_expiration_millis() const
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
    if (!source()->valid()) {
        errbuf->appendf("invalid source eid [%s]", source()->c_str());
        return false;
    }
    
    if (!sptr_dest_->valid()) {
        errbuf->appendf("invalid dest eid [%s]", sptr_dest_->c_str());
        return false;
    }

    if (!sptr_replyto_->valid()) {
        errbuf->appendf("invalid replyto eid [%s]", sptr_replyto_->c_str());
        return false;
    }

    if (!sptr_custodian_->valid()) {
        errbuf->appendf("invalid custodian eid [%s]", sptr_custodian_->c_str());
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
Bundle::set_expiration_timer(SPtr_ExpirationTimer e)
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

#ifdef BARD_ENABLED
//----------------------------------------------------------------------
size_t
Bundle::bard_in_use(bool by_src)
{
    if (by_src) {
        return bard_in_use_by_src_;
    } else {
        return bard_in_use_by_dst_;
    }
}

//----------------------------------------------------------------------
size_t
Bundle::bard_quota_reserved(bool by_src)
{
    if (by_src) {
        return bard_quota_reserved_by_src_;
    } else {
        return bard_quota_reserved_by_dst_;
    }
}

//----------------------------------------------------------------------
size_t
Bundle::bard_extquota_reserved(bool by_src)
{
    if (by_src) {
        return bard_extquota_reserved_by_src_;
    } else {
        return bard_extquota_reserved_by_dst_;
    }
}

//----------------------------------------------------------------------
void
Bundle::set_bard_in_use(bool by_src, size_t t)
{
    if (by_src) {
        bard_in_use_by_src_ = t;
    } else {
        bard_in_use_by_dst_ = t;
    }
}

//----------------------------------------------------------------------
void
Bundle::set_bard_quota_reserved(bool by_src, size_t t)
{
    if (by_src) {
        bard_quota_reserved_by_src_ = t;
    } else {
        bard_quota_reserved_by_dst_ = t;
    }
}

//----------------------------------------------------------------------
void
Bundle::set_bard_extquota_reserved(bool by_src, size_t t)
{
    if (by_src) {
        bard_extquota_reserved_by_src_ = t;
    } else {
        bard_extquota_reserved_by_dst_ = t;
    }
}

//----------------------------------------------------------------------
void
Bundle::add_failed_restage_link_name(std::string& link_name)
{
    if (qptr_failed_restage_link_names_ == nullptr) {
        qptr_failed_restage_link_names_ = QPtr_RestageLinkNameMap(new RESTAGE_LINK_NAME_MAP());
    }

    qptr_failed_restage_link_names_->insert(std::pair<std::string,bool>(link_name, true));
}

//----------------------------------------------------------------------
bool
Bundle::find_failed_restage_link_name(std::string& link_name)
{    
    bool result = false;

    if (qptr_failed_restage_link_names_) {
        RESTAGE_LINK_NAME_MAP::iterator iter;
        iter = qptr_failed_restage_link_names_->find(link_name);
        result = (iter != qptr_failed_restage_link_names_->end()); 
    }

    return result;
}


#endif // BARD_ENABLED

//----------------------------------------------------------------------
bool
Bundle::validate_bundle_imc_state()
{
    ASSERT(lock_.is_locked_by_me());

    if (!qptr_imc_state_) {
        qptr_imc_state_ = std::unique_ptr<BundleIMCState>(new BundleIMCState());
    }

    return (qptr_imc_state_ != nullptr);
}

//----------------------------------------------------------------------
bool
Bundle::validate_bundle_imc_state() const
{
    ASSERT(lock_.is_locked_by_me());

    return (qptr_imc_state_ != nullptr);
}

//----------------------------------------------------------------------
size_t
Bundle::num_imc_dest_nodes() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_dest_nodes();
    }
    return result;
}

//----------------------------------------------------------------------
void
Bundle::add_imc_orig_dest_node(size_t dest_node)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_orig_dest_node(dest_node);
    }
}

//----------------------------------------------------------------------
void
Bundle::add_imc_dest_node(size_t dest_node)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_dest_node(dest_node);
    }
}

//----------------------------------------------------------------------
void
Bundle::imc_dest_node_handled(size_t dest_node)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->imc_dest_node_handled(dest_node);
    }
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
Bundle::imc_dest_map() const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_dest_map();
    }

    return sptr_result;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
Bundle::imc_orig_dest_map() const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_orig_dest_map();
    }

    return sptr_result;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
Bundle::imc_alternate_dest_map() const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_alternate_dest_map();
    }

    return sptr_result;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
Bundle::imc_dest_map_for_link(std::string& linkname) const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_dest_map_for_link(linkname);
    }

    return sptr_result;
}

//----------------------------------------------------------------------
std::string
Bundle::imc_link_name_by_index(size_t index)
{
    std::string result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_link_name_by_index(index);
    }
    return result;
}

//----------------------------------------------------------------------
SPtr_IMC_PROCESSED_REGIONS_MAP
Bundle::imc_processed_regions_map() const
{
    SPtr_IMC_PROCESSED_REGIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_processed_regions_map();
    }

    return sptr_result;
}

//----------------------------------------------------------------------
size_t
Bundle::num_imc_processed_regions() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_processed_regions();
    }

    return result;
}

//----------------------------------------------------------------------
bool
Bundle::is_imc_region_processed(size_t region)
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->is_imc_region_processed(region);
    }

    return result;
}

//----------------------------------------------------------------------
void
Bundle::add_imc_region_processed(size_t region)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_region_processed(region);
    }
}

//----------------------------------------------------------------------
void
Bundle::add_imc_dest_node_via_link(std::string linkname, size_t dest_node)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_dest_node_via_link(linkname, dest_node);
    }
}

//----------------------------------------------------------------------
void
Bundle::copy_all_unhandled_nodes_to_via_link(std::string linkname)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->copy_all_unhandled_nodes_to_via_link(linkname);
    }
}

//----------------------------------------------------------------------
void
Bundle::clear_imc_link_lists()
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->clear_imc_link_lists();
    }
}

//----------------------------------------------------------------------
void
Bundle::imc_link_transmit_success(std::string linkname)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->imc_link_transmit_success(linkname);
    }
}

//----------------------------------------------------------------------
void
Bundle::imc_link_transmit_failure(std::string linkname)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->imc_link_transmit_failure(linkname);
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose_imc_orig_dest_map(oasys::StringBuffer* buf)
{
    if (validate_bundle_imc_state()) {
        qptr_imc_state_->format_verbose_imc_orig_dest_map(buf);
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose_imc_dest_map(oasys::StringBuffer* buf)
{
    if (validate_bundle_imc_state()) {
        qptr_imc_state_->format_verbose_imc_dest_map(buf);
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose_imc_dest_nodes_per_link(oasys::StringBuffer* buf)
{
    if (validate_bundle_imc_state()) {
        qptr_imc_state_->format_verbose_imc_dest_nodes_per_link(buf);
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose_imc_state(oasys::StringBuffer* buf)
{
    if (validate_bundle_imc_state()) {
        qptr_imc_state_->format_verbose_imc_state(buf);
    }
}

//----------------------------------------------------------------------
void
Bundle::add_imc_alternate_dest_node(size_t dest_node)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_alternate_dest_node(dest_node);
    }
}


//----------------------------------------------------------------------
void
Bundle::clear_imc_alternate_dest_nodes()
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->clear_imc_alternate_dest_nodes();
    }
}


//----------------------------------------------------------------------
size_t
Bundle::imc_alternate_dest_nodes_count() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);


    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_alternate_dest_nodes_count();
    }
    return result;
}



//----------------------------------------------------------------------
void
Bundle::add_imc_processed_by_node(size_t node_num)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_processed_by_node(node_num);
    }
}

//----------------------------------------------------------------------
bool 
Bundle::imc_has_proxy_been_processed_by_node(size_t node_num)
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_has_proxy_been_processed_by_node(node_num);
    }

    return result;
}


//----------------------------------------------------------------------
SPtr_IMC_PROCESSED_BY_NODE_MAP
Bundle::imc_processed_by_nodes_map() const
{
    SPtr_IMC_PROCESSED_BY_NODE_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_processed_by_nodes_map();
    }

    return sptr_result;
}

//----------------------------------------------------------------------
void
Bundle::copy_imc_processed_by_node_list(Bundle* other_bundle)
{
    oasys::ScopeLock l(&lock_, __func__);
    oasys::ScopeLock other_scoplokl(other_bundle->lock(), __func__);


    if (validate_bundle_imc_state()) {
        SPtr_IMC_PROCESSED_BY_NODE_MAP sptr_other_list;
        sptr_other_list = other_bundle->imc_processed_by_nodes_map();
        qptr_imc_state_->copy_imc_processed_by_node_list(sptr_other_list);
    }
}


//----------------------------------------------------------------------
size_t
Bundle::imc_size_of_sdnv_dest_nodes_array() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_size_of_sdnv_dest_nodes_array();
    }

    return result;
}

//----------------------------------------------------------------------
ssize_t
Bundle::imc_sdnv_encode_dest_nodes_array(u_char* buf_ptr, size_t buf_len) const
{
    ssize_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_sdnv_encode_dest_nodes_array(buf_ptr, buf_len);
    }

    return result;
}

//----------------------------------------------------------------------
size_t
Bundle::imc_size_of_sdnv_processed_regions_array() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_size_of_sdnv_processed_regions_array();
    }

    return result;
}

//----------------------------------------------------------------------
ssize_t
Bundle::imc_sdnv_encode_processed_regions_array(u_char* buf_ptr, size_t buf_len) const
{
    ssize_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_sdnv_encode_processed_regions_array(buf_ptr, buf_len);
    }

    return result;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
Bundle::imc_unrouteable_dest_map() const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_result;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        sptr_result = qptr_imc_state_->imc_unrouteable_dest_map();
    }

    return sptr_result;
}


//----------------------------------------------------------------------
void
Bundle::clear_imc_unrouteable_dest_nodes()
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->clear_imc_unrouteable_dest_nodes();
    }
}


//----------------------------------------------------------------------
void
Bundle::add_imc_unrouteable_dest_node(size_t dest_node, bool in_home_region)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->add_imc_unrouteable_dest_node(dest_node, in_home_region);
    }
}

//----------------------------------------------------------------------
size_t
Bundle::num_imc_home_region_unrouteable() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_home_region_unrouteable();
    }

    return result;
}

//----------------------------------------------------------------------
size_t
Bundle::num_imc_outer_regions_unrouteable() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_outer_regions_unrouteable();
    }

    return result;
}


//----------------------------------------------------------------------
size_t
Bundle::num_imc_nodes_handled() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_nodes_handled();
    }

    return result;
}

//----------------------------------------------------------------------
size_t
Bundle::num_imc_nodes_not_handled() const
{
    size_t result = 0;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->num_imc_nodes_not_handled();
    }

    return result;
}

//----------------------------------------------------------------------
void
Bundle::set_imc_is_proxy_petition(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_is_proxy_petition(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_is_proxy_petition() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_is_proxy_petition();
    }

    return result;
}


//----------------------------------------------------------------------
void
Bundle::set_imc_sync_request(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_sync_request(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_sync_request() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_sync_request();
    }

    return result;
}


//----------------------------------------------------------------------
void
Bundle::set_imc_sync_reply(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_sync_reply(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_sync_reply() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_sync_reply();
    }

    return result;
}


//----------------------------------------------------------------------
void
Bundle::set_imc_is_dtnme_node(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_is_dtnme_node(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_is_dtnme_node() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_is_dtnme_node();
    }

    return result;
}


//----------------------------------------------------------------------
void
Bundle::set_imc_is_router_node(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_is_router_node(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_is_router_node() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_is_router_node();
    }

    return result;
}


//----------------------------------------------------------------------
void
Bundle::set_imc_briefing(bool t)
{
    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        qptr_imc_state_->set_imc_briefing(t);
    }
}

//----------------------------------------------------------------------
bool
Bundle::imc_briefing() const
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __func__);

    if (validate_bundle_imc_state()) {
        result = qptr_imc_state_->imc_briefing();
    }

    return result;
}

//----------------------------------------------------------------------
void
Bundle::add_link_redirect_mapping(const std::string& redirect_link, const std::string& orig_link)
{
    if (!qptr_link_redirect_map_) {
        qptr_link_redirect_map_ = std::unique_ptr<LinkRedirectMap>(new LinkRedirectMap());
    }

    (*qptr_link_redirect_map_)[redirect_link] = orig_link;
}

//----------------------------------------------------------------------
bool
Bundle::get_redirect_orig_link(const std::string& redirect_link, std::string& orig_link) const
{
    bool result = false;

    if (qptr_link_redirect_map_) {
        LinkRedirectIter iter = qptr_link_redirect_map_->find(redirect_link);

        if (iter != qptr_link_redirect_map_->end()) {
            result = true;
            orig_link = iter->second;
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
Bundle::clear_redirect_orig_link(const std::string& redirect_link)
{
    if (qptr_link_redirect_map_) {
        LinkRedirectIter iter = qptr_link_redirect_map_->find(redirect_link);

        if (iter != qptr_link_redirect_map_->end()) {
            qptr_link_redirect_map_->erase(iter);
        }
    }
}


} // namespace dtn
