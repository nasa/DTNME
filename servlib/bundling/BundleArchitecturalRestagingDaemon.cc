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


#ifdef BARD_ENABLED

#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/storage/DurableStore.h>

#include "BARDForceRestageThread.h"
#include "BundleArchitecturalRestagingDaemon.h"
#include "BundleDaemon.h"
#include "FormatUtils.h"

#include "naming/DTNScheme.h"
#include "naming/EndpointID.h"
#include "naming/IMCScheme.h"
#include "naming/IPNScheme.h"
#include "storage/BARDQuotaStore.h"
#include "storage/BundleStore.h"

namespace dtn {

BundleArchitecturalRestagingDaemon::Params::Params()
{}

BundleArchitecturalRestagingDaemon::Params BundleArchitecturalRestagingDaemon::params_;



#define RESTAGE_BY_SRC  true
#define RESTAGE_BY_DST  false


const char* BARD_INTERNAL_STORAGE_MSG = "DTNME BARD: Internal Storage Quota for ";
const char* BARD_EXTERNAL_STORAGE_MSG = "DTNME BARD: External Storage Quota for ";
const char* BARD_LOW_WATERMARK_MSG = " has reached Low Watermark of ";
const char* BARD_HIGH_WATERMARK_MSG = " has reached High Watermark of ";
const char* BARD_QUOTA_REACHED_MSG = " has reached full capacity of ";
const char* BARD_BELOW_LOW_WATERMARK_MSG = " has fallen back below Low Watermark of ";


//----------------------------------------------------------------------
BundleArchitecturalRestagingDaemon::BundleArchitecturalRestagingDaemon()
    : Logger("BundleArchitecturalRestagingDaemon", "/dtn/bard"),
      Thread("BundleArchitecturalRestagingDaemon")
{
    eventq_ = new oasys::MsgQueue<std::string*>(logpath_);
    eventq_->notify_when_empty();
}

//----------------------------------------------------------------------
BundleArchitecturalRestagingDaemon::~BundleArchitecturalRestagingDaemon()
{
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::shutdown()
{
    set_should_stop();
    while (! is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::post_event(std::string* event, bool at_back)
{
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::update_datastore(BARDNodeStorageUsage* usage_ptr)
{
    // update the datastore with the changes to the BARD Quota
    BARDQuotaStore* quota_store = BARDQuotaStore::instance();

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (usage_ptr->quota_in_datastore()) {
        if (! quota_store->update(usage_ptr)) {
            log_crit("error updating BARD Quota %s in data store!!",
                     usage_ptr->durable_key().c_str());
        }
    } else {
        if (! quota_store->add(usage_ptr)) {
            log_crit("error adding BARD Quota %s to data store!!",
                     usage_ptr->durable_key().c_str());
        }
        usage_ptr->set_quota_in_datastore(true);
    }
    store->end_transaction();

    usage_ptr->set_quota_modified(false);
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::delete_from_datastore(BARDNodeStorageUsage* usage_ptr)
{
    // update the datastore with the changes to the BARD Quota
    BARDQuotaStore* quota_store = BARDQuotaStore::instance();

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (usage_ptr->quota_in_datastore()) {
        if (! quota_store->del(usage_ptr->durable_key())) {
            log_crit("error deleting BARD Quota %s from  data store!!",
                     usage_ptr->durable_key().c_str());
        }
    }
    usage_ptr->set_quota_in_datastore(false);

    store->end_transaction();
}



    
//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::load_saved_quotas()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    BARDNodeStorageUsage* usage_obj;
    BARDQuotaStore* quota_store = BARDQuotaStore::instance();
    BARDQuotaStore::iterator* qstore_iter = quota_store->new_iterator();

    BARDNodeStorageUsageMap::iterator quota_map_iter;


    log_info("loading BARD Quotas from data store");

    int status;

    for (status = qstore_iter->begin(); qstore_iter->more(); status = qstore_iter->next()) {
        if ( oasys::DS_NOTFOUND == status ) {
            log_warn("received DS_NOTFOUND from data store - BARD Quota <%s>",
                    qstore_iter->cur_val().c_str());
            break;
        } else if ( oasys::DS_ERR == status ) {
            log_err("error loading BARD Quota <%s> from data store",
                    qstore_iter->cur_val().c_str());
            break;
        }

        usage_obj = quota_store->get(qstore_iter->cur_val());
        SPtr_BARDNodeStorageUsage sptr_usage(usage_obj);
        
        if (sptr_usage == NULL) {
            log_err("error loading BARD Quota <%s> from data store",
                    qstore_iter->cur_val().c_str());
            continue;
        }


        // see if this quota is already in the list created from the startup config fle
        quota_map_iter = quota_map_.find(sptr_usage->key());

        if (quota_map_iter != quota_map_.end()) {
            // copy quota values from the datastore over the ones in the BARD quota and usage lists
            // both lists share the same object so only need to update ths one in the quota_map_
            ASSERT(false == quota_map_iter->second->quota_in_datastore());

            quota_map_iter->second->set_quota_internal_bundles(sptr_usage->quota_internal_bundles());
            quota_map_iter->second->set_quota_internal_bytes(sptr_usage->quota_internal_bytes());
            quota_map_iter->second->set_quota_external_bundles(sptr_usage->quota_external_bundles());
            quota_map_iter->second->set_quota_external_bytes(sptr_usage->quota_external_bytes());
            quota_map_iter->second->set_quota_refuse_bundle(sptr_usage->quota_refuse_bundle());
            quota_map_iter->second->set_quota_auto_reload(sptr_usage->quota_auto_reload());
            std::string link_name = sptr_usage->quota_restage_link_name();
            quota_map_iter->second->set_quota_restage_link_name(link_name);

            quota_map_iter->second->set_quota_modified(false); // not modified as far as database is concerned
            quota_map_iter->second->set_quota_in_datastore(true);

        } else {
            quota_map_[sptr_usage->key()] = sptr_usage;
            usage_map_[sptr_usage->key()] = sptr_usage;

            sptr_usage->set_quota_modified(false); // not modified as far as database is concerned
            sptr_usage->set_quota_in_datastore(true);
        }

    }

    delete qstore_iter;

    // Now spin through the quota_map_ list and add any new ones to persistent storage
    // see if this quota is already in the list created from the startup config fle
    quota_map_iter = quota_map_.begin();

    while (quota_map_iter != quota_map_.end()) {
        if ( ! quota_map_iter->second->quota_in_datastore() ) {
            update_datastore(quota_map_iter->second.get());
        }

        ++quota_map_iter;
    }
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle(Bundle* bundle)
{
    bool result = true;  // assume bundle can be accepted into internal storage

    // check both variants without reserving internal space to see if either quota is already exceeded
    // (to prevent secondary quota from kicking in due to reserved space and splitting restage location)
    bool reserve_space = false;
    bool result_by_dst_internal = query_accept_bundle_by_destination_into_internal_storage(bundle, reserve_space);
    bool result_by_src_internal = query_accept_bundle_by_source_into_internal_storage(bundle, reserve_space);

    // if either variant is already over quota then handle it
    if ( ! result_by_dst_internal )
    {
        // cannot accept bundle into internal storage - can it go into external storage?
        result = query_accept_bundle_by_destination_into_external_storage(bundle);
    }
    else if ( ! result_by_src_internal )
    {
        // cannot accept bundle into internal storage - can it go into external storage?
        result = query_accept_bundle_by_source_into_external_storage(bundle);
    }
    else
    {
        // now check again and reserve internal space if still okay to accept
        result_by_dst_internal = query_accept_bundle_by_destination_into_internal_storage(bundle, reserve_space);
        result_by_src_internal = query_accept_bundle_by_source_into_internal_storage(bundle, reserve_space);

        // if either variant is now over quota then handle it
        if ( ! result_by_dst_internal )
        {
            // cannot accept bundle into internal storage - can it go into external storage?
            result = query_accept_bundle_by_destination_into_external_storage(bundle);
        }
        else if ( ! result_by_src_internal )
        {
            // cannot accept bundle into internal storage - can it go into external storage?
            result = query_accept_bundle_by_source_into_external_storage(bundle);
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_after_failed_restage(Bundle* bundle)
{
    bool result = false;  // assume bundle cannot be accepted

    if (bundle->all_restage_attempts_failed()) {
        log_crit("BARD.%s: dzdebug - called after all restage attempts failed", __func__);
        return result;
    }

    // is there another [pooled] restage CL that can accept this bundle?
    std::string old_restage_link_name = bundle->bard_restage_link_name();

    bundle->set_last_restage_attempt_failed();
    bundle->clear_bard_restage_link_name();
    bundle->add_failed_restage_link_name(old_restage_link_name);

    oasys::ScopeLock scoplok(&lock_, __func__);

    // look up the link name that just failed to see if it is pooled
    bool pooled_link = true;


    // check the preferred restaging link
    SPtr_RestageCLStatus sptr_clstatus_preferred;
    RestageCLIterator iter = restagecl_map_.find(old_restage_link_name);

    if (iter != restagecl_map_.end()) {
        sptr_clstatus_preferred = iter->second;

        oasys::ScopeLock clstatus_scoplock(sptr_clstatus_preferred->lock(), __func__);

        pooled_link = sptr_clstatus_preferred->part_of_pool();

    } else {
        pooled_link = params_.treat_unknown_link_as_pooled_;
    }
    

    if (!result && pooled_link) {
        // preferred link not available - try to find an alternate location
        std::string next_link_name;

        SPtr_RestageCLStatus sptr_clstatus;
        iter = restagecl_map_.begin();

        while  (!result && (iter != restagecl_map_.end())) {
            sptr_clstatus = iter->second;

            oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);

            if (sptr_clstatus->part_of_pool()) {
                result = (sptr_clstatus->cl_state() == RESTAGE_CL_STATE_ONLINE);

                if (result) {
                    next_link_name = sptr_clstatus->restage_link_name();

                    if (bundle->find_failed_restage_link_name(next_link_name)) {
                        result = false;  // already tried this one
                    } else {
                        bundle->set_bard_restage_link_name(next_link_name);
                    }
                }
            }

            ++iter;
        }

    }


    if (!result) {
        // tried all possible restage links - flag bundle as failed to avoid an infinite loop
        bundle->set_all_restage_attempts_failed();
    }
     

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_internal_storage_only(Bundle* bundle)
{
    // check both variants without reserving internal space to see if either quota is already exceeded
    // (to prevent secondary quota from kicking in due to reserved space and splitting restage location)
    bool reserve_space = true;
    bool result_by_dst_internal = query_accept_bundle_by_destination_into_internal_storage(bundle, reserve_space);
    bool result_by_src_internal = query_accept_bundle_by_source_into_internal_storage(bundle, reserve_space);

    bool result = result_by_dst_internal && result_by_src_internal;

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_by_destination_into_internal_storage(Bundle* bundle, bool reserve_space)
{
    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_DST, bundle->dest());

    return query_accept_bundle_into_internal_storage(key, RESTAGE_BY_DST, bundle, reserve_space);
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_by_source_into_internal_storage(Bundle* bundle, bool reserve_space)
{
    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_SRC, bundle->source());

    return query_accept_bundle_into_internal_storage(key, RESTAGE_BY_SRC, bundle, reserve_space);
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_by_destination_into_external_storage(Bundle* bundle)
{
    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_DST, bundle->dest());

    return query_accept_bundle_into_external_storage(key, RESTAGE_BY_DST, bundle);
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_by_source_into_external_storage(Bundle* bundle)
{
    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_SRC, bundle->source());

    return query_accept_bundle_into_external_storage(key, RESTAGE_BY_SRC, bundle);
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_into_internal_storage(std::string& key, bool restage_by_src, 
                                                                              Bundle* bundle, bool reserve_space)
{
    bool okay_to_accept = true;

    size_t payload_len = internal_storage_block_size_ * 
                         ((bundle->payload().length() + internal_storage_block_size_) / internal_storage_block_size_);

    oasys::ScopeLock scoplok(&lock_, __func__);

    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        // there is a quota entry for this node
        SPtr_BARDNodeStorageUsage sptr_usage;
        sptr_usage = iter->second;


        if (sptr_usage->has_quota()) {

            // can the bundle be accepted based on bundle count?
            if (sptr_usage->quota_internal_bundles() > 0) {
                size_t committed_total_bundles = sptr_usage->committed_internal_bundles();

                if (bundle->bard_quota_reserved(restage_by_src) > 0) {
                    committed_total_bundles -= 1;
                }


                if ((committed_total_bundles + 1) > sptr_usage->quota_internal_bundles()) {
                    // fully committed - cannot accept another one
                    okay_to_accept = false;
                }
            }
               
            // can the bundle be accepted based on bytes in storage?
            if (okay_to_accept && sptr_usage->quota_internal_bytes() > 0) {
                size_t committed_total_bytes = sptr_usage->committed_internal_bytes();

                // committed total bytes may include space already reserved for this bundle so subtract it out if so
                committed_total_bytes -= bundle->bard_quota_reserved(restage_by_src);


                if ((committed_total_bytes + payload_len) > sptr_usage->quota_internal_bytes()) {
                    // fully committed - cannot accept another one
                    okay_to_accept = false;
                }
            }



            if (okay_to_accept) {
                // have any thresholds been triggered?
                size_t max_percent = max_internal_committed_quota_percent(sptr_usage.get());

                if (max_percent >= params_.internal_low_watermark_) {
                    if (max_percent >= params_.internal_high_watermark_) {
                        if (!sptr_usage->internal_threshold_high_reached_) {
                            sptr_usage->internal_threshold_high_reached_  = true;

                            generate_email_notification(sptr_usage, 
                                                        BARD_INTERNAL_STORAGE_MSG,
                                                        BARD_HIGH_WATERMARK_MSG,
                                                        params_.internal_high_watermark_,
                                                        sptr_usage->email_sent_internal_high_watermark_);
                        }
                    } else if (!sptr_usage->internal_threshold_low_reached_) {
                        sptr_usage->internal_threshold_low_reached_  = true;

                        generate_email_notification(sptr_usage, 
                                                    BARD_INTERNAL_STORAGE_MSG,
                                                    BARD_LOW_WATERMARK_MSG,
                                                    params_.internal_low_watermark_,
                                                    sptr_usage->email_sent_internal_low_watermark_);
                    }
                 } else {
                    // if dropped below the low watermark then reset the reached flags
                    // and send an email notification
                    if (sptr_usage->internal_threshold_low_reached_) {
                        sptr_usage->internal_quota_reached_ = false;
                        sptr_usage->internal_threshold_low_reached_  = false;
                        sptr_usage->internal_threshold_high_reached_ = false;

                        generate_email_notification(sptr_usage, 
                                                    BARD_INTERNAL_STORAGE_MSG,
                                                    BARD_BELOW_LOW_WATERMARK_MSG,
                                                    params_.internal_low_watermark_,
                                                    sptr_usage->email_sent_internal_below_low_watermark_);
                    }
                 }
             }
        }

        // only reserve space if can accept into internal storage
        if (okay_to_accept & reserve_space) {
            if (bundle->bard_quota_reserved(restage_by_src) != payload_len) {
                sptr_usage->reserved_internal_bundles_ += 1;
                sptr_usage->reserved_internal_bytes_ += payload_len;

                bundle->set_bard_quota_reserved(restage_by_src, payload_len);
            }
        }

    } else {
        // no quota record but always increment the reserved values in a usage record
        // just in case a quota does get added
        iter = usage_map_.find(key);

        SPtr_BARDNodeStorageUsage sptr_usage;

        if (iter == usage_map_.end()) {
            bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
            std::string nodename;

            if (restage_by_src) {
                endpoint_to_key_components(bundle->source(), scheme, nodename);
                sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_SRC, scheme, nodename);
            } else {
                endpoint_to_key_components(bundle->dest(), scheme, nodename);
                sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_DST, scheme, nodename);
            }

            usage_map_[key] = sptr_usage;
        } else {
            sptr_usage = iter->second;
        }

        // -- must allow for multiple attempts to accept the same bundle 
        //    from some convergence layers - may have been rejected by tehr EID
        //    (bundle will track if it was previously reserved)
        if (bundle->bard_quota_reserved(restage_by_src) != payload_len) {
            sptr_usage->reserved_internal_bundles_ += 1;
            sptr_usage->reserved_internal_bytes_ += payload_len;

            bundle->set_bard_quota_reserved(restage_by_src, payload_len);
        }
    }

    return okay_to_accept;
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_bundle_into_external_storage(std::string& key, bool restage_by_src, Bundle* bundle)
{
    // bundle has already been rejected for internal storage...
    bool okay_to_accept = false;

    // assume that external storage uses the same block size as internal 
    // (it will adjust when written if needed)
    size_t payload_len = internal_storage_block_size_ * 
                         ((bundle->payload().length() + internal_storage_block_size_) / internal_storage_block_size_);

    oasys::ScopeLock scoplok(&lock_, __func__);

    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        // there should always be a quota entry for this node if it got to here!
        SPtr_BARDNodeStorageUsage sptr_usage;
        sptr_usage = iter->second;


        if (sptr_usage->has_quota()) {
            // check external quotas if can't add it to internal storage

            // attempting to restage bundle so internal quota was reached 
            // - send notification if not already sent
            if (!sptr_usage->internal_quota_reached_) {
                sptr_usage->internal_quota_reached_ = true;

                generate_email_notification(sptr_usage, 
                                            BARD_INTERNAL_STORAGE_MSG,
                                            BARD_QUOTA_REACHED_MSG,
                                            100,
                                            sptr_usage->email_sent_internal_quota_reached_);
            }

            if (!sptr_usage->quota_refuse_bundle()) {
                // can it be accepted into external storage?
                // assume true to start with
                okay_to_accept = true;

                // can the bundle be accepted based on bundle count?
                if (sptr_usage->quota_external_bundles() > 0) {
                    size_t committed_total_bundles = 0;
                    if (rescanning_) {
                        committed_total_bundles = sptr_usage->last_committed_external_bundles();
                    } else {
                        committed_total_bundles = sptr_usage->committed_external_bundles();
                    }

                    if (bundle->bard_extquota_reserved(restage_by_src) > 0) {
                        committed_total_bundles -= 1;
                    }


                    if ((committed_total_bundles + 1) > sptr_usage->quota_external_bundles()) {
                        // fully committed - cannot accept another one
                        okay_to_accept = false;
                    }
                }
               
                // can the bundle be accepted based on bytes in storage?
                if (okay_to_accept && sptr_usage->quota_external_bytes() > 0) {
                    size_t committed_total_bytes = 0;
                    if (rescanning_) {
                        committed_total_bytes = sptr_usage->last_committed_external_bytes();
                    } else {
                        committed_total_bytes = sptr_usage->committed_external_bytes();
                    }

                    // committed total bytes may include space already reserved for this bundle so subtract it out if so
                    committed_total_bytes -= bundle->bard_extquota_reserved(restage_by_src);

                    if ((committed_total_bytes + payload_len) > sptr_usage->quota_external_bytes()) {
                        // fully committed - cannot accept another one
                        okay_to_accept = false;
                    }
                }

                std::string link_name = sptr_usage->quota_restage_link_name();

                if (okay_to_accept) {
                    // theoretically okay to accept but is the external storage in a good state?

                    // note that link_name may change to one of the pooled links if
                    // the preferred link_name is not in a good state
                    okay_to_accept = find_restage_link_in_good_state(link_name);
                }

                if (okay_to_accept) {
                    // have any thresholds been triggered?
                    size_t max_percent = max_external_committed_quota_percent(sptr_usage.get());

                    if (max_percent >= params_.external_low_watermark_) {
                        if (max_percent >= params_.external_high_watermark_) {
                            if (!sptr_usage->external_threshold_high_reached_) {
                                sptr_usage->external_threshold_high_reached_  = true;
    
                                generate_email_notification(sptr_usage, 
                                                            BARD_EXTERNAL_STORAGE_MSG,
                                                            BARD_HIGH_WATERMARK_MSG,
                                                            params_.external_high_watermark_,
                                                            sptr_usage->email_sent_external_high_watermark_);
                            }
                        } else if (!sptr_usage->external_threshold_low_reached_) {
                            sptr_usage->external_threshold_low_reached_  = true;

                            generate_email_notification(sptr_usage, 
                                                        BARD_EXTERNAL_STORAGE_MSG,
                                                        BARD_LOW_WATERMARK_MSG,
                                                        params_.external_low_watermark_,
                                                        sptr_usage->email_sent_external_low_watermark_);
                        }
                     } else {
                        // if dropped below the low watermark then reset the reached flags
                        // and send an email notification
                        if (sptr_usage->external_threshold_low_reached_) {
                            sptr_usage->external_quota_reached_ = false;
                            sptr_usage->external_threshold_low_reached_  = false;
                            sptr_usage->external_threshold_high_reached_ = false;

                            generate_email_notification(sptr_usage, 
                                                        BARD_EXTERNAL_STORAGE_MSG,
                                                        BARD_BELOW_LOW_WATERMARK_MSG,
                                                        params_.external_low_watermark_,
                                                        sptr_usage->email_sent_external_below_low_watermark_);
                        }
                    }

                    // can be restaged - if not previously reserved space the reserve it now
                    // - this allows the BDOutput to try to redirect the bundle to a
                    //   pooled location if the previous restage attempt failed
                    if (bundle->bard_extquota_reserved(restage_by_src) != payload_len) {
                        sptr_usage->reserved_external_bundles_ += 1;
                        sptr_usage->reserved_external_bytes_ += payload_len;

                        bundle->set_bard_extquota_reserved(restage_by_src, payload_len);
                    }
                    bundle->set_bard_restage_by_src(restage_by_src);
                    bundle->set_bard_restage_link_name(link_name);

                } else {
                    if (!sptr_usage->external_quota_reached_) {
                        sptr_usage->external_quota_reached_ = true;

                        generate_email_notification(sptr_usage, 
                                                    BARD_EXTERNAL_STORAGE_MSG,
                                                    BARD_QUOTA_REACHED_MSG,
                                                    100,
                                                    sptr_usage->email_sent_external_quota_reached_);
                    }

                }
            }
        }
    }

    return okay_to_accept;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::find_restage_link_in_good_state(std::string& link_name)
{
    bool result = false;
    bool pooled_link = true;

    ASSERT(lock_.is_locked_by_me());

    // check the preferred restaging link
    SPtr_RestageCLStatus sptr_clstatus_preferred;
    RestageCLIterator iter = restagecl_map_.find(link_name);

    if (iter != restagecl_map_.end()) {
        sptr_clstatus_preferred = iter->second;

        oasys::ScopeLock clstatus_scoplock(sptr_clstatus_preferred->lock(), __func__);

        pooled_link = sptr_clstatus_preferred->part_of_pool();

        result = (sptr_clstatus_preferred->cl_state() == RESTAGE_CL_STATE_ONLINE);
    } else {
        pooled_link = params_.treat_unknown_link_as_pooled_;
    }
    

    if (!result && pooled_link) {
        // preferred link not available - try to find an alternate location

        SPtr_RestageCLStatus sptr_clstatus;
        iter = restagecl_map_.begin();

        while  (!result && (iter != restagecl_map_.end())) {
            sptr_clstatus = iter->second;

            oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);

            if (sptr_clstatus->part_of_pool()) {
                result = (sptr_clstatus->cl_state() == RESTAGE_CL_STATE_ONLINE);

                if (result) {
                    link_name = sptr_clstatus->restage_link_name();
                }
            }

            ++iter;
        }

    }


    if (!result) {
        struct timeval now;
        ::gettimeofday(&now, 0);

        if (sptr_clstatus_preferred) {

            oasys::ScopeLock clstatus_scoplock(sptr_clstatus_preferred->lock(), __func__);

            if ((now.tv_sec - sptr_clstatus_preferred->last_error_msg_time_) >= 600) {
                // only print out an error message every 10 minutes
                log_err("unable to accept bundle into restage storage because link %s is not in a good state: %s",
                        link_name.c_str(), sptr_clstatus_preferred->cl_state_str());

                sptr_clstatus_preferred->last_error_msg_time_ = now.tv_sec;
            }
        } else {
            if (0 == last_not_found_error_msg_link_name_.compare(link_name)) {
                if ((now.tv_sec - last_not_found_error_msg__time_) >= 600) {
                    // only print out an error message every 10 minutes
                    log_err("unable to accept bundle into unknown restage storage link %s",
                            link_name.c_str());

                    last_not_found_error_msg__time_ = now.tv_sec;
                }
            } else {
                log_err("unable to accept bundle into unknown restage storage link %s",
                        link_name.c_str());

                last_not_found_error_msg_link_name_ = link_name;
                last_not_found_error_msg__time_ = now.tv_sec;
            }
        }
    }
     

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::find_restage_link_with_email(std::string& link_name)
{
    ASSERT(lock_.is_locked_by_me());

    if (link_name.compare(last_link_name_used_for_email_) == 0)
    {
        return true;
    }

    bool result = false;

    // check the preferred restaging link
    SPtr_RestageCLStatus sptr_clstatus_preferred;
    RestageCLIterator iter = restagecl_map_.find(link_name);

    if (iter != restagecl_map_.end()) {
        sptr_clstatus_preferred = iter->second;

        oasys::ScopeLock clstatus_scoplock(sptr_clstatus_preferred->lock(), __func__);


        result = sptr_clstatus_preferred->email_enabled();
    }
    

    if (!result) {
        // preferred link not available - try to find an alternate link

        SPtr_RestageCLStatus sptr_clstatus;
        iter = restagecl_map_.begin();

        while  (iter != restagecl_map_.end()) {
            sptr_clstatus = iter->second;

            oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);

            result = sptr_clstatus->email_enabled();
//                 && (sptr_clstatus->cl_state() == RESTAGE_CL_STATE_ONLINE);

            if (result) {
                link_name = sptr_clstatus->restage_link_name();
                last_link_name_used_for_email_ = link_name;
                break;
            }

            ++iter;
        }
    } else {
        last_link_name_used_for_email_ = link_name;
    }

    
    return result;
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::generate_email_notification(SPtr_BARDNodeStorageUsage sptr_usage,
                                                                const char* storage_msg,
                                                                const char* watermark_msg,
                                                                size_t watermark_val,
                                                                bool& email_sent_flag)
{
    ASSERT(lock_.is_locked_by_me());
    std::string quota_type;
    quota_type = sptr_usage->quota_type_src() ? "Source " : "Destination ";

    std::string node_name = sptr_usage->naming_scheme_cstr();
    node_name += ":"+sptr_usage->nodename();

    std::string message = storage_msg +
                          quota_type + node_name + 
                          watermark_msg +
                          std::to_string(watermark_val) +
                          "%";

    if (params_.email_notifications_enabled_) {
        sptr_usage->check_email_frequency_period(params_.email_frequency_);

        if (email_sent_flag) {
            log_info("email notification suppressed: %s", message.c_str());
        } else {
            std::string link_name = sptr_usage->quota_restage_link_name();
            bool have_email = find_restage_link_with_email(link_name);

            if (have_email) {
                // include a committed report
                oasys::StringBuffer buf;
                bardcmd_committed(buf);
                std::string message2 = "\n\n";
                message2 += buf.c_str();

                send_restage_link_email(link_name, message, message2);

                email_sent_flag = true;

            } else {
                log_err("Unable to find link to send email notification: %s", message.c_str());
            }
        }
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::send_restage_link_email(std::string link_name, 
                                                            std::string& message,
                                                            std::string& message2 )
{
    ASSERT(lock_.is_locked_by_me());

    // check the preferred restaging link
    SPtr_RestageCLStatus sptr_clstatus_preferred;
    RestageCLIterator iter = restagecl_map_.find(link_name);

    if (iter != restagecl_map_.end()) {
        sptr_clstatus_preferred=iter->second;
        sptr_clstatus_preferred->sptr_restageclif_->send_email_notifications(message, message2);
    }
}

//----------------------------------------------------------------------
const char*
BundleArchitecturalRestagingDaemon::get_restage_link_state_str(std::string link_name)
{
    ASSERT(lock_.is_locked_by_me());

    // check the preferred restaging link
    SPtr_RestageCLStatus sptr_clstatus_preferred;
    RestageCLIterator iter = restagecl_map_.find(link_name);

    if (iter != restagecl_map_.end()) {
        sptr_clstatus_preferred = iter->second;

        oasys::ScopeLock clstatus_scoplock(sptr_clstatus_preferred->lock(), __func__);

        return sptr_clstatus_preferred->cl_state_str();
    } else {
        return "offline";
    }
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::query_accept_reload_bundle(bard_quota_type_t quota_type, 
                                                  bard_quota_naming_schemes_t naming_scheme,
                                                  std::string& nodename, size_t payload_len_to_reload)
{
    bool okay_to_accept = true;

    size_t payload_len = internal_storage_block_size_ * 
                         ((payload_len_to_reload + internal_storage_block_size_) / internal_storage_block_size_);

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);

    oasys::ScopeLock scoplok(&lock_, __func__);

    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        // there is a quota entry for this node
        SPtr_BARDNodeStorageUsage sptr_usage;
        sptr_usage = iter->second;


       if (sptr_usage->has_quota()) {

           // check internal quotas

           // can the bundle be accepted based on bundle count?
           if (sptr_usage->quota_internal_bundles() > 0) {
               size_t committed_total_bundles = sptr_usage->committed_internal_bundles();

               if (committed_total_bundles >= sptr_usage->quota_internal_bundles()) {
                   // fully committed - cannot accept another one
                   okay_to_accept = false;
               }
           }
               
           // can the bundle be accepted based on bytes in storage?
           if (okay_to_accept && sptr_usage->quota_internal_bytes() > 0) {
               size_t committed_total_bytes = sptr_usage->committed_internal_bytes();

               if ((committed_total_bytes + payload_len) > sptr_usage->quota_internal_bytes()) {
                   // fully committed - cannot accept another one
                   okay_to_accept = false;
               }
           }
        }
    }

    return okay_to_accept;
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_accepted(Bundle* bundle)
{
    // Bundle was officially accepted into internal storage so update reserved and inuse values
    // Have to allow for possibility it was meant for external storage or to be refused 
    // but then had to be kept internally

    oasys::ScopeLock scoplok(&lock_, __func__);

    // update stats by both destination and source
    bundle_accepted_by_destination(bundle);
    bundle_accepted_by_source(bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_accepted_by_destination(Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_DST, bundle->dest());

    return bundle_accepted(key, RESTAGE_BY_DST, bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_accepted_by_source(Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_SRC, bundle->source());

    return bundle_accepted(key, RESTAGE_BY_SRC, bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_accepted(std::string& key, bool key_is_by_src, Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    // Bundle was officially accepted into internal storage even if it exceeds quota
    // so update reserved and inuse values
    //
    // Have to allow for possibility it was meant for external storage or to be refused 
    // but then had to be kept internally

    SPtr_BARDNodeStorageUsage sptr_usage;

    size_t payload_len = internal_storage_block_size_ * 
                         ((bundle->payload().length() + internal_storage_block_size_) / internal_storage_block_size_);

    BARDNodeStorageUsageMap::iterator iter = usage_map_.find(key);

    if (iter == usage_map_.end()) {
        bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        std::string nodename;

        if (key_is_by_src) {
            endpoint_to_key_components(bundle->source(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_SRC, scheme, nodename);
        } else {
            endpoint_to_key_components(bundle->dest(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_DST, scheme, nodename);
        }

        usage_map_[key] = sptr_usage;
    } else {
        sptr_usage = iter->second;
    }


    // reverse out any reserved values
    size_t reserved_bytes = bundle->bard_quota_reserved(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->reserved_internal_bundles_ >= 1);
        ASSERT(sptr_usage->reserved_internal_bytes_ >= reserved_bytes);

        sptr_usage->reserved_internal_bundles_ -= 1;
        sptr_usage->reserved_internal_bytes_ -= reserved_bytes;
        bundle->set_bard_quota_reserved(key_is_by_src, 0);
    }

    reserved_bytes = bundle->bard_extquota_reserved(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->reserved_external_bundles_ >= 1);
        ASSERT(sptr_usage->reserved_external_bytes_ >= reserved_bytes);

        sptr_usage->reserved_external_bundles_ -= 1;
        sptr_usage->reserved_external_bytes_ -= reserved_bytes;
        bundle->set_bard_extquota_reserved(key_is_by_src, 0);
    }
    bundle->clear_bard_restage_link_name();

    // add to the inuse values
    sptr_usage->inuse_internal_bundles_ += 1;
    sptr_usage->inuse_internal_bytes_ += payload_len;

    bundle->set_bard_in_use(key_is_by_src, payload_len);

    // remove the wait for a new reload if usage is rising
    if ((sptr_usage->inuse_external_bundles_ > 0) &&
         (sptr_usage->last_reload_command_time_ != 0)) {
        if (max_internal_committed_quota_percent(sptr_usage.get()) > 40) {
            sptr_usage->last_reload_command_time_ = 0;
        }
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_restaged(Bundle* bundle, size_t disk_usage)
{
    // Bundle was officially restaged into internal storage so update reserved and inuse values
    // Have to allow for possibility it was meant for external storage or to be refused 
    // but then had to be kept internally

    oasys::ScopeLock scoplok(&lock_, __func__);

    // update stats by both destination and source
    bundle_restaged_by_destination(bundle, disk_usage);
    bundle_restaged_by_source(bundle, disk_usage);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_restaged_by_destination(Bundle* bundle, size_t disk_usage)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_DST, bundle->dest());

    return bundle_restaged(key, RESTAGE_BY_DST, bundle, disk_usage);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_restaged_by_source(Bundle* bundle, size_t disk_usage)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_SRC, bundle->source());

    return bundle_restaged(key, RESTAGE_BY_SRC, bundle, disk_usage);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_restaged(std::string& key, bool key_is_by_src, Bundle* bundle, size_t disk_usage)
{
    ASSERT(lock_.is_locked_by_me());

    // Bundle was officially restaged to external storage
    //
    SPtr_BARDNodeStorageUsage sptr_usage;


    BARDNodeStorageUsageMap::iterator iter = usage_map_.find(key);

    if (iter == usage_map_.end()) {
        // shouldn't realy happen but CYA
        bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        std::string nodename;

        if (key_is_by_src) {
            endpoint_to_key_components(bundle->source(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_SRC, scheme, nodename);
        } else {
            endpoint_to_key_components(bundle->dest(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_DST, scheme, nodename);
        }

        usage_map_[key] = sptr_usage;
    } else {
        sptr_usage = iter->second;
    }


    // reverse out any external reserved values
    size_t reserved_bytes = bundle->bard_extquota_reserved(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->reserved_external_bundles_ >= 1);
        ASSERT(sptr_usage->reserved_external_bytes_ >= reserved_bytes);

        sptr_usage->reserved_external_bundles_ -= 1;
        sptr_usage->reserved_external_bytes_ -= reserved_bytes;
        bundle->set_bard_extquota_reserved(key_is_by_src, 0);
    }
    bundle->clear_bard_restage_link_name();

    // add to the inuse values only for the quota type it was restaged under
    if (bundle->bard_restage_by_src() == key_is_by_src) {
        ++total_restaged_;

        sptr_usage->inuse_external_bundles_ += 1;
        sptr_usage->inuse_external_bytes_ += disk_usage;
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::restaged_bundle_deleted(bard_quota_type_t quota_type, 
                                               bard_quota_naming_schemes_t naming_scheme, 
                                               std::string& nodename, size_t disk_usage)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // Bundle was officially restaged to external storage
    //
    SPtr_BARDNodeStorageUsage sptr_usage;

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);

    BARDNodeStorageUsageMap::iterator iter = usage_map_.find(key);

    ++total_deleted_restaged_;

    if (iter == usage_map_.end()) {
        log_crit("%s: key not found: %s", __func__, key.c_str());
    } else {
        sptr_usage = iter->second;

        // subtract the bundle from the external inuse
        // - numbers could be out of sync if a rescan is in progress
        //   or has occurred
        if (sptr_usage->inuse_external_bundles_ >= 1) {
            sptr_usage->inuse_external_bundles_ -= 1;
        }

        if (sptr_usage->inuse_external_bytes_ >= disk_usage) {
            sptr_usage->inuse_external_bytes_ -= disk_usage;
        }

        // have any thresholds been triggered?
        size_t max_percent = max_external_committed_quota_percent(sptr_usage.get());
        if (max_percent < params_.external_low_watermark_) {
            // if dropped below the low watermark then reset the reached flags
            // and send an email notification
            if (sptr_usage->external_threshold_low_reached_) {
                sptr_usage->external_quota_reached_ = false;
                sptr_usage->external_threshold_low_reached_  = false;
                sptr_usage->external_threshold_high_reached_ = false;

                generate_email_notification(sptr_usage, 
                                            BARD_EXTERNAL_STORAGE_MSG,
                                            BARD_BELOW_LOW_WATERMARK_MSG,
                                            params_.external_low_watermark_,
                                            sptr_usage->email_sent_external_below_low_watermark_);
            }
        }
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_deleted(Bundle* bundle)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // update stats by both destination and source
    bundle_deleted_by_destination(bundle);
    bundle_deleted_by_source(bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_deleted_by_destination(Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_DST, bundle->dest());

    return bundle_deleted(key, false, bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_deleted_by_source(Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    std::string key = make_bardnodestorageusage_key(BARD_QUOTA_TYPE_SRC, bundle->source());

    return bundle_deleted(key, true, bundle);
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bundle_deleted(std::string& key, bool key_is_by_src, Bundle* bundle)
{
    ASSERT(lock_.is_locked_by_me());

    SPtr_BARDNodeStorageUsage sptr_usage;

    BARDNodeStorageUsageMap::iterator iter = usage_map_.find(key);

    if (iter == usage_map_.end()) {
        bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        std::string nodename;

        if (key_is_by_src) {
            endpoint_to_key_components(bundle->source(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_SRC, scheme, nodename);
        } else {
            endpoint_to_key_components(bundle->dest(), scheme, nodename);
            sptr_usage = std::make_shared<BARDNodeStorageUsage>(BARD_QUOTA_TYPE_DST, scheme, nodename);
        }

        usage_map_[key] = sptr_usage;
    } else {
        sptr_usage = iter->second;
    }

    // reverse out any reserved values
    size_t reserved_bytes = bundle->bard_quota_reserved(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->reserved_internal_bundles_ >= 1);
        ASSERT(sptr_usage->reserved_internal_bytes_ >= reserved_bytes);

        sptr_usage->reserved_internal_bundles_ -= 1;
        sptr_usage->reserved_internal_bytes_ -= reserved_bytes;
        bundle->set_bard_quota_reserved(key_is_by_src, 0);
    }

    reserved_bytes = bundle->bard_extquota_reserved(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->reserved_external_bundles_ >= 1);
        ASSERT(sptr_usage->reserved_external_bytes_ >= reserved_bytes);

        sptr_usage->reserved_external_bundles_ -= 1;
        sptr_usage->reserved_external_bytes_ -= reserved_bytes;
        bundle->set_bard_extquota_reserved(key_is_by_src, 0);
    }
    bundle->clear_bard_restage_link_name();

    // reverse out the in_use values
    reserved_bytes = bundle->bard_in_use(key_is_by_src);
    if (reserved_bytes > 0) {
        ASSERT(sptr_usage->inuse_internal_bundles_ >= 1);
        ASSERT(sptr_usage->inuse_internal_bytes_ >= reserved_bytes);

        sptr_usage->inuse_internal_bundles_ -= 1;
        sptr_usage->inuse_internal_bytes_ -= reserved_bytes;

        bundle->set_bard_in_use(key_is_by_src, 0);
    }

    // see if internal committed drops below the low watermark
    size_t max_percent = max_internal_committed_quota_percent(sptr_usage.get());

    if (max_percent < params_.internal_low_watermark_) {
        // and send an email notification
        if (sptr_usage->internal_threshold_low_reached_) {
            sptr_usage->internal_quota_reached_ = false;
            sptr_usage->internal_threshold_low_reached_  = false;
            sptr_usage->internal_threshold_high_reached_ = false;

            generate_email_notification(sptr_usage, 
                                        BARD_INTERNAL_STORAGE_MSG,
                                        BARD_BELOW_LOW_WATERMARK_MSG,
                                        params_.internal_low_watermark_,
                                        sptr_usage->email_sent_internal_below_low_watermark_);
        }
    }

    // determine if an auto-reload event needs to be initiated
    if (sptr_usage->inuse_external_bundles_ > 0) {
        if (sptr_usage->quota_auto_reload()) {
            if ((sptr_usage->inuse_internal_bundles_ == 0) ||
                (max_internal_committed_quota_percent(sptr_usage.get()) <= 20)) {
                struct timeval now;
                ::gettimeofday(&now, 0);

                if ((sptr_usage->inuse_internal_bundles_ == 0) ||
//dzdebug                    ((now.tv_sec - sptr_usage->last_reload_command_time_) > 600)) {
                    ((now.tv_sec - sptr_usage->last_reload_command_time_) > 5)) {
                    size_t new_expiration = 0;
                    std::string nodename = sptr_usage->nodename();
                    std::string no_new_dest_eid;
                    std::string dummy_result;

                    if (bardcmd_reload(sptr_usage->quota_type(), sptr_usage->naming_scheme(), nodename,
                                      new_expiration, no_new_dest_eid, dummy_result)) {
                 
                        sptr_usage->last_reload_command_time_ = now.tv_sec; 
                    } else {
                        // error kicking off a reload
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::rescan_completed()
{
    oasys::ScopeLock scoplok(&lock_, __func__);
   
    if (rescanning_) {
        ++rescan_responses_;

        if (rescan_responses_ == expected_rescan_responses_) {
            // inform all Restage CLs that they can resume restaging and reloading activities

            log_always("Completed rescan of external storage - received notice of completion from all %zu Restage CLs",
                       expected_rescan_responses_);

            rescanning_ = false;

            SPtr_RestageCLStatus sptr_clstatus;
            RestageCLIterator restagecl_iter = restagecl_map_.begin();

            while (restagecl_iter != restagecl_map_.end()) {
                sptr_clstatus = restagecl_iter->second;

                sptr_clstatus->sptr_restageclif_->resume_after_rescan();

                ++restagecl_iter;
            }
        } else {
            log_debug("Rescan of external storage in progress - received notice of completion from %zu of %zu Restage CLs",
                      rescan_responses_, expected_rescan_responses_);
        }

    } else {
        log_err("Received rescan_completed notification while rescan is not in progress");
    }
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::run()
{
    char threadname[16] = "RestagingDmn";
    pthread_setname_np(pthread_self(), threadname);
   
    std::string* event;

    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 100;

    internal_storage_block_size_ = BundleStore::instance()->block_size();
    if (internal_storage_block_size_ == 0) {
        internal_storage_block_size_ = 4096;

        log_err("BundleArchitecturalRestagingDaemon: Internal storage block size overridden from zero to 4096");
    }

    // load in previously stored quotas possibly overrding those in the startup config file
    load_saved_quotas();

    // ready for business
    bard_started_ = true;


    while (1) {
        if (should_stop()) {
            log_always("BundleArchitecturalRestagingDaemon: stopping - event queue size: %zu",
                      eventq_->size());
            break;
        }

        log_debug("BundleArchitecturalRestagingDaemon: checking eventq_->size() > 0, its size is %zu", 
                  eventq_->size());

        if (rescanning_) {
            struct timeval now;
            ::gettimeofday(&now, 0);
            if ((now.tv_sec - rescan_time_initiated_) > 300) {
                log_err("External storage rescan has not completed in 5 minutes; Turning off rescaning flag");
                rescanning_ = false;
            } else {
                usleep(100000);
                continue;
            }
        }

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            oasys::Time now;
            now.get_time();

            
            // handle the event
            //handle_event(event);

            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);

        if (cc == oasys::IOTIMEOUT) {
            continue;

        } else if (cc <= 0) {
            continue;
        }
    }
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_add_quota(bard_quota_type_t quota_type, 
                                 bard_quota_naming_schemes_t naming_scheme, std::string& nodename,
                                 size_t internal_bundles, size_t internal_bytes, bool refuse_bundle, 
                                 std::string& restage_link_name, bool auto_reload, 
                                 size_t external_bundles, size_t external_bytes,
                                 std::string& warning_msg)
{
    warning_msg.clear();

    if ((quota_type != BARD_QUOTA_TYPE_DST) && (quota_type != BARD_QUOTA_TYPE_SRC)) {
        log_err("bardcmd_add_quota: invalid quota type (0 = dst, 1 = src); got: %u", quota_type);
        return false;
    }

    if ((naming_scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (naming_scheme == BARD_QUOTA_NAMING_SCHEME_IMC))
    {
        // maintain node number as both string and numeric value
        char* endp = nullptr;
        size_t node_number = strtoull(nodename.c_str(), &endp, 0);
        (void) node_number;

        if ((nodename.length() == 0) || (*endp != '\0')) {
            log_err("bardcmd_add_quota: invalid node number: %s", nodename.c_str());
            return false;
        }

    } else if (naming_scheme != BARD_QUOTA_NAMING_SCHEME_DTN) {
        log_err("bardcmd_add_quota: invalid naming scheme type (1 = IPN,  2 = DTN, 3 = IMC); got: %u", naming_scheme);
        return false;
    }

    bool new_quota = false;
    bool new_usage = false;

    SPtr_BARDNodeStorageUsage sptr_usage;

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);


    oasys::ScopeLock scoplok(&lock_, __func__);


    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        // updating an existing quota
        sptr_usage = iter->second;

    } else {
        new_quota = true;

        iter = usage_map_.find(key);

        if (iter != usage_map_.end()) {
            // updating an existing quota
            sptr_usage = iter->second;

        } else {
            new_usage = true;

            sptr_usage = std::make_shared<BARDNodeStorageUsage>(quota_type, naming_scheme, nodename);
        }
    }

    // update the quota values
    sptr_usage->set_quota_internal_bundles(internal_bundles);
    sptr_usage->set_quota_internal_bytes(internal_bytes);
    sptr_usage->set_quota_refuse_bundle(refuse_bundle);
    sptr_usage->set_quota_restage_link_name(restage_link_name);
    sptr_usage->set_quota_auto_reload(auto_reload);
    sptr_usage->set_quota_external_bundles(external_bundles);
    sptr_usage->set_quota_external_bytes(external_bytes);

    // add the usage objects to the maps
    if (new_quota) {
        quota_map_[key] = sptr_usage;
    }
    if (new_usage) {
        usage_map_[key] = sptr_usage;
    }

    if (bard_started_) {
        update_datastore(sptr_usage.get());
    }

    // check for new quota being less than is already in local storage
    bool issue_warning_msg = false;
    if (internal_bundles > 0) {
        if (sptr_usage->inuse_internal_bundles_ > internal_bundles) {
            issue_warning_msg = true;
        }
    }


    if (!issue_warning_msg) {
        if (internal_bytes > 0) {
            if (sptr_usage->inuse_internal_bytes_ > internal_bytes) {
                issue_warning_msg = true;
            }
        }
    }

    if (issue_warning_msg) {
        if (refuse_bundle) {
            warning_msg = "\nNew quota exceeds current bundles in local storage but it is too late to refuse them\n\n";
        } else {
            warning_msg = "\nNew quota exceeds current bundles in local storage\n"
                          "The following command can be used to force over quota bundles to be restaged:\n"
                          "      bard force_restage ";
            warning_msg += bard_quota_type_to_str(quota_type);
            warning_msg += " ";
            warning_msg += bard_naming_scheme_to_str(naming_scheme);
            warning_msg += " ";
            warning_msg += nodename;
            warning_msg += "\n\n";
        }
    }

    return true;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_delete_quota(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename)
{
    if ((quota_type != BARD_QUOTA_TYPE_DST) && (quota_type != BARD_QUOTA_TYPE_SRC)) {
        log_err("bardcmd_delete_quota: invalid quota type (1 = dst, 2 = src); got: %u", (uint32_t) quota_type);
        return false;
    }

    if ((naming_scheme != BARD_QUOTA_NAMING_SCHEME_IPN) && (naming_scheme != BARD_QUOTA_NAMING_SCHEME_IMC)) {
        log_err("bardcmd_delete_quota: invalid naming scheme type (1 = IPN, 3 = IMC); got: %u", naming_scheme);
        return false;
    }

    bool result = false;

    SPtr_BARDNodeStorageUsage sptr_usage;

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);


    oasys::ScopeLock scoplok(&lock_, __func__);


    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        result = true;

        // deleting an existing quota
        sptr_usage = iter->second;

        // clear the quota values in the object which is still in use by the usage_map_
        sptr_usage->set_quota_internal_bundles(0);
        sptr_usage->set_quota_internal_bytes(0);
        sptr_usage->set_quota_refuse_bundle(false);
        sptr_usage->clear_quota_restage_link_name();
        sptr_usage->set_quota_auto_reload(false);
        sptr_usage->set_quota_external_bundles(0);
        sptr_usage->set_quota_external_bytes(0);

        quota_map_.erase(key);

        if (bard_started_) {
            delete_from_datastore(sptr_usage.get());
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_unlimited_quota(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename)
{
    if ((quota_type != BARD_QUOTA_TYPE_DST) && (quota_type != BARD_QUOTA_TYPE_SRC)) {
        log_err("bardcmd_unlimited_quota: invalid quota type (0 = dst, 1 = src); got: %u", quota_type);
        return false;
    }

    if ((naming_scheme == BARD_QUOTA_NAMING_SCHEME_IPN) || (naming_scheme == BARD_QUOTA_NAMING_SCHEME_IMC))
    {
        // maintain node number as both string and numeric value
        char* endp = nullptr;
        size_t node_number = strtoull(nodename.c_str(), &endp, 0);
        (void) node_number;

        if ((nodename.length() == 0) || (*endp != '\0')) {
            log_err("bardcmd_umlimited: invalid node number: %s", nodename.c_str());
            return false;
        }

    } else if (naming_scheme != BARD_QUOTA_NAMING_SCHEME_DTN) {
        log_err("bardcmd_unlimited_quota: invalid naming scheme type (1 = IPN,  2 = DTN, 3 = IMC); got: %u", naming_scheme);
        return false;
    }

    bool new_quota = false;
    bool new_usage = false;

    SPtr_BARDNodeStorageUsage sptr_usage;

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);


    oasys::ScopeLock scoplok(&lock_, __func__);


    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        // updating an existing quota
        sptr_usage = iter->second;

    } else {
        new_quota = true;

        iter = usage_map_.find(key);

        if (iter != usage_map_.end()) {
            // updating an existing quota
            sptr_usage = iter->second;

        } else {
            new_usage = true;

            sptr_usage = std::make_shared<BARDNodeStorageUsage>(quota_type, naming_scheme, nodename);
        }
    }

    // update the quota values
    sptr_usage->set_quota_internal_bundles(0);
    sptr_usage->set_quota_internal_bytes(0);
    sptr_usage->set_quota_refuse_bundle(false);
    sptr_usage->clear_quota_restage_link_name();
    sptr_usage->set_quota_auto_reload(false);
    sptr_usage->set_quota_external_bundles(0);
    sptr_usage->set_quota_external_bytes(0);

    // add the usage objects to the maps
    if (new_quota) {
        quota_map_[key] = sptr_usage;
    }
    if (new_usage) {
        usage_map_[key] = sptr_usage;
    }

    if (bard_started_) {
        update_datastore(sptr_usage.get());
    }


    return true;
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_force_restage(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename)
{
    if ((quota_type != BARD_QUOTA_TYPE_DST) && (quota_type != BARD_QUOTA_TYPE_SRC)) {
        log_err("bardcmd_delete_quota: invalid quota type (1 = dst, 2 = src); got: %u", (uint32_t) quota_type);
        return false;
    }

    if ((naming_scheme != BARD_QUOTA_NAMING_SCHEME_IPN) && (naming_scheme != BARD_QUOTA_NAMING_SCHEME_IMC)) {
        log_err("bardcmd_delete_quota: invalid naming scheme type (1 = IPN, 3 = IMC); got: %u", naming_scheme);
        return false;
    }

    bool result = false;

    SPtr_BARDNodeStorageUsage sptr_usage;

    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);


    oasys::ScopeLock scoplok(&lock_, __func__);


    BARDNodeStorageUsageMap::iterator iter = quota_map_.find(key);

    if (iter != quota_map_.end()) {
        sptr_usage = iter->second;

        if (!sptr_usage->quota_refuse_bundle()) {
            result = true;

            size_t bundles_to_restage = 0;
            size_t bytes_to_restage = 0;
            if (sptr_usage->over_quota(bundles_to_restage, bytes_to_restage)) {
                // initiate a restaging thread to do the work
                BARDForceRestageThread* restager = new BARDForceRestageThread(quota_type, naming_scheme, nodename,
                                                                            sptr_usage->node_number(),
                                                                            bundles_to_restage, bytes_to_restage,
                                                                            sptr_usage->quota_restage_link_name());
                restager->start();
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
size_t
BundleArchitecturalRestagingDaemon::max_inuse_quota_percent(BARDNodeStorageUsage* usage_ptr)
{
    size_t result = 0;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;

        if (usage_ptr->quota_internal_bundles() > 0) {
            max_percent = double(usage_ptr->inuse_internal_bundles_) / double(usage_ptr->quota_internal_bundles());
        }
        if (usage_ptr->quota_internal_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = double(usage_ptr->inuse_internal_bytes_) / double(usage_ptr->quota_internal_bytes());

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
            }
        }
        max_percent *= 100.0;

        result = max_percent;
    }
    
    return result;
}

//----------------------------------------------------------------------
size_t
BundleArchitecturalRestagingDaemon::max_internal_committed_quota_percent(BARDNodeStorageUsage* usage_ptr)
{
    size_t result = 0;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;

        if (usage_ptr->quota_internal_bundles() > 0) {
            max_percent = double(usage_ptr->committed_internal_bundles()) / double(usage_ptr->quota_internal_bundles());
        }
        if (usage_ptr->quota_internal_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = double(usage_ptr->committed_internal_bytes()) / double(usage_ptr->quota_internal_bytes());

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
            }
        }
        max_percent *= 100.0;

        result = max_percent;
    }
    
    return result;
}

//----------------------------------------------------------------------
size_t
BundleArchitecturalRestagingDaemon::max_external_committed_quota_percent(BARDNodeStorageUsage* usage_ptr)
{
    size_t result = 0;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;

        if (usage_ptr->quota_external_bundles() > 0) {
            max_percent = double(usage_ptr->committed_external_bundles()) / double(usage_ptr->quota_external_bundles());
        }
        if (usage_ptr->quota_external_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = double(usage_ptr->committed_external_bytes()) / double(usage_ptr->quota_external_bytes());

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
            }
        }
        max_percent *= 100.0;

        result = max_percent;
    }
    
    return result;
}

//----------------------------------------------------------------------
std::string
BundleArchitecturalRestagingDaemon::format_internal_quota_usage(BARDNodeStorageUsage* usage_ptr)
{
    std::string result;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;
        bool max_percent_is_bytes = false;

        if (usage_ptr->quota_internal_bundles() > 0) {
            max_percent = (double(usage_ptr->inuse_internal_bundles_) / double(usage_ptr->quota_internal_bundles())) * 100.0;
        }
        if (usage_ptr->quota_internal_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = (double(usage_ptr->inuse_internal_bytes_) / double(usage_ptr->quota_internal_bytes())) * 100.0;

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
                max_percent_is_bytes = true;
            }
        }

        if (max_percent > 999.0) {
            max_percent = 999.0;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%3.0f%% %1.1s", max_percent, max_percent_is_bytes ? "B" : "#");

        result = buf;
    }

    return result;
}

//----------------------------------------------------------------------
std::string
BundleArchitecturalRestagingDaemon::format_external_quota_usage(BARDNodeStorageUsage* usage_ptr)
{
    std::string result;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;
        bool max_percent_is_bytes = false;

        if (usage_ptr->quota_external_bundles() > 0) {
            max_percent = (double(usage_ptr->inuse_external_bundles_) / double(usage_ptr->quota_external_bundles())) * 100.0;
        }
        if (usage_ptr->quota_external_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = (double(usage_ptr->inuse_external_bytes_) / double(usage_ptr->quota_external_bytes())) * 100.0;

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
                max_percent_is_bytes = true;
            }
        }

        if (max_percent > 999.0) {
            max_percent = 999.0;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%3.0f%% %1.1s", max_percent, max_percent_is_bytes ? "B" : "#");

        result = buf;
    }

    return result;
}

//----------------------------------------------------------------------
std::string
BundleArchitecturalRestagingDaemon::format_internal_quota_committed(BARDNodeStorageUsage* usage_ptr)
{
    std::string result;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;
        bool max_percent_is_bytes = false;

        if (usage_ptr->quota_internal_bundles() > 0) {
            max_percent = (double(usage_ptr->committed_internal_bundles()) / double(usage_ptr->quota_internal_bundles())) * 100.0;
        }
        if (usage_ptr->quota_internal_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = (double(usage_ptr->committed_internal_bytes()) / double(usage_ptr->quota_internal_bytes())) * 100.0;

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
                max_percent_is_bytes = true;
            }
        }

        if (max_percent > 999.0) {
            max_percent = 999.0;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%3.0f%% %1.1s", max_percent, max_percent_is_bytes ? "B" : "#");

        result = buf;
    }

    return result;
}

//----------------------------------------------------------------------
std::string
BundleArchitecturalRestagingDaemon::format_external_quota_committed(BARDNodeStorageUsage* usage_ptr)
{
    std::string result;

    if (usage_ptr->has_quota()) {
        double max_percent = 0.0;
        bool max_percent_is_bytes = false;

        if (usage_ptr->quota_external_bundles() > 0) {
            max_percent = (double(usage_ptr->committed_external_bundles()) / double(usage_ptr->quota_external_bundles())) * 100.0;
        }
        if (usage_ptr->quota_external_bytes() > 0) {
            double bytes_percent = 0.0;
            bytes_percent = (double(usage_ptr->committed_external_bytes()) / double(usage_ptr->quota_external_bytes())) * 100.0;

            if (bytes_percent > max_percent) {
                max_percent = bytes_percent;
                max_percent_is_bytes = true;
            }
        }

        if (max_percent > 999.0) {
            max_percent = 999.0;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%3.0f%% %1.1s", max_percent, max_percent_is_bytes ? "B" : "#");

        result = buf;
    }

    return result;
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_quotas(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_quotas = quota_map_.size();

    if (num_quotas == 0) {
        buf.append("No Bundle Restaging Quotas defined\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = quota_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != quota_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage   |  External Storage   |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes   |  Bundles :   Bytes  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------  |  --------:--------  |  ----------  ------  ----------\n");

        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_quotas == 1) {
            buf.append("Bundle Restaging Quotas (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Restaging Quotas (%zu entries):\n\n", num_quotas);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage   |  External Storage   |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes   |  Bundles :  Bytes   |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------  |  --------:--------  |  ----------  ------  ----------\n");

        buf.appendf("Quota   Name   %*.*s%-*s  |  Internal Storage   |  External Storage   |  %*.*s%-*s   Auto     Link    \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

        buf.appendf("Type   Scheme  %*.*s%-*s  |  Bundles :  Bytes   |  Bundles :  Bytes   |  %*.*s%-*s  Reload    State   \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");


        buf.appendf("-----  ------  %*.*s  |  --------:--------  |  --------:--------  |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);


        free(dashes);
        free(spaces);

        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = quota_map_.begin();

        while (iter != quota_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |    %5.5s :  %5.5s   |    %5.5s :  %5.5s   |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        FORMAT_WITH_MAG(sptr_usage->quota_internal_bundles()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->quota_internal_bytes()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->quota_external_bundles()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->quota_external_bytes()).c_str(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n(use command 'bard quotas exact' to see exact values)\n\n");
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_quotas_exact(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_quotas = quota_map_.size();

    if (num_quotas == 0) {
        buf.append("No Bundle Restaging Quotas defined\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = quota_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != quota_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |         Internal Storage        |         External Storage        |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes   |   Num Bundles : Payload Bytes   |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------  |  -------------:---------------  |  ----------  ------  ----------\n");

        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_quotas == 1) {
            buf.append("Bundle Restaging Quotas (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Restaging Quotas (%zu entries):\n\n", num_quotas);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |         Internal Storage        |         External Storage        |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes   |   Num Bundles :   File Bytes    |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------  |  -------------:---------------  |  ----------  ------  ----------\n");

         buf.appendf("Quota   Name   %*.*s%-*s  |         Internal Storage        |         External Storage        |  %*.*s%-*s   Auto     Link    \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

         buf.appendf("Type   Scheme  %*.*s%-*s  |   Num Bundles :    File Bytes   |   Num Bundles :   File Bytes    |  %*.*s%-*s  Reload    State   \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");


        buf.appendf("-----  ------  %*.*s  |  -------------:---------------  |  -------------:---------------  |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);

        free(dashes);
        free(spaces);


        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = quota_map_.begin();

        while (iter != quota_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |  %12zu : %14zu  |  %12zu : %14zu  |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        sptr_usage->quota_internal_bundles(),
                        sptr_usage->quota_internal_bytes(),
                        sptr_usage->quota_external_bundles(),
                        sptr_usage->quota_external_bytes(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n");
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_usage(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_recs = usage_map_.size();

    if (num_recs == 0) {
        buf.append("No Bundle Restaging Quotas defined and no bundles received\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage  : Quota  |  External Storage  : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------   ------ |  --------:--------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |    123M+ :  123M+    123% # |    123M+ :  123M+    123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");


        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_recs == 1) {
            buf.append("Bundle Usage Records (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Usage Records (%zu entries):\n\n", num_recs);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage  : Quota  |  External Storage  : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------   ------ |  --------:--------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |    123M+ :  123M+    123% # |    123M+ :  123M+    123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");

        buf.appendf("Quota   Name   %*.*s%-*s  |  Internal Storage  : Quota  |  External Storage  : Quota  |  %*.*s%-*s   Auto     Link    \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

        buf.appendf("Type   Scheme  %*.*s%-*s  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  %*.*s%-*s  Reload    State   \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");

        buf.appendf("-----  ------  %*.*s  |  --------:--------   ------ |  --------:--------   ------ |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);


        free(dashes);
        free(spaces);

        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = usage_map_.begin();

        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |    %5.5s :  %5.5s    %6.6s |    %5.5s :  %5.5s    %6.6s |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        FORMAT_WITH_MAG(sptr_usage->inuse_internal_bundles_).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->inuse_internal_bytes_).c_str(),
                        format_internal_quota_usage(sptr_usage.get()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->inuse_external_bundles_).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->inuse_external_bytes_).c_str(),
                        format_external_quota_usage(sptr_usage.get()).c_str(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n");
    buf.append("Quota Usage: highest percentage is displayed; '#' = based on the bundle count; 'B' = based on the bytes quota\n");
    buf.append("(use command 'bard usage exact' to see exact values)\n\n");
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_usage_exact(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_recs = usage_map_.size();

    if (num_recs == 0) {
        buf.append("No Bundle Restaging Quotas defined and no bundles received\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |        Internal Storage        : Quota  |        External Storage        : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------   ------ |  -------------:---------------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |  123456789012 : 12345678901234   123% # |  123456789012 : 12345678901234   123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");



        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_recs == 1) {
            buf.append("Bundle Usage Records (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Usage Records (%zu entries):\n\n", num_recs);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |        Internal Storage        : Quota  |        External Storage        : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------   ------ |  -------------:---------------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |  123456789012 : 12345678901234   123% # |  123456789012 : 12345678901234   123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");

        buf.appendf("Quota   Name   %*.*s%-*s  |        Internal Storage        : Quota  |        External Storage        : Quota  |  %*.*s%-*s   Auto     Link  \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

        buf.appendf("Type   Scheme  %*.*s%-*s  |   Num Bundles :    File Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  %*.*s%-*s  Reload    State \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");

        buf.appendf("-----  ------  %*.*s  |  -------------:---------------   ------ |  -------------:---------------   ------ |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);

        free(dashes);
        free(spaces);


        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = usage_map_.begin();

        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |  %12zu : %14zu   %6.6s |  %12zu : %14zu   %6.6s |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        sptr_usage->inuse_internal_bundles_,
                        sptr_usage->inuse_internal_bytes_,
                        format_internal_quota_usage(sptr_usage.get()).c_str(),
                        sptr_usage->inuse_external_bundles_,
                        sptr_usage->inuse_external_bytes_,
                        format_external_quota_usage(sptr_usage.get()).c_str(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n");
    buf.append("Quota Usage: highest percentage is displayed; '#' = based on the bundle count; 'B' = based on the bytes quota\n\n");
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_committed(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_recs = usage_map_.size();

    if (num_recs == 0) {
        buf.append("No Bundle Restaging Quotas defined and no bundles received\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage  : Quota  |  External Storage  : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------   ------ |  --------:--------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |    123M+ :  123M+    123% # |    123M+ :  123M+    123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");


        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_recs == 1) {
            buf.append("Bundle Committed* Records (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Committed* Records (%zu entries):\n\n", num_recs);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |  Internal Storage  : Quota  |  External Storage  : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  --------:--------   ------ |  --------:--------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |    123M+ :  123M+    123% # |    123M+ :  123M+    123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");

        buf.appendf("Quota   Name   %*.*s%-*s  |  Internal Storage  : Quota  |  External Storage  : Quota  |  %*.*s%-*s   Auto     Link    \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

        buf.appendf("Type   Scheme  %*.*s%-*s  |  Bundles :  Bytes  : Usage  |  Bundles :  Bytes  : Usage  |  %*.*s%-*s  Reload    State   \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");

        buf.appendf("-----  ------  %*.*s  |  --------:--------   ------ |  --------:--------   ------ |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);


        free(dashes);
        free(spaces);

        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = usage_map_.begin();

        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |    %5.5s :  %5.5s    %6.6s |    %5.5s :  %5.5s    %6.6s |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        FORMAT_WITH_MAG(sptr_usage->committed_internal_bundles()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->committed_internal_bytes()).c_str(),
                        format_internal_quota_committed(sptr_usage.get()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->committed_external_bundles()).c_str(),
                        FORMAT_WITH_MAG(sptr_usage->committed_external_bytes()).c_str(),
                        format_external_quota_committed(sptr_usage.get()).c_str(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n");
    buf.append("Quota Usage: highest percentage is displayed; '#' = based on the bundle count; 'B' = based on the bytes quota\n");
    buf.append("* \"Committed\" values include both stored/in-use and reserved but not written numbers\n");
    buf.append("(use command 'bard committed exact' to see exact values)\n\n");
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_committed_exact(oasys::StringBuffer& buf)
{
    std::string link_state = "";
    std::string auto_reload = "";
    std::string refuse_or_link = "";

    oasys::ScopeLock scoplok(&lock_, __func__);

    report_all_restagecl_status(buf);

    size_t num_recs = usage_map_.size();

    if (num_recs == 0) {
        buf.append("No Bundle Restaging Quotas defined and no bundles received\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        uint32_t link_len = 10;
        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }
            if (sptr_usage->quota_restage_link_name().length() > link_len) {
                link_len = sptr_usage->quota_restage_link_name().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |        Internal Storage        : Quota  |        External Storage        : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------   ------ |  -------------:---------------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |  123456789012 : 12345678901234   123% # |  123456789012 : 12345678901234   123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");



        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        uint32_t title1_pad_len_link = (link_len - 9) / 2;          // to center "Refuse or" in the field
        uint32_t title1_link_len = link_len - title1_pad_len_link;  // remainder for left aligned "Refuse or"

        uint32_t title2_pad_len_link = (link_len - 9) / 2;          // to center "Link Name" in the field
        uint32_t title2_link_len = link_len - title2_pad_len_link;  // remainder for left aligned "Link Name"

        int32_t max_fld_len = std::max(name_len, link_len);
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);




        if (num_recs == 1) {
            buf.append("Bundle Committed* Records (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Committed* Records (%zu entries):\n\n", num_recs);
        }

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |        Internal Storage        : Quota  |        External Storage        : Quota  |  Refuse or    Auto     Link  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  Link Name   Reload    State \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------   ------ |  -------------:---------------   ------ |  ----------  ------  ----------\n");
        //buf.appendf(" dst     ipn      12345  |  123456789012 : 12345678901234   123% # |  123456789012 : 12345678901234   123% B |  link_name    false    online  \n");
        //buf.append("\nQuota Usage: highest percentage is displayed; # means it is based on the bundle count; B means it is based on the bytes quota\n");

        buf.appendf("Quota   Name   %*.*s%-*s  |        Internal Storage        : Quota  |        External Storage        : Quota  |  %*.*s%-*s   Auto     Link  \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node",
                    title1_pad_len_link, title1_pad_len_link, spaces, title1_link_len, "Refuse or");

        buf.appendf("Type   Scheme  %*.*s%-*s  |   Num Bundles :    File Bytes  : Usage  |   Num Bundles :   File Bytes   : Usage  |  %*.*s%-*s  Reload    State \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num",
                    title2_pad_len_link, title2_pad_len_link, spaces, title2_link_len, "Link Name");

        buf.appendf("-----  ------  %*.*s  |  -------------:---------------   ------ |  -------------:---------------   ------ |  %*.*s  ------  ----------\n",
                    name_len, name_len, dashes, 
                    link_len, link_len, dashes);

        free(dashes);
        free(spaces);


        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = usage_map_.begin();

        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            if (!sptr_usage->has_quota()) {
                link_state = "";
                auto_reload = "";
                refuse_or_link = "";
            } else {
                if (sptr_usage->quota_refuse_bundle()) {
                    auto_reload = "";
                } else if (sptr_usage->quota_auto_reload()) {
                    auto_reload = "true";
                } else {
                    auto_reload = "false";
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    refuse_or_link = "refuse";
                } else {
                    refuse_or_link = sptr_usage->quota_restage_link_name().c_str();
                }

                if (sptr_usage->quota_refuse_bundle()) {
                    link_state = "";
                } else {
                    link_state = get_restage_link_state_str(sptr_usage->quota_restage_link_name());
                }

            }

            buf.appendf(" %3.3s     %3.3s   %*.*s  |  %12zu : %14zu   %6.6s |  %12zu : %14zu   %6.6s |  %-*.*s   %-5.5s  %s\n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),
                        sptr_usage->committed_internal_bundles(),
                        sptr_usage->committed_internal_bytes(),
                        format_internal_quota_committed(sptr_usage.get()).c_str(),
                        sptr_usage->committed_external_bundles(),
                        sptr_usage->committed_external_bytes(),
                        format_external_quota_committed(sptr_usage.get()).c_str(),
                        link_len, link_len, refuse_or_link.c_str(), auto_reload.c_str(), link_state.c_str()
                       ); 

            iter++;
        }
    }

    buf.append("\n");
    buf.append("Quota Usage: highest percentage is displayed; '#' = based on the bundle count; 'B' = based on the bytes quota\n\n");
    buf.append("* \"Committed\" values include both stored/in-use and reserved but not written numbers\n");
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::bardcmd_dump(oasys::StringBuffer& buf)
{
    oasys::ScopeLock scoplok(&lock_, __func__);


    buf.append("BARD Settings:\n");
    buf.appendf("  Strict quota enforcement: %s\n", BundleDaemon::params_.bard_quota_strict_ ? "true" : "false");
    buf.appendf("  Email notifications enabled: %s\n", params_.email_notifications_enabled_ ? "true" : "false");
    buf.appendf("  Internal storage low watermark: %zu\n", params_.internal_low_watermark_);
    buf.appendf("  Internal storage high watermark: %zu\n", params_.internal_high_watermark_);
    buf.appendf("  External storage low watermark: %zu\n", params_.external_low_watermark_);
    buf.appendf("  External storage high watermark: %zu\n", params_.external_high_watermark_);
    buf.append("\n");




    report_all_restagecl_status(buf);

    buf.appendf("\nTotal bundles restaged: %zu   deleted: %zu\n\n", 
               total_restaged_, total_deleted_restaged_);



    size_t num_recs = usage_map_.size();

    if (num_recs == 0) {
        buf.append("No Bundle Restaging Quotas defined and no bundles received\n");
    } else {
        SPtr_BARDNodeStorageUsage sptr_usage;
        BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

        if (num_recs == 1) {
            buf.append("Bundle Usage Records (1 entry):\n\n");
        } else {
            buf.appendf("Bundle Usage Records (%zu entries):\n\n", num_recs);
        }

        // calc length needed for node name and link name fields
        uint32_t name_len = 8;
        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (sptr_usage->nodename().length() > name_len) {
                name_len = sptr_usage->nodename().length();
            }

            ++iter;
        }


        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf.appendf("Quota   Name     Node    |     Internal Storage In Use    :   Internal Storage Reserved   |     External Storage In Use    :   External Storage Reserved  \n");
        //buf.appendf("Type   Scheme  Name/Num  |   Num Bundles : Payload Bytes  :  Num Bundles : Payload Bytes  |   Num Bundles : Payload Bytes  :  Num Bundles : Payload Bytes \n");
        //buf.appendf("-----  ------  --------  |  -------------:---------------   -------------:--------------- |  -------------:---------------   -------------:---------------\n");
        //buf.appendf(" dst     ipn      12345  |  123456789012 : 12345678901234   123456789012 : 12345678901234 |  123456789012 : 12345678901234   123456789012 : 12345678901234\n");



        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"

        int32_t max_fld_len = name_len;
        char* dashes = (char*) calloc(1, max_fld_len + 1);
        char* spaces = (char*) calloc(1, max_fld_len + 1);

        memset(dashes, '-', max_fld_len);
        memset(spaces, ' ', max_fld_len);


        buf.appendf("Quota   Name   %*.*s%-*s  |     Internal Storage In Use    :   Internal Storage Reserved   |     External Storage In Use    :   External Storage Reserved  \n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node");

        buf.appendf("Type   Scheme  %*.*s%-*s  |   Num Bundles :    File Bytes  :  Num Bundles :    File Bytes  |   Num Bundles :    File Bytes  :  Num Bundles :    File Bytes \n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num");

        buf.appendf("-----  ------  %*.*s  |  -------------:---------------   -------------:--------------- |  -------------:---------------   -------------:---------------\n",
                    name_len, name_len, dashes);

        free(dashes);
        free(spaces);

        // output the quota records
        bool first_entry = true;
        uint32_t last_quota_type = 0;
        uint32_t last_naming_scheme = 0;

        iter = usage_map_.begin();

        while (iter != usage_map_.end()) {
            sptr_usage = iter->second;

            if (first_entry) {
                first_entry = false;
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            } 
            else if ((last_quota_type != sptr_usage->quota_type()) ||
                     (last_naming_scheme != sptr_usage->naming_scheme()))
            {
                buf.append("\n");
                last_quota_type = sptr_usage->quota_type();
                last_naming_scheme = sptr_usage->naming_scheme();
            }


            buf.appendf(" %3.3s     %3.3s   %*.*s  |  %12zu : %14zu   %12zu : %14zu |  %12zu : %14zu   %12zu : %14zu \n",
                        sptr_usage->quota_type_cstr(),
                        sptr_usage->naming_scheme_cstr(),
                        name_len, name_len, sptr_usage->nodename().c_str(),

                        sptr_usage->inuse_internal_bundles_,
                        sptr_usage->inuse_internal_bytes_,
                        sptr_usage->reserved_internal_bundles_,
                        sptr_usage->reserved_internal_bytes_,

                        sptr_usage->inuse_external_bundles_,
                        sptr_usage->inuse_external_bytes_,
                        sptr_usage->reserved_external_bundles_,
                        sptr_usage->reserved_external_bytes_
                       ); 

            iter++;
        }
    }

    buf.append("\n");
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_rescan(std::string& result_str)
{
    lock_.lock(__func__);

    if (rescanning_) {
        result_str = "Rescan is already in progress - ignored\n";
        return false;
    }

    expected_rescan_responses_ = 0;
    rescan_responses_ = 0;


    // inform all of the Restage CLs to pause activities while doing a rescan
    SPtr_RestageCLStatus sptr_clstatus;
    RestageCLIterator restagecl_iter = restagecl_map_.begin();

    while (restagecl_iter != restagecl_map_.end()) {
        sptr_clstatus = restagecl_iter->second;

        sptr_clstatus->sptr_restageclif_->pause_for_rescan();

        ++restagecl_iter;
    }

    // delay to allow in-progress restaging or reloading of a bundle to complete
    lock_.unlock();
    sleep(1);


    oasys::ScopeLock scoplok(&lock_, __func__);


    // initialize all of the usage stats for the rescan
    SPtr_BARDNodeStorageUsage sptr_usage;
    BARDNodeStorageUsageMap::iterator usage_iter = usage_map_.begin();

    while (usage_iter != usage_map_.end()) {
        sptr_usage = usage_iter->second;

        // move the current external ini use values to the "last" values and zero them out
        sptr_usage->last_inuse_external_bundles_ = sptr_usage->inuse_external_bundles_;
        sptr_usage->last_inuse_external_bytes_ = sptr_usage->inuse_external_bytes_;

        sptr_usage->inuse_external_bundles_ = 0;
        sptr_usage->inuse_external_bytes_ = 0;

        ++usage_iter;
    }

    // now inform all of the Restage CLs to do the rescan
    restagecl_iter = restagecl_map_.begin();

    while (restagecl_iter != restagecl_map_.end()) {
        sptr_clstatus = restagecl_iter->second;

        sptr_clstatus->sptr_restageclif_->rescan();

        ++restagecl_iter;

        ++expected_rescan_responses_;
    }

    rescanning_ = (expected_rescan_responses_ > 0);
    struct timeval now;
    ::gettimeofday(&now, 0);
    rescan_time_initiated_ = now.tv_sec;

    if (rescanning_) {
        result_str = "Rescan initiated\n";
    } else {
        result_str = "No Restage CLs registered for a rescan\n";
    }

    return rescanning_;
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::report_all_restagecl_status(oasys::StringBuffer& buf)
{
    ASSERT(lock_.is_locked_by_me());

    buf.appendf("Restage Convergence Layer Status\n\n");

    if (restagecl_map_.size() == 0) {
        buf.appendf("<no active Restage Convergence Layers>\n");
    } else {
        buf.append("   Link Name        State      Pooled  Reload  Mount Pnt  VolSize  VolFree   Quota   Q Used   Q Free          Storage Path\n");
        buf.append("----------------  -----------  ------  ------  ---------  -------  -------  -------  -------  -------  ------------------------------\n");

        // loop through the list of RestageCLs
        RestageCLIterator iter = restagecl_map_.begin();

        while (iter != restagecl_map_.end()) {

            SPtr_RestageCLStatus sptr_clstatus = iter->second;

            oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);

            const char* state_str = sptr_clstatus->cl_state_str();
            const char* pooled_str = "true ";
            const char* reload_str = " auto ";
            const char* mount_pnt_str = "validated";

            if (!sptr_clstatus->part_of_pool()) {
                pooled_str = "false";
            }
            if (sptr_clstatus->auto_reload_interval() == 0) {
                reload_str = "manual";
            }
            if (!sptr_clstatus->mount_point()) {
                mount_pnt_str = "  false  ";
            } else {
                if (!sptr_clstatus->mount_pt_validated()) {
                    mount_pnt_str = " offline ";
                }
            }

            size_t disk_quota_free = 0;
            if (sptr_clstatus->disk_quota_in_use() < sptr_clstatus->disk_quota()) {
                disk_quota_free = sptr_clstatus->disk_quota() - sptr_clstatus->disk_quota_in_use();
            }

     //      buf.append("    Link Name       State     Pooled  Reload  Mount Pnt  VolSize  VolFree   Quota  Q Used   Q Free          Storage Path\n");
     //      buf.append("----------------  ----------  ------  ------  ---------  -------  -------  ------- -------  -------  ------------------------------\n");

            buf.appendf("%-16.16s  %10.10s   %5.5s  %6.6s  %9.9s   %5.5s    %5.5s     %5.5s    %5.5s    %5.5s   %s\n",
                        sptr_clstatus->restage_link_name().c_str(),
                        state_str, pooled_str, reload_str, mount_pnt_str,
                        FORMAT_WITH_MAG(sptr_clstatus->vol_total_space()).c_str(),
                        FORMAT_WITH_MAG(sptr_clstatus->vol_space_available()).c_str(),
                        FORMAT_WITH_MAG(sptr_clstatus->disk_quota()).c_str(),
                        FORMAT_WITH_MAG(sptr_clstatus->disk_quota_in_use()).c_str(),
                        FORMAT_WITH_MAG(disk_quota_free).c_str(),
                        sptr_clstatus->storage_path().c_str());


            ++iter;
        }
    }

    buf.append("                                   (Sizes: K=1000 Bytes, M=1000000 Bytes, G=1000000000 Bytes, T=1000000000000 Bytes)\n");

    buf.append("\n\n");
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_del_restaged_bundles(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, 
                                                   std::string& nodename, std::string& result_str)
{
    bool result = false;
    int num_dirs = 0;

    SPtr_RestageCLStatus sptr_clstatus;

    // loop through all registered CL restage instances and issue reload events
    oasys::ScopeLock scoplock(&lock_, __func__);

    if (restagecl_map_.size() == 0) {
        result_str = "No registered Restage CLs";
    } else {
        RestageCLIterator iter = restagecl_map_.begin();

        while (iter != restagecl_map_.end()) {
            sptr_clstatus = iter->second;

            num_dirs += sptr_clstatus->sptr_restageclif_->delete_restaged_bundles(quota_type, scheme, nodename);

            ++iter;
        }

        result = true;


        if (num_dirs == 0) {
            result_str = "No RestageCLs found with bundles of that type";
        } else {
            result_str = "Delete Restaged Bundles event(s) queued to be processed";
        }
    }
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_del_all_restaged_bundles(std::string& result_str)
{
    bool result = false;
    int num_dirs = 0;

    SPtr_RestageCLStatus sptr_clstatus;

    // loop through all registered CL restage instances and issue reload events
    oasys::ScopeLock scoplock(&lock_, __func__);

    if (restagecl_map_.size() == 0) {
        result_str = "No registered Restage CLs";
    } else {
        RestageCLIterator iter = restagecl_map_.begin();

        while (iter != restagecl_map_.end()) {
            sptr_clstatus = iter->second;

            num_dirs += sptr_clstatus->sptr_restageclif_->delete_all_restaged_bundles();

            ++iter;
        }

        result = true;
        if (num_dirs == 0) {
            result_str = "No RestageCLs found with restaged bundles";
        } else {
            result_str = "Delete event(s) queued to be processed";
        }
    }
    
    return result;
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_reload(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, std::string& nodename, 
                     size_t new_expiration, std::string& new_dest_eid, std::string& result_str)
{
    bool result = false;
    int num_dirs = 0;

    SPtr_RestageCLStatus sptr_clstatus;

    // loop through all registered CL restage instances and issue reload events
    oasys::ScopeLock scoplock(&lock_, __func__);

    if (restagecl_map_.size() == 0) {
        result_str = "No registered Restage CLs";
    } else {
        RestageCLIterator iter = restagecl_map_.begin();

        while (iter != restagecl_map_.end()) {
            sptr_clstatus = iter->second;

            num_dirs += sptr_clstatus->sptr_restageclif_->reload(quota_type, scheme, nodename, new_expiration, new_dest_eid);

            ++iter;
        }

        result = true;


        if (num_dirs == 0) {
            result_str = "No RestageCLs found with bundles of that type";
        } else {
            result_str = "Reload event(s) queued to be processed";
        }
    }
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_reload_all(size_t new_expiration, std::string& result_str)
{
    bool result = false;
    int num_dirs = 0;

    SPtr_RestageCLStatus sptr_clstatus;

    // loop through all registered CL restage instances and issue reload events
    oasys::ScopeLock scoplock(&lock_, __func__);

    if (restagecl_map_.size() == 0) {
        result_str = "No registered Restage CLs";
    } else {
        RestageCLIterator iter = restagecl_map_.begin();

        while (iter != restagecl_map_.end()) {
            sptr_clstatus = iter->second;

            num_dirs += sptr_clstatus->sptr_restageclif_->reload_all(new_expiration);

            ++iter;
        }

        result = true;
        if (num_dirs == 0) {
            result_str = "No RestageCLs found with restaged bundles";
        } else {
            result_str = "Reload event(s) queued to be processed";
        }
    }
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_get_internal_low_watermark(std::string& result_str)
{
    bool result = true;
    result_str = std::to_string(params_.internal_low_watermark_) + "%";
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_set_internal_low_watermark(size_t new_watermark, std::string& result_str)
{
    bool result = true;

    if (new_watermark > params_.internal_high_watermark_) {
        params_.internal_low_watermark_ = params_.internal_high_watermark_;
        params_.internal_high_watermark_ = new_watermark;

        result_str = "Success - new low watermark is higher than the high watermark so swapping them -  Low: " +
                     std::to_string(params_.internal_low_watermark_) + "% High: " + 
                     std::to_string(params_.internal_high_watermark_) + "%";
    } else {
        params_.internal_low_watermark_ = new_watermark;

        result_str = "Success - Low: " +
                     std::to_string(params_.internal_low_watermark_) + "% High: " + 
                     std::to_string(params_.internal_high_watermark_) + "%";
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_get_internal_high_watermark(std::string& result_str)
{
    bool result = true;
    result_str = std::to_string(params_.internal_high_watermark_) + "%";
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_set_internal_high_watermark(size_t new_watermark, std::string& result_str)
{
    bool result = true;

    if (new_watermark < params_.internal_low_watermark_) {
        params_.internal_high_watermark_ = params_.internal_low_watermark_;
        params_.internal_low_watermark_ = new_watermark;

        result_str = "Success - new high watermark is higher than the high watermark so swapping - Low: " +
                     std::to_string(params_.internal_low_watermark_) + "% High: " + 
                     std::to_string(params_.internal_high_watermark_) + "%";
    } else {
        params_.internal_high_watermark_ = new_watermark;

        result_str = "Success - Low: " +
                     std::to_string(params_.internal_low_watermark_) + "% High: " + 
                     std::to_string(params_.internal_high_watermark_) + "%";
    }

    return result;
}


//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_get_external_low_watermark(std::string& result_str)
{
    bool result = true;
    result_str = std::to_string(params_.external_low_watermark_) + "%";
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_set_external_low_watermark(size_t new_watermark, std::string& result_str)
{
    bool result = true;

    if (new_watermark > params_.external_high_watermark_) {
        params_.external_low_watermark_ = params_.external_high_watermark_;
        params_.external_high_watermark_ = new_watermark;

        result_str = "Success - new low watermark is higher than the high watermark so swapping them -  Low: " +
                     std::to_string(params_.external_low_watermark_) + "% High: " + 
                     std::to_string(params_.external_high_watermark_) + "%";
    } else {
        params_.external_low_watermark_ = new_watermark;

        result_str = "Success - Low: " +
                     std::to_string(params_.external_low_watermark_) + "% High: " + 
                     std::to_string(params_.external_high_watermark_) + "%";
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_get_external_high_watermark(std::string& result_str)
{
    bool result = true;
    result_str = std::to_string(params_.external_high_watermark_) + "%";
    
    return result;
}

//----------------------------------------------------------------------
bool
BundleArchitecturalRestagingDaemon::bardcmd_set_external_high_watermark(size_t new_watermark, std::string& result_str)
{
    bool result = true;

    if (new_watermark < params_.external_low_watermark_) {
        params_.external_high_watermark_ = params_.external_low_watermark_;
        params_.external_low_watermark_ = new_watermark;

        result_str = "Success - new high watermark is higher than the high watermark so swapping - Low: " +
                     std::to_string(params_.external_low_watermark_) + "% High: " + 
                     std::to_string(params_.external_high_watermark_) + "%";
    } else {
        params_.external_high_watermark_ = new_watermark;

        result_str = "Success - Low: " +
                     std::to_string(params_.external_low_watermark_) + "% High: " + 
                     std::to_string(params_.external_high_watermark_) + "%";
    }

    return result;
}



//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::register_restage_cl(SPtr_RestageCLStatus sptr_clstatus)
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);

    // add to the list of Restage CLs

    RestageCLIterator iter = restagecl_map_.find(sptr_clstatus->restage_link_name());

    if (iter != restagecl_map_.end()) {
        log_err("Received RestageCL registration with an existing name (%s); Releasing old instance and adding new one", 
                 sptr_clstatus->restage_link_name().c_str());

        restagecl_map_.erase(sptr_clstatus->restage_link_name());
    }
    
    restagecl_map_[sptr_clstatus->restage_link_name()] = sptr_clstatus;
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::unregister_restage_cl(std::string& link_name)
{
    // remove from the list of Restage CLs

    oasys::ScopeLock scoplock(&lock_, __func__);
    
    restagecl_map_.erase(link_name);

}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::update_restage_usage_stats(bard_quota_type_t quota_type, 
                                                  bard_quota_naming_schemes_t naming_scheme,
                                                  std::string& nodename, 
                                                  size_t num_files, size_t disk_usage)
{
    std::string key = BARDNodeStorageUsage::make_key(quota_type, naming_scheme, nodename);

    oasys::ScopeLock scoplock(&lock_, __func__);

    SPtr_BARDNodeStorageUsage sptr_usage;
    BARDNodeStorageUsageMap::iterator iter = usage_map_.find(key);

    if (iter != usage_map_.end()) {
        sptr_usage = iter->second;
    } else {
        sptr_usage = std::make_shared<BARDNodeStorageUsage>(quota_type, naming_scheme, nodename);

        usage_map_[key] = sptr_usage;
    }

    sptr_usage->inuse_external_bundles_+= num_files;
    sptr_usage->inuse_external_bytes_ += disk_usage;
}


//----------------------------------------------------------------------
std::string
BundleArchitecturalRestagingDaemon::make_bardnodestorageusage_key(bard_quota_type_t quota_type, const SPtr_EID& sptr_eid)
{
    if (sptr_eid->is_ipn_scheme()) {
        return BARDNodeStorageUsage::make_key(quota_type, BARD_QUOTA_NAMING_SCHEME_IPN, sptr_eid->node_num());

    } else if (sptr_eid->is_imc_scheme()) {
        return BARDNodeStorageUsage::make_key(quota_type, BARD_QUOTA_NAMING_SCHEME_IMC, sptr_eid->node_num());

    } else {
        std::string nodename = sptr_eid->ssp();
        return BARDNodeStorageUsage::make_key(quota_type, BARD_QUOTA_NAMING_SCHEME_DTN, nodename);
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::endpoint_to_key_components(const SPtr_EID& sptr_eid, bard_quota_naming_schemes_t& scheme, 
                                                  std::string& nodename)
{
    scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    nodename = "";


    if (sptr_eid->is_ipn_scheme()) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IPN;
        nodename = std::to_string(sptr_eid->node_num());

    } else if (sptr_eid->is_dtn_scheme()) {
        scheme = BARD_QUOTA_NAMING_SCHEME_DTN;

        nodename = sptr_eid->ssp();

    } else if (sptr_eid->is_imc_scheme()) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IMC;
        nodename = std::to_string(sptr_eid->node_num());
    }
}


//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::generate_usage_report_for_extrtr(BARDNodeStorageUsageMap& bard_usage_map,
                                                        RestageCLMap& restage_cl_map)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    extrtr_usage_rpt_load_usage_map(bard_usage_map);
    extrtr_usage_rpt_load_restage_cl_map(restage_cl_map);

}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::extrtr_usage_rpt_load_usage_map(BARDNodeStorageUsageMap& bard_usage_map)
{
    ASSERT(lock_.is_locked_by_me());

    SPtr_BARDNodeStorageUsage sptr_usage;
    SPtr_BARDNodeStorageUsage sptr_usage_copy;


    BARDNodeStorageUsageMap::iterator iter = usage_map_.begin();

    while (iter != usage_map_.end()) {
        sptr_usage = iter->second;

        sptr_usage_copy = std::make_shared<BARDNodeStorageUsage>();

        sptr_usage_copy->copy_data(sptr_usage.get());

        bard_usage_map[sptr_usage_copy->key()] = sptr_usage_copy;

        iter++;
    }
}

//----------------------------------------------------------------------
void
BundleArchitecturalRestagingDaemon::extrtr_usage_rpt_load_restage_cl_map(RestageCLMap& restage_cl_map)
{
    ASSERT(lock_.is_locked_by_me());

    SPtr_RestageCLStatus sptr_clstatus;
    SPtr_RestageCLStatus sptr_clstatus_copy;

    // loop through the list of RestageCLs
    RestageCLIterator iter = restagecl_map_.begin();

    while (iter != restagecl_map_.end()) {

        sptr_clstatus = iter->second;

        oasys::ScopeLock clstatus_scoplock(sptr_clstatus->lock(), __func__);


        sptr_clstatus_copy = std::make_shared<RestageCLStatus>();


        std::string tmp_str = sptr_clstatus->restage_link_name();
        sptr_clstatus_copy->set_restage_link_name(tmp_str);

        tmp_str = sptr_clstatus->storage_path();
        sptr_clstatus_copy->set_storage_path(tmp_str);

        tmp_str = sptr_clstatus->validated_mount_pt();
        sptr_clstatus_copy->set_validated_mount_pt(tmp_str);

        sptr_clstatus_copy->set_mount_point(sptr_clstatus->mount_point());
        sptr_clstatus_copy->set_mount_pt_validated(sptr_clstatus->mount_pt_validated());
        sptr_clstatus_copy->set_storage_path_exists(sptr_clstatus->storage_path_exists());
        sptr_clstatus_copy->set_part_of_pool(sptr_clstatus->part_of_pool());
        sptr_clstatus_copy->set_vol_total_space(sptr_clstatus->vol_total_space());
        sptr_clstatus_copy->set_vol_space_available(sptr_clstatus->vol_space_available());
        sptr_clstatus_copy->set_disk_quota(sptr_clstatus->disk_quota());
        sptr_clstatus_copy->set_disk_quota_in_use(sptr_clstatus->disk_quota_in_use());
        sptr_clstatus_copy->set_disk_num_files(sptr_clstatus->disk_num_files());
        sptr_clstatus_copy->set_days_retention(sptr_clstatus->days_retention());
        sptr_clstatus_copy->set_expire_bundles(sptr_clstatus->expire_bundles());
        sptr_clstatus_copy->set_ttl_override(sptr_clstatus->ttl_override());
        sptr_clstatus_copy->set_auto_reload_interval(sptr_clstatus->auto_reload_interval());
        sptr_clstatus_copy->set_cl_state(sptr_clstatus->cl_state());

        restage_cl_map[sptr_clstatus_copy->restage_link_name()] = sptr_clstatus_copy;

        ++iter;
    }
}



} // namespace dtn

#endif  // BARD_ENABLED
