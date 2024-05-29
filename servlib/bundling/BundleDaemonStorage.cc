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

#include <db.h>

#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/util/Time.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemonStorage.h"
#include "SDNV.h"
#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"
#include "storage/LinkStore.h"
#include "storage/PendingAcsStore.h"

#include "ExpirationTimer.h"


// enable or disable debug level logging in this file
#undef BDSTORAGE_LOG_DEBUG_ENABLED
//#define BDSTORAGE_LOG_DEBUG_ENABLED 1



namespace dtn {

BundleDaemonStorage::Params::Params()
    : db_storage_ms_interval_(10),
      db_log_auto_removal_(false),
      db_storage_enabled_(true),
      db_force_sync_to_disk_(true),
      payload_location_(BundlePayload::DISK)
{}

BundleDaemonStorage::Params BundleDaemonStorage::params_;

//----------------------------------------------------------------------
BundleDaemonStorage::BundleDaemonStorage(BundleDaemon* parent)
    : BundleEventHandler("BundleDaemonStorage", "/dtn/bundle/daemon/storage"),
      Thread("BundleDaemonStorage")
{
    daemon_ = parent;
    
    memset(&stats_, 0, sizeof(stats_));

    app_shutdown_proc_ = nullptr;
    app_shutdown_data_ = nullptr;

    run_loop_terminated_ = false;
    last_commit_completed_ = false;

    add_update_bundles_     = new DeleteBundleMap();
    delete_bundles_     = new DeleteBundleMap();

    add_update_registrations_ = new RegistrationMap();
    delete_registrations_ = new RegistrationMap();

    add_update_links_ = new LinkMap();
    delete_links_ = new LinkMap();

    add_update_pendingacs_ = new PendingAcsMap();
    delete_pendingacs_ = new PendingAcsMap();
}

//----------------------------------------------------------------------
BundleDaemonStorage::~BundleDaemonStorage()
{
    log_always("BDStorage destructor - run_loop_terminated_ = %s", run_loop_terminated_ ? "true" : "false");

    delete add_update_bundles_;
    delete delete_bundles_;

    delete add_update_registrations_;
    delete delete_registrations_;

    delete add_update_links_;
    delete delete_links_;

    delete add_update_pendingacs_;
    delete delete_pendingacs_;
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::post_event(SPtr_BundleEvent& sptr_event, bool at_back)
{ 
#ifdef BDSTORAGE_LOG_DEBUG_ENABLED
    log_debug("posting sptr_event (%p) with type %s (at %s)",
              sptr_event.get(), sptr_event->type_str(), at_back ? "back" : "head");
#endif
    sptr_event->posted_time_.get_time();
    me_eventq_.push(sptr_event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::get_stats(oasys::StringBuffer* buf)
{
    (void) buf;
    buf->appendf("%zu eventq\n"
                 "Bundle Storage: %" PRIu64 " instore -- "
                 "%" PRIu64 " added -- "
                 "%" PRIu64 " updated -- "
                 "%" PRIu64 " combinedUpdates -- "
                 "%" PRIu64 " deleted -- "
                 "%" PRIu64 " deletedUpdates -- "
                 "%" PRIu64 " delBeforeUpdates -- "
                 "%" PRIu64 " delBeforeAdd -- "
                 "%" PRIu64 " deletesskipped -- "
                 "%" PRIu64 " reloaded\n",
                 me_eventq_.size(),
                 stats_.bundles_in_db_,
                 stats_.bundles_added_,
                 stats_.bundles_updated_,
                 stats_.bundles_combinedupdates_,
                 stats_.bundles_deleted_,
                 stats_.bundles_delupdates_,
                 stats_.bundles_delbeforeupdates_,
                 stats_.bundles_delbeforeadd_,
                 stats_.bundles_deletesskipped_,
                 stats_.bundles_reloaded_);

    buf->appendf("Registration Storage: %" PRIu64 " instore -- "
                 "%" PRIu64 " added -- "
                 "%" PRIu64 " updated -- "
                 "%" PRIu64 " combinedupdates -- "
                 "%" PRIu64 " deleted -- "
                 "%" PRIu64 " deletedupdates -- "
                 "%" PRIu64 " reloaded\n",
                 stats_.regs_in_db_,
                 stats_.regs_added_,
                 stats_.regs_updated_,
                 stats_.regs_combinedupdates_,
                 stats_.regs_deleted_,
                 stats_.regs_delupdates_,
                 stats_.regs_reloaded_);

    buf->appendf("Links Storage: %" PRIu64 " instore -- "
                 "%" PRIu64 " added -- "
                 "%" PRIu64 " updated -- "
                 "%" PRIu64 " combinedupdates -- "
                 "%" PRIu64 " deleted -- "
                 "%" PRIu64 " deletedupdates -- "
                 "%" PRIu64 " reloaded\n",
                 stats_.links_in_db_,
                 stats_.links_added_,
                 stats_.links_updated_,
                 stats_.links_combinedupdates_,
                 stats_.links_deleted_,
                 stats_.links_delupdates_,
                 stats_.links_reloaded_);

    buf->appendf("Pending ACS Storage: %" PRIu64 " instore -- "
                 "%" PRIu64 " added -- "
                 "%" PRIu64 " updated -- "
                 "%" PRIu64 " combinedupdates -- "
                 "%" PRIu64 " deleted -- "
                 "%" PRIu64 " deletedupdates -- "
                 "%" PRIu64 " reloaded\n",
                 stats_.pacs_in_db_,
                 stats_.pacs_added_,
                 stats_.pacs_updated_,
                 stats_.pacs_combinedupdates_,
                 stats_.pacs_deleted_,
                 stats_.pacs_delupdates_,
                 stats_.pacs_reloaded_);
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("BundleDaemonStorage: %zu pending_events (max: %zu) -- "
                 "%" PRIu64 " processed_events \n",
    	         me_eventq_.size(),
    	         me_eventq_.max_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonStorage::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

    
//----------------------------------------------------------------------
void
BundleDaemonStorage::bundle_add_update(const BundleRef& bref)
{
    if (!params_.db_storage_enabled_) return;

    // XXX/dz  write new custody bundles immediately and trigger 
    // custody signals after they have been added to the bundle store?

    Bundle* bundle = bref.object();
    
    bundle->lock()->lock("BundleDaemonStorage::bundle_add_update - check in storage queue");

    bundle->set_queued_for_datastore(true);

    if ( ! bundle->in_storage_queue() ) {
        BundleStore::instance()->reserve_payload_space(bundle);
        bundle->set_in_storage_queue(true);
        StoreBundleUpdateEvent* event_to_post;
        event_to_post = new StoreBundleUpdateEvent(bundle);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    } else {
        ++stats_.bundles_combinedupdates_;
    }
    bundle->lock()->unlock();
}
    
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::bundle_add_update(Bundle* bundle)
{
    if (!params_.db_storage_enabled_) return;

    // XXX/dz  write new custody bundles immediately and trigger 
    // custody signals after they have been added to the bundle store?

    bundle->lock()->lock("BundleDaemonStorage::bundle_add_update - check in storage queue");

    bundle->set_queued_for_datastore(true);
    
    if ( ! bundle->in_storage_queue() ) {
        BundleStore::instance()->reserve_payload_space(bundle);
        bundle->set_in_storage_queue(true);

        StoreBundleUpdateEvent* event_to_post;
        event_to_post = new StoreBundleUpdateEvent(bundle);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    } else {
        ++stats_.bundles_combinedupdates_;
    }
    bundle->lock()->unlock();
}
    
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::bundle_delete(Bundle* bundle)
{
    if (!params_.db_storage_enabled_) return;

    bundle->lock()->lock("BundleDaemonStorage::bundle_add_update - check in storage queue");

    bundle->set_deleting(true);
    bool in_queue = bundle->in_storage_queue();
    bool in_datastore = bundle->in_datastore();
    bundle->lock()->unlock();

    if (in_queue || in_datastore) {
        int64_t size_and_flag = bundle->durable_size();
        if (!in_datastore) size_and_flag *= -1;

        StoreBundleDeleteEvent* event_to_post;
        event_to_post = new StoreBundleDeleteEvent(bundle->bundleid(), size_and_flag);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    } else {
        ++stats_.bundles_deletesskipped_;   // # deletes not posted because not in datastore yet
    }
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::registration_add_update(SPtr_Registration& sptr_reg)
{
    if (!params_.db_storage_enabled_) return;

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(sptr_reg.get());
    if (api_reg == nullptr) {
        log_err("Error casting registration as an APIRegistration");
    } else {
        api_reg->lock()->lock("BundleDaemonStorage::registration_add_update - check in storage queue");
        if ( ! api_reg->in_storage_queue() ) {
            api_reg->set_in_storage_queue(true);

            StoreRegistrationUpdateEvent* event_to_post;
            event_to_post = new StoreRegistrationUpdateEvent(sptr_reg);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);
        } else {
            ++stats_.regs_combinedupdates_;
        }
        api_reg->lock()->unlock();
    }
}
    
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_bundle_update(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StoreBundleUpdateEvent* event = nullptr;
    event = dynamic_cast<StoreBundleUpdateEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreBundleUpdateEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    ASSERT(nullptr == event->processed_notifier_);
    add_update_bundles_->insert(DeleteBundleMapPair(event->bundleid_, 0));
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_bundle_delete(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StoreBundleDeleteEvent* event = nullptr;
    event = dynamic_cast<StoreBundleDeleteEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreBundleDeleteEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    ASSERT(nullptr == event->processed_notifier_);
    delete_bundles_->insert(DeleteBundleMapPair(event->bundleid_, event->size_and_flag_));
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_pendingacs_update(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StorePendingAcsUpdateEvent* event = nullptr;
    event = dynamic_cast<StorePendingAcsUpdateEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StorePendingAcsUpdateEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    PendingAcs* pacs = event->pacs_;
    PendingAcsInsertResult result;

    pendingacs_lock_.lock("BundleDaemonStorage::handle_store_pendingacs_update");
    result = add_update_pendingacs_->insert(PendingAcsPair(pacs->durable_key(), pacs));
    if ( !result.second ) {
        ++stats_.pacs_combinedupdates_;
    }
    pendingacs_lock_.unlock();
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_pendingacs_delete(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StorePendingAcsDeleteEvent* event = nullptr;
    event = dynamic_cast<StorePendingAcsDeleteEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StorePendingAcsDeleteEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    PendingAcs* pacs = event->pacs_;

    pendingacs_lock_.lock("BundleDaemonStorage::handle_store_pendingacs_delete");
    delete_pendingacs_->insert(PendingAcsPair(pacs->durable_key(), pacs));
    pendingacs_lock_.unlock();
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_registration_update(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StoreRegistrationUpdateEvent* event = nullptr;
    event = dynamic_cast<StoreRegistrationUpdateEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreRegistrationUpdateEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    add_update_registrations_->insert(RegMapPair(event->sptr_reg_->durable_key(), event->sptr_reg_));
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_registration_delete(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    StoreRegistrationDeleteEvent* event = nullptr;
    event = dynamic_cast<StoreRegistrationDeleteEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreRegistrationDeleteEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    registration_lock_.lock("BundleDaemonStorage::handle_store_registration_delete");
    delete_registrations_->insert(RegMapPair(event->regid_, event->sptr_reg_));
    registration_lock_.unlock();
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_link_update(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    if (! BundleDaemon::params_.persistent_links_)
    {
        return;
    }

    StoreLinkUpdateEvent* event = nullptr;
    event = dynamic_cast<StoreLinkUpdateEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreLinkUpdateEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    Link* link = event->linkref_.object();
    LinkMapInsertResult result;


    // Do not store Restage CLs in persistent storage [at least for now]
    if (link->cl_name_str().compare("restage") == 0)
    {
      return;
    }


    link_lock_.lock("BundleDaemonStorage::handle_store_link_update");
    result = add_update_links_->insert(LinkMapPair(link->durable_key(), link));
    if ( !result.second ) {
        ++stats_.links_combinedupdates_;
    }
    link_lock_.unlock();
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_store_link_delete(SPtr_BundleEvent& sptr_event)
{
    if (!params_.db_storage_enabled_) return;

    if (! BundleDaemon::params_.persistent_links_)
    {
        return;
    }

    StoreLinkDeleteEvent* event = nullptr;
    event = dynamic_cast<StoreLinkDeleteEvent*>(sptr_event.get());
    if (event == nullptr) {
        log_err("Error casting event type %s as a StoreLinkDeleteEvent", 
                event_to_str(sptr_event->type_));
        return;
    }

    std::string key = event->linkname_;

    link_lock_.lock("BundleDaemonStorage::handle_store_link_delete");
    delete_links_->insert(LinkMapPair(key, nullptr));
    link_lock_.unlock();
}
    
//----------------------------------------------------------------------
void
BundleDaemonStorage::update_database()
{
    update_database_registrations();
    update_database_bundles();
    update_database_links();
    update_database_pendingacs();
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::update_database_registrations()
{
    if (!params_.db_storage_enabled_) {
        delete_registrations_->clear();
        add_update_registrations_->clear();
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    bool first_trans = true;
    int result;

    RegistrationStore* rs = RegistrationStore::instance();

    registration_lock_.lock("BundleDaemonStorage::update_database_registrations");

    // process the registrations to be deleted first
    u_int32_t regid;
    APIRegistration* apireg;
    SPtr_Registration sptr_reg;

    RegMapIterator itr = delete_registrations_->begin();
    while (itr != delete_registrations_->end()) {
        regid = itr->first;
        sptr_reg = itr->second;

        apireg = dynamic_cast<APIRegistration*>(sptr_reg.get());
        if (apireg == nullptr) {
            log_err("Error casting registration as an APIRegistration");
        } else {
            if (first_trans) {
                first_trans = false;
                store->begin_transaction();
            }

            GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_registrations");

            if ( rs->del(regid)) {
                ++stats_.regs_deleted_;
                --stats_.regs_in_db_;
            } else {
                log_err("error removing registration %d: no matching registration",
                        regid);
            }

            GlobalStore::instance()->unlock_db_access();

            // remove this registration from the add_update list if it is there
            result = add_update_registrations_->erase(regid);
            if (1 == result) {
                ++stats_.regs_delupdates_;
            }
        }

        // remove this entry from the list
        delete_registrations_->erase(itr);

        // get the next entry to delete
        itr = delete_registrations_->begin();
    }

    // process the registrations to be added or updated
    itr = add_update_registrations_->begin();
    while (itr != add_update_registrations_->end()) {
        sptr_reg = itr->second;

        apireg = dynamic_cast<APIRegistration*>(sptr_reg.get());
        if (apireg == nullptr) {
            log_err("Error casting registration as an APIRegistration");
        } else {
            if (first_trans) {
                first_trans = false;
                store->begin_transaction();
            }

            apireg->lock()->lock("BundleDaemonStorage::update_database_registrations");
            apireg->set_in_storage_queue(false);

            GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_registrations");

            if (apireg->in_datastore()) {
                if (! rs->update(apireg)) {
                    log_crit("error updating registration %d in data store!!",
                             itr->first);
                }
	        ++stats_.regs_updated_;
            } else {
                if (! rs->add(apireg)) {
                    log_crit("error adding registration %d to data store!!",
                             itr->first);
                }
                apireg->set_in_datastore(true);
	            ++stats_.regs_added_;
	            ++stats_.regs_in_db_;
            }

            GlobalStore::instance()->unlock_db_access();

            apireg->lock()->unlock();
        }

        // remove this entry from the list
        add_update_registrations_->erase(itr);

        // get the next entry to process
        itr = add_update_registrations_->begin();
    }

    registration_lock_.unlock();

    if (!first_trans) {
        store->end_transaction();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::update_database_bundles()
{
    if (!params_.db_storage_enabled_) {
        delete_bundles_->clear();
        add_update_bundles_->clear();
        return;
    }

    int result;

    oasys::DurableStore *store = oasys::DurableStore::instance();
    bool first_trans = true;

    BundleStore* bstore = BundleStore::instance();

    // process the bundles to be deleted first since that might save us the 
    // time of having to add/update some bundles
    bundleid_t bundleid;
    u_int64_t durable_size;
    bool in_db;
    DeleteBundleMap::iterator bdel_itr;

    BundleRef bref("BundleDaemonStorage::update_database_bundles temporary");
    DeleteBundleMap::iterator itr;
    Bundle* bundle;

    bdel_itr = delete_bundles_->begin();
    while (bdel_itr != delete_bundles_->end()) {
        bundleid = bdel_itr->first;

        if (bdel_itr->second < 0) {
            in_db = false;
            durable_size = -bdel_itr->second;
        } else {
            in_db = true;
            durable_size = bdel_itr->second;
        }

        if (first_trans) {
            first_trans = false;
            store->begin_transaction();
        }

        if (in_db) {
            GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_bundles - delete");
            if (bstore->del(bundleid, durable_size)) {
                ++stats_.bundles_deleted_;
                --stats_.bundles_in_db_;
            } else if (in_db) {
                log_warn("error removing bundle %" PRIbid " from data store",
                         bundleid);
            }

            GlobalStore::instance()->unlock_db_access();
        } else {
            // release the memory that was reserved
            BundleStore::instance()->release_payload_space(durable_size);
        }


        result = add_update_bundles_->erase(bundleid);
        if (1 == result) {
            ++stats_.bundles_delupdates_;
        }

        ++bdel_itr;
    }
    delete_bundles_->clear();

    // process the bundles to be added or updated
    itr = add_update_bundles_->begin();
    while (itr != add_update_bundles_->end()) {
        bundleid = itr->first;
        bref = all_bundles_->find_for_storage(bundleid);
        bundle = bref.object();

        if (bundle != nullptr ) {
            if (first_trans) {
                first_trans = false;
                store->begin_transaction();
            }

            bundle->lock()->lock("BundleDaemonStorage::update_database_bundles #1");

            if (!bundle->is_freed()) {
                if (bundle->in_datastore()) {
                    if (!bundle->deleting()) {

                        bundle->lock()->unlock();

                        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_bundles - update");
                        bundle->lock()->lock("BundleDaemonStorage::update_database_bundles #2");
                        bundle->set_in_storage_queue(false);

                        if (! bstore->update(bundle)) {
                            log_crit("error updating bundle %" PRIbid " in data store!!",
                                     bundle->durable_key());
                        }

                        GlobalStore::instance()->unlock_db_access();

                        ++stats_.bundles_updated_;
                    } else {
                        ++stats_.bundles_delbeforeupdates_; // # updates not processed because deleting
                    }
                } else {
                    if (!bundle->deleting()) {
                        // flush the payload to disk
                        bundle->lock()->unlock();

                        if (params_.db_force_sync_to_disk_) {
#ifdef BDSTORAGE_LOG_DEBUG_ENABLED
                            sync_payload_timer_.get_time();
#endif

                            bundle->mutable_payload()->sync_payload();

#ifdef BDSTORAGE_LOG_DEBUG_ENABLED
                            if (sync_payload_timer_.elapsed_ms() > 20) {
                                log_warn("Sync payload took %u ms", sync_payload_timer_.elapsed_ms());
                            }
#endif
                        }

                        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_bundles - add");

                        //get lock after have db access
                        bundle->lock()->lock("BundleDaemonStorage::update_database_bundles #3");
                        bundle->set_in_storage_queue(false);

                        store->make_transaction_durable();
                        if (! bstore->add(bundle)) {
                            log_crit("error adding bundle %" PRIbid " to data store!!",
                                     bundle->durable_key());
                        }

                        GlobalStore::instance()->unlock_db_access();

                        bundle->set_in_datastore(true);

                        ++stats_.bundles_added_;
                        ++stats_.bundles_in_db_;
                    } else {
                        ++stats_.bundles_delbeforeadd_;     // # adds not processed because deleting
                    }
                }
            } else {
                ++stats_.bundles_combinedupdates_;
            }

            bundle->lock()->unlock();

            bref.release();
        } else {
            ++stats_.bundles_delupdates_;
        }

        ++itr;
    }
    add_update_bundles_->clear();

    if (!first_trans) {
        store->end_transaction();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::update_database_links()
{
    if (!params_.db_storage_enabled_) {
        delete_links_->clear();
        add_update_links_->clear();
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    bool first_trans = true;
    int result;

    LinkStore* ls = LinkStore::instance();

    link_lock_.lock("BundleDaemonStorage::update_database_links");

    // process the links to be deleted first
    std::string key;
    Link* link;

    LinkMapIterator itr = delete_links_->begin();
    while (itr != delete_links_->end()) {
        key = itr->first;

        if (first_trans) {
            first_trans = false;
            store->begin_transaction();
        }

        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_links");

        if (ls->del(key)) {
            ++stats_.links_deleted_;
            --stats_.links_in_db_;
        } else {
            log_err("error removing link %s: no matching link",
                    key.c_str());
        }

        GlobalStore::instance()->unlock_db_access();

        // remove this registration from the add_update list if it is there
        result = add_update_links_->erase(key);
        if (1 == result) {
            ++stats_.links_delupdates_;
        }

        // remove this entry from the list
        delete_links_->erase(itr);

        // get the next entry to delete
        itr = delete_links_->begin();
    }

    // process the registrations to be added or updated
    itr = add_update_links_->begin();
    while (itr != add_update_links_->end()) {
        link = itr->second;

        if (first_trans) {
            first_trans = false;
            store->begin_transaction();
        }

        link->lock()->lock("BundleDaemonStorage::update_database_links");

        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_links");

        if (link->in_datastore()) {
            if (! ls->update(link)) {
                log_crit("error updating link %s in data store!!",
                         itr->first.c_str());
            }
	    ++stats_.links_updated_;
        } else {
            if (! ls->add(link)) {
                log_crit("error adding link %s to data store!!",
                         itr->first.c_str());
            }
            link->set_in_datastore(true);
	    ++stats_.links_added_;
	    ++stats_.links_in_db_;
        }

        GlobalStore::instance()->unlock_db_access();

        link->lock()->unlock();

        // remove this entry from the list
        add_update_links_->erase(itr);

        // get the next entry to process
        itr = add_update_links_->begin();
    }

    link_lock_.unlock();

    if (!first_trans) {
        store->end_transaction();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::update_database_pendingacs()
{
    if (!params_.db_storage_enabled_) {
        delete_pendingacs_->clear();
        add_update_pendingacs_->clear();
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    bool first_trans = true;
    int result;

    PendingAcsStore* pastore = PendingAcsStore::instance();

    pendingacs_lock_.lock("BundleDaemonStorage::update_database_pendingacs");

    // process the pendingacs to be deleted first
    PendingAcs* pacs;

    PendingAcsIterator itr = delete_pendingacs_->begin();
    while (itr != delete_pendingacs_->end()) {
        pacs = itr->second;

        if (first_trans) {
            first_trans = false;
            store->begin_transaction();
        }

        pacs->lock().lock("BundleDaemonStorage::update_database_pendingacs (delete)");

        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_pendingacs");

        if (pastore->del(pacs->durable_key())) {
            ++stats_.pacs_deleted_;
            --stats_.pacs_in_db_;
        } else {
            log_err("error removing Pending ACS %s: no matching key",
                    pacs->durable_key().c_str());
        }

        GlobalStore::instance()->unlock_db_access();

        // remove this entry from the add_update list if it is there
        result = add_update_pendingacs_->erase(pacs->durable_key());
        if (1 == result) {
            ++stats_.pacs_delupdates_;
        }

        // remove this entry from the list
        delete_pendingacs_->erase(itr);

        // delete the Pending ACS itself
        delete pacs;

        // get the next entry to delete
        itr = delete_pendingacs_->begin();
    }

    // process the pendingacs to be added or updated
    itr = add_update_pendingacs_->begin();
    while (itr != add_update_pendingacs_->end()) {
        pacs = itr->second;

        if (first_trans) {
            first_trans = false;
            store->begin_transaction();
        }

        pacs->lock().lock("BundleDaemonStorage::update_database_pendingacs");

        GlobalStore::instance()->lock_db_access("BundleDaemonStorage::update_database_pendingacs");

        if (pacs->in_datastore()) {
            if (! pastore->update(pacs)) {
                log_crit("error updating Pending ACS %s in data store!!",
                         itr->first.c_str());
            }
	    ++stats_.pacs_updated_;
        } else {
            if (! pastore->add(pacs)) {
                log_crit("error adding Pending ACS %s to data store!!",
                         itr->first.c_str());
            }
            pacs->set_in_datastore(true);
	    ++stats_.pacs_added_;
	    ++stats_.pacs_in_db_;
        }

        GlobalStore::instance()->unlock_db_access();

        pacs->lock().unlock();

        // remove this entry from the list
        add_update_pendingacs_->erase(itr);

        // get the next entry to process
        itr = add_update_pendingacs_->begin();
    }

    pendingacs_lock_.unlock();

    if (!first_trans) {
        store->end_transaction();
    }
}

    
//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_event(SPtr_BundleEvent& sptr_event)
{
    handle_event(sptr_event, true);
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::handle_event(SPtr_BundleEvent& sptr_event, bool closeTransaction)
{
    dispatch_event(sptr_event);
    
    if (closeTransaction) {
        oasys::DurableStore* ds = oasys::DurableStore::instance();
        if ( ds->is_transaction_open() ) {
            ds->end_transaction();
        }
    }

    stats_.events_processed_++;

    if (sptr_event->processed_notifier_) {
        sptr_event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonStorage::run()
{
    char threadname[16] = "BD-Storage";
    pthread_setname_np(pthread_self(), threadname);

    SPtr_BundleEvent sptr_event;

    all_bundles_ = daemon_->all_bundles();

    // XXX/dz incorporate log removal and transaction checkpoint into oasys?
    // if using Berkeley DB then set the initial log auto removal mode
    // and then monitor for changes
    bool db_log_auto_removal = params_.db_log_auto_removal_;
    oasys::DurableStoreImpl *impl = oasys::DurableStore::instance()->impl();
    DB_ENV* berkeley_db = static_cast<DB_ENV*>(impl->get_underlying());

    int status = 0;
    (void) status; // prevent unused message if debug logging is disabled

    if (berkeley_db) {
        if (berkeley_db && db_log_auto_removal) {
#if defined(DB_LOG_AUTOREMOVE)
            status = berkeley_db->set_flags(berkeley_db, DB_LOG_AUTOREMOVE, 1);
#elif defined(DB_LOG_AUTO_REMOVE)
            status = berkeley_db->log_set_config(berkeley_db, DB_LOG_AUTO_REMOVE, 1);
#else
            log_error("Unable to set Berkeley DB Log Auto-Removal");
#endif 
        }
    }
    
    bool ok; 

    oasys::Time log_removal_timer;
    log_removal_timer.get_time();

    oasys::Time now;
    last_db_update_.get_time();

    while (1) {
        if (should_stop()) {
            if ( me_eventq_.size() > 0 ) {
                log_info("BundleDaemonStorage: stopping - event queue size: %zu",
                          me_eventq_.size());
            }
            break;
        }

        
        if (last_db_update_.elapsed_ms() >= params_.db_storage_ms_interval_) {
            update_database();
            last_db_update_.get_time();

        }

        // update the log removal mode if it has changed (& is Berkeley DB)
        if (berkeley_db && 
            (db_log_auto_removal != params_.db_log_auto_removal_)) {
            db_log_auto_removal = params_.db_log_auto_removal_;
            int onoff = db_log_auto_removal ? 1 : 0;

#if defined(DB_LOG_AUTOREMOVE)
            berkeley_db->set_flags(berkeley_db, DB_LOG_AUTOREMOVE, onoff);
#elif defined(DB_LOG_AUTO_REMOVE)
            berkeley_db->log_set_config(berkeley_db, DB_LOG_AUTO_REMOVE, onoff);
#else
            log_error("Unable to set Berkeley DB Log Auto-Removal state");
#endif 
        }

        if (berkeley_db && db_log_auto_removal) {
            if (log_removal_timer.elapsed_ms() > 60000) {
                status = berkeley_db->txn_checkpoint(berkeley_db, 10240, 10, 0);
                log_removal_timer.get_time();
            }
        }

        if (me_eventq_.size() > 0) {
            ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);

            // handle the event
            handle_event(sptr_event);

            // clean up the event
            sptr_event.reset();
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }

    run_loop_terminated_ = true;
    while (!last_commit_completed_) {
        usleep(100);
    }
}


//----------------------------------------------------------------------
void
BundleDaemonStorage::commit_all_updates()
{
    set_should_stop();

    while (!run_loop_terminated_) {
        usleep(100);
    }

    if (!last_commit_completed_) {
        log_always("commit_all_updates - event queue size = %zu", me_eventq_.size());

        oasys::Time timer;
        timer.get_time();

        // clean out the event queue
        SPtr_BundleEvent sptr_event;

        while (me_eventq_.size() > 0) {
            bool ok = me_eventq_.try_pop(&sptr_event);
            ASSERT(ok);

            // handle the event
            handle_event(sptr_event);

            // clean up the event
            sptr_event.reset();

            if (timer.elapsed_ms() > 1000) {
                log_always("Committing storage to disk... pending events: %zu", me_eventq_.size());
                timer.get_time();
            }
        }

        // one final update to the databse
        update_database();

        last_commit_completed_ = true;
    }
}


} // namespace dtn
