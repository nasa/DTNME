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

#include <third_party/oasys/io/IO.h>

#include "bundling/BundleProtocol.h"
#include "bundling/BundleProtocolVersion6.h"
#include "bundling/SDNV.h"
#include "EhsDtnNode.h"
#include "EhsExternalRouterImpl.h"
#include "EhsRouter.h"


namespace dtn {


//----------------------------------------------------------------------
EhsDtnNode::EhsDtnNode(EhsExternalRouterImpl* parent, 
                       std::string eid, std::string eid_ipn)
    : EhsEventHandler("EhsDtnNode", "/ehs/node/%s", eid.c_str()),
      Thread("EhsDtnNode"),
      ExternalRouterClientIF("EhsDtnNode"),
      eid_(eid),
      eid_ping_(eid),
      eid_ipn_(eid_ipn),
      eid_ipn_ping_(eid_ipn),
      eid_ipn_dtpc_(eid_ipn),
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

        // change the service number from 0 to 129
        eid_ipn_dtpc_.resize(eid_ipn_dtpc_.length()-1);
        eid_ipn_dtpc_.append("129");
    }
    
    if (0 == strncmp("dtn:", eid_.c_str(), 4)) {
        eid_ping_.append("/ping");
    }


    eventq_ = new oasys::MsgQueue<EhsEvent*>(logpath_);
    eventq_->notify_when_empty();

    router_ = new EhsRouter(this);

    last_recv_seq_ctr_ = 0;
    send_seq_ctr_ = 0;
}


//----------------------------------------------------------------------
EhsDtnNode::~EhsDtnNode()
{
    do_shutdown();

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

       bundles_by_dest_map_.erase(iter);

       iter = bundles_by_dest_map_.begin();
    }
    bundles_by_dest_map_.clear();

    delete eventq_;
}

//----------------------------------------------------------------------
void
EhsDtnNode::do_shutdown()
{
    if (should_stop()) {
        return;
    }

    set_should_stop();
    
    router_->set_should_stop();
    while (!router_->is_stopped()) {
        usleep(100000);
    }
    router_ = nullptr;

    while (!is_stopped()) {
        usleep(100000);
    }
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
bool
EhsDtnNode::shutdown_server()
{
    // issue request to shutdown the DTNME server
    client_send_shutdown_req_msg();

    return true;
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_hello_msg(CborValue& cvElement, uint64_t msg_version)
{
    // the hello msg_version can be used as master version level for the 
    // ExternalRouter I/F to determine compatibility - also each message
    // has a version for use at a lower level



    if (msg_version == 0) {
        uint64_t bundles_received = 0;
        uint64_t bundles_pending = 0;
        if (CBORUTIL_SUCCESS != decode_hello_msg_v0(cvElement, bundles_received, bundles_pending))
        {
            log_msg(oasys::LOG_ERR, "process_hello_msg: error parsing CBOR");
            return;
        }

        if ((bundles_received == last_hello_bundles_received_) &&
            (bundles_pending == last_hello_bundles_pending_)) {
            // no change in 10 seconds so we should be stablized and 
            // can compare pending values

            //XXX/dz TODO
            // if pending does not match mark all bundles as "not in report"
            // request a bundle report that sets the flag in each bundle
            // after processing the report check for bundles "not in report"
            // which are out of sync and can be deleted from our list

            uint64_t local_pending = all_bundles_.get_bundles_pending();

            if (bundles_pending != local_pending) {
                log_msg(oasys::LOG_ALWAYS, "Pending bundles (%" PRIu64 ") out of sync with DTNME server (%" PRIu64 ")",
                        local_pending, bundles_pending);

                resync_bundles_in_process_ = true;
                // set all bundles to indicate they were not in the resync report
                all_bundles_.prepare_for_resync();
                // request a current bundle report to resync with
                client_send_bundle_query_msg();

            }
        }

        last_hello_bundles_received_ = bundles_received;
        last_hello_bundles_pending_ = bundles_pending;
    }


}

//----------------------------------------------------------------------
void
EhsDtnNode::do_resync_processing()
{
    if (resync_bundles_in_process_) {
        oasys::ScopeLock l(&lock_, __func__);

        // provide the undeliveerd and custody lists so that bundles can be deleted from them also
        size_t num_deleted = all_bundles_.finalize_resync(undelivered_bundles_, custody_bundles_);

        log_msg(oasys::LOG_ALWAYS, "Reync deleted %zu bundles", num_deleted);

        resync_bundles_in_process_ = false;
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_alert_msg_v0(CborValue& cvElement)
{
    std::string alert_msg;
    if (CBORUTIL_SUCCESS != decode_alert_msg_v0(cvElement, alert_msg))
    {
        log_msg(oasys::LOG_ERR, "process_alert_msg: error parsing CBOR");
        return;
    }

    log_msg(oasys::LOG_ALWAYS, "Alert msg: %s", alert_msg.c_str());
}



//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_report_v0(CborValue& cvElement)
{
    extrtr_bundle_vector_t bundle_vec;

    bool last_msg = false;
    if (CBORUTIL_SUCCESS != decode_bundle_report_msg_v0(cvElement, bundle_vec, last_msg))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_report_v0: error parsing CBOR - num bundles = %zu",
                                bundle_vec.size());
    }

    EhsBundleRef bref("process_bundle_report_v0");



    extrtr_bundle_vector_iter_t iter = bundle_vec.begin();
    while (iter != bundle_vec.end()) {
        extrtr_bundle_ptr_t bundleptr = *iter;


        uint64_t id = bundleptr->bundleid_;

        bref = all_bundles_.find(id);
        if (bref == nullptr) {
            EhsBundle* eb = new EhsBundle(this, bundleptr);

            eb->set_is_fwd_link_destination(router_->is_fwd_link_destination(eb->ipn_dest_node()));

            bref = eb;

            if (! all_bundles_.bundle_received(bref)) {
                log_msg(oasys::LOG_CRIT, "process_bundle_report_v0 - ignoring duplicate Bundle ID: %" PRIu64
                                         " src: %s dest: %s len: %d", 
                        bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length());
                return;
            }

            add_bundle_by_dest(bref);

            if (bref->is_ecos_critical()) {
                if (critical_bundles_.insert_bundle(bref)) {
                    log_msg(oasys::LOG_WARN, "process_bundle_report_v0 - accepted ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    // TODO: start a timer to check for expired critical bundles
                } else {
                    log_msg(oasys::LOG_WARN, "process_bundle_report_v0 - rejecting duplicate ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    all_bundles_.bundle_rejected(bref);

                    // issue delete request if not authorized
                    client_send_delete_bundle_req_msg(bref->bundleid());

                    return;
                }
            }

            if (eb->local_custody()) {
                custody_bundles_.insert(EhsBundlePair(eb->bundleid(), bref));
            } else if (accept_custody_before_routing(bref)) {
                //log_msg(oasys::LOG_DEBUG, "bundle_report- added bundle - ID: %" PRIu64 " dest: %s len: %d --- waiting to accept custody before routing", 
                //        id, bref->dest().c_str(), bref->length());
                continue;
            }

            //log_msg(oasys::LOG_DEBUG, "bundle_report - added bundle - ID: %" PRIu64 " dest: %s len: %d", 
            //         id, bref->dest().c_str(), bref->length());

            if (!bundleptr->expired_in_transit_) {
                //dzdebug
                //log_msg(oasys::LOG_ALWAYS, "bundle_report/received- routing bundle ID: %" PRIu64 " dest: %s len: %d", 
                //     id, bref->dest().c_str(), bref->length());
                route_bundle(bref);
            } else {
                //dzdebug
                log_msg(oasys::LOG_WARN, "bundle_report- expired in transit: ID: %" PRIu64 " dest: %s len: %d", 
                     id, bref->dest().c_str(), bref->length());

            }

        } else {
            bref->process_bundle_report(bundleptr);

            //log_msg(oasys::LOG_DEBUG, "bundle_report - updated Bundle ID: %" PRIu64 " dest: %s len: %d", 
            //        id, bref->dest().c_str(), bref->length());
        }

        ++iter;
    }


    if (last_msg) {
        do_resync_processing();
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_received_v0(CborValue& cvElement)
{
    std::string link_id;
    extrtr_bundle_vector_t bundle_vec;

    if (CBORUTIL_SUCCESS != decode_bundle_received_msg_v0(cvElement, link_id, bundle_vec))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_received_v0: error parsing CBOR - num bundles = %zu",
                                bundle_vec.size());
    }

    EhsBundleRef bref("process_bundle_received");

    extrtr_bundle_vector_iter_t iter = bundle_vec.begin();
    while (iter != bundle_vec.end()) {
        extrtr_bundle_ptr_t bundleptr = *iter;


        uint64_t id = bundleptr->bundleid_;




        bref = all_bundles_.find(id);
        if (bref == nullptr) {
            EhsBundle* eb = new EhsBundle(this, bundleptr);

            eb->set_is_fwd_link_destination(router_->is_fwd_link_destination(eb->ipn_dest_node()));

            bref = eb;

            if (! all_bundles_.bundle_received(bref)) {
                log_msg(oasys::LOG_CRIT, "process_bundle_received_v0 - ignoring duplicate Bundle ID: %" PRIu64
                                         " src: %s dest: %s len: %d", 
                        bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length());
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

            // this call fills in the remote address for us
            std::string remote_addr = "";
            if (!router_->accept_bundle(bref, link_id, remote_addr)) {
                // not okay - send a request to delete the bundle
                all_bundles_.bundle_rejected(bref);

                log_msg(oasys::LOG_CRIT, "bundle_received - rejecting Bundle ID: %" PRIu64
                                         " src: %s dest: %s len: %d received from: %s (%s)",
                                         bref->bundleid(), bref->source().c_str(), bref->dest().c_str(), bref->length(),
                                         remote_addr.c_str(), link_id.c_str());

                 // issue delete request since not authorized
                 client_send_delete_bundle_req_msg(bref->bundleid());

                 return;
             }



            if (bref->is_ecos_critical()) {
                if (critical_bundles_.insert_bundle(bref)) {
                    log_msg(oasys::LOG_WARN, "process_bundle_received_v0 - accepted ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    // TODO: start a timer to check for expired critical bundles
                } else {
                    log_msg(oasys::LOG_WARN, "process_bundle_received_v0 - rejecting duplicate ECOS Critical Bundle: %s"
                                             "  (ID: %" PRIu64 ") received on link: %s", 
                            bref->gbofid_str().c_str(), bref->bundleid(), bref->received_from_link_id().c_str());

                    all_bundles_.bundle_rejected(bref);

                    // issue delete request if not authorized
                    client_send_delete_bundle_req_msg(bref->bundleid());

                    return;
                }
            }

            if (eb->local_custody()) {
                custody_bundles_.insert(EhsBundlePair(eb->bundleid(), bref));
            } else if (accept_custody_before_routing(bref)) {
                log_msg(oasys::LOG_DEBUG, "bundle_received- added bundle - ID: %" PRIu64 " dest: %s len: %d --- waiting to accept custody before routing", 
                        id, bref->dest().c_str(), bref->length());

                continue;
            }

            if (!bundleptr->expired_in_transit_) {
                route_bundle(bref);
            } else {
                //dzdebug
                log_msg(oasys::LOG_WARN, "bundle_received - expired in transit: ID: %" PRIu64 " dest: %s len: %d", 
                     id, bref->dest().c_str(), bref->length());

            }

        } else {
            bref->process_bundle_report(bundleptr);  // same format as Bundle Received


            log_msg(oasys::LOG_DEBUG, "bundle_received - updated Bundle ID: %" PRIu64 " dest: %s len: %d", 
                    id, bref->dest().c_str(), bref->length());
        }

        ++iter;
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_transmitted_v0(CborValue& cvElement)
{
    std::string link_id;
    uint64_t bundleid = 0;
    uint64_t bytes_sent = 0;

    if (CBORUTIL_SUCCESS != decode_bundle_transmitted_msg_v0(cvElement, link_id, bundleid, bytes_sent))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_transmitted_v0: error parsing CBOR");
        return;
    }

    bool success = bytes_sent > 0;

    EhsBundleRef bref(__func__);
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_transmitted(bundleid, success);
    if (bref != nullptr) {
        if (0 == bytes_sent) {
            // LTPUDP CLA timed out trying to transmit the bundle
            // reroute to try again 
            //log_msg(oasys::LOG_DEBUG, 
            //        "Transmit Failure - rerouting Bundle ID: %" PRIu64,
            //        bundleid);
            route_bundle(bref);
        } else {
            if (bref->local_custody()) {
                //log_msg(oasys::LOG_DEBUG, 
                //        "process_bundle_transmitted_v0 - Bundle ID: %" PRIu64 " "
                //        "link: %s  bytes: %d reliably: %d - in custody not deleting yet", 
                //        bundleid, link_id.c_str(), bytes_sent, reliably_sent);
            } else {
                bref->set_deleted();
                all_bundles_.erase_bundle(bref);

                del_bundle_by_dest(bref);

                //log_msg(oasys::LOG_DEBUG, 
                //        "process_bundle_transmitted_v0 - Bundle ID: %" PRIu64 " "
                //        "link: %s  bytes: %d reliably: %d - deleting since not in local custody", 
                //        bundleid, link_id.c_str(), bytes_sent, reliably_sent);
            }

            router_->post_event(new EhsBundleTransmittedEvent(link_id));

            // Inform the router in case it also has a copy
            if (bref->refcount() > 1) {
                router_->post_event(new EhsDeleteBundleReq(bref));
            }
        }
    } else {
        // Probably received notice from LTP after the bundle expired (dtnping testing with short TTL)
        //log_msg(oasys::LOG_DEBUG,
        //        "process_bundle_transmitted_v0 - unknown Bundle ID: %" PRIu64 " "
        //        "link: %s  bytes: %d", 
        //        bundleid, link_id.c_str(), bytes_sent);
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_delivered_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;

    if (CBORUTIL_SUCCESS != decode_bundle_delivered_msg_v0(cvElement, bundleid))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_delivered_v0: error parsing CBOR");
        return;
    }

    finalize_bundle_delivered_event(bundleid);

}


//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_expired_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;

    if (CBORUTIL_SUCCESS != decode_bundle_expired_msg_v0(cvElement, bundleid))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_expired_v0: error parsing CBOR");
        return;
    }

    EhsBundleRef bref("process_bundle_expired_v0");
    bref = all_bundles_.bundle_expired(bundleid);
    if (bref != nullptr) {
        // Flag bundle as deleted
        bref->set_deleted();

        undelivered_bundles_.erase(bref->bundleid());

        del_bundle_by_dest(bref);

        custody_bundles_.erase(bref->bundleid());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        //log_msg(oasys::LOG_DEBUG, "bundle expired - ID = %lu", id);
    } else {
        //log_msg(oasys::LOG_ALWAYS, "bundle ID: %" PRIu64 " -- expired but not found", bundleid);
        //
        //
        //XXX/dz TODO:  remove from undelivered and delievred lists -- not in all_bundles_
        //   add source / dest to update stats???
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_bundle_cancelled_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;

    if (CBORUTIL_SUCCESS != decode_bundle_cancelled_msg_v0(cvElement, bundleid))
    {
        log_msg(oasys::LOG_ERR, "process_bundle_cancelled_v0: error parsing CBOR");
        return;
    }

        //dzdebug
        //log_msg(oasys::LOG_ALWAYS, "bundle ID: %" PRIu64 " -- cancelled", bundleid);

    EhsBundleRef bref(__func__);
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_transmitted(bundleid, false);
    if (bref != nullptr) {
        // reroute to try again 
        if (!resync_bundles_in_process_) {
            //log_msg(oasys::LOG_ALWAYS, 
            //        "Bundle Send Cancelled (link disconnected) - rerouting Bundle ID: %" PRIu64,
            //        bundleid);
            route_bundle(bref);
        }
    } else {
        // Probably received notice from LTP after the bundle expired (dtnping with 30 secs)
        log_msg(oasys::LOG_ALWAYS, 
                "bundle_send_cancelled - unknown Bundle ID: %" PRIu64 " ",
                bundleid);
                //"link: %sd", 
                //bundleid, link_id.c_str());
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_custody_timeout_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;

    if (CBORUTIL_SUCCESS != decode_custody_timeout_msg_v0(cvElement, bundleid))
    {
        log_msg(oasys::LOG_ERR, "process_custody_timeout_v0: error parsing CBOR");
        return;
    }

    EhsBundleRef bref(__func__);
    bref = all_bundles_.find(bundleid);
    if (bref != nullptr) {
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
EhsDtnNode::process_custody_accepted_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;
    uint64_t custody_id = 0;

    if (CBORUTIL_SUCCESS != decode_custody_accepted_msg_v0(cvElement, bundleid, custody_id))
    {
        log_msg(oasys::LOG_ERR, "process_custody_accepted_v0: error parsing CBOR");
        return;
    }

    EhsBundleRef bref(__func__);
    EhsBundleIterator find_iter;
    bref = all_bundles_.bundle_custody_accepted(bundleid);
    if (bref == nullptr) {
        //log_msg(oasys::LOG_WARN, "bundle_custody_accepted - Bundle ID: %" PRIu64 " --- Not found - request details?", 
        //        bundleid);
    } else {
        bref->process_bundle_custody_accepted_event(custody_id);
        //log_msg(oasys::LOG_DEBUG, 
        //        "bundle_custody_accepted - Bundle ID: %" PRIu64 " dest: %s --- Routing bundle", 
        //        bundleid, bref->dest().c_str());

        custody_bundles_.insert(EhsBundlePair(bref->bundleid(), bref));
        route_bundle(bref);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::process_custody_signal_v0(CborValue& cvElement)
{
    uint64_t bundleid = 0;
    bool success = false;
    uint64_t reason = 0;

    if (CBORUTIL_SUCCESS != decode_custody_signal_msg_v0(cvElement, bundleid, success, reason))
    {
        log_msg(oasys::LOG_ERR, "process_custody_signal_v0: error parsing CBOR");
        return;
    }

    // Release custody and delete if succeeded or duplicate  else error
    if (!success && (BundleProtocol::CUSTODY_REDUNDANT_RECEPTION != reason)) {
        log_msg(oasys::LOG_ERR,
                "process_custody_signal_v0 - Received failure CS -- reason: %" PRIu64, 
                reason);
        return;
    }

        //dz debug
        //log_msg(oasys::LOG_ALWAYS,
        //        "process_custody_signal_v0 - Bundle: %" PRIu64 " succeeded: %s reason: %" PRIu64,
        //        bundleid, success?"true":"false",reason);

    
    EhsBundleRef bref(__func__);
    
    bref = all_bundles_.bundle_custody_released(bundleid);
    if (bref != nullptr) {
        custody_bundles_.erase(bref->bundleid());

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
        //        bundleid);
    }
}


//----------------------------------------------------------------------
void
EhsDtnNode::process_agg_custody_signal_v0(CborValue& cvElement)
{
    log_msg(oasys::LOG_CRIT, "process_agg_custody_signal_v0: should no longer be processed!!");
    

    std::string acs_data;

    if (CBORUTIL_SUCCESS != decode_agg_custody_signal_msg_v0(cvElement, acs_data))
    {
        log_msg(oasys::LOG_ERR, "process_agg_custody_signal_v0: error parsing CBOR");
        return;
    }

    // parse the ACS data to release bundles that have been transferred
    size_t len = acs_data.size();    
    if (len < 3) {
        log_msg(oasys::LOG_ERR,
                "process_aggregate_custody_signal_event - Received ACS with too few bytes: %d",
                len);
        return;
    }

    const u_char* bp = (const u_char*) acs_data.c_str();

    // 1 byte Admin Payload Type + Flags:
    char admin_type = (*bp >> 4);
    //char admin_flags = *bp & 0xf;  -- not used
    bp++;
    len--;

    // validate the admin type
    if (admin_type != BundleProtocolVersion6::ADMIN_AGGREGATE_CUSTODY_SIGNAL) {
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
EhsDtnNode::process_bard_usage_rpt_v0(CborValue& cvElement)
{
    (void) cvElement;

#ifdef BARD_ENABLED
    oasys::ScopeLock l(&lock_, __func__);

    if (!have_bard_usage_reprt_) {
        bard_usage_rpt_usage_stats_.clear();
        bard_usage_rpt_cl_stats_.clear();

        BARDNodeStorageUsageMap bard_usage_map;
        RestageCLMap restagecl_map;

        if (CBORUTIL_SUCCESS != decode_bard_usage_report_msg_v0(cvElement, bard_usage_map, restagecl_map))
        {
            log_msg(oasys::LOG_ERR, "%s: error parsing CBOR", __func__);
        }


        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator usage_iter =  bard_usage_map.begin();
        while (usage_iter != bard_usage_map.end()) {
            sptr_usage = usage_iter->second;

            EhsBARDUsageStats usage_stats;
            usage_stats.quota_type_ = bard_quota_type_to_int(sptr_usage->quota_type());
            usage_stats.naming_scheme_ = bard_naming_scheme_to_int(sptr_usage->naming_scheme());
            usage_stats.node_number_ = sptr_usage->node_number();

            // usage elements
            usage_stats.inuse_internal_bundles_ = sptr_usage->inuse_internal_bundles_;
            usage_stats.inuse_internal_bytes_ = sptr_usage->inuse_internal_bytes_;
            usage_stats.inuse_external_bundles_ = sptr_usage->inuse_external_bundles_;
            usage_stats.inuse_external_bytes_ = sptr_usage->inuse_external_bytes_;

            // reserved elements
            usage_stats.reserved_internal_bundles_ = sptr_usage->reserved_internal_bundles_;
            usage_stats.reserved_internal_bytes_ = sptr_usage->reserved_internal_bytes_;
            usage_stats.reserved_external_bundles_ = sptr_usage->reserved_external_bundles_;
            usage_stats.reserved_external_bytes_ = sptr_usage->reserved_external_bytes_;

            // quota elements
            usage_stats.quota_internal_bundles_ = sptr_usage->quota_internal_bundles();
            usage_stats.quota_internal_bytes_ = sptr_usage->quota_internal_bytes();
            usage_stats.quota_external_bundles_ = sptr_usage->quota_external_bundles();
            usage_stats.quota_external_bytes_ = sptr_usage->quota_external_bytes();
            usage_stats.quota_refuse_bundle_ = sptr_usage->quota_refuse_bundle();
            usage_stats.quota_auto_reload_ = sptr_usage->quota_auto_reload();

            usage_stats.quota_restage_link_name_ = sptr_usage->quota_restage_link_name();

            bard_usage_rpt_usage_stats_.push_back(usage_stats);


            ++usage_iter;
        }


        SPtr_RestageCLStatus sptr_clstatus;
        RestageCLIterator cl_iter = restagecl_map.begin();
        while (cl_iter != restagecl_map.end()) {

            sptr_clstatus = cl_iter->second;

            EhsRestageCLStats cl_stats;

            cl_stats.restage_link_name_ = sptr_clstatus->restage_link_name_;

            cl_stats.storage_path_ = sptr_clstatus->storage_path_;

            cl_stats.validated_mount_pt_ = sptr_clstatus->validated_mount_pt_;

            cl_stats.mount_point_ = sptr_clstatus->mount_point_;
            cl_stats.mount_pt_validated_ = sptr_clstatus->mount_pt_validated_;
            cl_stats.storage_path_exists_ = sptr_clstatus->storage_path_exists_;

            cl_stats.part_of_pool_ = sptr_clstatus->part_of_pool_;

            cl_stats.vol_total_space_ = sptr_clstatus->vol_total_space_;
            cl_stats.vol_space_available_ = sptr_clstatus->vol_space_available_;

            cl_stats.disk_quota_ = sptr_clstatus->disk_quota_;
            cl_stats.disk_quota_in_use_ = sptr_clstatus->disk_quota_in_use_;
            cl_stats.disk_num_files_ = sptr_clstatus->disk_num_files_;

            cl_stats.days_retention_ = sptr_clstatus->days_retention_;
            cl_stats.expire_bundles_ = sptr_clstatus->expire_bundles_;
            cl_stats.ttl_override_ = sptr_clstatus->ttl_override_;
            cl_stats.auto_reload_interval_ = sptr_clstatus->auto_reload_interval_;

            cl_stats.cl_state_ = sptr_clstatus->cl_state_;

            bard_usage_rpt_cl_stats_.push_back(cl_stats);

            ++cl_iter;
        }


        bard_usage_report_requested_ = false;
        have_bard_usage_reprt_ = true;
    }
#endif // BARD_ENABLED
}





//----------------------------------------------------------------------
bool
EhsDtnNode::accept_custody_before_routing(EhsBundleRef& bref)
{
    bool result = false;

    if (bref->custody_requested()) {
        if (!bref->local_custody()) {
            if (accept_custody_.check_pair(bref->ipn_source_node(), bref->ipn_dest_node())) {
                //log_msg(oasys::LOG_DEBUG, "accept_custody_before_routing -issuing request to take custody of bundle: %zu", 
                //        bref->bundleid());

                client_send_take_custody_req_msg(bref->bundleid());

                result = true;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsDtnNode::finalize_bundle_delivered_event(uint64_t id)
{
    EhsBundleRef bref("bundle_delivered_event");
    bref = all_bundles_.bundle_delivered(id);
    if (bref != nullptr) {
        if (bref->local_custody()) {
            EhsBundleRef cust_bref("bundle_delivered_event");
            cust_bref = all_bundles_.bundle_custody_released(id);
            if (cust_bref == nullptr) {
                //log_msg(oasys::LOG_DEBUG, "finalize_bundle_delivered_event - releasing custody of bundle");
            } else {
                //log_msg(oasys::LOG_DEBUG, "finalize_bundle_delivered_event - not releasing custody of bundle??");
            }
        }

        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(id);

        del_bundle_by_dest(bref);

        custody_bundles_.erase(bref->bundleid());

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
        //log_msg(oasys::LOG_DEBUG, 
        //        "bundle ID: %" PRIu64 " -- delivered but unknown",
        //        id);
        delivered_bundle_id_list_.insert(DeliveredBundleIDPair(id, id));
    }
}


//----------------------------------------------------------------------
void 
EhsDtnNode::handle_cbor_received(EhsCborReceivedEvent* event)
{
    //dzdebug
//    log_always("%s - entered with msg of length %zu", __func__, event->msg_->length());

    bool msg_processed = false;

    CborParser parser;
    CborValue cvMessage;
    CborValue cvElement;
    CborError err;

    err = cbor_parser_init((const uint8_t*) event->msg_->c_str(), event->msg_->length(), 0, &parser, &cvMessage);
    if (err != CborNoError) return;

    err = cbor_value_enter_container(&cvMessage, &cvElement);
    if (err != CborNoError) return;

    uint64_t msg_type;
    uint64_t msg_version;
    std::string server_eid;
    int status = decode_server_msg_header(cvElement, msg_type, msg_version, server_eid);
    if (status != CBORUTIL_SUCCESS) return;


    if (!have_link_report_)
    {
        // discard all messages until we have gotten the link report
        // - prevents new incoming bundles being rejected due to an unknown link
        if (msg_type != EXTRTR_MSG_LINK_REPORT) {
            log_msg(oasys::LOG_WARN, "Discarding message because link report has not been received yet");
        } else {
            router_->post_event(new EhsCborReceivedEvent(event->msg_));

            // give router time to process the report
            usleep(500000);
            have_link_report_ = true;
            log_msg(oasys::LOG_ALWAYS, "EhsDtnNode enabling full processing after receiving link report");
        }
        return;
    }




    switch (msg_type) {
        case EXTRTR_MSG_HELLO:
            msg_processed = true;
            process_hello_msg(cvElement, msg_version);
            break;

        case EXTRTR_MSG_ALERT:
            if (msg_version == 0) {
                msg_processed = true;
                process_alert_msg_v0(cvElement);
            }
            break;



        case EXTRTR_MSG_LINK_REPORT:
            msg_processed = true;
            router_->post_event(new EhsCborReceivedEvent(event->msg_));
            break;

        case EXTRTR_MSG_LINK_AVAILABLE:
            msg_processed = true;
            router_->post_event(new EhsCborReceivedEvent(event->msg_));
            break;

        case EXTRTR_MSG_LINK_OPENED:
            msg_processed = true;
            router_->post_event(new EhsCborReceivedEvent(event->msg_));
            break;

        case EXTRTR_MSG_LINK_CLOSED:
            msg_processed = true;
            router_->post_event(new EhsCborReceivedEvent(event->msg_));
            break;

        case EXTRTR_MSG_LINK_UNAVAILABLE:
            msg_processed = true;
            router_->post_event(new EhsCborReceivedEvent(event->msg_));
            break;




        case EXTRTR_MSG_BUNDLE_REPORT:
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_report_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BUNDLE_RECEIVED:
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_received_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BUNDLE_TRANSMITTED:
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_transmitted_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BUNDLE_DELIVERED:
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_delivered_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BUNDLE_EXPIRED:
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_expired_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BUNDLE_CANCELLED:
            //log_always("%s - got Bundle Cancelled msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_bundle_cancelled_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_CUSTODY_TIMEOUT:
            //log_always("%s - got Custody Timeout msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_custody_timeout_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_CUSTODY_ACCEPTED:
            //log_always("%s - got Custody Accepted msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_custody_accepted_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_CUSTODY_SIGNAL:
            //log_always("%s - got Custody Signal msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_custody_signal_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_AGG_CUSTODY_SIGNAL:
            //log_always("%s - got Aggregate Custody Signal msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_agg_custody_signal_v0(cvElement);
            }
            break;

        case EXTRTR_MSG_BARD_USAGE_REPORT:
            //log_always("%s - got BARD Usage Report msg", __func__);
            if (msg_version == 0) {
                msg_processed = true;
                process_bard_usage_rpt_v0(cvElement);
            }
            break;

        default: break;
    }


    if (msg_processed) {
//        log_always("EhsDtnNode Msg Type; %" PRIu64 " (%s) - processed",
//                msg_type, msg_type_to_str(msg_type));

    } else {
        log_err("EhsDtnNode Msg Type; %" PRIu64 " (%s) - unsupported type or version: %" PRIu64,
                msg_type, msg_type_to_str(msg_type), msg_version);

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
    event->bundle_ = nullptr;
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
    char threadname[16] = "EhsDtnNode";
    pthread_setname_np(pthread_self(), threadname);

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

    log_msg(oasys::LOG_DEBUG, 
            "DTN node discovered: %s [%s] - request list of links, bundles, etc.", 
            eid_.c_str(), eid_ipn_.c_str());


    client_send_link_query_msg();
    client_send_bundle_query_msg();


    EhsEvent* event = nullptr;

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
EhsDtnNode::send_msg(std::string* msg)
{
    parent_->send_msg(msg);
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
    return ((0 == check_eid.compare(eid_ipn_)) || (0 == check_eid.compare(eid_ipn_ping_))
            || (0 == check_eid.compare(eid_ipn_dtpc_)) || (0 == check_eid.compare(eid_)) 
            || (0 == check_eid.compare(eid_ping_)));
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
    oasys::ScopeLock l(&lock_, __func__);

    all_bundles_.bundle_stats_by_src_dst(count, stats);
}

//----------------------------------------------------------------------
void
EhsDtnNode::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    oasys::ScopeLock l(&lock_, __func__);

    all_bundles_.fwdlink_interval_stats(count, stats);
}

//----------------------------------------------------------------------
void
EhsDtnNode::request_bard_usage_stats()
{
    oasys::ScopeLock l(&lock_, __func__);

    if (!have_bard_usage_reprt_ && !bard_usage_report_requested_) {
        bard_usage_report_requested_ = true;

        client_send_bard_usage_req_msg();
    }
}


//----------------------------------------------------------------------
bool
EhsDtnNode::bard_usage_stats(EhsBARDUsageStatsVector& usage_stats, 
                            EhsRestageCLStatsVector& cl_stats)
{
    oasys::ScopeLock l(&lock_, __func__);

    bool result = have_bard_usage_reprt_;

    if (have_bard_usage_reprt_) {
        // TODO: add a time received...

        usage_stats = bard_usage_rpt_usage_stats_;
        cl_stats = bard_usage_rpt_cl_stats_;

        // clear the local copies
        bard_usage_rpt_usage_stats_.clear();
        bard_usage_rpt_cl_stats_.clear();

        have_bard_usage_reprt_ = false;
    }

    return result;
}


//----------------------------------------------------------------------
void
EhsDtnNode::bard_add_quota(EhsBARDUsageStats& quota)
{
    (void) quota;
    oasys::ScopeLock l(&lock_, __func__);

#ifdef BARD_ENABLED    
    BARDNodeStorageUsage bard_quota;

    bard_quota.set_quota_type(int_to_bard_quota_type(quota.quota_type_));
    bard_quota.set_naming_scheme(int_to_bard_naming_scheme(quota.naming_scheme_));
    bard_quota.set_node_number(quota.node_number_);

    bard_quota.set_quota_internal_bundles(quota.quota_internal_bundles_);
    bard_quota.set_quota_internal_bytes(quota.quota_internal_bytes_);
    bard_quota.set_quota_external_bundles(quota.quota_external_bundles_);
    bard_quota.set_quota_external_bytes(quota.quota_external_bytes_);

    bard_quota.set_quota_restage_link_name(quota.quota_restage_link_name_);
    bard_quota.set_quota_auto_reload(quota.quota_auto_reload_);

    client_send_bard_add_quota_req_msg(bard_quota);
#endif // BARD_ENABLED
}

//----------------------------------------------------------------------
void
EhsDtnNode::bard_del_quota(EhsBARDUsageStats& quota)
{
    (void) quota;
    oasys::ScopeLock l(&lock_, __func__);

#ifdef BARD_ENABLED    
    BARDNodeStorageUsage bard_quota;

    bard_quota.set_quota_type(int_to_bard_quota_type(quota.quota_type_));
    bard_quota.set_naming_scheme(int_to_bard_naming_scheme(quota.naming_scheme_));
    bard_quota.set_node_number(quota.node_number_);

    client_send_bard_del_quota_req_msg(bard_quota);
#endif // BARD_ENABLED
}

//----------------------------------------------------------------------
void
EhsDtnNode::send_link_add_msg(std::string& link_id, std::string& next_hop, std::string& link_mode,
                              std::string& cl_name,  LinkParametersVector& params)
{
    oasys::ScopeLock l(&lock_, __func__);

    client_send_link_add_req_msg(link_id, next_hop, link_mode, cl_name, params);
}


//----------------------------------------------------------------------
void
EhsDtnNode::send_link_del_msg(std::string& link_id)
{
    oasys::ScopeLock l(&lock_, __func__);

    client_send_link_del_req_msg(link_id);
}




//----------------------------------------------------------------------
void
EhsDtnNode::unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf)
{
    router_->unrouted_bundle_stats_by_src_dst(buf);
}

//----------------------------------------------------------------------
void 
EhsDtnNode::update_statistics(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                              uint64_t& delivered, uint64_t& rejected,
                              uint64_t& pending, uint64_t& custody,
                              bool& fwdlnk_aos, bool& fwdlnk_enabled)
{
    all_bundles_.get_bundle_stats(received, transmitted, transmit_failed, delivered, rejected, pending, custody);
    fwdlnk_aos = fwdlnk_aos_;
    fwdlnk_enabled = fwdlnk_enabled_;
}

//----------------------------------------------------------------------
void 
EhsDtnNode::update_statistics2(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                               uint64_t& delivered, uint64_t& rejected,
                               uint64_t& pending, uint64_t& custody, uint64_t& expired,
                               bool& fwdlnk_aos, bool& fwdlnk_enabled)
{
    all_bundles_.get_bundle_stats2(received, transmitted, transmit_failed, delivered, rejected, pending, custody, expired);
    fwdlnk_aos = fwdlnk_aos_;
    fwdlnk_enabled = fwdlnk_enabled_;
}

//----------------------------------------------------------------------
void 
EhsDtnNode::update_statistics3(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                               uint64_t& delivered, uint64_t& rejected,
                               uint64_t& pending, uint64_t& custody, uint64_t& expired,
                               bool& fwdlnk_aos, bool& fwdlnk_enabled, 
                               uint64_t& links_open, uint64_t& num_links)
{
    all_bundles_.get_bundle_stats2(received, transmitted, transmit_failed, delivered, rejected, pending, custody, expired);
    fwdlnk_aos = fwdlnk_aos_;
    fwdlnk_enabled = fwdlnk_enabled_;
    router_->get_num_links(links_open, num_links);
}

//----------------------------------------------------------------------
void
EhsDtnNode::link_dump(oasys::StringBuffer* buf)
{
    if (nullptr != router_) {
        router_->link_dump(buf);
    } else {
        buf->append("Link dump: Router not instantiated\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::fwdlink_transmit_dump(oasys::StringBuffer* buf)
{
    if (nullptr != router_) {
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
    if (nullptr != router_) {
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
    if (nullptr != router_) {
        router_->fwdlink_transmit_enable(src_list, dst_list);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::fwdlink_transmit_disable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list)
{
    if (nullptr != router_) {
        router_->fwdlink_transmit_disable(src_list, dst_list);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::set_fwdlnk_force_LOS_while_disabled(bool force_los)
{
    if (nullptr != router_) {
        router_->set_fwdlnk_force_LOS_while_disabled(force_los);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::link_enable(EhsLinkCfg* cl)
{
    if (nullptr != router_) {
        router_->link_enable(cl);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::link_disable(std::string& linkid)
{
    if (nullptr != router_) {
        router_->link_disable(linkid);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::set_link_statistics(bool enabled)
{
    if (nullptr != router_) {
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
    if (nullptr != router_) {
        router_->reconfigure_source_priority();
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::reconfigure_dest_priority()
{
    if (nullptr != router_) {
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

    if (nullptr != router_) {
        router_->set_fwdlnk_enabled_state(fwdlnk_enabled_);
    }
}

//----------------------------------------------------------------------
void 
EhsDtnNode::set_fwdlnk_aos_state(bool state)
{
    fwdlnk_aos_ = state;

    if (nullptr != router_) {
        router_->set_fwdlnk_aos_state(fwdlnk_aos_);
    }
}

//----------------------------------------------------------------------
void
EhsDtnNode::set_fwdlnk_throttle(uint32_t bps)
{
    if (nullptr != router_) {
        router_->set_fwdlnk_throttle(bps);
    }
}

//----------------------------------------------------------------------
int
EhsDtnNode::bundle_delete(uint64_t source_node_id, uint64_t dest_node_id)
{
    oasys::ScopeLock l(&lock_, __func__);

    EhsBundleMap bidlist;
    all_bundles_.bundle_id_list(bidlist, source_node_id, dest_node_id);

    extrtr_bundle_id_ptr_t bidptr;
    extrtr_bundle_id_vector_t bid_vec;

    int cnt = 0;

    EhsBundleRef bref("bundle_delete");
    EhsBundleIterator iter = bidlist.begin();
    while (iter != bidlist.end()) {
        bref = iter->second;
        ++cnt;

        bidptr = std::make_shared<extrtr_bundle_id_t>();
        bidptr->bundleid_ = bref->bundleid();


        // flag bundle as deleted
        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(bref->bundleid());

        del_bundle_by_dest(bref);

        custody_bundles_.erase(bref->bundleid());

        log_msg(oasys::LOG_DEBUG, 
                "bundle_delete (by src-dst) - deleting Bundle ID: %" PRIu64,
                bref->bundleid());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        bidlist.erase(iter);

        bid_vec.push_back(bidptr);
        if (bid_vec.size() >= 100000 )  {
            client_send_delete_bundle_req_msg(bid_vec);

            bid_vec.clear();
        }

        iter = bidlist.begin();
    }

    if (!bid_vec.empty()) {
        client_send_delete_bundle_req_msg(bid_vec);
        bid_vec.clear();
    }

    return cnt;
}

//----------------------------------------------------------------------
int
EhsDtnNode::bundle_delete_all()
{
    oasys::ScopeLock l(&lock_, __func__);

    client_send_delete_all_bundles_req_msg();

    return all_bundles_.size();
}

//----------------------------------------------------------------------
int
EhsDtnNode::bundle_delete_all_orig()
{
    oasys::ScopeLock l(&lock_, __func__);

    extrtr_bundle_id_ptr_t bidptr;
    extrtr_bundle_id_vector_t bid_vec;

    EhsBundleMap bidlist;
    all_bundles_.bundle_id_list(bidlist);

    int cnt = 0;
    EhsBundleRef bref("bundle_delete");
    EhsBundleIterator iter = bidlist.begin();
    while (iter != bidlist.end()) {
        bref = iter->second;
        ++cnt;

        bidptr = std::make_shared<extrtr_bundle_id_t>();
        bidptr->bundleid_ = bref->bundleid();

        // flag bundle as deleted
        bref->set_deleted();

        // delete the bundle from our lists
        all_bundles_.erase_bundle(bref);
        undelivered_bundles_.erase(bref->bundleid());

        del_bundle_by_dest(bref);

        custody_bundles_.erase(bref->bundleid());

        log_msg(oasys::LOG_DEBUG, 
                "bundle_delete (all) - deleting Bundle ID: %" PRIu64,
                bref->bundleid());

        // Inform the router in case it also has a copy
        if (bref->refcount() > 1) {
            router_->post_event(new EhsDeleteBundleReq(bref));
        }

        bidlist.erase(iter);

        bid_vec.push_back(bidptr);
        if (bid_vec.size() >= 100000 )  {
            client_send_delete_bundle_req_msg(bid_vec);

            bid_vec.clear();
        }

        iter = bidlist.begin();
    }

    if (!bid_vec.empty()) {
        client_send_delete_bundle_req_msg(bid_vec);
        bid_vec.clear();
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

