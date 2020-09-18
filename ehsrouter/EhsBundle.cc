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

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <inttypes.h>

#include <oasys/debug/Log.h>


#include "EhsBundle.h"
#include "EhsDtnNode.h"




namespace dtn {


//----------------------------------------------------------------------
EhsBundle::EhsBundle(EhsDtnNode* parent, rtrmessage::bundle_report::bundle::type& xbundle)
    : parent_(parent)
{
    init();
    process_bundle_report(xbundle);
}

//----------------------------------------------------------------------
EhsBundle::EhsBundle(EhsDtnNode* parent, rtrmessage::bundle_received_event& event)
    : parent_(parent)
{
    init();
    process_bundle_received_event(event);
}


//----------------------------------------------------------------------
void
EhsBundle::init()
{
    bundleid_ = 0;
    custodyid_ = 0;
    source_ = "dtn:none";
    dest_ = "dtn:none";
    custodian_ = "dtn:none";
    replyto_ = "dtn:none";
    prev_hop_ = "dtn:none";
    length_ = 0;
    location_ = "";
    payload_file_ = "";
    is_fragment_ = false;
    is_admin_ = false;
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
}

//----------------------------------------------------------------------
void
EhsBundle::process_bundle_report(rtrmessage::bundle_report::bundle::type& xbundle)
{
    bundleid_ = xbundle.bundleid();
    custodyid_ = xbundle.custodyid();
    source_ = xbundle.source().uri();
    dest_ = xbundle.dest().uri();
    custodian_ = xbundle.custodian().uri();
    replyto_ = xbundle.replyto().uri();
    prev_hop_ = xbundle.prevhop().uri();
    length_ = xbundle.length();
    location_ = xbundle.location();
    if (NULL != xbundle.payload_file()) {
        payload_file_ = xbundle.payload_file().get();
    } else {
        payload_file_ = "";
    }
    is_fragment_ = xbundle.is_fragment();
    is_admin_ = xbundle.is_admin();
    do_not_fragment_ = xbundle.do_not_fragment();
    priority_ = xbundle.priority();

    ecos_flags_ = xbundle.ecos_flags();
    ecos_ordinal_ = xbundle.ecos_ordinal();
    if (xbundle.ecos_flowlabel().present()) {
        ecos_flowlabel_ = xbundle.ecos_flowlabel().get();
    }

    custody_requested_ = xbundle.custody_requested();
    local_custody_ = xbundle.local_custody();
    singleton_dest_ = xbundle.singleton_dest();
    custody_rcpt_ = xbundle.custody_rcpt();
    receive_rcpt_ = xbundle.receive_rcpt();
    forward_rcpt_ = xbundle.forward_rcpt();
    delivery_rcpt_ = xbundle.delivery_rcpt();
    deletion_rcpt_ = xbundle.deletion_rcpt();
    app_acked_rcpt_ = xbundle.app_acked_rcpt();
    creation_ts_seconds_ = xbundle.creation_ts_seconds();
    creation_ts_seqno_ = xbundle.creation_ts_seqno();
    expiration_ = xbundle.expiration();
    orig_length_ = xbundle.orig_length();
    frag_offset_ = xbundle.frag_offset();
    frag_length_ = is_fragment_ ? length_ : 0;
    owner_ = xbundle.owner();

    local_id_ = bundleid_;

    if (0 == strncmp("ipn:", source_.c_str(), 4)) {
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
EhsBundle::process_bundle_received_event(rtrmessage::bundle_received_event& event)
{
//dz debug
//    rtrmessage::gbofIdType& gbofid = event.gbof_id();
//    source_ = gbofid.source().uri();
//    creation_ts_seconds_ = gbofid.creation_ts();
//    creation_ts_seqno_ = creation_ts_seconds_ & 0x0ffffffff;
//    creation_ts_seconds_ >>= 32;
//    is_fragment_ = gbofid.is_fragment();
//    frag_offset_ = gbofid.frag_offset();
//    frag_length_ = gbofid.frag_length();
   
    source_ = event.source();
    dest_ = event.dest();

    custodian_ = event.custodian();
    local_custody_ = parent_->is_local_node(custodian_);

    replyto_ = event.replyto();
    prev_hop_ = event.prevhop();

    expiration_ = event.expiration();
    length_ = event.bytes_received();

    bundleid_ = event.local_id();
    local_id_ = event.local_id();
    gbofid_str__ = event.gbofid_str();

    custodyid_ = event.custodyid();
    custody_requested_ = event.custody_transfer_requested();

    if (0 == strncmp("ipn:", source_.c_str(), 4)) {
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
    } else {
        ipn_dest_node_ = 0;
        is_ipn_dest_ = false;
    }

    received_from_link_id_ = event.link_id();

    priority_ = event.priority();

    ecos_flags_ = event.ecos_flags();
    ecos_ordinal_ = event.ecos_ordinal();
    if (event.ecos_flowlabel().present()) {
        ecos_flowlabel_ = event.ecos_flowlabel().get();
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
EhsBundle::process_bundle_custody_accepted_event(rtrmessage::bundle_custody_accepted_event& event)
{
    bundleid_ = event.local_id();
    local_id_ = event.local_id();

    custodyid_ = event.custodyid();

    custodian_ = event.custodian_str();
    local_custody_ = parent_->is_local_node(custodian_);


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
    oasys::ScopeLock l(&lock_, "add_ref");

    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    log_msg(oasys::LOG_DEBUG, 
            "bundle id %" PRIbid " refcount %d -> %d add %s %s",
            bundleid_, refcount_ - 1, refcount_,
            what1, what2);

    return ret;
}

//----------------------------------------------------------------------
int
EhsBundle::del_ref(const char* what1, const char* what2)
{
    oasys::ScopeLock l(&lock_, "del_ref");

    int ret = --refcount_;
    log_msg(oasys::LOG_DEBUG, 
            "bundle id %" PRIbid " refcount %d -> %d  del %s %s",
            bundleid_, refcount_ + 1, refcount_,
                what1, what2);

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
    buf->appendf("            source: %s\n", source_.c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf(" local custodiy id: %" PRIu64 "\n", custodyid_);
    buf->appendf("           replyto: %s\n", replyto_.c_str());
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
    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment_));
    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
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

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED

