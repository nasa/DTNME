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


#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/storage/DurableStore.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemonACS.h"
#include "BundleDaemonStorage.h"
#include "SDNV.h"
#include "naming/EndpointID.h"
#include "naming/IPNScheme.h"
#include "storage/GlobalStore.h"
#include "storage/PendingAcsStore.h"

#include "ExpirationTimer.h"

namespace dtn {

BundleDaemonACS::Params::Params()
    :  acs_enabled_(true),
       acs_size_(1000),
       acs_delay_(30) {}

BundleDaemonACS::Params BundleDaemonACS::params_;

bool BundleDaemonACS::shutting_down_ = false;


//----------------------------------------------------------------------
BundleDaemonACS::BundleDaemonACS()
    : BundleEventHandler("BundleDaemonACS", "/dtn/bd/acs"),
      Thread("BundleDaemonACS")
{
    memset(&stats_, 0, sizeof(stats_));

    pending_acs_map_ = new PendingAcsMap();

    acs_params_revision_ = 1;

    dbg_last_ae_op_ = AE_OP_NOOP;

    log_always("BundleDaemonACS initialized");
}

//----------------------------------------------------------------------
BundleDaemonACS::~BundleDaemonACS()
{
    delete pending_acs_map_;
}

//----------------------------------------------------------------------
void
BundleDaemonACS::post_event(BundleEvent* event, bool at_back)
{
    SPtr_BundleEvent sptr_event(event);

    //event->posted_time_.get_time();
    me_eventq_.push(sptr_event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonACS::get_bundle_stats(oasys::StringBuffer* buf)
{
    buf->appendf( "ACS Statistics: "
                 "%zu custody -- "
                 "%u accepted -- "
                 "%u acs_released -- "
                 "%u redundant -- "
                 "%u released -- "
                 "%u not found -- "
                 "%u generated -- "
                 "%u reloaded -- "
                 "%u invalid",
                 custody_list_.size(),
                 stats_.acs_accepted_,
                 stats_.acs_released_,
                 stats_.acs_redundant_,
                 stats_.total_released_,
                 stats_.acs_not_found_,
                 stats_.acs_generated_,
                 stats_.acs_reloaded_,
                 stats_.acs_invalid_);
}

//----------------------------------------------------------------------
void
BundleDaemonACS::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("%zu pending_events -- "
                 "%" PRIu64 " processed_events",
                 event_queue_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonACS::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
BundleDaemonACS::set_route_acs_params(SPtr_EIDPattern& sptr_pat, bool enabled, 
                                      u_int acs_delay, u_int acs_size)
{
    ACSRouteParamsList::iterator iter;
    ACSRouteParams* arp;

    oasys::ScopeLock(&acs_route_params_list_lock_, "BundleDaemonACS::set_route_acs_params");

    bool updated = false;
    for (iter = acs_route_params_list_.begin(); 
         iter != acs_route_params_list_.end(); ++iter) {
        arp = *iter;

        if (0 == strcmp(arp->sptr_endpoint_->c_str(), sptr_pat->c_str())) {
            // Endpoint already exists so just update it
            updated = true;
            arp->match_len_ = strlen(arp->sptr_endpoint_->c_str());
            arp->acs_enabled_ = enabled;
            arp->acs_delay_ = acs_delay;
            arp->acs_size_ = acs_size;

            log_info("set_route_acs_values - updated dest: %s  enabled: %s delay: %d size: %d",
                     sptr_pat->str().c_str(), (enabled ? "true" : "false"), 
                     acs_delay, acs_size);
            break;
        }
    }

    if (!updated) {
        // add a new entry
        arp = new ACSRouteParams();
        arp->sptr_endpoint_ = sptr_pat;
        arp->match_len_ = strlen(arp->sptr_endpoint_->c_str());
        arp->acs_enabled_ = enabled;
        arp->acs_delay_ = acs_delay;
        arp->acs_size_ = acs_size;
        acs_route_params_list_.push_back(arp);

        log_info("set_route_acs_values - added dest: %s  enabled: %s delay: %d size: %d",
                 sptr_pat->str().c_str(), (enabled ? "true" : "false"), 
                 acs_delay, acs_size);
    }

    ++acs_params_revision_;
}

//----------------------------------------------------------------------
int
BundleDaemonACS::delete_route_acs_params(SPtr_EIDPattern& sptr_pat)
{
    ACSRouteParamsList::iterator iter;
    ACSRouteParams* arp;

    oasys::ScopeLock(&acs_route_params_list_lock_, "BundleDaemonACS::delete_route_acs_params");

    int ret = -1; // assume not found

    for (iter = acs_route_params_list_.begin(); 
         iter != acs_route_params_list_.end(); ++iter) {
        arp = *iter;

        if (0 == strcmp(arp->sptr_endpoint_->c_str(), sptr_pat->c_str())) {
            ret = 0;
            acs_route_params_list_.erase(iter);
            log_info("delete_route_acs_values - deleted dest: %s",
                     sptr_pat->c_str());
            break;
        }
    }

    ++acs_params_revision_;

    return ret;
}

//----------------------------------------------------------------------
void
BundleDaemonACS::dump_acs_params(oasys::StringBuffer* buf)
{
    buf->appendf("BIBE/Aggregate Custody Signal Parameters:\n");
    buf->appendf("    acs_enabled: %s\n", (params_.acs_enabled_ ? "true" : "false"));
    buf->appendf("      acs_delay: %u\n", params_.acs_delay_);
    buf->appendf("       acs_size: %u\n", params_.acs_size_);
//    buf->appendf("last custody id: %" PRIu64 "\n", GlobalStore::instance()->last_custodyid());
    buf->appendf(" num in custody: %zu\n", custody_list_.size());

    ACSRouteParamsList::iterator iter;
    ACSRouteParams* arp;

    oasys::ScopeLock(&acs_route_params_list_lock_, "BundleDaemonACS::dump_acs_params");

    buf->appendf("\nRoute Override Values:\n");
    if (0 == acs_route_params_list_.size()) {
        buf->appendf("    <none>\n\n");
    } else {
        for (iter = acs_route_params_list_.begin(); 
             iter != acs_route_params_list_.end(); ++iter) {
            arp = *iter;

            buf->appendf("    %s  enabled: %s  delay: %u  size: %u\n",
                         arp->sptr_endpoint_->c_str(),
                         (arp->acs_enabled_ ? "true" : "false"), 
                         arp->acs_delay_, arp->acs_size_);

        }
        buf->appendf("\n");
    }

    get_bundle_stats(buf);
    buf->appendf("\n\n");
}

//----------------------------------------------------------------------
bool
BundleDaemonACS::get_acs_params_for_endpoint(const SPtr_EID& sptr_ep, u_int* revision,
                                             u_int* acs_delay, u_int* acs_size)
{
    bool result = false;
    if (*revision != acs_params_revision_) {
        result = true;
        *revision = acs_params_revision_;

        ACSRouteParamsList::iterator iter;
        ACSRouteParams* arp;
        ACSRouteParams* arp_best_match = nullptr;

        oasys::ScopeLock(&acs_route_params_list_lock_, "BundleDaemonACS::get_acs_params_for_endpoint");

        for (iter = acs_route_params_list_.begin(); 
             iter != acs_route_params_list_.end(); ++iter) {
            arp = *iter;

            if (arp->sptr_endpoint_->match(sptr_ep)) {
                if (!arp_best_match) {
                    arp_best_match = arp;
                } else if (arp->match_len_ > arp_best_match->match_len_) {
                    arp_best_match = arp;
                }
            }
        }

        if (nullptr != arp_best_match) {
            *acs_delay = arp_best_match->acs_delay_;
            *acs_size = arp_best_match->acs_size_;
            log_debug("get_acs_params_for_endpoint: %s > matched: %s : secs: %u size: %u", 
                      sptr_ep->c_str(), arp_best_match->sptr_endpoint_->c_str(), 
                      *acs_delay, *acs_size);
        } else {
            *acs_delay = params_.acs_delay_;
            *acs_size = params_.acs_size_;
            log_debug("get_acs_params_for_endpoint: %s > default: secs: %u size: %u", 
                      sptr_ep->c_str(), *acs_delay, *acs_size);
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleDaemonACS::acs_enabled_for_endpoint(const SPtr_EID& sptr_ep)
{
    bool result = params_.acs_enabled_;

    if (acs_params_revision_ > 1) {
        ACSRouteParamsList::iterator iter;
        ACSRouteParams* arp;
        ACSRouteParams* arp_best_match = nullptr;

        oasys::ScopeLock(&acs_route_params_list_lock_, "BundleDaemonACS::get_acs_params_for_endpoint");

        for (iter = acs_route_params_list_.begin(); 
             iter != acs_route_params_list_.end(); ++iter) {
            arp = *iter;

            if (arp->sptr_endpoint_->match(sptr_ep)) {
                if (!arp_best_match) {
                    arp_best_match = arp;
                } else if (arp->match_len_ > arp_best_match->match_len_) {
                    arp_best_match = arp;
                }
            }
        }

        if (nullptr != arp_best_match) {
            result = arp_best_match->acs_enabled_;
            log_debug("get_acs_enabled_for_endpoint: %s > matched: %s : enabled: %s", 
                      sptr_ep->c_str(), arp_best_match->sptr_endpoint_->c_str(), 
                      (result ? "true" : "false"));
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
BundleDaemonACS::add_acs_entry(AcsEntryMap* aemap, uint64_t left_edge, uint64_t fill_len,
                               uint64_t diff_to_prev_right_edge, int sdnv_len)
{
    // create the acs entry
    AcsEntry* entry = new AcsEntry();
    entry->left_edge_ = left_edge;
    entry->length_of_fill_ = fill_len;
    entry->diff_to_prev_right_edge_ = diff_to_prev_right_edge;
    entry->sdnv_length_ = sdnv_len;

    aemap->insert(AcsEntryPair(entry->left_edge_, entry));
}

//----------------------------------------------------------------------
void
BundleDaemonACS::handle_add_bundle_to_acs(SPtr_BundleEvent& sptr_event)
{
    AddBundleToAcsEvent* event = nullptr;
    event = dynamic_cast<AddBundleToAcsEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a AddBundleToAcsEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // Check if an ACS tree already exists for this custodian+succeeded+reason combo
    std::string key;
    key.append(event->sptr_custody_eid_->c_str());
    key.append(event->succeeded_ ? "~success~" : "~fail~" );
    char buf[16];
    snprintf(buf, sizeof(buf), "%2.2x", event->reason_);
    key.append(buf);
   
    PendingAcs* pacs;

    // try to find this key
    PendingAcsIterator pa_itr = pending_acs_map_->find(key);

    if ( pa_itr == pending_acs_map_->end() ) {
        // create a Pending ACS entry for this key
        pacs = new PendingAcs(key);

        // Add this new Pending ACS to the map
        pending_acs_map_->insert(PendingAcsPair(key, pacs));

        // fill in the Pending ACS info
        pacs->pacs_id_ = GlobalStore::instance()->next_pacsid();
        pacs->sptr_custody_eid_ = event->sptr_custody_eid_;
        pacs->succeeded_ = event->succeeded_;
        pacs->reason_ = event->reason_;
        pacs->set_acs_entry_map(new AcsEntryMap()); 
        pacs->clear_acs_expiration_timer();
        pacs->bp_version_ = 6;
    } else {
        // Add this custody id to the Pending ACS
        pacs = pa_itr->second;
    }

    oasys::ScopeLock l(&pacs->lock(), __func__);

    // Determine where this custody ID needs to be entered 
    // and how it will delta the ACS payload. Also identify
    // the operation that needs to be performed if the size
    // constraint is not triggered.
    uint64_t custody_id = event->custody_id_;
    AcsEntryIterator ae_itr;
    AcsEntry* entry = nullptr;
    AcsEntry* next_entry = nullptr;
    ae_operation_t ae_op = AE_OP_NOOP;
    int new_entry_size = 0;
    int new_next_size = 0;
    uint64_t new_entry_diff = 0;
    uint64_t new_next_diff = 0;

    int delta = determine_acs_changes(pacs, ae_op, custody_id, ae_itr,
                                      entry, next_entry,
                                      new_entry_size, new_next_size,
                                      new_entry_diff, new_next_diff);

    // get the latest parameter values in case they changed
    u_int old_acs_delay = pacs->acs_delay();
    bool acs_params_changed = get_acs_params_for_endpoint(
                                      pacs->custody_eid(),
                                      pacs->mutable_params_revision(),
                                      pacs->mutable_acs_delay(),
                                      pacs->mutable_acs_size());

    // before applying changes determine if the new length would be larger
    // than the configured max size and if so we need to send the current ACS
    // and then start a new ACS with this entry.
    // also sending ACS if the accumulate seconds was lowered
    if ( ( (pacs->acs_size() > 0) && (pacs->acs_payload_length() > 0) && 
           ((pacs->acs_payload_length() + delta) > pacs->acs_size()) ) ||
         ( acs_params_changed && (pacs->acs_delay() < old_acs_delay ) ) ) {
        // cancel the expiration timer
        oasys::SPtr_Timer exp_sptr = pacs->acs_expiration_timer();
        oasys::SharedTimerSystem::instance()->cancel_timer(exp_sptr);

        // the timer object will be deleted by the oasys::TimerSystem
        pacs->clear_acs_expiration_timer();

        log_debug("Size constraint triggered ACS: num entries: %zd", 
                  pacs->acs_entry_map()->size());

        // Create the ACS (clears and resets the Pending ACS object).
        // Skip the datastore update as we will do it shortly...
        generate_acs(pacs, false);
    }

    // now apply the changes
    apply_acs_changes(pacs, ae_op, custody_id, ae_itr, delta, 
                      entry, next_entry,
                      new_entry_size, new_next_size,
                      new_entry_diff, new_next_diff);

    // and update the database
    update_pending_acs_store(pacs);
}

//----------------------------------------------------------------------
void
BundleDaemonACS::add_bibe_bundle_to_acs(int32_t bp_version, const SPtr_EID& sptr_custody_eid, 
                                        uint64_t transmission_id, bool success, uint8_t reason)
{
    // Check if an ACS tree already exists for this custodian+succeeded+reason combo
    std::string key;
    key.append(sptr_custody_eid->c_str());
    key.append(success ? "~success~" : "~fail~" );
    char buf[16];
    snprintf(buf, sizeof(buf), "%2.2x", reason);
    key.append(buf);
   
    PendingAcs* pacs;

    // try to find this key
    PendingAcsIterator pa_itr = pending_acs_map_->find(key);

    if ( pa_itr == pending_acs_map_->end() ) {
        // create a Pending ACS entry for this key
        pacs = new PendingAcs(key);

        // Add this new Pending ACS to the map
        pending_acs_map_->insert(PendingAcsPair(key, pacs));

        // fill in the Pending ACS info
        pacs->pacs_id_ = GlobalStore::instance()->next_pacsid();
        pacs->custody_eid() = sptr_custody_eid;
        pacs->succeeded_ = success;
        pacs->reason_ = reason;
        pacs->set_acs_entry_map(new AcsEntryMap()); 
        pacs->clear_acs_expiration_timer();
        pacs->bp_version_ = bp_version;
    } else {
        // Add this custody id to the Pending ACS
        pacs = pa_itr->second;
    }

    oasys::ScopeLock l(&pacs->lock(), __func__);

    // Determine where this custody ID needs to be entered 
    // and how it will delta the ACS payload. Also identify
    // the operation that needs to be performed if the size
    // constraint is not triggered.
    uint64_t custody_id = transmission_id;
    AcsEntryIterator ae_itr;
    AcsEntry* entry = nullptr;
    AcsEntry* next_entry = nullptr;
    ae_operation_t ae_op = AE_OP_NOOP;
    int new_entry_size = 0;
    int new_next_size = 0;
    uint64_t new_entry_diff = 0;
    uint64_t new_next_diff = 0;

    int delta = determine_acs_changes(pacs, ae_op, custody_id, ae_itr,
                                      entry, next_entry,
                                      new_entry_size, new_next_size,
                                      new_entry_diff, new_next_diff);

    // get the latest parameter values in case they changed
    u_int old_acs_delay = pacs->acs_delay();
    bool acs_params_changed = get_acs_params_for_endpoint(
                                      pacs->custody_eid(),
                                      pacs->mutable_params_revision(),
                                      pacs->mutable_acs_delay(),
                                      pacs->mutable_acs_size());

    // before applying changes determine if the new length would be larger
    // than the configured max size and if so we need to send the current ACS
    // and then start a new ACS with this entry.
    // also sending ACS if the accumulate seconds was lowered
    if ( ( (pacs->acs_size() > 0) && (pacs->acs_payload_length() > 0) && 
           ((pacs->acs_payload_length() + delta) > pacs->acs_size()) ) ||
         ( acs_params_changed && (pacs->acs_delay() < old_acs_delay ) ) ) {
        // cancel the expiration timer
        oasys::SPtr_Timer exp_sptr = pacs->acs_expiration_timer();
        oasys::SharedTimerSystem::instance()->cancel_timer(exp_sptr);

        // the timer object will be deleted by the oasys::TimerSystem
        pacs->clear_acs_expiration_timer();

        log_debug("Size constraint triggered ACS: num entries: %zd", 
                  pacs->acs_entry_map()->size());

        // Create the ACS (clears and resets the Pending ACS object).
        // Skip the datastore update as we will do it shortly...
        generate_acs(pacs, false);
    }

    // now apply the changes
    apply_acs_changes(pacs, ae_op, custody_id, ae_itr, delta, 
                      entry, next_entry,
                      new_entry_size, new_next_size,
                      new_entry_diff, new_next_diff);

    // and update the database
    update_pending_acs_store(pacs);
}

//----------------------------------------------------------------------
int
BundleDaemonACS::determine_acs_changes(PendingAcs* pacs, ae_operation_t &ae_op,
                                       uint64_t custody_id, 
                                       AcsEntryIterator &ae_itr,
                                       AcsEntry* &entry, AcsEntry* &next_entry,
                                       int &new_entry_size, int &new_next_size,
                                       uint64_t &new_entry_diff, 
                                       uint64_t &new_next_diff)
{
    int delta = 0;
    int old_size = 0;
    uint64_t tlen;

    ae_itr = pacs->acs_entry_map()->lower_bound(custody_id);
    if (ae_itr == pacs->acs_entry_map()->end()) {
        if ( ! pacs->acs_entry_map()->empty() ) {
            // this custody id is greater than any entry so see if it is 
            // contiguous with the previous entry (also the last entry)
            --ae_itr;
            entry = ae_itr->second;
            if ( custody_id == (entry->left_edge_ + entry->length_of_fill_) ) {
                // this custody id is contiguous with the last fill entry
                // and there is no next entry to update
                ae_op = AE_OP_EXTEND_ENTRY;

                if (dbg_last_ae_op_ != ae_op) {
                    dbg_last_ae_op_ = ae_op;
                    log_debug("AE_OP_EXTEND_ENTRY at end of map - id: %" PRIu64, 
                              custody_id);
                }

                // calc size delta
                tlen = entry->length_of_fill_ + 1;
                new_entry_size = SDNV::encoding_len(entry->diff_to_prev_right_edge_) + 
                           SDNV::encoding_len(tlen); 
                delta = new_entry_size - entry->sdnv_length_;
            } else {
                // this is a new fill entry with no next fill entry to update
                ae_op = AE_OP_INSERT_ENTRY_AT_END;

                if (dbg_last_ae_op_ != ae_op) {
                    dbg_last_ae_op_ = ae_op;
                    log_debug("AE_OP_INSERT_ENTRY_AT_END at end of map - id: %" PRIu64, 
                              custody_id);
                }

                // calc size delta - NOTE: parentheses are important
                new_entry_diff = custody_id - 
                                 (entry->left_edge_ + (entry->length_of_fill_ - 1));
                new_entry_size = SDNV::encoding_len(new_entry_diff) + 1; // 1=sdnvlen of fill len(1)
                delta = new_entry_size;
            }
        } else {
            // this is a new fill entry with no next fill to update
            ae_op = AE_OP_INSERT_FIRST_ENTRY;

            if (dbg_last_ae_op_ != ae_op) {
                dbg_last_ae_op_ = ae_op;
                log_debug("AE_OP_INSERT_FIRST_ENTRY - insert first entry - id: %" PRIu64, 
                          custody_id);
            }

            // calc size delta
            new_entry_diff = custody_id;  // first and only entry
            new_entry_size = SDNV::encoding_len(new_entry_diff) + 1; // 1=sdnvlen of fill len(1)
            delta = new_entry_size;
        }
    } else {
        entry = ae_itr->second;

        if (custody_id == entry->left_edge_) {
            // this is a duplicate which can happen if we are receiving
            // duplicate bundles and we are NACKing them.
            // No op to perform no change in size but we will fall 
            // through and test the length in case it was lowered.
            ae_op = AE_OP_NOOP;

            if (dbg_last_ae_op_ != ae_op) {
                dbg_last_ae_op_ = ae_op;
                log_debug("AE_OP_NOOP - duplicate of left_edge_ - id: %" PRIu64, 
                          custody_id);
            }

        } else if (custody_id == (entry->left_edge_ - 1)) {
            // this custody id is contiguous with this entry so does it
            // fill the gap between this entry and the previous one?
            if ((2 == entry->diff_to_prev_right_edge_) && 
                (entry->left_edge_ != 2)) {
                // this custody id fills the gap between two entries 
                // and is a special case. the length is guaranteed to 
                // shrink so we will make the changes and then fall 
                // through to test the length in case it was lowered.
                ae_op = AE_OP_NOOP;

                if (dbg_last_ae_op_ != ae_op) {
                    dbg_last_ae_op_ = ae_op;
                    log_debug("AE_OP_NOOP - fills gap between two entries"
                              " - id: %" PRIu64, custody_id);
                }

                AcsEntryIterator prev_ae_itr = ae_itr;
                --prev_ae_itr;
                AcsEntry* prev_entry = prev_ae_itr->second;

                ASSERT(custody_id == 
                       (prev_entry->left_edge_ + prev_entry->length_of_fill_));

                old_size = entry->sdnv_length_ + prev_entry->sdnv_length_;

                // combine the two entries by adjusting the fill length of
                // the previous entry and then we can remove the found entry
                prev_entry->length_of_fill_ += entry->length_of_fill_ + 1;
                prev_entry->sdnv_length_ = 
                    SDNV::encoding_len(prev_entry->diff_to_prev_right_edge_) + 
                    SDNV::encoding_len(prev_entry->length_of_fill_);

                // bump the num custody ids
                ++pacs->num_custody_ids_;

                // remove the entry we no longer need
                pacs->acs_entry_map()->erase(ae_itr);
                delete entry;

                // adjust the payload length and fall through
                ASSERT(prev_entry->sdnv_length_ <= old_size);
                pacs->acs_payload_length_ += prev_entry->sdnv_length_ - old_size;
                delta = 0;
            } else {
                // this custody id prepends the left edge of the entry
                // and there are no other entries that need updating
                ae_op = AE_OP_PREPEND_ENTRY;

                if (dbg_last_ae_op_ != ae_op) {
                    dbg_last_ae_op_ = ae_op;
                    log_debug("AE_OP_PREPEND_ENTRY - one less than the "
                              "left edge - id: %" PRIu64, custody_id);
                }

                // calc size delta
                tlen = entry->length_of_fill_ + 1;
                new_entry_diff = entry->diff_to_prev_right_edge_ - 1;
                new_entry_size = SDNV::encoding_len(new_entry_diff) + 
                                 SDNV::encoding_len(tlen);
                delta = new_entry_size - entry->sdnv_length_;
            }
           
        } else if ((custody_id > 1) && 
                   (custody_id == 
                       (entry->left_edge_ - 
                        entry->diff_to_prev_right_edge_ + 1))) {
            // this custody id is contiguous with the previous fill entry
            // so both of the entries will need to be updated
            ae_op = AE_OP_EXTEND_ENTRY;

            if (dbg_last_ae_op_ != ae_op) {
                dbg_last_ae_op_ = ae_op;
                log_debug("AE_OP_EXTEND_ENTRY - extend len of entry and subtract "
                          "from diff of next entry - id: %" PRIu64, 
                          custody_id);
            }

            // normalize the entry pointers so that next_entry points to
            // the found entry and entry points to the "previous" entry.
            next_entry = entry;
            AcsEntryIterator prev_ae_itr = ae_itr;
            --prev_ae_itr;
            entry = prev_ae_itr->second;

            ASSERT((entry->left_edge_ + entry->length_of_fill_) == custody_id);

            // calc size delta (both entries affected)
            old_size = entry->sdnv_length_ + next_entry->sdnv_length_;

            // new size of entry being extended
            tlen = entry->length_of_fill_ + 1;
            new_entry_diff = entry->diff_to_prev_right_edge_;
            new_entry_size = SDNV::encoding_len(new_entry_diff) + 
                             SDNV::encoding_len(tlen);
            // plus new size of the next_entry whose 
            // diff_to_prev_right_edge_ will change
            new_next_diff = next_entry->diff_to_prev_right_edge_ - 1;
            new_next_size = SDNV::encoding_len(new_next_diff) + 
                            SDNV::encoding_len(next_entry->length_of_fill_);
            delta = new_entry_size + new_next_size - old_size;
        } else {
            // new entry to be inserted which will affect the 
            // diff_to_prev_right_edge_ of the found entry
            ae_op = AE_OP_INSERT_ENTRY;

            if (dbg_last_ae_op_ != ae_op) {
                dbg_last_ae_op_ = ae_op;
                log_debug("AE_OP_INSERT_ENTRY - new entry and update diff "
                          "of next entry - id: %" PRIu64, custody_id);
            }

            // normalize entry and next_entry so that next_entry points
            // to the next entry after the insertion point 
            // (entry will not be used since we are inserting a new one)
            next_entry = entry;

            // calc size delta (a new entry and adjusting this entry)
            old_size = next_entry->sdnv_length_;

            // new size of the new entry which will 
            // next_entry's diff_to_prev_right_edge_ 
            new_entry_diff = next_entry->diff_to_prev_right_edge_ -
                             (next_entry->left_edge_ - custody_id) ;
                                             // (1=sdnvlen of new fill len(1))
            new_entry_size = SDNV::encoding_len(new_entry_diff) + 1;

            // plus new size of the adjusted entry
            new_next_diff = entry->left_edge_ - custody_id;
            new_next_size = SDNV::encoding_len(new_next_diff) + 
                            SDNV::encoding_len(entry->length_of_fill_);
            delta = new_entry_size + new_next_size - old_size;
        }
    }

    // return how many bytes this would change the ACS payload size
    return delta;
}

//----------------------------------------------------------------------
void
BundleDaemonACS::apply_acs_changes(PendingAcs* pacs, ae_operation_t &ae_op,
                                   uint64_t custody_id, 
                                   AcsEntryIterator &ae_itr, int delta,
                                   AcsEntry* &entry, AcsEntry* &next_entry,
                                   int &new_entry_size, int &new_next_size,
                                   uint64_t &new_entry_diff, 
                                   uint64_t &new_next_diff)
{
    // now apply the changes
    if (AE_OP_NOOP == ae_op) {
        // fall through and do nothing - if size triggered sending the ACS
        // then the payload length would be zero and we do not want to
        // start a new pending ACS so this is here to document this case.
    } else if (0 == pacs->acs_payload_length()) {
        ASSERT(0 == pacs->acs_entry_map()->size());

        log_debug("Inserting first entry - id: %" PRIu64, custody_id);

        // initiating a new Pending ACS...
        // regardless of what the determined ae_op was we 
        // are now inserting a new entry
        new_entry_diff = custody_id;
        new_entry_size = SDNV::encoding_len(new_entry_diff) + 1;
        add_acs_entry(pacs->acs_entry_map(), custody_id, 1, 
                      new_entry_diff, new_entry_size);

        // set the size of the payload
        pacs->acs_payload_length_ = new_entry_size;

        // set the initial num custody ids
        pacs->num_custody_ids_ = 1;

        // create a new timer object and start it ticking
        SPtr_AcsExpirationTimer exp_sptr = std::make_shared<AcsExpirationTimer>(pacs->durable_key(), pacs->pacs_id());
        pacs->set_acs_expiration_timer(exp_sptr);
        oasys::SPtr_Timer base_sptr = exp_sptr;
        oasys::SharedTimerSystem::instance()->schedule_in(pacs->acs_delay() * 1000, base_sptr);

    } else {
        switch (ae_op) {
        case AE_OP_INSERT_FIRST_ENTRY:
        case AE_OP_INSERT_ENTRY:
        case AE_OP_INSERT_ENTRY_AT_END:
            // add the new entry
            add_acs_entry(pacs->acs_entry_map(), custody_id, 1, 
                          new_entry_diff, new_entry_size);

            // bump the num custody ids
            ++pacs->num_custody_ids_;

            // if a next_entry is set up then update it
            if (next_entry) {
                next_entry->diff_to_prev_right_edge_ = new_next_diff;
                next_entry->sdnv_length_ = new_next_size;
            }

            // update the Pending ACS payload length
            pacs->acs_payload_length_ += delta;
            break;

        case AE_OP_EXTEND_ENTRY:
            // extending the length of entry...
            entry->length_of_fill_ += 1;
            entry->sdnv_length_ = new_entry_size;

            // bump the num custody ids
            ++pacs->num_custody_ids_;

            // if a next_entry is set up then update it
            if (next_entry) {
                next_entry->diff_to_prev_right_edge_ = new_next_diff;
                next_entry->sdnv_length_ = new_next_size;
            }

            // update the Pending ACS payload length
            pacs->acs_payload_length_ += delta;
            break;

        case AE_OP_PREPEND_ENTRY:
            // update the entry values
            entry->left_edge_ = custody_id;
            entry->length_of_fill_ += 1;
            entry->sdnv_length_ = new_entry_size;
            entry->diff_to_prev_right_edge_ = new_entry_diff;

            // and then reposition it in the map
            pacs->acs_entry_map()->erase(ae_itr);
            pacs->acs_entry_map()->insert(AcsEntryPair(custody_id, entry));

            // bump the num custody ids
            ++pacs->num_custody_ids_;

            // update the Pending ACS payload length
            pacs->acs_payload_length_ += delta;
            break;

        default:
            break;
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemonACS::update_pending_acs_store(PendingAcs* pacs)
{
    // update the datastore with the changes to the Pending ACS
    PendingAcsStore* pastore = PendingAcsStore::instance();

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (pacs->in_datastore()) {
        if (! pastore->update(pacs)) {
            log_crit("error updating Pending ACS %s in data store!!",
                     pacs->durable_key().c_str());
        }
    } else {
        if (! pastore->add(pacs)) {
            log_crit("error adding Pending ACS %s to data store!!",
                     pacs->durable_key().c_str());
        }
        pacs->set_in_datastore(true);
    }
    store->end_transaction();
}

    
//----------------------------------------------------------------------
void
BundleDaemonACS::generate_acs(PendingAcs* pacs, bool update_datastore)
{
    if (pacs->bp_version_ == 7) {
        bibe_generate_acs(pacs, update_datastore);
        return;
    }

    if (pacs->acs_payload_length() > 0) {
        log_info("generating ACS for key: %s",
                   pacs->durable_key().c_str());

        // Create a new bundle and submit it to be processed
        Bundle* signal = new Bundle(BundleProtocol::BP_VERSION_6);
        // Note that the Pending ACS is cleared and unusable after this call

        // Note that the Pending ACS is cleared and unusable after this call
        if (pacs->custody_eid()->is_ipn_scheme()) {
            AggregateCustodySignal::create_aggregate_custody_signal(
                    signal, pacs, BundleDaemon::instance()->local_eid_ipn());
        } else {
            AggregateCustodySignal::create_aggregate_custody_signal(
                    signal, pacs, BundleDaemon::instance()->local_eid());
        }

#ifdef ECOS_ENABLED
        // prepare flags and data for our ECOS block
        u_int8_t block_flags = 0;    // overridden in block processor so not set here
        u_int8_t ecos_flags = 0x08;  // Reliable transmission bit
        u_int8_t ecos_ordinal = 255; // reserved for Custody Signals
 
        signal->set_ecos_enabled(true);
        signal->set_ecos_flags(ecos_flags);
        signal->set_ecos_ordinal(ecos_ordinal);

        oasys::ScratchBuffer<u_char*, 16> scratch_ecos;
        int len = 2;
        u_char* data = scratch_ecos.buf(len);
        *data = ecos_flags;
        ++data;
        *data = ecos_ordinal;

        // add our ECOS as an API block
        u_int16_t block_type = BundleProtocolVersion6::EXTENDED_CLASS_OF_SERVICE_BLOCK;
        BlockProcessor* ecosbpr = BundleProtocolVersion6::find_processor(block_type);
        SPtr_BlockInfo sptr_info = signal->api_blocks()->append_block(ecosbpr);
        SPtr_BlockInfoVec sptr_api_blocks = signal->api_blocks();
        ecosbpr->init_block(sptr_info.get(), sptr_api_blocks.get(), nullptr, block_type,
                            block_flags, scratch_ecos.buf(), len);
#endif // ECOS_ENABLED

        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
        BundleReceivedEvent* event_to_post;
        event_to_post = new BundleReceivedEvent(signal, EVENTSRC_ADMIN, sptr_dummy_prevhop);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

        ++stats_.acs_generated_;
    }

    if (update_datastore) {
        update_pending_acs_store(pacs);
    }
}


//----------------------------------------------------------------------
void
BundleDaemonACS::handle_aggregate_custody_signal(SPtr_BundleEvent& sptr_event)
{
    (void) sptr_event;
    ASSERTF(false, "handle_aggregate_custody_signal must be processed "
                   "in BundleDaemon not BundleDaemonACS");
}


//----------------------------------------------------------------------
void
BundleDaemonACS::handle_acs_expired(SPtr_BundleEvent& sptr_event)
{
    AcsExpiredEvent* event = nullptr;
    event = dynamic_cast<AcsExpiredEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a AcsExpiredEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // try to find this key
    PendingAcsIterator pa_itr = pending_acs_map_->find(event->pacs_key_);

    if ( pa_itr == pending_acs_map_->end() ) {
        log_info("handle_acs_expired: pending ACS was deleted before timeout: %s",
                   event->pacs_key_.c_str());
    } else {
        PendingAcs* pacs = pa_itr->second;

        oasys::ScopeLock l(&pacs->lock(), "BundleDaemonACS::handle_issue_acs");

        if (event->pacs_id_ != pacs->pacs_id()) { 
            // if not successful then the ACS has already been sent and we are done
            log_info("handle_acs_expired: pending ACS was issued "
                     "before timeout: %s pacs_id: %u",
                     event->pacs_key_.c_str(), event->pacs_id_);
        } else {
            log_info("handle_acs_expired: generating ACS for key: %s "
                     "pacs_id: %u num_custody_ids: %u entries: %zd",     
                     event->pacs_key_.c_str(), event->pacs_id_, 
                     pacs->num_custody_ids(), pacs->acs_entry_map()->size());

            generate_acs(pacs);
        }
    }
}


//----------------------------------------------------------------------
void
BundleDaemonACS::process_acs(SPtr_BundleEvent& sptr_event)
{
    AggregateCustodySignalEvent* event = nullptr;
    event = dynamic_cast<AggregateCustodySignalEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a AggregateCustodySignalEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    // release custody if either the signal succeded or if it
    // (paradoxically) failed due to duplicate transmission
    bool redundant = event->data_.redundant_;
    bool release = ( event->data_.succeeded_ || redundant );
    if ( !release ) {
        // XXX/dz could be storage depleted or some such - wait for custody
        // transfer timer to kick off or try to do something immediately??
        log_info("AGGREGATE_CUSTODY_SIGNAL: %s (%s) - does not release "
                 "bundles: ACS not processed",
                 event->data_.succeeded_ ? "succeeded" : "failed",
                 CustodySignal::reason_to_str(event->data_.reason_));
        ++stats_.acs_invalid_;
        return;
    } else {
        log_info("AGGREGATE_CUSTODY_SIGNAL: %s (%s) - processing and "
                 "releasing bundles",
                 event->data_.succeeded_ ? "succeeded" : "failed",
                 CustodySignal::reason_to_str(event->data_.reason_));
    }

    if ( 0 == event->data_.acs_entry_map_->size()) {
        log_warn("AGGREGATE_CUSTODY_SIGNAL: Invalid ACS: no custody ids "
                 "specified - ACS not processed");
        ++stats_.acs_invalid_;
        return;
    }

    uint64_t num_acks = 0;
    uint64_t num_not_fnd = 0; 


    uint64_t last_custody_id = custody_list_.last_custody_id(event->dest_eid_);

    // loop through the ACS entries and update the bundles
    u_int64_t idx;
    u_int64_t cid;
    AcsEntry* entry;
    AcsEntryIterator del_itr;
    AcsEntryIterator ae_itr = event->data_.acs_entry_map_->begin();
    while ( ae_itr != event->data_.acs_entry_map_->end() ) {
        entry = ae_itr->second;

        // process each custody id in this fill entry
        for ( idx=0; idx<entry->length_of_fill_; ++idx ) {
            cid = entry->left_edge_ + idx;

            if (cid > last_custody_id) {
                log_err("received BIBE/aggregate custody signal for custody id: %" PRIu64
                         " which was greater than the last one issued for dest (%s): %" PRIu64,
                         cid, event->dest_eid_.c_str(), last_custody_id);
                break;
            }

            // find this bundle
            BundleRef bref = custody_list_.find(event->dest_eid_, cid);
    
            if (bref == nullptr) {
                //log_debug("received BIBE/aggregate custody signal for custody id: %" PRIu64
                //         " which was deleted", cid);
                ++num_not_fnd; 
                ++stats_.acs_not_found_;
                // continue processing the other bundle ids
            } else {
                Bundle* bundle = bref.object();

                if ( ! bundle->local_custody() && ! bundle->bibe_custody() ) {
                    // data base was cleared and bundle ids reused??
                    //** this should have been caught above and processing aborted
                    log_warn("received BIBE/aggregate custody signal for dest: %s custody id %"  PRIu64
                             ", but don't have custody", event->dest_eid_.c_str(), cid);
                    // continue processing the other bundle ids
                } else {
                    // log a notice for each bundle
                    if ( redundant ) {
                        ++stats_.acs_redundant_;
                        //log_info("releasing custody for custody id: %" PRIu64
                        //         " due to redundant reception", cid);
                    } else {
                        ++stats_.acs_released_;
                        //log_info("releasing custody for custody id: %" PRIu64
                        //         " due to custody transfer", cid);
                    }

                    BundleDaemon::instance()->release_custody(bundle);
                    BundleDaemon::instance()->try_to_delete(bref, "ACS CustodyRelease");
                    ++num_acks;
                }
            }
        }

        del_itr = ae_itr;
        ++ae_itr;

        delete entry;
        event->data_.acs_entry_map_->erase(del_itr);
    }

    // delete the entry map which was allocated in 
    // AggregateCustodySignal::parse_aggregate_custody_signal()
    delete event->data_.acs_entry_map_;

    log_info("Received BIBE/ACS - Bundles acked: %" PRIu64 "  not fnd: %" PRIu64, 
             num_acks, num_not_fnd);
}


//----------------------------------------------------------------------
void
BundleDaemonACS::load_pendingacs()
{
    PendingAcs* pacs;
    PendingAcsStore* pastore = PendingAcsStore::instance();
    PendingAcsStore::iterator* iter = pastore->new_iterator();

    log_info("loading Pending ACSs from data store");

    int status;

    std::vector<PendingAcs*> delete_pacs;
    
    for (status = iter->begin(); iter->more(); status = iter->next()) {
        if ( oasys::DS_NOTFOUND == status ) {
            log_warn("received DS_NOTFOUND from data store - Pending ACS <%s>",
                    iter->cur_val().c_str());
            break;
        } else if ( oasys::DS_ERR == status ) {
            log_err("error loading Pending ACS <%s> from data store",
                    iter->cur_val().c_str());
            break;
        }

        pacs = pastore->get(iter->cur_val());
        
        if (pacs == nullptr) {
            log_err("error loading Pending ACS <%s> from data store",
                    iter->cur_val().c_str());
            continue;
        }

        // reset the flags indicating it is in the datastore
        pacs->set_in_datastore(true);
        pacs->set_queued_for_datastore(true);
        BundleDaemon::instance()->reloaded_pendingacs();

        log_info("generating ACS immediately after load from datastore for %s",
                 pacs->durable_key().c_str());

        // generate the ACS immediately with no need to update the datastore
        generate_acs(pacs, false);

        // save off the Pending ACS to be deleted from the datastore 
        delete_pacs.push_back(pacs);
    }
    
    delete iter;

    // now that the durable iterator is gone, purge the Pending ACSs
    for (unsigned int i = 0; i < delete_pacs.size(); ++i) {
        pastore->del(delete_pacs[i]->durable_key());
        delete delete_pacs[i];
    }

    log_info("exit load_pendingacs");
}

//----------------------------------------------------------------------
void
BundleDaemonACS::accept_custody(Bundle* bundle)
{
    if (bundle->custodyid() > 0) {
        // this bundle is reloaded from the datastore so we 
        // just add it to the list as is
        custody_list_.insert(bundle);
        ++stats_.acs_reloaded_;
        return;
    }

    if (acs_enabled_for_endpoint(bundle->dest())) {
        // assign the next custody id to this bundle and 
        // insert in the custody_list_
        custody_list_.insert(bundle);
        ++stats_.acs_accepted_;

        // prepare flags and data for our CTEB block
        u_int8_t flags = 0;
        oasys::ScratchBuffer<u_char*, 64> scratch;

        // calc length of data and get a scratch buffer to load
        int idlen = SDNV::encoding_len(bundle->custodyid());
        ASSERT(idlen > 0);
        int custlen = bundle->custodian()->length();
        int len = idlen + custlen;
        int available = len;
        u_char* data = scratch.buf(len);

        // insert the custody id
        int sdnv_encoding_len = SDNV::encode(bundle->custodyid(), data, available);
        ASSERT(sdnv_encoding_len > 0);
        ASSERT(sdnv_encoding_len == idlen);
        data      += sdnv_encoding_len;
        available -= sdnv_encoding_len;

        // insert our local eid as the cteb creator
        memcpy(data, bundle->custodian()->c_str(), available);

        // add our CTEB as an API block
        // NOTE: received CTEBs will be stripped during transmission
        u_int16_t block_type = BundleProtocolVersion6::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK;
        BlockProcessor* ctebptr = BundleProtocolVersion6::find_processor(block_type);
        SPtr_BlockInfo sptr_info = bundle->api_blocks()->append_block(ctebptr);
        SPtr_BlockInfoVec sptr_api_blocks = bundle->api_blocks();
        ctebptr->init_block(sptr_info.get(), sptr_api_blocks.get(), nullptr, block_type,
                            flags, scratch.buf(), len);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonACS::erase_from_custodyid_list(Bundle* bundle)
{
    if (bundle->custodyid() > 0) {
        custody_list_.erase(bundle);
        bundle->set_custodyid(0);
        ++stats_.total_released_;
    }
}



//----------------------------------------------------------------------
bool
BundleDaemonACS::generate_custody_signal(Bundle* bundle, bool succeeded,
                                         custody_signal_reason_t reason)
{
    bool ret = false;

    if (acs_enabled_for_endpoint(bundle->custodian())) {
        post_event(new AddBundleToAcsEvent(bundle->custodian(), succeeded, 
                                     reason, bundle->cteb_custodyid()), false);
        ret = true;
    } 

    return ret;
}


//----------------------------------------------------------------------
void
BundleDaemonACS::bibe_accept_custody(Bundle* bundle)
{
    ASSERT(bundle->custodyid() == 0);

    // assign the next custody id to this bundle and 
    // insert in the custody_list_
    custody_list_.insert(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemonACS::bibe_erase_from_custodyid_list(Bundle* bundle)
{
    if (bundle->custodyid() > 0) {
        custody_list_.erase(bundle);
        bundle->set_custodyid(0);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonACS::bibe_generate_acs(PendingAcs* pacs, bool update_datastore)
{
    if (pacs->acs_payload_length() > 0) {
        log_info("generating BIBE ACS for key: %s",
                   pacs->durable_key().c_str());

        // Create a new bundle and submit it to be processed
        Bundle* signal = new Bundle(BundleProtocol::BP_VERSION_7);
        // Note that the Pending ACS is cleared and unusable after this call

        // Note that the Pending ACS is cleared and unusable after this call
        if (pacs->custody_eid()->is_ipn_scheme()) {
            AggregateCustodySignal::create_bibe_custody_signal(
                    signal, pacs, BundleDaemon::instance()->local_eid_ipn());
        } else {
            AggregateCustodySignal::create_bibe_custody_signal(
                    signal, pacs, BundleDaemon::instance()->local_eid());
        }

        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
        BundleReceivedEvent* event_to_post;
        event_to_post = new BundleReceivedEvent(signal, EVENTSRC_ADMIN, sptr_dummy_prevhop);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);

        ++stats_.acs_generated_;
    }

    if (update_datastore) {
        update_pending_acs_store(pacs);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonACS::handle_event(SPtr_BundleEvent& sptr_event)
{
    dispatch_event(sptr_event);
   
    // no transactions to close
 
    stats_.events_processed_++;

    if (sptr_event->processed_notifier_) {
        sptr_event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonACS::run()
{
    char threadname[16] = "BD-AggCustdy";
    pthread_setname_np(pthread_self(), threadname);
   
    SPtr_BundleEvent sptr_event;


    while (1) {
        if (should_stop()) {
            break;
        }

        if (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);
            
            // handle the event
            handle_event(sptr_event);

            // clean up the event
            sptr_event.reset();
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

} // namespace dtn

