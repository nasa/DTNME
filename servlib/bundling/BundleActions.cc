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

#include "BundleActions.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleDaemonStorage.h"
#include "conv_layers/ConvergenceLayer.h"
#include "contacts/Link.h"
#include "storage/BundleStore.h"
#include "FragmentManager.h"
#include "FragmentState.h"

namespace dtn {

//----------------------------------------------------------------------
void
BundleActions::open_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_debug("BundleActions::open_link: "
                  "cannot open deleted link %s", link->name());
        return;
    }

    oasys::ScopeLock l(link->lock(), "BundleActions::open_link");

    if (link->isopen() || link->contact() != NULL) {
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
    ASSERT(link != NULL);

    if (! link->isopen() && ! link->isopening()) {
        log_err("not closing link %s since not open", link->name());
        return;
    }

    log_debug("BundleActions::close_link: closing link %s", link->name());

    link->close();
    ASSERT(link->contact() == NULL);
}

//----------------------------------------------------------------------
bool
BundleActions::queue_bundle(Bundle* bundle, const LinkRef& link,
                            ForwardingInfo::action_t action,
                            const CustodyTimerSpec& custody_timer)
{
    size_t total_len=0;
  
    BundleRef bref(bundle, "BundleActions::queue_bundle");
    
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_warn("BundleActions::queue_bundle: "
                 "failed to send bundle *%p on link %s",
                 bundle, link->name());
        BundleProtocol::delete_blocks(bundle, link);  //dzdebug
        return false;
    }
    
    //log_debug("trying to find xmit blocks for bundle id:%" PRIbid " on link %s",
    //          bundle->bundleid(), link->name());

    if (bundle->xmit_blocks()->find_blocks(link) != NULL) {
        // check to see if the bundle is queued or inflight
        if (!link->queue()->contains(bundle)) {
            if (link->inflight()->contains(bundle)) {
                log_crit("BundleActions::queue_bundle: "
                         "ignoring attempt to queue bundle %"  PRIbid " on link %s"
                         " while it is already in flight",
                         bundle->bundleid(), link->name());
                return false;
            } else {
                log_crit("BundleActions::queue_bundle: "
                        "found xmit blocks but bundle %" PRIbid " not queued on link %s"
                        " - deleting old xmit blocks and continuing",
                         bundle->bundleid(), link->name());
                BundleProtocol::delete_blocks(bundle, link);  //dzdebug
            }
        } else {
            log_err("BundleActions::queue_bundle: "
//                    "link not ready to handle bundle (block vector already exists), "
                    "bundle %" PRIbid " already queued on link %s" 
                    "dropping send request",
                     bundle->bundleid(), link->name());
            return false;
        }
    } 

    // XXX/demmer this should be moved somewhere in the router
    // interface so it can select options for the outgoing bundle
    // blocks (e.g. security)
    // XXX/ngoffee It's true the router should be able to select
    // blocks for various purposes, but I'd like the security policy
    // checks and subsequent block selection to remain inside the BPA,
    // with the DP pushing (firewall-like) policies and keys down via
    // a PF_KEY-like interface.
    //log_debug("trying to create xmit blocks for bundle id:%" PRIbid " on link %s",
    //          bundle->bundleid(), link->name());
    BlockInfoVec* blocks = BundleProtocol::prepare_blocks(bundle, link);
    if(blocks == NULL) {
        log_err("BundleActions::queue_bundle: prepare_blocks returned NULL on bundle %" PRIbid, bundle->bundleid());
        return false;
    }
    total_len = BundleProtocol::generate_blocks(bundle, blocks, link);
    if(total_len == 0) {
        log_err("BundleActions::queue_bundle: generate_blocks returned 0 on bundle %" PRIbid, bundle->bundleid());
        return false;
    }
    //log_debug("BundleActions::queue_bundle: total_len=%zu", total_len);
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

#ifdef LTP_ENABLED

	// XXXSF: The MTU check makes no sense for LTP where MTU relates to 
	// segment size and not bundle size. However, the UDP CL does need 
	// this and perhaps others, so I shouldn't move it just yet. But
	// properly speaking I think this should be a CL specific check to
	// make or not make -- Stephen Farrell
	if(strcmp(link->clayer()->name(), "ltp")) {
#endif
    if ((link->params().mtu_ != 0) && (total_len > link->params().mtu_)) {
        log_err("queue bundle *%p on %s link %s (%s): length %zu > mtu %u",
                bundle, link->type_str(), link->name(), link->nexthop(),
                total_len, link->params().mtu_);
        FragmentState *fs = BundleDaemon::instance()->fragmentmgr()->proactively_fragment(bundle, link, link->params().mtu_);
        log_debug("BundleActions::queue_bundle: bundle->payload().length()=%zu", bundle->payload().length());
        if(fs != 0) {
            //BundleDaemon::instance()->delete_bundle(bref, BundleProtocol::REASON_NO_ADDTL_INFO);
            log_debug("BundleActions::queue_bundle forcing fragmentation because TestParameters (the twiddle command) says we should");
            oasys::ScopeLock l(fs->fragment_list().lock(), "BundleActions::queue_bundle");
            for(BundleList::iterator it=fs->fragment_list().begin();it!= fs->fragment_list().end();it++) {
                BundleDaemon::post_at_head(
                                new BundleReceivedEvent(*it, EVENTSRC_FRAGMENTATION));
                
            }
            //if(fs->check_completed(true)) {
            //    for(BundleList::iterator it=fs->fragment_list().begin();it!= fs->fragment_list().end();it++) {
            //        fs->erase_fragment(*it);
            //    }
            //}
            // We need to eliminate the original bundle in this case,
            // because we've already sent it.
            bundle->fwdlog()->add_entry(link, action, ForwardingInfo::SUPPRESSED);
            BundleDaemon::post_at_head(
                  new BundleDeleteRequest(bundle, BundleProtocol::REASON_NO_ADDTL_INFO));
            //BundleDaemon::instance()->delete_bundle(bref, BundleProtocol::REASON_NO_ADDTL_INFO);
            return true;

        }
 
        return false;
    }
#ifdef LTP_ENABLED
	}
#endif

    // Make sure that the bundle isn't unexpectedly already on the
    // queue or in flight on the link
    if (bundle->is_queued_on(link->queue()))
    {
        log_err("queue bundle *%p on link *%p: already queued on link",
                bundle, link.object());
        return false;
    }

    if (bundle->is_queued_on(link->inflight()))
    {
        log_err("queue bundle *%p on link *%p: already in flight on link",
                bundle, link.object());
        return false;
    }

    //log_debug("adding QUEUED forward log entry for %s link %s "
    //          "with nexthop %s and remote eid %s to *%p    dzdebug> action: %d",
    //          link->type_str(), link->name(),
    //          link->nexthop(), link->remote_eid().c_str(), bundle, (int)action);
    
    bundle->fwdlog()->add_entry(link, action, ForwardingInfo::QUEUED,
                                custody_timer);
    // XXX/dz - this was just done in the add_entry!
    //BundleDaemon *daemon = BundleDaemon::instance();
    //daemon->actions()->store_update(bundle);

    //log_debug("adding *%p to link %s's queue (length %u)",
    //          bundle, link->name(), link->bundles_queued());

    if (! link->add_to_queue(bref, total_len)) {
        log_warn("error adding bundle *%p to link *%p queue",
                 bundle, link.object());
    }
    
    // finally, kick the convergence layer
//dzdebug 2020-03-12    link->clayer()->bundle_queued(link, bref);
    
    return true;
}

//----------------------------------------------------------------------
void
BundleActions::cancel_bundle(Bundle* bundle, const LinkRef& link)
{
    BundleRef bref(bundle, "BundleActions::cancel_bundle");
    
    ASSERT(link != NULL);
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
    
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    if (blocks == NULL) {
        log_warn("BundleActions::cancel_bundle: "
                 "cancel *%p but no blocks queued or inflight on *%p",
                 bundle, link.object());
        return; 
    }
        
    size_t total_len = BundleProtocol::total_length(blocks);
        
    if (link->del_from_queue(bref, total_len)) {
        BundleDaemon::post(new BundleSendCancelledEvent(bundle, link));
            
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
bool
BundleActions::delete_bundle(Bundle* bundle,
                             BundleProtocol::status_report_reason_t reason,
                             bool log_on_error)
{
    BundleRef bref(bundle, "BundleActions::delete_bundle");
    
    //log_debug("attempting to delete bundle *%p from data store", bundle);
    bool del = BundleDaemon::instance()->delete_bundle(bref, reason);

    if (log_on_error && !del) {
        log_err("Failed to delete bundle *%p from data store", bundle);
    }
    return del;
}

//----------------------------------------------------------------------
void
BundleActions::store_add(Bundle* bundle)
{
    BundleDaemonStorage::instance()->bundle_add_update(bundle);
}

//----------------------------------------------------------------------
void
BundleActions::store_update(Bundle* bundle)
{
    BundleDaemonStorage::instance()->bundle_add_update(bundle);
}

//----------------------------------------------------------------------
void
BundleActions::store_del(Bundle* bundle)
{
    BundleDaemonStorage::instance()->bundle_delete(bundle);
}

} // namespace dtn
