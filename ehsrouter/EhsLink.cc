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

#include <oasys/io/IO.h>

#include "EhsRouter.h"
#include "EhsLink.h"


namespace dtn {


//----------------------------------------------------------------------
EhsLink::EhsLink(rtrmessage::link_report::link::type& xlink, EhsRouter* parent, bool fwdlnk_aos)
    : EhsEventHandler("EhsLink", "/ehs/link/%s", xlink.link_id().c_str()),
      Thread(logpath(), oasys::Thread::DELETE_ON_EXIT),
      parent_(parent),
      state_("unknown"),
      is_open_(false),
      is_fwdlnk_(false),
      fwdlnk_enabled_(false),
      fwdlnk_aos_(fwdlnk_aos),
      update_fwdlink_aos_(true),
      fwdlnk_force_LOS_while_disabled_(true),
      log_level_(1)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    init();

    eventq_ = new oasys::MsgQueue<EhsEvent*>(logpath_);
    eventq_->notify_when_empty();

    sender_ = new Sender(this);

    link_id_ = xlink.link_id();
    remote_eid_ = "";

    process_link_report(xlink);

    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("Created new link: \n");
        format_verbose(&buf);
        log_msg(oasys::LOG_DEBUG, buf.c_str());
    }
}

//----------------------------------------------------------------------
EhsLink::EhsLink(rtrmessage::link_opened_event& event, EhsRouter* parent, bool fwdlnk_aos)
    : EhsEventHandler("EhsLink", "/ehs/link/%s", event.link_id().c_str()),
      Thread(logpath(), oasys::Thread::DELETE_ON_EXIT),
      parent_(parent),
      state_("unknown"),
      is_open_(false),
      is_fwdlnk_(false),
      fwdlnk_enabled_(false),
      fwdlnk_aos_(fwdlnk_aos),
      update_fwdlink_aos_(true),
      fwdlnk_force_LOS_while_disabled_(true),
      log_level_(1)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    init();

    eventq_ = new oasys::MsgQueue<EhsEvent*>(logpath_);
    eventq_->notify_when_empty();

    sender_ = new Sender(this);

    link_id_ = event.link_id();
    remote_eid_ = "";

    process_link_opened_event(event);

    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("Created new link from opened event: \n");
        format_verbose(&buf);
        log_msg(oasys::LOG_DEBUG, buf.c_str());
    }
}

//----------------------------------------------------------------------
EhsLink::~EhsLink()
{
    set_should_stop();

    source_nodes_.clear();
    dest_nodes_.clear();

    delete eventq_;
}

//----------------------------------------------------------------------
void
EhsLink::init()
{
    local_addr_ = "";
    local_port_ = 0;
    remote_addr_ = "";
    remote_port_ = 0;
    segment_ack_enabled_ = false;
    negative_ack_enabled_ = false;
    keepalive_interval_ = 0;
    segment_length_ = 0;
    busy_queue_depth_ = 0;
    reactive_frag_enabled_ = false;
    sendbuf_length_ = 0;
    recvbuf_length_ = 0;
    data_timeout_ = 0;
    rate_ = 0;
    bucket_depth_ = 0;
    channel_ = 0;

    is_rejected_ = false;

    stats_enabled_ = false;
    stats_bundles_received_ = 0;
    stats_bytes_received_ = 0;
}

//----------------------------------------------------------------------
void
EhsLink::post_event(EhsEvent* event, bool at_back)
{
    if (should_stop()) {
        delete event;
    } else {
        event->posted_time_.get_time();
        eventq_->push(event, at_back);
    }
}

//----------------------------------------------------------------------
void 
EhsLink::apply_ehs_cfg(EhsLinkCfg* linkcfg)
{
    lock_.lock(__FUNCTION__);

    // the cfg link_id_ could be an IP Address or a Link ID but it
    // was used to find the cfg so no need to set it in here
    // linkcfg->link_id_;

    //linkcfg->local_ip_addr_;
    //linkcfg->local_port_;

    is_fwdlnk_ = linkcfg->is_fwdlnk_;
    establish_connection_ = linkcfg->establish_connection_;


    EhsNodeBoolIterator iter = linkcfg->source_nodes_.begin();
    while (iter != linkcfg->source_nodes_.end()) {
        source_nodes_[iter->first] = iter->second;
        ++iter;
    }

    iter = linkcfg->dest_nodes_.begin();
    while (iter != linkcfg->dest_nodes_.end()) {
        dest_nodes_[iter->first] = iter->second;
        ++iter;
    }

    rate_ = linkcfg->throttle_bps_;
    apply_rate_throttle();

    lock_.unlock();

    sender_->set_throttle_bps(linkcfg->throttle_bps_);

    log_msg(oasys::LOG_INFO, 
            "apply_ehs_cfg: Link ID: %s RemoteAddr: %s FwdLink: %s SrcNodes: %zu DstNodes: %zu",
            link_id_.c_str(), remote_addr_.c_str(), is_fwdlnk_?"true":"false", 
            source_nodes_.size(), dest_nodes_.size());
}

//----------------------------------------------------------------------
void
EhsLink::set_link_state(const std::string state)
{
    ASSERT(lock_.is_locked_by_me());

    if (state_.compare(state)) {
        if (reason_.length() > 0) {
            log_msg(oasys::LOG_DEBUG, "Link: %s changed state from %s to %s   reason: %s", 
                    link_id_.c_str(), state_.c_str(), state.c_str(), reason_.c_str());
        } else {
            log_msg(oasys::LOG_DEBUG, "Link: %s changed state from %s to %s", 
                    link_id_.c_str(), state_.c_str(), state.c_str());
        }
        state_ = state;
        is_open_ = (0 == state.compare("open"));
    }
}

//----------------------------------------------------------------------
void EhsLink::force_closed()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    set_link_state("closed");
}

//----------------------------------------------------------------------
uint64_t
EhsLink::return_disabled_bundles(EhsBundleTree& unrouted_bundles, 
                                 EhsSrcDstWildBoolMap& fwdlink_xmt_enabled)
{
    return sender_->return_disabled_bundles(unrouted_bundles, fwdlink_xmt_enabled);
}

//----------------------------------------------------------------------
uint64_t
EhsLink::return_all_bundles(EhsBundleTree& unrouted_bundles)
{
    return sender_->return_all_bundles(unrouted_bundles);
}

//----------------------------------------------------------------------
void
EhsLink::bundle_transmitted()
{
    sender_->bundle_transmitted();
}

//----------------------------------------------------------------------
void
EhsLink::set_throttle_bps(uint64_t rate)
{
    sender_->set_throttle_bps(rate);

    rate_ = rate;
    apply_rate_throttle();
}

//----------------------------------------------------------------------
uint64_t
EhsLink::throttle_bps()
{
    return sender_->throttle_bps();
}

//----------------------------------------------------------------------
void
EhsLink::set_fwdlnk_enabled_state(bool state)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    fwdlnk_enabled_ = state;
    update_fwdlink_aos_ = true;

//    send_reconfigure_link_comm_aos_msg();
}

//----------------------------------------------------------------------
void
EhsLink::set_fwdlnk_aos_state(bool state)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    fwdlnk_aos_ = state;

    log_msg(oasys::LOG_DEBUG, "Set FwdLink AOS State - Link ID: %s - AOS: %s  is fwdlnk? %s",
            link_id().c_str(), fwdlnk_aos_?"true":"false", is_fwdlnk_?"true":"false");

    update_fwdlink_aos_ = true;
//    send_reconfigure_link_comm_aos_msg();
}

//----------------------------------------------------------------------
void
EhsLink::set_fwdlnk_force_LOS_while_disabled(bool force_los)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    fwdlnk_force_LOS_while_disabled_ = force_los;

    update_fwdlink_aos_ = true;
//    send_reconfigure_link_comm_aos_msg();
}


//----------------------------------------------------------------------
void
EhsLink::send_reconfigure_link_comm_aos_msg()
{
    update_fwdlink_aos_ = false;

    if (is_fwdlnk_) {
        bool bval = parent_->fwdlnk_aos();
        if (!parent_->fwdlnk_enabled() && parent_->fwdlnk_force_LOS_while_disabled()) {
            bval = false; // force LOS
        }

        // send a reconfigure message to the CL 
        rtrmessage::reconfigure_link_request req;
        req.link_id(link_id());

        rtrmessage::linkConfigType lct;
        rtrmessage::linkConfigType::cl_params::container params;

        rtrmessage::key_value_pair kvp("comm_aos", bval);

        params.push_back( kvp );
        lct.cl_params(params);

        req.link_config_params(lct);

        rtrmessage::bpa message; 
        message.reconfigure_link_request(req);
        send_msg(message);

        log_msg(oasys::LOG_DEBUG, "Reconfigure Link ID: %s - AOS: %s",
                link_id().c_str(), fwdlnk_aos_?"true":"false");

     }
}

//----------------------------------------------------------------------
void 
EhsLink::set_ipn_node_id(uint64_t nodeid)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    ipn_node_id_ = nodeid;

    add_source_node(ipn_node_id_);
}

//----------------------------------------------------------------------
bool
EhsLink::okay_to_send()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bool result = false;
    if (is_open_) {
        if (is_fwdlnk_) {
            result = fwdlnk_enabled_ && fwdlnk_aos_;
        } else {
            result = true;
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool 
EhsLink::valid_source_node(uint64_t nodeid)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsNodeBoolIterator iter = source_nodes_.find(nodeid);

    //dzdebug
    if (iter == source_nodes_.end()) {
        log_msg(oasys::LOG_WARN, "Rejecting bundle due to invalid source ndoe - Link ID: %s - source: %" PRIu64 " -  valid nodes count: %zu",
                link_id().c_str(), nodeid, source_nodes_.size());
    } else {
        log_msg(oasys::LOG_WARN, "Acceping bundle - Link ID: %s - source: %" PRIu64 " -  valid nodes count: %zu",
                link_id().c_str(), nodeid, source_nodes_.size());
    }

    return iter != source_nodes_.end();
    
}

//----------------------------------------------------------------------
bool 
EhsLink::valid_dest_node(uint64_t nodeid)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsNodeBoolIterator iter = dest_nodes_.find(nodeid);

    //dzdebug
    if (iter == dest_nodes_.end()) {
        log_msg(oasys::LOG_WARN, "Rejecting bundle due to invalid dest ndoe - Link ID: %s - dest: %" PRIu64 " -  valid nodes count: %zu",
                link_id().c_str(), nodeid, dest_nodes_.size());
    } else {
        log_msg(oasys::LOG_WARN, "Acceping bundle - Link ID: %s - dest: %" PRIu64 " -  valid nodes count: %zu",
                link_id().c_str(), nodeid, dest_nodes_.size());
    }

    return iter != dest_nodes_.end();
}

//----------------------------------------------------------------------
void
EhsLink::clear_destinations()
{
    ASSERT(lock_.is_locked_by_me());

    dest_nodes_.clear();
}

//----------------------------------------------------------------------
void
EhsLink::clear_sources()
{
    ASSERT(lock_.is_locked_by_me());

    source_nodes_.clear();
}

//----------------------------------------------------------------------
//dz future for LTPUDP? bool
//dz future for LTPUDP? EhsLink::add_destination(uint64_t node_id, std::string& ip_addr, uint16_t port)
//dz future for LTPUDP? {
//dz future for LTPUDP?     ASSERT(lock_.is_locked_by_me());
//dz future for LTPUDP? 
//dz future for LTPUDP?     bool result = num_destinations_ < MAX_DESTINATIONS_PER_LINK;
//dz future for LTPUDP? 
//dz future for LTPUDP?     if (result) {
//dz future for LTPUDP?         dest_node_id_[num_destinations_] = node_id;
//dz future for LTPUDP?         dest_ip_addr_[num_destinations_] = ip_addr;
//dz future for LTPUDP?         dest_port_[num_destinations_] = port;
//dz future for LTPUDP?         ++num_destinations_;
//dz future for LTPUDP?     }
//dz future for LTPUDP? 
//dz future for LTPUDP?     return result;
//dz future for LTPUDP? }

//----------------------------------------------------------------------
//void
//EhsLink::clear_reachable_nodes()
//{
//    ASSERT(lock_.is_locked_by_me());
//
//    source_nodes_.clear();
//}

//----------------------------------------------------------------------
//void
//EhsLink::add_reachable_node(uint64_t nodeid)
//{
//    ASSERT(lock_.is_locked_by_me());
//
//    source_nodes_[nodeid] = true;
//}

//----------------------------------------------------------------------
void
EhsLink::add_source_node(uint64_t nodeid)
{
    ASSERT(lock_.is_locked_by_me());

    source_nodes_[nodeid] = true;
}

//----------------------------------------------------------------------
void
EhsLink::add_destination_node(uint64_t nodeid)
{
    ASSERT(lock_.is_locked_by_me());

    dest_nodes_[nodeid] = true;
}

