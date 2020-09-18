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

#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "EhsDtnNode.h"
#include "EhsExternalRouterImpl.h"
#include "EhsRouter.h"


namespace dtn {


//----------------------------------------------------------------------
EhsDtnNode::EhsDtnNode(EhsExternalRouterImpl* parent, 
                       std::string eid, std::string eid_ipn)
    : EhsEventHandler("EhsDtnNode", "/ehs/node/%s", eid.c_str()),
      Thread("EhsDtnNode", oasys::Thread::DELETE_ON_EXIT),
      eid_(eid),
      eid_ping_(eid),
      eid_ipn_(eid_ipn),
      eid_ipn_ping_(eid_ipn),
      parent_(parent),
      prime_mode_(true),
      fwdlnk_enabled_(false),
      fwdlnk_aos_(false),
      have_link_report_(false),
      log_level_(1)
{
    if (0 == strncmp("ipn:", eid_ipn_.c_str(), 4)) {
        char* end;
        node_id_ = strtoull(eid_ipn_.c_str()+4, &end, 10);

        // change the service number from 0 to 2047
        eid_ipn_ping_.resize(eid_ipn_ping_.length()-1);
        eid_ipn_ping_.append("2047");
    }
    
    if (0 == strncmp("dtn:", eid_.c_str(), 4)) {
        eid_ping_.append("/ping");
    }


    eventq_ = new oasys::MsgQueue<EhsEvent*>(logpath_);
    eventq_->notify_when_empty();

    router_ = new EhsRouter(this);

    last_recv_seq_ctr_ = 0;
    send_seq_ctr_ = 0;

    memset(&stats_, 0, sizeof(stats_));
}


//----------------------------------------------------------------------
EhsDtnNode::~EhsDtnNode()
{
    set_should_stop();
    router_->set_should_stop();
    router_ = NULL;

    all_bundles_.clear();

    EhsBundleRef bref("~EhsDtnNode");
    EhsBundleMap::iterator biter = custody_bundles_.begin();
    while (biter != custody_bundles_.end()) {
        bref = biter->second;

        custody_bundles_.erase(biter);

        bref->set_exiting();
        bref.release();

        biter = custody_bundles_.begin();
    }

    biter = undelivered_bundles_.begin();
    while (biter != undelivered_bundles_.end()) {
        bref = biter->second;

        undelivered_bundles_.erase(biter);

        bref->set_exiting();
        bref.release();

        biter = undelivered_bundles_.begin();
    }

    delivered_bundle_id_list_.clear();

    
    EhsBundleMap* bmap;
    BUNDLES_BY_DEST_ITER iter = bundles_by_dest_map_.begin();
    while (iter != bundles_by_dest_map_.end()) {
       bmap = iter->second;
       bmap->clear();
    }
    bundles_by_dest_map_.clear();

    sleep(1);

    delete eventq_;
}

//----------------------------------------------------------------------
void
EhsDtnNode::post_event(EhsEvent* event, bool at_back)
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
EhsDtnNode::set_prime_mode(bool prime_mode)
{
    // set the prime mode and pass it on to the router thread
    prime_mode_ = prime_mode;
    router_->set_prime_mode(prime_mode_);
}


//----------------------------------------------------------------------
void
EhsDtnNode::inc_bundle_stats_by_src_dst(EhsBundle* eb)
{
    // update src-dst stats
    EhsSrcDstKey* skey = NULL;
    EhsSrcDstKey tmpkey = EhsSrcDstKey(eb->ipn_source_node(), eb->ipn_dest_node());
    SrcDstStatsIterator siter = src_dst_stats_list_.find(&tmpkey);
    if (siter == src_dst_stats_list_.end()) {
        skey = new EhsSrcDstKey(eb->ipn_source_node(), eb->ipn_dest_node());
        src_dst_stats_list_[skey] = skey;
    } else {
        skey = siter->second;
    }
    ++skey->total_bundles_;
    skey->total_bytes_ += eb->length();
}
        
//----------------------------------------------------------------------
void
EhsDtnNode::dec_bundle_stats_by_src_dst(EhsBundle* eb)
{
    // update src-dst stats
    EhsSrcDstKey tmpkey = EhsSrcDstKey(eb->ipn_source_node(), eb->ipn_dest_node());
    SrcDstStatsIterator siter = src_dst_stats_list_.find(&tmpkey);
    if (siter != src_dst_stats_list_.end()) {
        EhsSrcDstKey* skey = siter->second;
        --skey->total_bundles_;
        skey->total_bytes_ -= eb->length();
    }
}
        
//----------------------------------------------------------------------
bool
EhsDtnNode::shutdown_server()
{
    // issue request to shutdown the DTNME server
    rtrmessage::bpa msg;
    rtrmessage::bpa::shutdown_request::type req;
    msg.shutdown_request(req);
    send_msg(msg);

    return true;
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_report(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_report::bundle::container& c = msg->bundle_report().get().bundle();

    EhsBundleRef bref("process_bundle_report");

    rtrmessage::bundle_report::bundle::container::iterator iter = c.begin();
    while (iter != c.end()) {
        rtrmessage::bundle_report::bundle::type& xbundle = *iter;
        uint64_t id = xbundle.bundleid();

        bref = all_bundles_.find(id);
        if (bref == NULL) {
            EhsBundle* eb = new EhsBundle(this, xbundle);
            eb->set_is_fwd_link_destination(router_->is_fwd_link_destination(eb->ipn_dest_node()));

            bref = eb;

            if (! all_bundles_.bundle_received(bref)) {
                log_msg(oasys::LOG_CRIT, "process_bundle_report - ignoring duplicate Bundle ID: %" PRIu64
                                         " src: %s dest: %s len: %d", 
                        bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length());
                return;
            }

            add_bundle_by_dest(bref);

            if (bref->is_ecos_critical()) {
                if (critical_bundles_.insert_bundle(bref)) {
                    log_msg(oasys::LOG_WARN, "process_bundle_report - accepted ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    // TODO: start a timer to check for expired critical bundles
                } else {
                    log_msg(oasys::LOG_WARN, "process_bundle_report - rejecting duplicate ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    all_bundles_.bundle_rejected(bref);

                    // issue delete request if not authorized
                    rtrmessage::delete_bundle_request req;
                    req.local_id() = bref->bundleid();
                    req.gbofid_str(bref->gbofid_str());
    
                    rtrmessage::bpa message; 
                    message.delete_bundle_request(req);
                    send_msg(message);

                    return;
                }
            }

            if (eb->custodyid() > 0) {
                custody_bundles_.insert(EhsBundlePair(eb->custodyid(), bref));
            } else if (accept_custody_before_routing(bref)) {
                //log_msg(oasys::LOG_DEBUG, "bundle_report- added bundle - ID: %" PRIu64 " dest: %s len: %d --- waiting to accept custody before routing", 
                //        id, bref->dest().c_str(), bref->length());
                continue;
            }

            //log_msg(oasys::LOG_DEBUG, "bundle_report - added bundle - ID: %" PRIu64 " dest: %s len: %d", 
            //         id, bref->dest().c_str(), bref->length());

            route_bundle(bref);

        } else {
            bref->process_bundle_report(xbundle);

            //log_msg(oasys::LOG_DEBUG, "bundle_report - updated Bundle ID: %" PRIu64 " dest: %s len: %d", 
            //        id, bref->dest().c_str(), bref->length());
        }

        ++iter;
    }
}


//----------------------------------------------------------------------
bool
EhsDtnNode::accept_custody_before_routing(EhsBundleRef& bref)
{
    bool result = false;

    if (bref->custody_requested()) {
        if (!bref->local_custody()) {
            if (accept_custody_.check_pair(bref->ipn_source_node(), bref->ipn_dest_node())) {
                //log_msg(oasys::LOG_DEBUG, "accept_custody_before_routing -issuing request to take custody of bundle");
 
                // Issue request to take custody of the bundle
                rtrmessage::take_custody_of_bundle_request req;
                req.local_id() = bref->bundleid();
                req.gbofid_str(bref->gbofid_str());

                rtrmessage::bpa message; 
                message.take_custody_of_bundle_request(req);
                send_msg(message);

                result = true;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_received_event(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_received_event& event = msg->bundle_received_event().get();

    uint64_t id = event.local_id();

    EhsBundleRef bref("process_bundle_received_event");
    bref = all_bundles_.find(id);
    if (bref == NULL) {
        EhsBundle* eb = new EhsBundle(this, event);
        eb->set_is_fwd_link_destination(router_->is_fwd_link_destination(eb->ipn_dest_node()));

        bref = eb;

        std::string remote_addr = "";
        std::string link_id = event.link_id();

        if (! all_bundles_.bundle_received(bref)) {
            log_msg(oasys::LOG_CRIT, "bundle_received - ignoring duplicate Bundle ID: %" PRIu64
                                     " src: %s dest: %s len: %d received from: %s", 
                    bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length(),
                    remote_addr.c_str());
            return;
        }
        
        add_bundle_by_dest(bref);

        DeliveredBundleIDIterator iter = delivered_bundle_id_list_.find(id);
        if (iter != delivered_bundle_id_list_.end()) {
            //log_msg(oasys::LOG_WARN, "bundle_received - ignoring delivered Bundle ID: %" PRIu64, id);
            delivered_bundle_id_list_.erase(iter);
            finalize_bundle_delivered_event(id);
            return;
        }

        if (!router_->accept_bundle(bref, link_id, remote_addr)) {
            // not okay - send a request to delete the bundle
            all_bundles_.bundle_rejected(bref);

            log_msg(oasys::LOG_CRIT, "bundle_received - rejecting Bundle ID: %" PRIu64
                                     " src: %s dest: %s len: %d received from: %s", 
                    bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length(),
                    remote_addr.c_str());

            // issue delete request if not authorized
            rtrmessage::delete_bundle_request req;
            req.local_id() = bref->bundleid();
            req.gbofid_str(bref->gbofid_str());

            rtrmessage::bpa message; 
            message.delete_bundle_request(req);
            send_msg(message);

            return;
        } 

        if (bref->is_ecos_critical()) {
            if (critical_bundles_.insert_bundle(bref)) {
                log_msg(oasys::LOG_WARN, "bundle_received - accepted ECOS Critical Bundle: %s"
                                         "  (ID: %" PRIu64 ") received on link: %s", 
                        bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                // TODO: start a timer to check for expired critical bundles
            } else {
                log_msg(oasys::LOG_WARN, "bundle_received - rejecting duplicate ECOS Critical Bundle: %s"
                                         "  (ID: %" PRIu64 ") received on link: %s", 
                        bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                all_bundles_.bundle_rejected(bref);

                // issue delete request if not authorized
                rtrmessage::delete_bundle_request req;
                req.local_id() = bref->bundleid();
                req.gbofid_str(bref->gbofid_str());
    
                rtrmessage::bpa message; 
                message.delete_bundle_request(req);
                send_msg(message);

                return;
            }
        }

        if (bref->custodyid() > 0) {
            custody_bundles_.insert(EhsBundlePair(bref->custodyid(), bref));
        } else if (accept_custody_before_routing(bref)) {
            //log_msg(oasys::LOG_DEBUG, "bundle_received - added bundle - ID: %" PRIu64 " dest: %s len: %d --- waiting to accept custody before routing", 
            //        id, bref->dest().c_str(), bref->length());
            return;
        }

        //log_msg(oasys::LOG_DEBUG, "bundle_received - added bundle - ID: %" PRIu64 " dest: %s len: %d custody: %s --- routing", 
        //        id, bref->dest().c_str(), bref->length(), bref->custody_str().c_str());

        route_bundle(bref);

    } else {
        bref->process_bundle_received_event(event);
        //log_msg(oasys::LOG_WARN, 
        //        "bundle_received - updated existing?? Bundle ID: %" PRIu64 " dest: %s len: %d", 
        //        id, bref->dest().c_str(), bref->length());
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_custody_accepted_event(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_custody_accepted_event& event = msg->bundle_custody_accepted_event().get();

    uint64_t id = event.local_id();

    EhsBundleRef bref("process_bundle_custody_accepted_event");
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_custody_accepted(id);
    if (bref == NULL) {
        //log_msg(oasys::LOG_WARN, "bundle_custody_accepted - Bundle ID: %" PRIu64 " --- Not found - request details?", 
        //        id);
    } else {
        bref->process_bundle_custody_accepted_event(event);
        //log_msg(oasys::LOG_DEBUG, 
        //        "bundle_custody_accepted - Bundle ID: %" PRIu64 " dest: %s --- Routing bundle", 
        //        id, bref->dest().c_str());

        custody_bundles_.insert(EhsBundlePair(bref->custodyid(), bref));
        route_bundle(bref);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_expired_event(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_expired_event& event = msg->bundle_expired_event().get();

    uint64_t id = event.local_id();

    EhsBundleRef bref("process_bundle_expired_event");
    bref = all_bundles_.bundle_expired(id);
    if (bref != NULL) {
        // Flag bundle as deleted
        bref->set_deleted();

        if (bref->custodyid() > 0) {
            custody_bundles_.erase(bref->custodyid());
        }

        dec_bundle_stats_by_src_dst(bref.object());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        //log_msg(oasys::LOG_INFO, "bundle expired - ID = %lu", id);
    } else {
        //log_msg(oasys::LOG_DEBUG, "bundle expired - not found - ID: %lu", id);
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_data_transmitted_event(rtrmessage::bpa* msg)
{
    rtrmessage::data_transmitted_event& event = msg->data_transmitted_event().get();

    uint64_t id = event.local_id();
    int bytes_sent = event.bytes_sent();
    bool success = bytes_sent > 0;
    int reliably_sent = event.reliably_sent();
    std::string link_id = event.link_id();

    EhsBundleRef bref("data_transmitted_event");
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_transmitted(id, success);
    if (bref != NULL) {
        if (0 == bytes_sent) {
            // LTPUDP CLA timed out trying to transmit the bundle
            // reroute to try again 
            //log_msg(oasys::LOG_DEBUG, 
            //        "Transmit Failure - rerouting Bundle ID: %" PRIu64,
            //        id);
            route_bundle(bref);
        } else {
            if (bref->local_custody()) {
                //log_msg(oasys::LOG_DEBUG, 
                //        "data_transmitted - Bundle ID: %" PRIu64 " "
                //        "link: %s  bytes: %d reliably: %d - in custody not deleting yet", 
                //        id, link_id.c_str(), bytes_sent, reliably_sent);
            } else {
                bref->set_deleted();
                all_bundles_.erase_bundle(bref);

                del_bundle_by_dest(bref);

                //log_msg(oasys::LOG_DEBUG, 
                //        "data_transmitted - Bundle ID: %" PRIu64 " "
                //        "link: %s  bytes: %d reliably: %d - deleting since not in local custody", 
                //        id, link_id.c_str(), bytes_sent, reliably_sent);
            }

            router_->post_event(new EhsBundleTransmittedEvent(link_id));

            // Inform the router in case it also has a copy
            if (bref->refcount() > 1) {
                router_->post_event(new EhsDeleteBundleReq(bref));
            }
        }
    } else {
        // Probably received notice from LTP after the bundle expired (dtnping with 30 secs)
        log_msg(oasys::LOG_WARN, 
                "data_transmitted - unknown Bundle ID: %" PRIu64 " "
                "link: %s  bytes: %d reliably: %d", 
                id, link_id.c_str(), bytes_sent, reliably_sent);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_send_cancelled_event(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_send_cancelled_event& event = msg->bundle_send_cancelled_event().get();

    uint64_t id = event.local_id();
    std::string link_id = event.link_id();
    bool success = false;

    EhsBundleRef bref("bundle_send_cancelled_event");
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_transmitted(id, success);
    if (bref != NULL) {
        // reroute to try again 
        //log_msg(oasys::LOG_DEBUG, 
        //        "Bundle Send Cancelled (link disconnected) - rerouting Bundle ID: %" PRIu64,
        //        id);
        route_bundle(bref);
    } else {
        // Probably received notice from LTP after the bundle expired (dtnping with 30 secs)
        //log_msg(oasys::LOG_WARN, 
        //        "bundle_send_cancelled - unknown Bundle ID: %" PRIu64 " "
        //        "link: %sd", 
        //        id, link_id.c_str());
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_delivered_event(rtrmessage::bpa* msg)
{
    rtrmessage::bundle_delivered_event& event = msg->bundle_delivered_event().get();

    uint64_t id = event.local_id();

    finalize_bundle_delivered_event(id);
}


//----------------------------------------------------------------------
void
EhsDtnNode::finalize_bundle_delivered_event(uint64_t id)
{
    EhsBundleRef bref("bundle_delivered_event");
    bref = all_bundles_.bundle_delivered(id);
    if (bref != NULL) {
        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(id);

        del_bundle_by_dest(bref);

        if (bref->custodyid() > 0) {
            custody_bundles_.erase(bref->custodyid());
        }

        //log_msg(oasys::LOG_DEBUG, 
        //        "bundle_delivered - Bundle ID: %" PRIu64,
        //        id);

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            //log_msg(oasys::LOG_WARN, 
            //        "bundle_delivered - Bundle ID: %" PRIu64 " - refcount = %d ??",
            //        id, bref->refcount());
            router_->post_event(new EhsDeleteBundleReq(bref));
        }
        
    } else {
        //log_msg(oasys::LOG_WARN, 
        //        "bundle_delivered - unknown Bundle ID: %" PRIu64,
        //        id);
        delivered_bundle_id_list_.insert(DeliveredBundleIDPair(id, id));
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_custody_signal_event(rtrmessage::bpa* msg)
{
    rtrmessage::custody_signal_event& event = msg->custody_signal_event().get();
    rtrmessage::custody_signal_event::custody_signal_attr::type attr = event.custody_signal_attr();

    uint64_t id = event.local_id();

    // Success flag and reason code
    bool succeeded = attr.succeeded();
    unsigned char reason = attr.reason();

    // Release custody and delete if succeeded or duplicate  else error
    if (!succeeded && (BundleProtocol::CUSTODY_REDUNDANT_RECEPTION != reason)) {
        log_msg(oasys::LOG_ERR,
                "process_custody_signal_event - Received failure CS -- reason: %u", 
                reason);
        return;
    }

        //dz debug
        //log_msg(oasys::LOG_ALWAYS,
        //        "process_custody_signal_event - Bundle: %" PRIu64 " succeeded: %s reason: %u",
        //        id, succeeded?"true":"false",reason);

    
    EhsBundleRef bref("process_custody_signal_event");
    
    bref = all_bundles_.bundle_custody_released(id);
    if (bref != NULL) {
        custody_bundles_.erase(bref->custodyid());

        bref->release_custody();
        bref->set_deleted();
        all_bundles_.erase_bundle(bref);

        del_bundle_by_dest(bref);

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        //log_msg(oasys::LOG_DEBUG, 
        //        "Custody signal - Bundle ID: %" PRIu64 " (Custody ID: %" PRIu64 ")",
        //        bref->bundleid(), bref->custodyid());
    } else {
        //log_msg(oasys::LOG_WARN, "Custody Bundle not found for custody signal - Bundle ID: %" PRIu64, 
        //        id);
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_aggregate_custody_signal_event(rtrmessage::bpa* msg)
{
    rtrmessage::aggregate_custody_signal_event& event = msg->aggregate_custody_signal_event().get();

    rtrmessage::aggregate_custody_signal_event::acs_data::type hex_data = event.acs_data();

    // parse the ACS data to release bundles that have been transferred
    int len = hex_data.size();    
    if (len < 3) {
        log_msg(oasys::LOG_ERR,
                "process_aggregate_custody_signal_event - Received ACS with too few bytes: %d",
                len);
        return;
    }

    char* bp = hex_data.begin();

    // 1 byte Admin Payload Type + Flags:
    char admin_type = (*bp >> 4);
    //char admin_flags = *bp & 0xf;  -- not used
    bp++;
    len--;

    // validate the admin type
    if (admin_type != BundleProtocol::ADMIN_AGGREGATE_CUSTODY_SIGNAL) {
        log_msg(oasys::LOG_ERR,
                "process_aggregate_custody_signal_event - Received ACS with invalid Admin Type: %02x", 
                admin_type);
        return;
    }

    // Success flag and reason code
    bool succeeded = (0 != (*bp & 0x80));
    unsigned char reason = (*bp & 0x7f);
    bp++;
    len--;

    // Release custody and delete if succeeded or duplicate 
    if (!succeeded && (BundleProtocol::CUSTODY_REDUNDANT_RECEPTION != reason)) {
        log_msg(oasys::LOG_ERR,
                "process_aggregate_custody_signal_event - Received failure ACS -- reason: %u", 
                (*bp & 0x7f));
        return;
    }

    
//    u_int64_t last_custody_id =  highest_bundle_id();

    EhsBundleIterator find_iter;
    EhsBundleRef bref("process_aggregate_custody_signal_event");
    

    // parse out the entries to fill the acs entry map
    u_int64_t fill_len = 0;
    u_int64_t right_edge = 0;
    u_int64_t left_edge = 0;
    u_int64_t diff;
    int num_bytes;
    while ( len > 0 ) {
        // read the diff between previous right edge and this left edge
        num_bytes = SDNV::decode(bp, len, &diff);
        if (num_bytes == -1) { 
            log_msg(oasys::LOG_ERR,
                    "process_aggregate_custody_signal_event - Error SDNV decoding left edge diff - aborting");
            return; 
        }
        bp  += num_bytes;
        len -= num_bytes;

        // read the length of the fill
        num_bytes = SDNV::decode(bp, len, &fill_len);
        if (num_bytes == -1) { 
            log_msg(oasys::LOG_ERR,
                    "process_aggregate_custody_signal_event - Error SDNV decoding length of fill - aborting");
            return; 
        }
        bp  += num_bytes;
        len -= num_bytes;

        // calc left edge for this fill from previous right edge
        left_edge = right_edge + diff;

        // calc new right edge for this fill
        right_edge = left_edge + (fill_len - 1);  // new right edge for this fill
                                                   // parenthesized to prevent uint overflow

        if (0 == left_edge) {
            log_msg(oasys::LOG_ERR,
                    "process_aggregate_custody_signal_event - Received ACS with a Custody ID of zero "
                    "which is invalid - abort");
            return;
        }

        // check for a 64 bit overflow 
        if (fill_len > (UINT64_MAX - left_edge + 1)) { 
            log_msg(oasys::LOG_ERR,
                    "process_aggregate_custody_signal_event - Received ACS with Length of Fill "
                    "which overflows 64 bits - abort");
            return; 
        }

        // A malicious attack could take the form of a single entry 
        // specifying left edge=0 and length_of_fill=0xffffffffffffffff 
        // which would wipe out all custody bundles and tie up the thread 
        // in a near infinite loop. We can check that the last custody id 
        // in the ACS is not greater than the last issued Custody ID and 
        // kick it out if it is.
        //if ( (entry->left_edge_ > last_custody_id) ||
        //     (fill_len > (last_custody_id - entry->left_edge_ + 1)) ) { 
        //    log_crit_p("/dtn/acs/", "Received ACS attempting to "
        //               "acknowledge bundles beyond those issued - "
        //               "abort");
        //    return false; 
        //}

        for (u_int64_t custodyid=left_edge; custodyid<=right_edge; ++custodyid) {
            find_iter = custody_bundles_.find(custodyid);
            if (find_iter != custody_bundles_.end()) {
                bref = find_iter->second;

                custody_bundles_.erase(find_iter);

                all_bundles_.bundle_custody_released(bref->bundleid());

                bref->release_custody();

                bref->set_deleted();
                all_bundles_.erase_bundle(bref);

                del_bundle_by_dest(bref);

                // Inform the router in case it also has a copy
                if (bref->refcount() > 1) {
                    router_->post_event(new EhsDeleteBundleReq(bref));
                }

                //log_msg(oasys::LOG_DEBUG, 
                //        "ACS signal - Bundle ID: %" PRIu64 " (Custody ID: %" PRIu64 ")",
                //        bref->bundleid(), custodyid);
            } else {
                //log_msg(oasys::LOG_WARN, "Custody Bundle not found for ACS signal - Custody ID: %" PRIu64, 
                //        custodyid);
            }
        }
    }


}

//----------------------------------------------------------------------
void
EhsDtnNode::process_custody_timeout_event(rtrmessage::bpa* msg)
{
    rtrmessage::custody_timeout_event& event = msg->custody_timeout_event().get();

    uint64_t id = event.local_id();

    EhsBundleRef bref("bundle_delivered_event");
    bref = all_bundles_.find(id);
    if (bref != NULL) {
        //log_msg(oasys::LOG_DEBUG, 
        //        "custody_timeout - Bundle ID: %" PRIu64,
        //        id);

        route_bundle(bref);
        
    } else {
        //log_msg(oasys::LOG_WARN, 
        //        "custody_timeout - unknown Bundle ID: %" PRIu64,
        //        id);
    }
}




//----------------------------------------------------------------------
void 
EhsDtnNode::handle_bpa_received(EhsBpaReceivedEvent* event)
{
    rtrmessage::bpa* in_bpa = event->bpa_ptr_.get();

    uint64_t seq_ctr = in_bpa->sequence_ctr();

    // check for gaps in sequence counter
    if (last_recv_seq_ctr_ != 0) {
        if (seq_ctr != last_recv_seq_ctr_+1) {
            if (seq_ctr > last_recv_seq_ctr_) {
                log_msg(oasys::LOG_ERR,
                        "Possible missed messages - Last SeqCtr: %" PRIu64 " Curr SeqCtr: %" PRIu64 " - diff: %" PRIu64,
                        last_recv_seq_ctr_, seq_ctr, (seq_ctr - last_recv_seq_ctr_));
            } else if (last_recv_seq_ctr_ != 0 && seq_ctr != 0) {
                log_msg(oasys::LOG_ERR,
                        "Sequence Counter jumped backwards - Last SeqCtr: %" PRIu64 " Curr SeqCtr: %" PRIu64,
                        last_recv_seq_ctr_, seq_ctr);
            }
        }
    }
    last_recv_seq_ctr_ = seq_ctr; 

    // Determine the type of message and handle it
    if (in_bpa->hello_interval().present()) {
        // The hello message may also contain an alert of "justBooted"
        if (! in_bpa->alert().present()) {
            //log_msg(oasys::LOG_DEBUG, "received msg type: hello - interval = %d  eid = %s", 
            //         in_bpa->hello_interval().get(),
            //         in_bpa->eid().get().c_str());
        } else {
            log_msg(oasys::LOG_ALWAYS, "received msg type: hello - interval = %d  eid = %s alert = %s", 
                     in_bpa->hello_interval().get(),
                     in_bpa->eid().get().c_str(),
                     in_bpa->alert().get().c_str());
        }
        return;
    }

    if (!have_link_report_)
    {
        // discard all messages until we have gotten the link report
        // - prevent new incoming bundles being rejected due to an unknown link
        if (in_bpa->link_report().present()) {
            router_->post_event(new EhsBpaReceivedEvent(event->bpa_ptr_), false); // false = post at head

            // give router time to process the report
            sleep(1);
            have_link_report_ = true;
            log_msg(oasys::LOG_ALWAYS, "EhsDtn enabling full processing after receiving link report - SeqCtr: %" PRIu64,
                     last_recv_seq_ctr_);
        } else {
            log_msg(oasys::LOG_WARN, "Discarding message becaues link report has net been received yet - SeqCtr: %" PRIu64,
                     last_recv_seq_ctr_);
        }
        return;
    }


    // pass all of the link events to the router 
    if (in_bpa->link_report().present() ||
               in_bpa->link_available_event().present() ||
               in_bpa->link_opened_event().present() ||
               in_bpa->link_unavailable_event().present() ||
               in_bpa->link_closed_event().present() ||
               in_bpa->link_busy_event().present()) {
        //XXX/dz Tempting, but I think a post at front of queue could cause issues
        router_->post_event(new EhsBpaReceivedEvent(event->bpa_ptr_));

    } else if (in_bpa->bundle_report().present()) {
        log_msg(oasys::LOG_DEBUG, "received msg type: bundle_report");
        process_bundle_report(in_bpa);

    } else if (in_bpa->bundle_received_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: bundle_received_event");
        process_bundle_received_event(in_bpa);

    } else if (in_bpa->bundle_custody_accepted_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: bundle_custody_accepted_event");
        process_bundle_custody_accepted_event(in_bpa);

    } else if (in_bpa->bundle_expired_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: bundle_expired_event");
        process_bundle_expired_event(in_bpa);

    } else if (in_bpa->data_transmitted_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: data_transmitted_event");
        process_data_transmitted_event(in_bpa);

    } else if (in_bpa->custody_signal_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: custody_signal_event");
        process_custody_signal_event(in_bpa);

    } else if (in_bpa->aggregate_custody_signal_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: aggregate_custody_signal_event");
        process_aggregate_custody_signal_event(in_bpa);

    } else if (in_bpa->bundle_delivered_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: bundle_delivered_event");
        process_bundle_delivered_event(in_bpa);

    } else if (in_bpa->custody_timeout_event().present()) {
        //log_msg(oasys::LOG_INFO, "received msg type: custody_timeout_event");
        process_custody_timeout_event(in_bpa);

    } else if (in_bpa->bundle_send_cancelled_event().present()) {
        process_bundle_send_cancelled_event(in_bpa);

    } else if (in_bpa->bundle_delivery_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: bundle_delivery_event");
    } else if (in_bpa->bundle_injected_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: bundle_injected_event");
    } else if (in_bpa->link_created_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: link_created_event");
    } else if (in_bpa->link_deleted_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: link_deleted_event");
    } else if (in_bpa->link_available_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: link_available_event");
    } else if (in_bpa->link_attribute_changed_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: link_attribute_changed_event");
    } else if (in_bpa->contact_attribute_changed_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: contact_attribute_changed_event");
    } else if (in_bpa->eid_reachable_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: eid_reachable_event");
    } else if (in_bpa->route_add_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: route_add_event");
    } else if (in_bpa->route_delete_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: route_delete_event");
    } else if (in_bpa->intentional_name_resolved_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: intentional_name_resolved_event");
    } else if (in_bpa->registration_added_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: registration_added_event");
    } else if (in_bpa->registration_removed_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: registration_removed_event");
    } else if (in_bpa->registration_expired_event().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: registration_expired_event");
    } else if (in_bpa->open_link_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: open_link_request");
    } else if (in_bpa->close_link_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: close_link_request");
    } else if (in_bpa->add_link_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: add_link_request");
    } else if (in_bpa->delete_link_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: delete_link_request");
    } else if (in_bpa->reconfigure_link_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: reconfigure_link_request");
    } else if (in_bpa->send_bundle_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: send_bundle_request");
    } else if (in_bpa->send_bundle_broadcast_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: send_bundle_broadcast_request");
    } else if (in_bpa->cancel_bundle_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: cancel_bundle_request");
    } else if (in_bpa->inject_bundle_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: inject_bundle_request");
    } else if (in_bpa->delete_bundle_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: delete_bundle_request");
    } else if (in_bpa->set_cl_params_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: set_cl_params_request");
    } else if (in_bpa->intentional_name_resolution_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: intentional_name_resolution_request");
    } else if (in_bpa->deliver_bundle_to_app_request().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: deliver_bundle_to_app_request");
    } else if (in_bpa->link_attributes_report().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: link_attributes_report");
    } else if (in_bpa->contact_report().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: contact_report");
    } else if (in_bpa->route_report().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: route_report");
    } else if (in_bpa->bundle_attributes_report().present()) {
        log_msg(oasys::LOG_NOTICE, "received msg type: bundle_attributes_report");
    } else {
        //??  log_msg(oasys::LOG_ERR, "Unhandled message: %s", in_bpa->type());
        log_msg(oasys::LOG_ERR, "Unhandled message: ");
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::handle_free_bundle_req(EhsFreeBundleReq* event)
{
    if (event->bundle_->refcount() > 0) {
        log_msg(oasys::LOG_CRIT, "Free Referenced Bundle: %lu  refcount: %d", 
                event->bundle_->bundleid(), event->bundle_->refcount());
    } else {
        //log_msg(oasys::LOG_DEBUG, "Free Bundle: %lu  refcount: %d", 
        //        event->bundle_->bundleid(), event->bundle_->refcount());
    }

    delete event->bundle_;
    event->bundle_ = NULL;
}

//----------------------------------------------------------------------
//void 
//EhsDtnNode::handle_delete_bundle_req(EhsDeleteBundleReq* event)
//{
//    EhsBundleIterator find_iter;
//    find_iter = all_bundles_.find(event->bref_->bundleid());
//    if (find_iter != all_bundles_.end()) {
//        event->bref_->set_deleted();
//
//        all_bundles_.erase(find_iter);
//
//        if (event->bref_->custodyid() > 0) {
//            all_bundles_.dec_custody(event->bref);
//            custody_bundles_.erase(event->bref_->custodyid());
//        }
//
//        dec_bundle_stats_by_src_dst(event->bref_.object());
//
//        // Inform the router in case it also has a copy
//        // (just in case we gave it back before we processed this)
//        if (event->bref_->refcount() > 1) {
//            router_->post_event(new EhsDeleteBundleReq(event->bref_));
//        }
//
//        log_msg(oasys::LOG_DEBUG, "delete_bundle_req - ID = %lu", event->bref_->bundleid());
//    } else {
//        log_msg(oasys::LOG_WARN, "delete_bundle_req - not found ID = %lu", event->bref_->bundleid());
//    }
//}

//----------------------------------------------------------------------
void
EhsDtnNode::handle_event(EhsEvent* event)
{
    dispatch_event(event);
    
    event_handlers_completed(event);

//    stats_.events_processed_++;
}

//----------------------------------------------------------------------
void
EhsDtnNode::event_handlers_completed(EhsEvent* event)
{
    (void) event;
}


//----------------------------------------------------------------------
void
EhsDtnNode::run()
{
    parent_->get_accept_custody_list(accept_custody_);


    // start the router thread running
    router_->set_prime_mode(prime_mode_);
    router_->set_log_level(log_level_);
    router_->start();
    router_->set_fwdlnk_enabled_state(fwdlnk_enabled_);
    router_->set_fwdlnk_aos_state(fwdlnk_aos_);

    // initiate configuration of the new EhsDtnNode
    // -- request list of bundles, links etc.

    // discard all messages until we have gotten the link report
    // - prevent new incoming bundles being rejected due to an unknown link
    have_link_report_ = false;

    log_msg(oasys::LOG_NOTICE, 
            "DTN node discovered: %s [%s] - request list of links, bundles, etc.", 
            eid_.c_str(), eid_ipn_.c_str());

    rtrmessage::bpa msg1;
    rtrmessage::bpa::link_query::type lq;
    msg1.link_query(lq);
    send_msg(msg1);

    rtrmessage::bpa msg2;
    rtrmessage::bpa::bundle_query::type bq;
    msg2.bundle_query(bq);
    send_msg(msg2);

    rtrmessage::bpa msg3;
    rtrmessage::bpa::contact_query::type cq;
    msg3.contact_query(cq);
    send_msg(msg3);



    EhsEvent* event;

    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    while (1) {
        if (should_stop()) {
            log_debug("EhsDtnNode: stopping");
            break;
        }

        int timeout = 100;

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            //oasys::Time now;
            //now.get_time();

            //if (now >= event->posted_time_) {
                //oasys::Time in_queue;
                //in_queue = now - event->posted_time_;
                //if (in_queue.sec_ > 2) {
                //    log_msg(oasys::LOG_DEBUG, "event %s was in queue for %u.%u seconds",
                //               event->type_str(), in_queue.sec_, in_queue.usec_);
                //}
            //}
            
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
}

//----------------------------------------------------------------------
void
EhsDtnNode::send_msg(rtrmessage::bpa& msg)
{
    // limit lock to just incrementing the sequence counter
    do {
        oasys::ScopeLock l(&seqctr_lock_, __FUNCTION__);

        msg.sequence_ctr(send_seq_ctr_++);
    } while (false);

    parent_->send_msg(msg, eid());
}

//----------------------------------------------------------------------
void
EhsDtnNode::route_bundle(EhsBundleRef& bref)
{
    // if destination is local node then add to the undelivered list
    if (is_local_destination(bref)) {
        undelivered_bundles_.insert(EhsBundlePair(bref->bundleid(), bref));
    } else {
        // Pass the new bundle to the router
        router_->post_event(new EhsRouteBundleReq(bref));
    }
}

//----------------------------------------------------------------------
bool
EhsDtnNode::is_local_destination(EhsBundleRef& bref)
{
    bool result = ((bref->is_ipn_dest() &&  
                    bref->ipn_dest_node() == node_id_) ||
                   // check the DTN scheme??
                   (0 == eid_.compare(0, eid_.length(), bref->dest())));

    return result;
}

//----------------------------------------------------------------------
bool
EhsDtnNode::is_local_node(std::string& check_eid)
{
    bool result = false;

    if (0 == strncmp("ipn:", check_eid.c_str(), 4)) {
        char* end;
        uint64_t node_id = strtoull(check_eid.c_str()+4, &end, 10);
        result = is_local_node(node_id);
    } else {
        result = (0 == eid_.compare(0, eid_.length(), check_eid));
    }

    return result;
}

//----------------------------------------------------------------------
bool
EhsDtnNode::is_local_admin_eid(std::string check_eid)
{
    //allow ping destinations also
    return ((0 == check_eid.compare(eid_ipn_)) || (0 == check_eid.compare(eid_ipn_ping_)) ||
            (0 == check_eid.compare(eid_)) || (0 == check_eid.compare(eid_ping_)));
}

//----------------------------------------------------------------------
bool
EhsDtnNode::is_local_node(uint64_t check_node)
{
    return (check_node == node_id_);
}

//----------------------------------------------------------------------
void
EhsDtnNode::bundle_list(oasys::StringBuffer* buf)
{
    all_bundles_.bundle_list(buf);
}


//----------------------------------------------------------------------
void
EhsDtnNode::bundle_stats(oasys::StringBuffer* buf)
{
    all_bundles_.bundle_stats(buf, undelivered_bundles_.size(), router_->num_unrouted_bundles());
}

//----------------------------------------------------------------------
void
EhsDtnNode::bundle_stats_by_src_dst(int* count, EhsBundleStats** stats)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    all_bundles_.bundle_stats_by_src_dst(count, stats);
}

//----------------------------------------------------------------------
void
EhsDtnNode::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    all_bundles_.fwdlink_interval_stats(count, stats);
}


//----------------------------------------------------------------------
void
EhsDtnNode::unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf)
{
    router_->unrouted_bundle_stats_by_src_dst(buf);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::update_statistics(uint64_t* received, uint64_t* transmitted, uint64_t* transmit_failed,
                              uint64_t* delivered, uint64_t* rejected,
                              uint64_t* pending, uint64_t* custody,
                              bool* fwdlnk_aos, bool* fwdlnk_enabled)
{
    all_bundles_.get_bundle_stats(received, transmitted, transmit_failed, delivered, rejected, pending, custody);
    *fwdlnk_aos = fwdlnk_aos_;
    *fwdlnk_enabled = fwdlnk_enabled_;
}

//----------------------------------------------------------------------
void
EhsDtnNode::link_dump(oasys::StringBuffer* buf)
{
    if (NULL != router_) {
        router_->link_dump(buf);
    } else {
        buf->append("Link dump: Router not instantiated\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::fwdlink_transmit_dump(oasys::StringBuffer* buf)
{
    if (NULL != router_) {
        router_->fwdlink_transmit_dump(buf);
    } else {
        buf->append("FwdLink Transmit dump: Router not instantiated\n\n");
    }
}

//----------------------------------------------------------------------
bool 
EhsDtnNode::get_link_configuration(EhsLink* el)
{
    return parent_->get_link_configuration(el);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_link(std::string link_id)
{
    if (NULL != router_) {
        router_->reconfigure_link(link_id);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_max_expiration_fwd(uint64_t exp)
{
    all_bundles_.set_max_expiration_fwd(exp);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_max_expiration_rtn(uint64_t exp)
{
    all_bundles_.set_max_expiration_rtn(exp);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::fwdlink_transmit_enable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list)
{
    if (NULL != router_) {
        router_->fwdlink_transmit_enable(src_list, dst_list);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::fwdlink_transmit_disable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list)
{
    if (NULL != router_) {
        router_->fwdlink_transmit_disable(src_list, dst_list);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::set_fwdlnk_force_LOS_while_disabled(bool force_los)
{
    if (NULL != router_) {
        router_->set_fwdlnk_force_LOS_while_disabled(force_los);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::link_enable(EhsLinkCfg* cl)
{
    if (NULL != router_) {
        router_->link_enable(cl);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::link_disable(std::string& linkid)
{
    if (NULL != router_) {
        router_->link_disable(linkid);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::set_link_statistics(bool enabled)
{
    if (NULL != router_) {
        router_->set_link_statistics(enabled);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::get_fwdlink_transmit_enabled_list(EhsSrcDstWildBoolMap& enabled_list)
{
    parent_->get_fwdlink_transmit_enabled_list(enabled_list);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_accept_custody()
{
    parent_->get_accept_custody_list(accept_custody_);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_source_priority()
{
    if (NULL != router_) {
        router_->reconfigure_source_priority();
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_dest_priority()
{
    if (NULL != router_) {
        router_->reconfigure_dest_priority();
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::get_source_node_priority(NodePriorityMap& pmap)
{
    parent_->get_source_node_priority(pmap);
}

//----------------------------------------------------------------------
void
EhsDtnNode::get_dest_node_priority(NodePriorityMap& pmap)
{
    parent_->get_dest_node_priority(pmap);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::set_fwdlnk_enabled_state(bool state)
{
    fwdlnk_enabled_ = state;

    if (NULL != router_) {
        router_->set_fwdlnk_enabled_state(fwdlnk_enabled_);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::set_fwdlnk_aos_state(bool state)
{
    fwdlnk_aos_ = state;

    if (NULL != router_) {
        router_->set_fwdlnk_aos_state(fwdlnk_aos_);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::set_fwdlnk_throttle(uint32_t bps)
{
    if (NULL != router_) {
        router_->set_fwdlnk_throttle(bps);
    }
}

//----------------------------------------------------------------------
int
EhsDtnNode::bundle_delete(uint64_t source_node_id, uint64_t dest_node_id)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleMap bidlist;
    all_bundles_.bundle_id_list(bidlist, source_node_id, dest_node_id);

    int cnt = 0;
    EhsBundleRef bref("bundle_delete");
    EhsBundleIterator iter = bidlist.begin();
    while (iter != bidlist.end()) {
        bref = iter->second;
        ++cnt;

        // issue delete request for the bundle
        rtrmessage::delete_bundle_request req;
        req.local_id() = bref->bundleid();
        req.gbofid_str(bref->gbofid_str());

        rtrmessage::bpa message; 
        message.delete_bundle_request(req);
        send_msg(message);

        // flag bundle as deleted
        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(bref->bundleid());

        del_bundle_by_dest(bref);

        if (bref->custodyid() > 0) {
            custody_bundles_.erase(bref->custodyid());
        }

        log_msg(oasys::LOG_DEBUG, 
                "bundle_delete (by src-dst) - deleting Bundle ID: %" PRIu64,
                bref->bundleid());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        bidlist.erase(iter);
        iter = bidlist.begin();
    }

    return cnt;
}

//----------------------------------------------------------------------
int
EhsDtnNode::bundle_delete_all()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleMap bidlist;
    all_bundles_.bundle_id_list(bidlist);

    int cnt = 0;
    EhsBundleRef bref("bundle_delete");
    EhsBundleIterator iter = bidlist.begin();
    while (iter != bidlist.end()) {
        bref = iter->second;
        ++cnt;

        // issue delete request for the bundle
        rtrmessage::delete_bundle_request req;
        req.local_id() = bref->bundleid();
        req.gbofid_str(bref->gbofid_str());

        rtrmessage::bpa message; 
        message.delete_bundle_request(req);
        send_msg(message);

        // flag bundle as deleted
        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(bref->bundleid());

        del_bundle_by_dest(bref);

        if (bref->custodyid() > 0) {
            custody_bundles_.erase(bref->custodyid());
        }

        log_msg(oasys::LOG_DEBUG, 
                "bundle_delete (all) - deleting Bundle ID: %" PRIu64,
                bref->bundleid());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        bidlist.erase(iter);
        iter = bidlist.begin();
    }

    return cnt;
}

//----------------------------------------------------------------------
void 
EhsDtnNode::add_bundle_by_dest(EhsBundleRef& bref)
{
    uint64_t dst = bref->ipn_dest_node();

    EhsBundleMap* bmap;

    BUNDLES_BY_DEST_ITER iter = bundles_by_dest_map_.find(dst);
    if (iter == bundles_by_dest_map_.end()) {
       bmap = new EhsBundleMap();

       bundles_by_dest_map_[dst] = bmap;
    } else {
       bmap = iter->second;
    }

    bmap->insert(EhsBundlePair(bref->bundleid(), bref));
}

//----------------------------------------------------------------------
void 
EhsDtnNode::del_bundle_by_dest(EhsBundleRef& bref)
{
    uint64_t dst = bref->ipn_dest_node();

    EhsBundleMap* bmap;

    BUNDLES_BY_DEST_ITER iter = bundles_by_dest_map_.find(dst);
    if (iter == bundles_by_dest_map_.end()) {
        log_msg(oasys::LOG_ERR, 
                "del_bundle_by_dest - Bundle Map not found for destination %s",
                bref->dest().c_str());
    } else {
        bmap = iter->second;
        bmap->erase(bref->bundleid());
    }
}


//----------------------------------------------------------------------
uint64_t
EhsDtnNode::check_for_missed_bundles(uint64_t ipn_node_id)
{
        log_msg(oasys::LOG_DEBUG, 
                "check_for_missed_bundles - Destination IPN Node ID: %" PRIu64,
                ipn_node_id);

    uint64_t num_routed = 0;

    EhsBundleMap* bmap;

    BUNDLES_BY_DEST_ITER iter = bundles_by_dest_map_.find(ipn_node_id);
    if (iter != bundles_by_dest_map_.end()) {
       bmap = iter->second;

       if (!bmap->empty()) {
            EhsBundleRef bref(__func__);
            EhsBundleMap::iterator biter = bmap->begin();
            while (biter != bmap->end()) {
                bref = biter->second;

                route_bundle(bref);
                ++num_routed;

                ++biter;
            }
        }
    }

    return num_routed;
}




//----------------------------------------------------------------------
void 
EhsDtnNode::set_log_level(int level)
{
    log_level_ = level;
    router_->set_log_level(log_level_);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::log_msg(const std::string& path, oasys::log_level_t level, const char*format, ...)
{
    if ((int)level >= log_level_) {
        va_list args;
        va_start(args, format);
        parent_->log_msg_va(path, level, format, args);
        va_end(args);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args)
{
    if ((int)level >= log_level_) {
        parent_->log_msg_va(path, level, format, args);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::log_msg(const std::string& path, oasys::log_level_t level, std::string& msg)
{
    if ((int)level >= log_level_) {
        parent_->log_msg(path, level, msg);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::log_msg(oasys::log_level_t level, const char*format, ...)
{
    // internal use only - passes the logpath_ of this object
    if ((int)level >= log_level_) {
        va_list args;
        va_start(args, format);
        parent_->log_msg_va(logpath_, level, format, args);
        va_end(args);
    }
}


} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED
