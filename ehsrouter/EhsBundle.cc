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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>

#include <third_party/oasys/debug/Log.h>


#include "EhsBundle.h"
#include "EhsDtnNode.h"




namespace dtn {


//----------------------------------------------------------------------
EhsBundle::EhsBundle(EhsDtnNode* parent, ExternalRouterIF::extrtr_bundle_ptr_t& bundleptr)
    : parent_(parent)
{
    init();
    process_bundle_report(bundleptr);
}

//----------------------------------------------------------------------
void
EhsBundle::init()
{
    bundleid_ = 0;
    custodyid_ = 0;
    source_ = "dtn:none";
    dest_ = "dtn:none";
//    custodian_ = "dtn:none";
//    replyto_ = "dtn:none";
    prev_hop_ = "dtn:none";
    length_ = 0;
    location_ = "";
    payload_file_ = "";
//    is_fragment_ = false;
//    is_admin_ = false;
    do_not_fragment_ = false;
    priority_ = COS_BULK;
    ecos_flags_ = 0;
    ecos_ordinal_ = 0;
    ecos_flowlabel_ = 0;
    custody_requested_ = false;
    local_custody_ = false;
    singleton_dest_ = true;
    custody_rcpt_ = false;
    receive_rcpt_ = false;
    forward_rcpt_ = false;
    delivery_rcpt_ = false;
    deletion_rcpt_ = false;
    app_acked_rcpt_ = false;
    creation_ts_seconds_ = 0;
    creation_ts_seqno_ = 0;
    expiration_ = 0;
    orig_length_ = 0;
    frag_offset_ = 0;
    frag_length_ = 0;
    local_id_ = 0;
    owner_ = "";
    ipn_dest_node_ = 0;
    is_ipn_dest_ = true;
    is_fwd_link_destination_ = false;
    priority_key_ = "";
    priority_key_set_ = false;;
    refcount_ = 0;
    exiting_ = false;
    deleted_ = false;
    received_from_link_id_ = "";
    not_in_resync_report_ = false;
}

//----------------------------------------------------------------------
void
EhsBundle::process_bundle_report(ExternalRouterIF::extrtr_bundle_ptr_t& bundleptr)
{
    bundleid_ = bundleptr->bundleid_;
    not_in_resync_report_ = false;

    gbofid_str_ = bundleptr->gbofid_str_;
    bp_version_ = bundleptr->bp_version_;
    custodyid_ = bundleptr->custodyid_;
    source_ = bundleptr->source_;
    dest_ = bundleptr->dest_;
//    custodian_ = bundleptr->custodian_;
//    replyto_ = bundleptr->replyto_;
    prev_hop_ = bundleptr->prev_hop_;
    length_ = bundleptr->length_;
//    is_fragment_ = bundleptr->is_fragment_;
//    is_admin_ = bundleptr->is_admin_;
    priority_ = bundleptr->priority_;

    ecos_flags_ = bundleptr->ecos_flags_;
    ecos_ordinal_ = bundleptr->ecos_ordinal_;
    ecos_flowlabel_ = bundleptr->ecos_flowlabel_;

    custody_requested_ = bundleptr->custody_requested_;
    local_custody_ = bundleptr->local_custody_;
    singleton_dest_ = bundleptr->singleton_dest_;

    local_id_ = bundleid_;

    if (0 == strncmp("ipn:", source_.c_str(), 4)) {
        char* end;
        ipn_source_node_ = strtoull(source_.c_str()+4, &end, 10);
        is_ipn_source_ = true;
    } else if (0 == strncmp("imc:", source_.c_str(), 4)) {
        // treating IMC like an IPN scheme for now  - 2021-03-24
        char* end;
        ipn_source_node_ = strtoull(source_.c_str()+4, &end, 10);
        is_ipn_source_ = true;
    } else {
        ipn_source_node_ = 0;
        is_ipn_source_ = false;
    }

    if (0 == strncmp("ipn:", dest_.c_str(), 4)) {
        char* end;
        ipn_dest_node_ = strtoull(dest_.c_str()+4, &end, 10);
        is_ipn_dest_ = true;
    } else if (0 == strncmp("imc:", dest_.c_str(), 4)) {
        // treating IMC like an IPN scheme for now  - 2021-03-24
        char* end;
        ipn_dest_node_ = strtoull(dest_.c_str()+4, &end, 10);
        is_ipn_dest_ = true;
    } else {
        ipn_dest_node_ = 0;
        is_ipn_dest_ = false;
    }


    if (log_enabled(oasys::LOG_DEBUG, "/ehs/bundle")) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("Created new bundle: \n");
        format_verbose(&buf);
        log_msg(oasys::LOG_DEBUG, buf.c_str());
    }
}

//----------------------------------------------------------------------
void
EhsBundle::process_bundle_custody_accepted_event(uint64_t custody_id)
{
    custodyid_ = custody_id;
    local_custody_ = true;
}

//----------------------------------------------------------------------
std::string 
EhsBundle::priority_key()
{
    if (!priority_key_set_) {
        // string constructed so sort is from higher to lower priority 
        char buf[48];

        // Don't let someone game the system by using the reserved priority :)
        int bundle_priority = (priority_ == COS_RESERVED) ? COS_BULK : priority_;
        int ecos_priority = 0;
        if (ecos_flags_ & 0x01) {
            // critical - per spec forward as if expedited and ordinal 254
            bundle_priority = COS_EXPEDITED;
            ecos_priority = 254;
        } else if (priority_ == COS_EXPEDITED) {
            ecos_priority = ecos_ordinal_;
        }
        
        int rev_priority = 99 - bundle_priority;
        int rev_ecos_priority = MAX_ECOS_PRIORITY - ecos_priority;

        snprintf(buf, sizeof(buf), "%2.2d~%3.3d~%24.24" PRIu64,
                 rev_priority, rev_ecos_priority, bundleid_);
                 
        priority_key_ = buf;
        priority_key_set_ = true;
    }

    return priority_key_;
}

//----------------------------------------------------------------------
void
EhsBundle::release_custody()
{
    oasys::ScopeLock l(&lock_, "release_custody");

    local_custody_ = false;
    custodyid_ = 0;
}

//----------------------------------------------------------------------
int
EhsBundle::add_ref(const char* what1, const char* what2)
{
    (void) what1;
    (void) what2;

    oasys::ScopeLock l(&lock_, "add_ref");

    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    return ret;
}

//----------------------------------------------------------------------
int
EhsBundle::del_ref(const char* what1, const char* what2)
{
    (void) what1;
    (void) what2;

    oasys::ScopeLock l(&lock_, "del_ref");

    int ret = --refcount_;
    if (0 == refcount_) {
        if (!exiting_) {
            parent_->post_event(new EhsFreeBundleReq(this));
        } else {
            l.unlock();
            delete this;
        }
    }
    return ret;
}

int 
EhsBundle::refcount()
{
    oasys::ScopeLock l(&lock_, "refcount");
    return refcount_;
}

//----------------------------------------------------------------------
void
EhsBundle::bundle_list(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, "bundle_list");

    if (local_custody_) {
        buf->appendf("%9" PRIu64 " : %s -> %s length: %zu  (Custodian - ID: %" PRIu64 ")\n", 
                     bundleid_,
                     source_.c_str(),
                     dest_.c_str(),
                     length_,
                     custodyid_);
    } else {
        buf->appendf("%9" PRIu64 " : %s -> %s length: %zu\n", 
                     bundleid_,
                     source_.c_str(),
                     dest_.c_str(),
                     length_);
    }
}

//----------------------------------------------------------------------
void
EhsBundle::format_verbose(oasys::StringBuffer* buf)
{
#define bool_to_str(x)   ((x) ? "true" : "false")

    oasys::ScopeLock l(&lock_, "format_verbose");

    u_int32_t cur_time_sec = get_current_time();

    buf->appendf("bundle id %" PRIu64 ":\n", bundleid_);
    buf->appendf("            GBoFID: %s\n", gbofid_str_.c_str());
    buf->appendf("            source: %s\n", source_.c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
//    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf(" local custodiy id: %" PRIu64 "\n", custodyid_);
//    buf->appendf("           replyto: %s\n", replyto_.c_str());
    buf->appendf("      previous hop: %s\n", prev_hop_.c_str());
    buf->appendf("    payload_length: %zu\n", length_);
    buf->appendf("  payload_location: %s\n", location_.c_str());
    buf->appendf("  payload_filename: %s\n", payload_file_.c_str());
    buf->appendf("          priority: %d\n", priority_);
    buf->appendf(" custody_requested: %s\n", bool_to_str(custody_requested_));
    buf->appendf("     local_custody: %s\n", bool_to_str(local_custody_));
    buf->appendf("    singleton_dest: %s\n", bool_to_str(singleton_dest_));
    buf->appendf("      receive_rcpt: %s\n", bool_to_str(receive_rcpt_));
    buf->appendf("      custody_rcpt: %s\n", bool_to_str(custody_rcpt_));
    buf->appendf("      forward_rcpt: %s\n", bool_to_str(forward_rcpt_));
    buf->appendf("     delivery_rcpt: %s\n", bool_to_str(delivery_rcpt_));
    buf->appendf("     deletion_rcpt: %s\n", bool_to_str(deletion_rcpt_));
    buf->appendf("    app_acked_rcpt: %s\n", bool_to_str(app_acked_rcpt_));
    buf->appendf("       creation_ts: %" PRIu64 ".%" PRIu64 "\n",
                 creation_ts_seconds_, creation_ts_seqno_);
    buf->appendf("        expiration: %" PRIu64 " (%" PRIu64 " left)\n", expiration_,
                 creation_ts_seconds_ + expiration_ - cur_time_sec);
//    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment_));
//    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
    buf->appendf("   do_not_fragment: %s\n", bool_to_str(do_not_fragment_));
    buf->appendf("       orig_length: %d\n", orig_length_);
    buf->appendf("       frag_offset: %d\n", frag_offset_);
    buf->appendf("             owner: %s\n", owner_.c_str());
    buf->appendf("          local_id: %" PRIu64 "\n", local_id_);
    buf->appendf("  ----------------    \n");
    buf->appendf("     dest ipn node: %" PRIu64 "\n", ipn_dest_node_);
    buf->append("\n");
}

//----------------------------------------------------------------------
bool
EhsBundle::expired()
{
    u_int32_t cur_time_sec = get_current_time();
    return cur_time_sec >= (creation_ts_seconds_ + expiration_);
}

//----------------------------------------------------------------------
u_int32_t
EhsBundle::get_current_time()
{
    /**
     * The number of seconds between 1/1/1970 and 1/1/2000.
     */
    u_int32_t TIMEVAL_CONVERSION = 946684800;

    struct timeval now;
    ::gettimeofday(&now, 0);

    ASSERT((u_int)now.tv_sec >= TIMEVAL_CONVERSION);
    return now.tv_sec - TIMEVAL_CONVERSION;
}

//----------------------------------------------------------------------
void 
EhsBundle::set_exiting() 
{ 
    oasys::ScopeLock l(&lock_, "set_exiting");
    exiting_ = true; 
}

//----------------------------------------------------------------------
bool
EhsBundle::exiting() 
{ 
    oasys::ScopeLock l(&lock_, "exiting");
    return exiting_;
}

//----------------------------------------------------------------------
void 
EhsBundle::set_deleted() 
{ 
    oasys::ScopeLock l(&lock_, "set_deleted");
     deleted_ = true; 
}

//----------------------------------------------------------------------
bool
EhsBundle::deleted() 
{ 
    oasys::ScopeLock l(&lock_, "deleted");
    return deleted_;
}


//----------------------------------------------------------------------
void 
EhsBundle::log_msg(oasys::log_level_t level, const char*format, ...)
{
    // internal use only - passes the logpath_ of this object
    va_list args;
    va_start(args, format);
    parent_->log_msg_va("/ehs/extrtr/bundle", level, format, args);
    va_end(args);
}



} // namespace dtn