//----------------------------------------------------------------------
bool
EhsLink::is_node_reachable(uint64_t node_id)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bool result = false;

    if (is_open_) {
        EhsNodeBoolIterator find_iter = source_nodes_.find(node_id);
        if (find_iter != source_nodes_.end()) {
            result = find_iter->second;
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsLink::send_msg(rtrmessage::bpa& msg)
{
    parent_->send_msg(msg);
}

//----------------------------------------------------------------------
void
EhsLink::process_link_report(rtrmessage::link_report::link::type& xlink)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    remote_eid_ = xlink.remote_eid().uri();

    clayer_ = xlink.clayer();
    nexthop_ = xlink.nexthop();
    set_link_state(xlink.state());
    is_reachable_ = xlink.is_reachable();
    is_usable_ = xlink.is_usable();
    how_reliable_ = xlink.how_reliable();
    how_available_ = xlink.how_available();
    min_retry_interval_ = xlink.min_retry_interval();
    max_retry_interval_ = xlink.max_retry_interval();
    idle_close_time_ = xlink.idle_close_time();
    

    if (xlink.clinfo().present()) {
        rtrmessage::clInfoType& cit = xlink.clinfo().get();
        if (cit.local_addr().present()) {
            local_addr_ = cit.local_addr().get();
        }
        if (cit.local_port().present()) {
            local_port_ = cit.local_port().get();
        }
        if (cit.remote_addr().present()) {
            remote_addr_ = cit.remote_addr().get();
        }
        if (cit.remote_port().present()) {
            remote_port_ = cit.remote_port().get();
        }
        segment_ack_enabled_ = cit.segment_ack_enabled();
        negative_ack_enabled_ = cit.negative_ack_enabled();
        keepalive_interval_ = cit.keepalive_interval().get();
        segment_length_ = cit.segment_length().get();
        busy_queue_depth_ = cit.busy_queue_depth().get();
        reactive_frag_enabled_ = cit.reactive_frag_enabled();
        sendbuf_length_ = cit.sendbuf_length().get();
        recvbuf_length_ = cit.recvbuf_length().get();
        data_timeout_ = cit.data_timeout().get();
//dz - do not overwrite configured rate        rate_ = cit.rate().get();
        bucket_depth_ = cit.bucket_depth().get();
        channel_ = cit.channel().get();

        log_msg(oasys::LOG_DEBUG, "process_link_report - Link ID: %s - Rate: %u (not overridden)",
                link_id().c_str(), rate_);
    } else {
        local_addr_ = "";
        local_port_ = 0;
        remote_addr_ = "";
        remote_port_ = 0;
        segment_ack_enabled_ = false;
        negative_ack_enabled_ = false;
        keepalive_interval_ = 0;
        segment_length_ = 0;
        busy_queue_depth_ = 0;
        reactive_frag_enabled_ = false;
        sendbuf_length_ = 0;
        recvbuf_length_ = 0;
        data_timeout_ = 0;
//dz - do not overwrite configured rate        rate_ = 0;
        bucket_depth_ = 0;
        channel_ = 0;
    }


    if (nexthop_.find(":") != std::string::npos) {
        std::string tmp_addr = nexthop_.substr(0, nexthop_.find(":"));
        if (tmp_addr.compare(remote_addr_) != 0) {
            log_msg(oasys::LOG_WARN, "process_link_report - Link ID: %s - override remote_addr_(%s) with nexthop(%s)",
                    link_id().c_str(), remote_addr_.c_str(), tmp_addr.c_str());
            remote_addr_ = tmp_addr;
        }
    }


    // Contact Attributes
    start_time_sec_ = 0;
    start_time_usec_ = 0;
    duration_ = 0;
    bps_ = 0;
    latency_ = 0;
    pkt_loss_prob_ = 0;

    reason_ = "from report";
    set_link_state(xlink.state());


    // if node's EID is known and is IPN then add it to the reachable node list
    if (0 == remote_eid_.compare(0, 4, "ipn:")) {
        char* endc = NULL;
        std::string node_num_str = remote_eid_.substr(4);
        uint64_t nodeid = strtoull(node_num_str.c_str(), &endc, 10);
        if ((NULL != endc) && ('.' == *endc)) {
            source_nodes_[nodeid] = true;
        }
    }
}

//----------------------------------------------------------------------
void
EhsLink::process_link_available_event(rtrmessage::link_available_event& event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    reason_ = event.reason();
    set_link_state("available");

    sender_->reset_check_for_misseed_bundles();
}

//----------------------------------------------------------------------
void
EhsLink::process_link_opened_event(rtrmessage::link_opened_event& event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    rtrmessage::contactType& ct = event.contact_attr();

    rtrmessage::linkType& lt = ct.link_attr();
    remote_eid_ = lt.remote_eid().uri();
    if (link_id_.compare(lt.link_id())) {
        log_msg(oasys::LOG_ERR, "process_link_opened_event - link_id does not match: %s (expected: %s)",
                lt.link_id().c_str(), link_id_.c_str());
    }

    clayer_ = lt.clayer();
    nexthop_ = lt.nexthop();
    //set_link_state(lt.state());
    is_reachable_ = lt.is_reachable();
    is_usable_ = lt.is_usable();
    how_reliable_ = lt.how_reliable();
    how_available_ = lt.how_available();
    min_retry_interval_ = lt.min_retry_interval();
    max_retry_interval_ = lt.max_retry_interval();
    idle_close_time_ = lt.idle_close_time();

    if (lt.clinfo().present()) {
        rtrmessage::clInfoType& cit = lt.clinfo().get();
        local_addr_ = cit.local_addr().get();
        local_port_ = cit.local_port().get();
        remote_addr_ = cit.remote_addr().get();
        remote_port_ = cit.remote_port().get();
        segment_ack_enabled_ = cit.segment_ack_enabled();
        negative_ack_enabled_ = cit.negative_ack_enabled();
        keepalive_interval_ = cit.keepalive_interval().get();
        segment_length_ = cit.segment_length().get();
        busy_queue_depth_ = cit.busy_queue_depth().get();
        reactive_frag_enabled_ = cit.reactive_frag_enabled();
        sendbuf_length_ = cit.sendbuf_length().get();
        recvbuf_length_ = cit.recvbuf_length().get();
        data_timeout_ = cit.data_timeout().get();
        //dz - do not overwrite configured rate     rate_ = cit.rate().get();
        bucket_depth_ = cit.bucket_depth().get();
        channel_ = cit.channel().get();

        log_msg(oasys::LOG_DEBUG, "process_link_opened - Link ID: %s - Rate: %u (not overridden)",
                link_id().c_str(), rate_);
    }

    if (nexthop_.find(":") != std::string::npos) {
        std::string tmp_addr = nexthop_.substr(0, nexthop_.find(":"));
        if (tmp_addr.compare(remote_addr_) != 0) {
            log_msg(oasys::LOG_WARN, "process_link_opened - Link ID: %s - override remote_addr_(%s) with nexthop(%s)",
                    link_id().c_str(), remote_addr_.c_str(), tmp_addr.c_str());
            remote_addr_ = tmp_addr;
        }
    }



    // Contact Attributes
    start_time_sec_ = ct.start_time_sec();
    start_time_usec_ = ct.start_time_usec();
    duration_ = ct.duration();
    bps_ = ct.bps();
    latency_ = ct.latency();
    pkt_loss_prob_ = ct.pkt_loss_prob();

    // if node's EID is known and is IPN then add it to the reachable node list
    if (0 == remote_eid_.compare(0, 4, "ipn:")) {
        char* endc = NULL;
        std::string node_num_str = remote_eid_.substr(4);
        uint64_t nodeid = strtoull(node_num_str.c_str(), &endc, 10);
        if ((NULL != endc) && ('.' == *endc)) {
            add_source_node(nodeid);
        }
    }


    sender_->reset_check_for_misseed_bundles();

    reason_ = "";
    set_link_state("open");

}

//----------------------------------------------------------------------
void
EhsLink::process_link_unavailable_event(rtrmessage::link_unavailable_event& event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    reason_ = event.reason();
    set_link_state("unavailable");

    sender_->reset_check_for_misseed_bundles();

    // XXX/dz TODO - Return bundles to the router ???
}

//----------------------------------------------------------------------
void
EhsLink::process_link_busy_event(rtrmessage::link_busy_event& event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    (void)event;
    reason_ = "";
    set_link_state("busy");

    sender_->reset_check_for_misseed_bundles();
}

//----------------------------------------------------------------------
void
EhsLink::process_link_closed_event(rtrmessage::link_closed_event& event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    reason_ = event.reason();
    set_link_state("closed");

    rtrmessage::contactType& ct = event.contact_attr();

    rtrmessage::linkType& lt = ct.link_attr();
    remote_eid_ = lt.remote_eid().uri();
    if (link_id_.compare(lt.link_id())) {
        log_msg(oasys::LOG_ERR, "process_link_closed_event - link_id does not match: %s (expected: %s)",
                lt.link_id().c_str(), link_id_.c_str());
    }

    clayer_ = lt.clayer();
    nexthop_ = lt.nexthop();
    //set_link_state(lt.state()); // forced closed above
    is_reachable_ = lt.is_reachable();
    is_usable_ = lt.is_usable();
    how_reliable_ = lt.how_reliable();
    how_available_ = lt.how_available();
    min_retry_interval_ = lt.min_retry_interval();
    max_retry_interval_ = lt.max_retry_interval();
    idle_close_time_ = lt.idle_close_time();

    if (lt.clinfo().present()) {
        rtrmessage::clInfoType& cit = lt.clinfo().get();
        local_addr_ = cit.local_addr().get();
        local_port_ = cit.local_port().get();
        remote_addr_ = cit.remote_addr().get();
        remote_port_ = cit.remote_port().get();
        segment_ack_enabled_ = cit.segment_ack_enabled();
        negative_ack_enabled_ = cit.negative_ack_enabled();
        keepalive_interval_ = cit.keepalive_interval().get();
        segment_length_ = cit.segment_length().get();
        busy_queue_depth_ = cit.busy_queue_depth().get();
        reactive_frag_enabled_ = cit.reactive_frag_enabled();
        sendbuf_length_ = cit.sendbuf_length().get();
        recvbuf_length_ = cit.recvbuf_length().get();
        data_timeout_ = cit.data_timeout().get();
        //dz - do not override configured rate     rate_ = cit.rate().get();
        bucket_depth_ = cit.bucket_depth().get();
        channel_ = cit.channel().get();
    }

    // Contact Attributes
    start_time_sec_ = ct.start_time_sec();
    start_time_usec_ = ct.start_time_usec();
    duration_ = ct.duration();
    bps_ = ct.bps();
    latency_ = ct.latency();
    pkt_loss_prob_ = ct.pkt_loss_prob();

    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("Closed new link: \n");
        format_verbose(&buf);
        log_msg(oasys::LOG_DEBUG, buf.c_str());
    }


    // Inform the Sender so it can return its bundles to the Router
    sender_->process_link_closed_event();
}

//----------------------------------------------------------------------
void 
EhsLink::return_all_bundles_to_router(EhsBundlePriorityTree& priority_tree)
{
    if (stats_enabled_) {
        stats_lock_.lock(__FUNCTION__);
        stats_bundles_received_ -= priority_tree.size();
        stats_bytes_received_ -= (priority_tree.total_bytes() + 
                                  (priority_tree.size() * EHSLINK_BUNDLE_OVERHEAD));
        stats_lock_.unlock();
    }

    uint64_t cnt = parent_->return_all_bundles_to_router(priority_tree);

    log_msg(oasys::LOG_DEBUG, "Link %s [%s] returned %" PRIu64 " bundles to Router",
            link_id_.c_str(), remote_addr_.c_str(), cnt);
}

//----------------------------------------------------------------------
void 
EhsLink::check_for_missed_bundles()
{
    if (is_open_) {
        // see if EhsRouter has bundles that were not passed to the link
        uint64_t num_routed = parent_->check_for_missed_bundles(this);

        if (num_routed == 0) {

            // see if EhsDtnNode has bundles for each source node ID that were missed
            EhsNodeBoolIterator iter = source_nodes_.begin();
            while (iter != source_nodes_.end()) {
                num_routed += parent_->check_dtn_node_for_missed_bundles(iter->first);
                ++iter;
            }
        }

        log_msg(oasys::LOG_ALWAYS, "EhsLink.check_for_missed_bundles(%s): num found = %" PRIu64,
                link_id_.c_str(), num_routed);
    }
}

//----------------------------------------------------------------------
void 
EhsLink::handle_route_bundle_req(EhsRouteBundleReq* event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if (stats_enabled_) {
        stats_lock_.lock(__FUNCTION__);
        stats_bundles_received_ += 1;
        stats_bytes_received_ += event->bref_->length() + EHSLINK_BUNDLE_OVERHEAD;
        stats_lock_.unlock();
    }

    sender_->queue_bundle(event->bref_);
}

//----------------------------------------------------------------------
void 
EhsLink::handle_route_bundle_list_req(EhsRouteBundleListReq* event)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if (stats_enabled_) {
        stats_lock_.lock(__FUNCTION__);
        stats_bundles_received_ += event->blist_->size();
        stats_bytes_received_ += event->blist_->total_bytes() + 
                                 (event->blist_->size() * EHSLINK_BUNDLE_OVERHEAD);
        stats_lock_.unlock();
    }

    log_msg(oasys::LOG_DEBUG, "EhsLink - handle_route_bundle_list");

    sender_->queue_bundle_list(event->blist_);
}

//----------------------------------------------------------------------
void 
EhsLink::handle_reconfigure_link_req(EhsReconfigureLinkReq* event)
{
    (void) event;
    parent_->get_link_configuration(this);
}


//----------------------------------------------------------------------
void 
EhsLink::apply_rate_throttle()
{
    // apply rate limit to ISS link
    if (is_fwdlnk_) {
         // send a reconfigure message to the CL 
        rtrmessage::reconfigure_link_request req;
        req.link_id(link_id());

        rtrmessage::linkConfigType lct;
        rtrmessage::linkConfigType::cl_params::container params;

        rtrmessage::key_value_pair kvp("rate", rate_);

        params.push_back( kvp );
        lct.cl_params(params);

        req.link_config_params(lct);

        rtrmessage::bpa message; 
        message.reconfigure_link_request(req);
        send_msg(message);

        log_msg(oasys::LOG_INFO, "Reconfigure Link ID: %s - Rate: %u",
                link_id().c_str(), rate_);
    }
}

//----------------------------------------------------------------------
void
EhsLink::handle_event(EhsEvent* event)
{
    dispatch_event(event);
    
    event_handlers_completed(event);

//    stats_.events_processed_++;
}

//----------------------------------------------------------------------
void
EhsLink::event_handlers_completed(EhsEvent* event)
{
    (void) event;
}


//----------------------------------------------------------------------
void
EhsLink::run()
{
    // Send the initial rate throttle if appropriate
    lock_.lock(__FUNCTION__);
    apply_rate_throttle();
    send_reconfigure_link_comm_aos_msg();
    lock_.unlock();

    // Start the Sender thread running
    sender_->start();

    int timeout = 100;
    bool ok;
    EhsEvent* event;
    oasys::Time now;
    oasys::Time in_queue;

    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    while (1) {
        if (should_stop()) {
            log_msg(oasys::LOG_DEBUG, "EhsLink: stopping");
            break;
        }

        lock_.lock(__FUNCTION__);
        if (update_fwdlink_aos_) {
            send_reconfigure_link_comm_aos_msg();
        }
        lock_.unlock();

        if (stats_enabled_) {
            if (stats_timer_.elapsed_ms() >= 1000) {
                do_stats();
                stats_timer_.get_time();
            }
        }

        if (eventq_->size() > 0) {
            ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            now.get_time();

            if (now >= event->posted_time_) {
                in_queue = now - event->posted_time_;
                if (in_queue.sec_ > 2) {
                    log_msg(oasys::LOG_DEBUG, "event %s was in queue for %u.%u seconds",
                               event->type_str(), in_queue.sec_, in_queue.usec_);
                }
            }
            
            // handle the event
            handle_event(event);

            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);

        if (cc == oasys::IOTIMEOUT) {
            continue;

        } else if (cc <= 0) {
            log_msg(oasys::LOG_ERR, "unexpected return %d from poll_multiple!", cc);
            continue;
        }
    }

    sender_->set_should_stop();
}

//----------------------------------------------------------------------
void
EhsLink::do_stats()
{
    uint64_t bundles_sent;
    uint64_t bytes_sent;
    uint64_t send_waits;
    int64_t bundles_received;
    int64_t bytes_received;

    stats_lock_.lock(__FUNCTION__);

    sender_->get_stats(&bundles_sent, &bytes_sent, &send_waits);

    bundles_received = stats_bundles_received_;
    bytes_received = stats_bytes_received_;

    stats_bundles_received_ = 0;
    stats_bytes_received_ = 0;

    stats_lock_.unlock();

    double rcvd_mbps = bytes_received * 8.0 / 1000000.0;
    double sent_mbps = bytes_sent * 8.0 / 1000000.0;

    log_msg(oasys::LOG_ALWAYS, "Stats - Link: %s - Rcvd: %" PRIi64 " (%.3f Mbps)  Sent: %" PRIu64 " (%.3f Mbps) SendWaits: %" PRIu64,
            link_id().c_str(), bundles_received, rcvd_mbps, bundles_sent, sent_mbps, send_waits);
}

//----------------------------------------------------------------------
void
EhsLink::set_link_statistics(bool enabled)
{
    uint64_t bundles_sent;
    uint64_t bytes_sent;
    uint64_t send_waits; 

    oasys::ScopeLock l(&stats_lock_, __FUNCTION__);

    stats_enabled_ = enabled;
    // clear stats variables

    sender_->get_stats(&bundles_sent, &bytes_sent, &send_waits);
    stats_bundles_received_ = 0;
    stats_bytes_received_ = 0;
    stats_timer_.get_time();
}
//----------------------------------------------------------------------
void
EhsLink::format_verbose(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);


#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("link name: %s\n", key().c_str());
    buf->appendf("             remote_eid: %s\n", remote_eid_.c_str());
    buf->appendf("                  state: %s\n", state_.c_str());
    buf->appendf("                 clayer: %s\n", clayer_.c_str());
    buf->appendf("          local address: %s\n", local_addr_.c_str());
    buf->appendf("             local port: %d\n", local_port_);
    buf->appendf("         remote address: %s\n", remote_addr_.c_str());
    buf->appendf("            remote port: %d\n", remote_port_);
    buf->appendf("    segment_ack_enabled: %s\n", bool_to_str(segment_ack_enabled_));
    buf->appendf("   negative_ack_enabled: %s\n", bool_to_str(negative_ack_enabled_));
    buf->appendf("     keepalive_interval: %u\n", keepalive_interval_);
    buf->appendf("         segment_length: %u\n", segment_length_);
    buf->appendf("       busy_queue_depth: %u\n", busy_queue_depth_);
    buf->appendf("  reactive_frag_enabled: %s\n", bool_to_str(reactive_frag_enabled_));
    buf->appendf("         sendbuf_length: %u\n", sendbuf_length_);
    buf->appendf("         recvbuf_length: %u\n", recvbuf_length_);
    buf->appendf("           data_timeout: %u\n", data_timeout_);
    buf->appendf("                   rate: %u\n", rate_);
    buf->appendf("           bucket_depth: %u\n", bucket_depth_);
    buf->appendf("                channel: %u\n", channel_);
    buf->appendf("               next hop: %s\n", nexthop_.c_str());
    buf->appendf("           is reachable: %s\n", bool_to_str(is_reachable_));
    buf->appendf("              is_usable: %s\n", bool_to_str(is_usable_));
    buf->appendf("           how_reliable: %u%%\n", how_reliable_);
    buf->appendf("          how_available: %u%%\n", how_available_);
    buf->appendf("     min_retry_interval: %u\n", min_retry_interval_);
    buf->appendf("     max_retry_interval: %u\n", max_retry_interval_);
    buf->appendf("        idle_close_time: %u\n", idle_close_time_);
    buf->appendf("----- Contact Info -----\n");
    buf->appendf("         start_time_sec: %u\n", start_time_sec_);
    buf->appendf("        start_time_usec: %u\n", start_time_usec_);
    buf->appendf("               duration: %u\n", duration_);
    buf->appendf("           bits_per_sec: %u\n", bps_);
    buf->appendf("                latency: %u\n", latency_);
    buf->appendf("    pkt_loss_probablity: %u\n", pkt_loss_prob_);
    buf->append("\n");
}

//----------------------------------------------------------------------
void 
EhsLink::link_dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    buf->appendf("%-15s [%s:%d %s %s state=%s]  eventq: %zu sendq: %zu rate: %" PRIu64 "\n",
                 key().c_str(),
                 remote_addr_.c_str(),
                 remote_port_,
                 remote_eid_.c_str(),
                 clayer_.c_str(),
                 state_.c_str(),
                 eventq_->size(),
                 sender_->priority_tree_size(),
                 sender_->throttle_bps());
}


//----------------------------------------------------------------------
void 
EhsLink::log_msg(oasys::log_level_t level, const char*format, ...)
{
    if (!should_stop()) {
        // internal use only - passes the logpath_ of this object
        va_list args;
        va_start(args, format);
        parent_->log_msg_va(logpath_, level, format, args);
        va_end(args);
    }
}

//----------------------------------------------------------------------
EhsLink::Sender::Sender(EhsLink* parent)
    : oasys::Thread("EhsLink::Sender", oasys::Thread::DELETE_ON_EXIT),
      bucket_(parent->logpath(), 0, 2000000)   // leaky bucket

      //XXX/dz Standard bucket does not work well when the bundles size is greater than a packet size
      //dz bucket_(parent->logpath(), 65535*8, 2000000)   // standard bucket
{
    parent_ = parent;
    stats_bundles_sent_ = 0;
    stats_bytes_sent_ = 0;
    send_waits_ = 0;

    bundle_was_queued_ = false;
    bundle_was_transmitted_ = false;
}

//----------------------------------------------------------------------
EhsLink::Sender::~Sender()
{
    priority_tree_.clear();
}

//----------------------------------------------------------------------
void
EhsLink::Sender::set_throttle_bps(uint64_t rate)
{
    parent_->log_msg(oasys::LOG_INFO, "Link: %s - Set throttle rate to %" PRIu64, 
                     parent_->link_id().c_str(), rate);

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bucket_.set_rate(rate);
}

//----------------------------------------------------------------------
void
EhsLink::Sender::queue_bundle(EhsBundleRef& bref)
{
    priority_tree_.insert_bundle(bref);
}

//----------------------------------------------------------------------
void
EhsLink::Sender::queue_bundle_list(EhsBundlePriorityQueue* blist)
{
    priority_tree_.insert_queue(blist);    
}

