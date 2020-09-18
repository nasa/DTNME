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

#include "EhsDtnNode.h"
#include "EhsRouter.h"


namespace dtn {



//----------------------------------------------------------------------
EhsRouter::EhsRouter(EhsDtnNode* parent)
    : EhsEventHandler("EhsRouter", "/ehs/router", ""),
      Thread(logpath(), oasys::Thread::DELETE_ON_EXIT),
      parent_(parent),
      prime_mode_(true),
      log_level_(1),
      fwdlnk_enabled_(false),
      fwdlnk_aos_(false),
      fwdlnk_force_LOS_while_disabled_(true),
      ehslink_fwd_(NULL),
      have_link_report_(false)
{
    eventq_ = new oasys::MsgQueue<EhsEvent*>(logpath_);
    eventq_->notify_when_empty();
}

//----------------------------------------------------------------------
EhsRouter::~EhsRouter()
{
    EhsLinkIterator liter = all_links_.begin();

    while (liter != all_links_.end()) {
        EhsLink* el = liter->second;
        el->set_should_stop();
        all_links_.erase(liter);

        liter = all_links_.begin();
    }

    unrouted_bundles_.clear();

    sleep(1);

    delete eventq_;
}

//----------------------------------------------------------------------
void
EhsRouter::post_event(EhsEvent* event, bool at_back)
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
EhsRouter::set_prime_mode(bool prime_mode)
{
    // Need a lock?
    prime_mode_ = prime_mode;
}

//----------------------------------------------------------------------
void 
EhsRouter::reconfigure_link(std::string link_id)
{
    EhsLinkIterator find_iter;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    find_iter = all_links_.find(link_id);

    if (find_iter != all_links_.end()) {
        EhsLink* el = find_iter->second;

        el->post_event(new EhsReconfigureLinkReq(), false); // post at head
    } else {
        // not found by link_id --
        // this may be an IP address for a ground node
        find_iter = all_links_.begin();

        while (find_iter != all_links_.end()) {
            EhsLink* el = find_iter->second;
            if (el->remote_addr().compare(link_id) == 0) {
                el->post_event(new EhsReconfigureLinkReq(), false); // post at head
            }
            ++find_iter;
        }
    }

}

//----------------------------------------------------------------------
bool
EhsRouter::get_link_configuration(EhsLink* el)
{
    return parent_->get_link_configuration(el);
}

//----------------------------------------------------------------------
void 
EhsRouter::link_enable(EhsLinkCfg* cl)
{
    (void) cl;
    // Nothing to do here! -- delete?
}

//----------------------------------------------------------------------
void 
EhsRouter::link_disable(std::string& linkid)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator del_iter;
    EhsLinkIterator iter;
    iter = all_links_.begin();
    while (iter != all_links_.end()) {
        EhsLink* el = iter->second;
        if (linkid.compare(el->link_id()) == 0 ||
            linkid.compare(el->remote_addr()) == 0) {

            log_msg(oasys::LOG_WARN, "link_disable: closing link: %s [%s]", 
                    el->link_id().c_str(), el->remote_addr().c_str());

            el->force_closed();
            el->set_is_rejected(true);

            rtrmessage::close_link_request req;
            req.link_id(el->link_id());

            rtrmessage::bpa message; 
            message.close_link_request(req);
            send_msg(message);

            el->return_all_bundles(unrouted_bundles_);

            del_iter = iter;
            ++iter;

            el->set_should_stop();
            all_links_.erase(del_iter);
        } else {
            ++iter;
        }
    }
    
}

//----------------------------------------------------------------------
void 
EhsRouter::fwdlink_transmit_enable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // update the list of enabled src/dest combos
    EhsNodeBoolIterator src_iter = src_list.begin();
    EhsNodeBoolIterator dst_iter;
    while (src_iter != src_list.end()) {
        dst_iter = dst_list.begin();
        while (dst_iter != dst_list.end()) {
            fwdlink_xmt_enabled_.put_pair(src_iter->first, dst_iter->first, true);

            ++dst_iter;
        }
        ++src_iter;
    }


    // hand off any newly enabled bundles to the link
    EhsLink* el;
    EhsLinkIterator iter = all_links_.begin();
    while (iter != all_links_.end()) {
        el = iter->second;
        if (el->is_fwdlnk()) {
            uint64_t num_routed = unrouted_bundles_.route_to_link(el, fwdlink_xmt_enabled_);

            log_msg(oasys::LOG_INFO, "fwdlink_transmit_enable: routed %" PRIu64 " bundles to %s [%s]", 
                    num_routed, el->link_id().c_str(), el->remote_addr().c_str());
        }
        ++iter;
    }
}


//----------------------------------------------------------------------
void 
EhsRouter::fwdlink_transmit_disable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // update the list of enabled src/dest combos
    EhsNodeBoolIterator src_iter = src_list.begin();
    EhsNodeBoolIterator dst_iter;
    while (src_iter != src_list.end()) {
        dst_iter = dst_list.begin();
        while (dst_iter != dst_list.end()) {
            fwdlink_xmt_enabled_.clear_pair(src_iter->first, dst_iter->first);
            ++dst_iter;
        }
        ++src_iter;
    }

    // retrieve newly disabled bundles from the links
    EhsLink* el;
    EhsLinkIterator iter = all_links_.begin();
    while (iter != all_links_.end()) {
        el = iter->second;
        if (el->is_fwdlnk()) {
            uint64_t num_returned = el->return_disabled_bundles(unrouted_bundles_, fwdlink_xmt_enabled_);

            log_msg(oasys::LOG_INFO, "fwdlink_transmit_disable: returned %" PRIu64 " bundles to unrouted list from %s", 
                    num_returned, el->link_id().c_str());
        }
        ++iter;
    }
}


//----------------------------------------------------------------------
void
EhsRouter::set_fwdlnk_enabled_state(bool state)
{
    fwdlnk_enabled_ = state;

    // inform the links of the new fwd link state
    EhsLinkIterator liter = all_links_.begin();

    while (liter != all_links_.end()) {
        EhsLink* el = liter->second;
        el->set_fwdlnk_enabled_state(fwdlnk_enabled_);
        ++liter;
    }
}

//----------------------------------------------------------------------
void
EhsRouter::set_fwdlnk_aos_state(bool state)
{
    fwdlnk_aos_ = state;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // inform the links of the new fwd link state
    EhsLinkIterator liter = all_links_.begin();

    while (liter != all_links_.end()) {
        EhsLink* el = liter->second;
        el->set_fwdlnk_aos_state(fwdlnk_aos_);
        ++liter;
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::set_fwdlnk_force_LOS_while_disabled(bool force_los)
{
    fwdlnk_force_LOS_while_disabled_ = force_los;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // inform the links of the new fwd link state
    EhsLinkIterator liter = all_links_.begin();

    while (liter != all_links_.end()) {
        EhsLink* el = liter->second;
        el->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
        ++liter;
    }
}

//----------------------------------------------------------------------
void
EhsRouter::set_fwdlnk_throttle(uint32_t bps)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // inform the fwd linki(s) of the new throttle
    EhsLinkIterator liter = all_links_.begin();

    while (liter != all_links_.end()) {
        EhsLink* el = liter->second;
        if (el->is_fwdlnk()) {
            el->set_throttle_bps(bps);
        }
        ++liter;
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::reconfigure_source_priority()
{
    //XXX/dz TODO
}

//----------------------------------------------------------------------
void 
EhsRouter::reconfigure_dest_priority()
{
    //XXX/dz TODO
}

bool
EhsRouter::is_fwd_link_destination(uint64_t dest_node)
{
    //XXX/dz allow multiple forward links for future LTPUDP CL capablity

//    if (NULL == ehslink_fwd_) {
        oasys::ScopeLock l(&lock_, __FUNCTION__);
        EhsLinkIterator liter = all_links_.begin();

        ehslink_fwd_ = NULL;

        while (liter != all_links_.end()) {
            EhsLink* el = liter->second;
            if (el->is_fwdlnk()) {
                ehslink_fwd_ = el;
                break;
            }
            ++liter;
        }
//    }

    if (NULL != ehslink_fwd_)
        return ehslink_fwd_->is_node_reachable(dest_node);
    else
        return false;
}

//----------------------------------------------------------------------
void
EhsRouter::process_link_report(rtrmessage::bpa* msg)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    rtrmessage::link_report::link::container& c = msg->link_report().get().link();

    EhsLinkIterator find_iter;
    rtrmessage::link_report::link::container::iterator iter = c.begin();
    while (iter != c.end()) {
        rtrmessage::link_report::link::type xlink_copy = *iter;
        rtrmessage::link_report::link::type& xlink (xlink_copy);;

        std::string link_id = xlink.link_id();

        find_iter = all_links_.find(link_id);

        if (find_iter == all_links_.end()) {
            EhsLink* el = new EhsLink(xlink, this, fwdlnk_aos_);

            if (!parent_->get_link_configuration(el)) {
                log_msg(oasys::LOG_WARN, "process_link_report - Unconfigured Link: %s [%s]  initial state: %s - forcing closure", 
                        link_id.c_str(), el->remote_addr().c_str(), el->state().c_str());

                el->force_closed();
                el->set_is_rejected(true);

                rtrmessage::close_link_request req;
                req.link_id(link_id);

                rtrmessage::bpa message; 
                message.close_link_request(req);
                send_msg(message);
            }

            all_links_.insert(EhsLinkPair(link_id, el));
    
            el->set_log_level(log_level_);
            el->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
            el->set_fwdlnk_enabled_state(fwdlnk_enabled_);
            el->set_fwdlnk_aos_state(fwdlnk_aos_);
//dz - add this?            el->set_throttle_bps(bps);
            el->start();

            log_msg(oasys::LOG_ALWAYS, "process_link_report - Added Link: %s [%s]  initial state: %s", 
                     link_id.c_str(), el->remote_addr().c_str(), el->state().c_str());

            if (el->is_open()) {
                uint64_t num_routed = unrouted_bundles_.route_to_link(el, fwdlink_xmt_enabled_);

                log_msg(oasys::LOG_INFO, "process_link_report: routed %" PRIu64 " bundles to %s [%s]", 
                        num_routed, el->link_id().c_str(), el->remote_addr().c_str());
            }

        } else {
            log_msg(oasys::LOG_INFO, "process_link_report - found Link: %s - ignoring", 
                     link_id.c_str());
        }

        ++iter;
    }

    have_link_report_ = true;

    log_msg(oasys::LOG_ALWAYS, "EhsRouter processed link report - full processing now enabled");
}

//----------------------------------------------------------------------
void
EhsRouter::process_link_available_event(rtrmessage::bpa* msg)
{
    rtrmessage::link_available_event& event = msg->link_available_event().get();
    std::string link_id = event.link_id();

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator find_iter = all_links_.find(link_id);

    if (find_iter == all_links_.end()) {
        log_msg(oasys::LOG_CRIT, "Unknown Link is now Available: %s", link_id.c_str());
    } else {
        EhsLink* el = find_iter->second;
        el->process_link_available_event(event);
    }
}

//----------------------------------------------------------------------
void
EhsRouter::process_link_opened_event(rtrmessage::bpa* msg)
{
    rtrmessage::link_opened_event& event = msg->link_opened_event().get();
    std::string link_id = event.link_id();

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator find_iter = all_links_.find(link_id);

    EhsLink* el = NULL;
    if (find_iter == all_links_.end()) {
        rtrmessage::link_opened_event xlink_copy = msg->link_opened_event().get();
        rtrmessage::link_opened_event& xlink(xlink_copy);
        el = new EhsLink(xlink, this, fwdlnk_aos_);

        if (!parent_->get_link_configuration(el)) {
            el->force_closed();
            el->set_is_rejected(true);

            rtrmessage::close_link_request req;
            req.link_id(link_id);

            rtrmessage::bpa message; 
            message.close_link_request(req);
            send_msg(message);
        }

        all_links_.insert(EhsLinkPair(link_id, el));

        el->set_log_level(log_level_);
        el->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
        el->set_fwdlnk_enabled_state(fwdlnk_enabled_);
        el->set_fwdlnk_aos_state(fwdlnk_aos_);
        el->start();

    } else {
        el = find_iter->second;
        el->process_link_opened_event(event);
        el->set_is_rejected(false);

        if (!parent_->get_link_configuration(el)) {
            el->force_closed();
            el->set_is_rejected(true);

            rtrmessage::close_link_request req;
            req.link_id(link_id);

            rtrmessage::bpa message; 
            message.close_link_request(req);
            send_msg(message);
        }
    }

    if (el->is_rejected()) {
        log_msg(oasys::LOG_CRIT, "process_link_opened_event: Rejecting Link %s from %s", 
                link_id.c_str(), el->remote_addr().c_str());
    } else {
        log_msg(oasys::LOG_ALWAYS, "process_link_opened_event: Accepted Link (%s) from %s", 
                link_id.c_str(), el->remote_addr().c_str());
    }

    // route bundles to this link...
    uint64_t num_routed = unrouted_bundles_.route_to_link(el, fwdlink_xmt_enabled_);

    log_msg(oasys::LOG_DEBUG, "process_link_opened_event: routed %" PRIu64 " bundles to %s [%s]", 
            num_routed, el->link_id().c_str(), el->remote_addr().c_str());
}

//----------------------------------------------------------------------
uint64_t
EhsRouter::check_for_missed_bundles(EhsLink* link)
{
    // route bundles to this link...
    return unrouted_bundles_.route_to_link(link, fwdlink_xmt_enabled_);
}

//----------------------------------------------------------------------
uint64_t
EhsRouter::check_dtn_node_for_missed_bundles(uint64_t ipn_node_id)
{
    // pass through to the DtnNode
    return parent_->check_for_missed_bundles(ipn_node_id);
}

//----------------------------------------------------------------------
void
EhsRouter::process_link_unavailable_event(rtrmessage::bpa* msg)
{
    rtrmessage::link_unavailable_event& event = msg->link_unavailable_event().get();
    std::string link_id = event.link_id();

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator find_iter = all_links_.find(link_id);

    if (find_iter == all_links_.end()) {
        log_msg(oasys::LOG_INFO, "Unknown Link is now Unavailable: %s", link_id.c_str());
    } else {
        EhsLink* el = find_iter->second;
        el->process_link_unavailable_event(event);
    }
}

//----------------------------------------------------------------------
void
EhsRouter::process_link_busy_event(rtrmessage::bpa* msg)
{
    rtrmessage::link_busy_event& event = msg->link_busy_event().get();
    std::string link_id = event.link().link_id();

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator find_iter = all_links_.find(link_id);

    if (find_iter == all_links_.end()) {
        log_msg(oasys::LOG_INFO, "Unknown Link is now Busy: %s", link_id.c_str());
    } else {
        EhsLink* el = find_iter->second;
        el->process_link_busy_event(event);
    }
}


//----------------------------------------------------------------------
void
EhsRouter::process_link_closed_event(rtrmessage::bpa* msg)
{
    rtrmessage::link_closed_event& event = msg->link_closed_event().get();
    std::string link_id = event.link_id();

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator find_iter = all_links_.find(link_id);

    if (find_iter == all_links_.end()) {
        log_msg(oasys::LOG_INFO, "Unknown Link is now Closed: %s", link_id.c_str());
    } else {
        log_msg(oasys::LOG_ALWAYS, "Link is now Closed: %s", link_id.c_str());

        EhsLink* el = find_iter->second;
        el->process_link_closed_event(event);

        el->set_should_stop();
        all_links_.erase(find_iter);
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::handle_bpa_received(EhsBpaReceivedEvent* event)
{
//dzdebug    rtrmessage::bpa* in_bpa = event->bpa_ptr_.get();
    rtrmessage::bpa in_bpa_copy = *event->bpa_ptr_.get();
    rtrmessage::bpa* in_bpa = &in_bpa_copy;

    if (in_bpa->link_report().present()) {
        process_link_report(in_bpa);

    } else if (in_bpa->link_available_event().present()) {
        //log_msg(oasys::LOG_ALWAYS, "received msg type: link_available_event");
        process_link_available_event(in_bpa);

    } else if (in_bpa->link_opened_event().present()) {
        //log_msg(oasys::LOG_ALWAYS, "received msg type: link_opened_event");
        process_link_opened_event(in_bpa);

    } else if (in_bpa->link_unavailable_event().present()) {
        //log_msg(oasys::LOG_ALWAYS, "received msg type: link_unavailable_event");
        process_link_unavailable_event(in_bpa);

    } else if (in_bpa->link_closed_event().present()) {
        //log_msg(oasys::LOG_ALWAYS, "received msg type: link_closed_event");
        process_link_closed_event(in_bpa);

    } else if (in_bpa->link_busy_event().present()) {
        //log_msg(oasys::LOG_DEBUG, "received msg type: link_busy_event");
        process_link_busy_event(in_bpa);

    } else {
        //??  log_msg(oasys::LOG_ERR, "Unhandled message: %s", in_bpa->type());
        log_msg(oasys::LOG_ERR, "Unhandled message: ??");
    }
}

//----------------------------------------------------------------------
void
EhsRouter::handle_bundle_transmitted_event(EhsBundleTransmittedEvent* event)
{
    EhsLinkIterator find_iter = all_links_.find(event->link_id_);

    if (find_iter == all_links_.end()) {
        log_msg(oasys::LOG_INFO, "Bundle transmitted on unknown Link: %s", event->link_id_.c_str());
    } else {
        EhsLink* el = find_iter->second;
        el->bundle_transmitted();
    }
}



//----------------------------------------------------------------------
bool
EhsRouter::accept_bundle(EhsBundleRef& bref, std::string& link_id, 
                         std::string& remote_addr)
{
    bool result = true;
    remote_addr = link_id; // default if unable to determine remote IP address


    if (!have_link_report_) {
        // this is mainly for debug - this should no longer happen!!
        log_msg(oasys::LOG_CRIT, "EhsRouter received accept_bundle query before link report was processed - accepting it for now:\n" 
                                 "ID: %" PRIu64 " Src Node: %" PRIu64 " Dest Node: %" PRIu64 " from Link: %s - Result: %s",
                bref->bundleid(), bref->ipn_source_node(), bref->ipn_dest_node(), link_id.c_str(), result?"true":"false");
        return result;
    }

    // Empty Link ID means the bundle was generated local to the DTN server
    if (parent_->is_local_admin_eid(bref->dest())) {
        // accept custody signal or ping to admin EID
    } else if (link_id.compare("") != 0) {
        lock_.lock(__FUNCTION__);
        EhsLinkIterator find_iter = all_links_.find(link_id);
        lock_.unlock();

        if (find_iter == all_links_.end()) {
            result = false;
            log_msg(oasys::LOG_WARN, "accept_bundle: Unknown Link is now Closed?: %s - Result: false", link_id.c_str());
        } else {
            EhsLink* el = find_iter->second;
            if (!el->is_fwdlnk()) {
                remote_addr = el->remote_addr();
                result = el->valid_source_node(bref->ipn_source_node()) &&
                         el->valid_dest_node(bref->ipn_dest_node());
                log_msg(oasys::LOG_DEBUG, "accept_bundle: ID: %" PRIu64 " Src Node: %" PRIu64 " Dest Node: %" PRIu64 " from Link: %s - Result: %s",
                        bref->bundleid(), bref->ipn_source_node(), bref->ipn_dest_node(), link_id.c_str(), result?"true":"false");
            } else {
                log_msg(oasys::LOG_DEBUG, "accept_bundle: ID: %" PRIu64 " Src Node: %" PRIu64 " Dest Node: %" PRIu64 " from Link: %s - Result: %s (forward link)",
                        bref->bundleid(), bref->ipn_source_node(), bref->ipn_dest_node(), link_id.c_str(), result?"true":"false");
            }
        }
    } else {
        log_msg(oasys::LOG_DEBUG, "accept_bundle: ID: %" PRIu64 " Src Node: %" PRIu64 " Dest Node: %" PRIu64 " (locally generated or from an interface)",
                bref->bundleid(), bref->ipn_source_node(), bref->ipn_dest_node());
    }
    return result;
}

//----------------------------------------------------------------------
uint64_t
EhsRouter::return_all_bundles_to_router(EhsBundlePriorityTree& tree)
{
    return tree.return_all_bundles(unrouted_bundles_);
}

//----------------------------------------------------------------------
void 
EhsRouter::handle_route_bundle_req(EhsRouteBundleReq* event)
{
    log_msg(oasys::LOG_DEBUG, "handle_route_bundle_req...");

    // Route it if prime
    if (prime_mode_) {
        oasys::ScopeLock l(&lock_, __FUNCTION__);

        // find the Link for the next hop
        EhsLinkIterator liter = all_links_.begin();

        uint64_t dest_node = event->bref_->ipn_dest_node();
        //log_msg(oasys::LOG_DEBUG, 
        //        "handle_route_bundle_req - find link.... Dest: %" PRIu64 "  BundleID: %" PRIu64, 
        //        dest_node, event->bref_->bundleid());

        bool is_ecos_critical = event->bref_->is_ecos_critical();

        bool is_fwdlink = false;
        bool routed = false;
        while (liter != all_links_.end()) {
            EhsLink* el = liter->second;

            //XXX/dz TODO - add additional check using previous hop
            if (is_ecos_critical && (0 == el->link_id().compare(event->bref_->received_from_link_id()))) {
                // don't send critical bundle on the link it arrived on
                log_msg(oasys::LOG_DEBUG, 
                        "handle_route_bundle_req - not sending ECOS Critical Bundle on receiving link (%s):"
                        " BundleID: %" PRIu64 " Src: %" PRIu64 " Dest: %" PRIu64, 
                        el->link_id().c_str(), event->bref_->bundleid(), 
                        event->bref_->ipn_source_node(), dest_node);
            } else if (is_ecos_critical || el->is_node_reachable(dest_node)) {
                if (el->is_fwdlnk()) {
                    is_fwdlink = true;
                    // bundles with local source are always enabled for transmit (custody signals and such)
                    if (!parent_->is_local_node(event->bref_->ipn_source_node()) &&
                        !fwdlink_xmt_enabled_.check_pair(event->bref_->ipn_source_node(), dest_node)) {
                        // destined for onboard but user not enabled so can't send it yet
                        if (!is_ecos_critical) break;  
                    } else {
                        routed = true;
                        el->post_event(new EhsRouteBundleReq(event->bref_));

                        log_msg(oasys::LOG_DEBUG, 
                                "handle_route_bundle_req - forward link and user enabled - post to send (%s):"
                                " BundleID: %" PRIu64 " Src: %" PRIu64 " Dest: %" PRIu64, 
                                el->link_id().c_str(), event->bref_->bundleid(), 
                                event->bref_->ipn_source_node(), dest_node);
                    }
                } else {
                    routed = true;
                    el->post_event(new EhsRouteBundleReq(event->bref_));

                    //log_msg(oasys::LOG_DEBUG, 
                    //        "handle_route_bundle_req - found link - post to send (%s)..."
                    //        " BundleID: %" PRIu64 " Dest: %" PRIu64, 
                    //        el->link_id().c_str(), event->bref_->bundleid(), dest_node);
                }

                // if not critical break out of loop after finding a route
                if (!is_ecos_critical) break;  
            }

            ++liter;
        }

        if (!routed) {
            if (is_fwdlink) {
                //log_msg(oasys::LOG_DEBUG, 
                //        "handle_route_bundle_req - Deferring because FwdLink transmit not enabled:"
                //        " BundleID: %" PRIu64 "  Src: %" PRIu64 " Dest %" PRIu64, 
                //        event->bref_->bundleid(), event->bref_->ipn_source_node(), dest_node);
            } else {
                //log_msg(oasys::LOG_DEBUG, 
                //        "handle_route_bundle_req - Deferring because open link not found:"
                //        "  BundleID: %" PRIu64 " Src: %" PRIu64 " Dest: %" PRIu64, 
                //        event->bref_->bundleid(), event->bref_->ipn_source_node(), dest_node);
            }

            unrouted_bundles_.insert_bundle(event->bref_);
        }

    } else {
        log_msg(oasys::LOG_DEBUG, 
                "handle_route_bundle_req - Deferring because not Prime:"
                " BundleID: %" PRIu64 " Src: %" PRIu64 " Dest: %" PRIu64, 
                event->bref_->bundleid(), event->bref_->ipn_source_node(), event->bref_->ipn_dest_node());

        unrouted_bundles_.insert_bundle(event->bref_);
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::handle_delete_bundle_req(EhsDeleteBundleReq* event)
{
    (void) event;

    //XXX/dz TODO: check each Link???

//    EhsBundleIterator find_iter;
//    find_iter = unrouted_bundles_.find(event->bref_->bundleid());
//    if (find_iter != unrouted_bundles_.end()) {
//        event->bref_->set_deleted();
//
//        unrouted_bundles_.erase(find_iter);
//
//        log_msg(oasys::LOG_DEBUG, "delete_bundle_req - ID = %lu", event->bref_->bundleid());
//    }
}

//----------------------------------------------------------------------
void
EhsRouter::handle_event(EhsEvent* event)
{
    dispatch_event(event);
    
    event_handlers_completed(event);

//    stats_.events_processed_++;
}

//----------------------------------------------------------------------
void
EhsRouter::event_handlers_completed(EhsEvent* event)
{
    (void) event;
}


//----------------------------------------------------------------------
void
EhsRouter::run()
{
    EhsEvent* event;

    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    // get the initial next hop configuration
    parent_->get_fwdlink_transmit_enabled_list(fwdlink_xmt_enabled_);

    while (1) {
        if (should_stop()) {
            log_msg(oasys::LOG_DEBUG, "EhsRouter: stopping");
            break;
        }

        int timeout = 100;

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            oasys::Time now;
            now.get_time();

            if (now >= event->posted_time_) {
                oasys::Time in_queue;
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
}

//----------------------------------------------------------------------
void
EhsRouter::send_msg(rtrmessage::bpa& msg)
{
    parent_->send_msg(msg);
}

//----------------------------------------------------------------------
void
EhsRouter::set_link_statistics(bool enabled)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkIterator iter = all_links_.begin();
    while (iter != all_links_.end()) {
        EhsLink* el = iter->second;
        el->set_link_statistics(enabled);
        ++iter;
    }
}

//----------------------------------------------------------------------
void
EhsRouter::link_dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsLinkIterator iter = all_links_.begin();
    while (iter != all_links_.end()) {
        EhsLink* el = iter->second;
        el->link_dump(buf);
        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->append("Link dump: No Links detected\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsRouter::fwdlink_transmit_dump(oasys::StringBuffer* buf)
{
    fwdlink_xmt_enabled_.dump(buf);
}

//----------------------------------------------------------------------
void
EhsRouter::unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf)
{
    unrouted_bundles_.dump(buf);
}

//----------------------------------------------------------------------
void 
EhsRouter::set_log_level(int level)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    log_level_ = level;

    // update all of the Links
    EhsLinkIterator iter = all_links_.begin();
    while (iter != all_links_.end()) {
        EhsLink* el = iter->second;
        el->set_log_level(log_level_);
        ++iter;
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::log_msg(oasys::log_level_t level, const char*format, ...)
{
    if ((int)level >= log_level_) {
        if (!should_stop()) {
            // internal use only - passes the logpath_ of this object
            va_list args;
            va_start(args, format);
            parent_->log_msg_va(logpath_, level, format, args);
            va_end(args);
        }
    }
}

//----------------------------------------------------------------------
void 
EhsRouter::log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args)
{
    if ((int)level >= log_level_) {
        parent_->log_msg_va(path, level, format, args);
    }
}


} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED
