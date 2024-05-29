/*
 *    Copyright 2005-2006 Intel Corporation
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

#include "BundleActions.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "conv_layers/ConvergenceLayer.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "storage/BundleStore.h"
#include "FragmentManager.h"
#include "FragmentState.h"

namespace dtn {

//----------------------------------------------------------------------
void
BundleActions::open_link(const LinkRef& link)
{
    ASSERT(link != nullptr);
    if (link->isdeleted()) {
        log_debug("BundleActions::open_link: "
                  "cannot open deleted link %s", link->name());
        return;
    }

    oasys::ScopeLock l(link->lock(), "BundleActions::open_link");

    if (link->isopen() || link->contact() != nullptr) {
        log_err("not opening link %s since already open", link->name());
        return;
    }

    if (! link->isavailable()) {
        log_err("not opening link %s since not available", link->name());
        return;
    }
    
    log_debug("BundleActions::open_link: opening link %s", link->name());

    link->open();
}

//----------------------------------------------------------------------
void
BundleActions::close_link(const LinkRef& link)
{
    ASSERT(link != nullptr);

    if (! link->isopen() && ! link->isopening()) {
        log_err("not closing link %s since not open", link->name());
        return;
    }

    log_debug("BundleActions::close_link: closing link %s", link->name());

    link->close();
    ASSERT(link->contact() == nullptr);
}

//----------------------------------------------------------------------
bool
BundleActions::queue_bundle(Bundle* bundle, const LinkRef& link,
                            ForwardingInfo::action_t action,
                            const CustodyTimerSpec& custody_timer)
{
    ASSERT(link != nullptr);
    // check for link deleted before getting the bundle lock to avoid a deadly embrace
    bool link_deleted = link->isdeleted();

    size_t total_len = 0;

    BundleRef bref(bundle, "BundleActions::queue_bundle");

    if (link_deleted) {
        log_warn("BundleActions::queue_bundle: "
                 "failed to send bundle *%p on link %s",
                 bundle, link->name());
        BundleProtocol::delete_blocks(bundle, link);
        return false;
    }


    // check for BP version redirection (typically for BIBE)
    if (bundle->is_bpv6()) {
        if ( ! link->params().bp6_redirect_.empty() && (link->name() != link->params().bp6_redirect_)) {
            return redirect_queued_bundle(bundle, link, link->params().bp6_redirect_, action, custody_timer);
        }
    } else if (bundle->is_bpv7()) {
        if ( ! link->params().bp7_redirect_.empty() && (link->name() != link->params().bp7_redirect_)) {
            return redirect_queued_bundle(bundle, link, link->params().bp7_redirect_, action, custody_timer);
        }
    }


    // check to see if the bundle is queued or inflight
    if (!link->queue()->contains(bundle)) {
        if (link->inflight()->contains(bundle)) {
            log_debug("BundleActions::queue_bundle: "
                     "ignoring attempt to queue bundle %"  PRIbid " on link %s"
                     " while it is already in flight",
                     bundle->bundleid(), link->name());
            return false;
        } else {
            BundleProtocol::delete_blocks(bundle, link);  //just in case
        }
    } else {
        log_debug("BundleActions::queue_bundle: "
                "bundle %" PRIbid " already queued on link %s" 
                " dropping send request",
                 bundle->bundleid(), link->name());
        return false;
    }

    // XXX/dz don't hold bundle lock while calling contains() above - deadlock can happen

    oasys::ScopeLock scoplok(bundle->lock(), __func__);

    // XXX/demmer this should be moved somewhere in the router
    // interface so it can select options for the outgoing bundle
    // blocks (e.g. security)
    // XXX/ngoffee It's true the router should be able to select
    // blocks for various purposes, but I'd like the security policy
    // checks and subsequent block selection to remain inside the BPA,
    // with the DP pushing (firewall-like) policies and keys down via
    // a PF_KEY-like interface.
    SPtr_BlockInfoVec sptr_blocks = BundleProtocol::prepare_blocks(bundle, link);
    if(sptr_blocks == nullptr) {
        log_err("BundleActions::queue_bundle: prepare_blocks returned nullptr on bundle %" PRIbid, bundle->bundleid());
        return false;
    }
    total_len = BundleProtocol::generate_blocks(bundle, sptr_blocks.get(), link);
    if(total_len == 0) {
        log_err("BundleActions::queue_bundle: generate_blocks returned 0 on bundle %" PRIbid, bundle->bundleid());
        return false;
    }
    //log_debug("queue bundle *%p on %s link %s (%s) (total len %zu)",
    //          bundle, link->type_str(), link->name(), link->nexthop(),
    //          total_len);

    ForwardingInfo::state_t state = bundle->fwdlog()->get_latest_entry(link);
    if (state == ForwardingInfo::QUEUED) {
        //dz debug
        if (! BundleDaemon::params_.persistent_links_)
        {
            // previous shutdown while bundle was queued
            bundle->fwdlog()->update(link, ForwardingInfo::TRANSMIT_FAILED);
        }
        else 
        {
            log_err("queue bundle *%p on %s link %s (%s): "
                    "already queued or in flight",
                    bundle, link->type_str(), link->name(), link->nexthop());
            return false;
        }
    }

    scoplok.unlock();

    if ((link->params().mtu_ != 0) && (total_len > link->params().mtu_)) {
        log_warn("queue bundle *%p on %s link %s (%s): length %zu > mtu %zu",
                bundle, link->type_str(), link->name(), link->nexthop(),
                total_len, link->params().mtu_);

        FragmentManager *fmgr = BundleDaemon::instance()->fragmentmgr();
        FragmentState *fs = fmgr->proactively_fragment(bundle, link, link->params().mtu_);

        if(fs != 0) {
            oasys::ScopeLock l(fs->fragment_list().lock(), "BundleActions::queue_bundle");
            for(BundleList::iterator it=fs->fragment_list().begin();it!= fs->fragment_list().end();it++) {

                // if a new fragment turns out to be too big then it can be fragmented again
                // before this loop complete resulting in multiple events being generated
                // for a fragment causing it to be deleted as a duplicate 
                // - that is why the created_from param was added and checked
                if ((*it)->frag_created_from_bundleid() == bundle->bundleid()) {
                    BundleReceivedEvent* event_to_post;
                    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                    event_to_post = new BundleReceivedEvent(*it, EVENTSRC_FRAGMENTATION, sptr_dummy_prevhop);
                    SPtr_BundleEvent sptr_event_to_post(event_to_post);
                    BundleDaemon::post(sptr_event_to_post);
                }
            }

            //if(fs->check_completed(true)) {
            //    for(BundleList::iterator it=fs->fragment_list().begin();it!= fs->fragment_list().end();it++) {
            //        fs->erase_fragment(*it);
            //    }
            //}
            // We need to eliminate the original bundle in this case,
            // because we've already sent it.
            bundle->fwdlog()->add_entry(link, action, ForwardingInfo::SUPPRESSED);

            BundleDeleteRequest* event_to_post;
            event_to_post = new BundleDeleteRequest(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post_at_head(sptr_event_to_post);
            return true;

        }
 
        return false;
    }

    bundle->fwdlog()->add_entry(link, action, ForwardingInfo::QUEUED,
                                custody_timer);

    //log_debug("adding *%p to link %s's queue (length %u)",
    //          bundle, link->name(), link->bundles_queued());

    if (! link->add_to_queue(bref)) {
        log_warn("error adding bundle *%p to link *%p queue",
                 bundle, link.object());
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
BundleActions::redirect_queued_bundle(Bundle* bundle, const LinkRef& link,
                            std::string& redirect_cla,
                            ForwardingInfo::action_t action,
                            const CustodyTimerSpec& custody_timer)
{
    // look up the link for the redirection
    LinkRef redir_link = BundleDaemon::instance()->contactmgr()->find_link(redirect_cla.c_str());
    if (redir_link == nullptr) {
        log_err("Link *%p redirects bundles of BP version %d to unknown link name: %s",
                link.object(), bundle->bp_version(), redirect_cla.c_str());
        return false;
    } 

    // store the original link name associated with this redirection to properly
    // serialize IMC bundles if this is redirected to a BIBE CL
    bundle->add_link_redirect_mapping(redir_link->name_str(), link->name_str());

    bundle->fwdlog()->add_entry(link, action, ForwardingInfo::REDIRECTED,
                                custody_timer);

    return queue_bundle(bundle, redir_link, action, custody_timer);
}


//----------------------------------------------------------------------
void
BundleActions::cancel_bundle(Bundle* bundle, const LinkRef& link)
{
    BundleRef bref(bundle, "BundleActions::cancel_bundle");
    
    ASSERT(link != nullptr);
    if (link->isdeleted()) {
        log_debug("BundleActions::cancel_bundle: "
                  "cannot cancel bundle on deleted link %s", link->name());
        return;
    }

    log_debug("BundleActions::cancel_bundle: cancelling *%p on *%p",
              bundle, link.object());

    // First try to remove the bundle from the link's delayed-send
    // queue. If it's there, then safely remove it and post the send
    // cancelled request without involving the convergence layer.
    //
    // If instead it's actually in flight on the link, then call down
    // to the convergence layer to see if it can interrupt
    // transmission, in which case it's responsible for posting the
    // send cancelled event.
    
    if (link->del_from_queue(bref)) {
        BundleSendCancelledEvent* event_to_post;
        event_to_post = new BundleSendCancelledEvent(bundle, link);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
            
    } else if (link->inflight()->contains(bundle)) {
        link->clayer()->cancel_bundle(link, bref);
    }
    else {
        log_warn("BundleActions::cancel_bundle: "
                 "cancel *%p but not queued or inflight on *%p",
                 bundle, link.object());
    }
}

//----------------------------------------------------------------------
void
BundleActions::inject_bundle(Bundle* bundle)
{
    PANIC("XXX/demmer fix inject bundle");
    
    log_debug("inject bundle *%p", bundle);
    BundleDaemon::instance()->pending_bundles()->push_back(bundle);
    store_add(bundle);
}

//----------------------------------------------------------------------
//bool
//BundleActions::delete_bundle(Bundle* bundle,
//                             BundleProtocol::status_report_reason_t reason,
//                             bool log_on_error)
//{
//    BundleRef bref(bundle, "BundleActions::delete_bundle");
//    
//    //log_debug("attempting to delete bundle *%p from data store", bundle);
//    bool del = BundleDaemon::instance()->delete_bundle(bref, reason);
//
//    if (log_on_error && !del) {
//        log_err("Failed to delete bundle *%p from data store", bundle);
//    }
//    return del;
//}

//----------------------------------------------------------------------
void
BundleActions::store_add(Bundle* bundle)
{
    BundleDaemon::instance()->bundle_add_update_in_storage(bundle);
}

//----------------------------------------------------------------------
void
BundleActions::store_update(Bundle* bundle)
{
    BundleDaemon::instance()->bundle_add_update_in_storage(bundle);
}

//----------------------------------------------------------------------
void
BundleActions::store_del(Bundle* bundle)
{
    BundleDaemon::instance()->bundle_delete_from_storage(bundle);
}

} // namespace dtn