//----------------------------------------------------------------------
void
EhsLink::Sender::bundle_transmitted()
{
    bundle_was_transmitted_ = true;
}

//----------------------------------------------------------------------
void
EhsLink::Sender::reset_check_for_misseed_bundles()
{
    bundle_was_queued_ = false;
    bundle_was_transmitted_ = false;
}

//----------------------------------------------------------------------
void
EhsLink::Sender::process_link_closed_event()
{
    parent_->return_all_bundles_to_router(priority_tree_);

    reset_check_for_misseed_bundles();
}

//----------------------------------------------------------------------
uint64_t
EhsLink::Sender::return_disabled_bundles(EhsBundleTree& unrouted_bundles, 
                                         EhsSrcDstWildBoolMap& fwdlink_xmt_enabled)
{
    return priority_tree_.return_disabled_bundles(unrouted_bundles, fwdlink_xmt_enabled);
}

//----------------------------------------------------------------------
uint64_t
EhsLink::Sender::return_all_bundles(EhsBundleTree& unrouted_bundles)
{
    return priority_tree_.return_all_bundles(unrouted_bundles);
}

//----------------------------------------------------------------------
size_t
EhsLink::Sender::priority_tree_size()
{
    return priority_tree_.size();
}

//----------------------------------------------------------------------
void 
EhsLink::Sender::get_stats(uint64_t* bundles_sent, uint64_t* bytes_sent, uint64_t* waits)
{
    oasys::ScopeLock l(&stats_lock_, __FUNCTION__);

    *bundles_sent = stats_bundles_sent_;
    *bytes_sent = stats_bytes_sent_;
    *waits = send_waits_;

    stats_bundles_sent_ = 0;
    stats_bytes_sent_ = 0;
    send_waits_ = 0;
}

//----------------------------------------------------------------------
void
EhsLink::Sender::run()
{
    EhsBundleStrIterator iter;
    EhsBundleRef bref("EhsLink::Sender::run");
    uint64_t est_bits_to_send;
    bool abort_send;

    // detect 30 seconds after a queue/transmit to check for missed pending bundles
    bundle_was_queued_ = false;
    bundle_was_transmitted_ = false;
    last_bundle_activity_.get_time();


    while (not should_stop()) {
        if (priority_tree_.empty() || !parent_->okay_to_send()) {
            usleep(100);
        } else {
            bref = priority_tree_.pop_bundle();

            if (bref == NULL) {
                //dz debug
                parent_->log_msg(oasys::LOG_ALWAYS, "EhsLink.Sender got null bundle from priority_tree_ - empty=%s size=%zu",
                                 priority_tree_.empty()?"true":"false", priority_tree_.size());

                usleep(100);
                continue;
            }

            abort_send = false;

            if (!bref->deleted()) {
                // wait for throttle okay to transmit
                lock_.lock(__FUNCTION__);
                if (bucket_.rate() != 0) {
                    est_bits_to_send = (bref->length() + EHSLINK_BUNDLE_OVERHEAD) * 8;
                    while (!bucket_.try_to_drain(est_bits_to_send)) {
                       if (should_stop() || !parent_->okay_to_send()) {
                           abort_send = true;
                           if (!should_stop()) {
                               parent_->log_msg(oasys::LOG_DEBUG, 
                                                "Terminating wait to send bundle due to LOS or disabled");
                           }
                           break;
                       } 
                       usleep(1);
                       ++send_waits_;
                    }
                }
                lock_.unlock();


                if (!bref->deleted()) {
                    if (abort_send) {
                        // put the bundle back on the tree to send later
                        priority_tree_.insert_bundle(bref);
                    } else {
                        // inform the DTN server to send the bundle
                        rtrmessage::send_bundle_request req;
                        req.local_id() = bref->bundleid();
                        req.link_id(parent_->link_id());

                        rtrmessage::bundleForwardActionType action("forward");
                        req.fwd_action(action);

                        req.gbofid_str(bref->gbofid_str());

                        rtrmessage::bpa message; 
                        message.send_bundle_request(req);
                        parent_->send_msg(message);

                        stats_lock_.lock(__FUNCTION__);
                        stats_bundles_sent_ += 1;
                        stats_bytes_sent_ += bref->length() + EHSLINK_BUNDLE_OVERHEAD;
                        stats_lock_.unlock();

                        bundle_was_queued_ = true;
                        last_bundle_activity_.get_time();

                        //parent_->log_msg(oasys::LOG_DEBUG, "Forwarded Bundle ID: %" PRIu64 " to %s",
                        //                 bref->bundleid(), parent_->link_id().c_str());
                    }
                }
            }

            bref.release();
        }

        // update last activity time if any bundles were transmitted
        if (bundle_was_transmitted_) {
            bundle_was_transmitted_ = false;
            last_bundle_activity_.get_time();
        }

        // time to check for missed bundles?
        if (bundle_was_queued_ && priority_tree_.empty())
        {
            if (last_bundle_activity_.elapsed_ms() >= 30000)
            {
                bundle_was_queued_ = false;
                // check for missed bundles
                parent_->check_for_missed_bundles();
            }
        }
    }
}





} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED
