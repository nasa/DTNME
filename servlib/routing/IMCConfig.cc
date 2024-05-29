/*
 *    Copyright 2022 United States Government as represented by NASA
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

#include <mutex>

#include "IMCConfig.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocolVersion7.h"
#include "bundling/SDNV.h"
#include "naming/IPNScheme.h"
#include "storage/IMCRegionGroupRecStore.h"

#define SET_FLDNAMES(fld) \
    cborutil.set_fld_name(fld);

#define CHECK_CBOR_ENCODE_ERR_RETURN \
    if (err && (err != CborErrorOutOfMemory)) \
    { \
      return CBORUTIL_FAIL; \
    }

#define CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE \
    if (err && (err != CborErrorOutOfMemory)) \
    { \
      return err; \
    }

#define CHECK_CBOR_DECODE_ERR \
    if (err != CborNoError) \
    { \
      return false; \
    }

#define CHECK_CBOR_STATUS \
    if (status != CBORUTIL_SUCCESS) \
    { \
      return false; \
    }

namespace dtn {

IMCConfig::IMCConfig()
{
    sptr_home_region_rec_ = std::make_shared<IMCRegionGroupRec>(INC_REC_TYPE_HOME_REGION);
    sptr_clear_region_db_rec_ = std::make_shared<IMCRegionGroupRec>(IMC_REC_TYPE_REGION_DB_CLEAR);
    sptr_clear_group_db_rec_ = std::make_shared<IMCRegionGroupRec>(IMC_REC_TYPE_GROUP_DB_CLEAR);
    sptr_clear_manual_join_db_rec_ = std::make_shared<IMCRegionGroupRec>(IMC_REC_TYPE_MANUAL_JOIN_DB_CLEAR);

}

//----------------------------------------------------------------------
void
IMCConfig::add_or_update_in_datastore(SPtr_IMCRegionGroupRec& sptr_rec)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // add/update the record in the database
    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->begin_transaction();

    imc_store->lock_db_access("IMCConfig::add_or_update_in_datastore");

    if (sptr_rec->in_datastore_) {

        if (! imc_store->update(sptr_rec.get())) {
            log_crit_p("/imc/cfg", "error updating IMC Region/Group Rec %s in data store!!",
                     sptr_rec->durable_key().c_str());
        }

    } else {
        if (! imc_store->add(sptr_rec.get())) {
            log_crit_p("/imc/cfg", "error adding IMC Region/Group Rec %s to data store!!",
                     sptr_rec->durable_key().c_str());
        }
        sptr_rec->in_datastore_ = true;

    }

    imc_store->unlock_db_access();

    imc_store->end_transaction();
}

//----------------------------------------------------------------------
void
IMCConfig::delete_from_datastore(std::string key_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // delete the record from the datastore
    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->begin_transaction();

    imc_store->lock_db_access("IMCConfig::delete_from_datastore");

    if (! imc_store->del(key_str)) {
        log_crit_p("/imc/cfg", "error deleting IMC Region/Group Rec %s from  data store!!",
                 key_str.c_str());
    }

    imc_store->unlock_db_access();

    imc_store->end_transaction();
}

//----------------------------------------------------------------------
void
IMCConfig::load_imc_config_overrides()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    check_for_clear_region_db_at_startup();
    check_for_clear_group_db_at_startup();
    check_for_clear_manual_join_db_at_startup();

    SPtr_IMC_REGION_MAP sptr_region_map;
    size_t current_region = 0;

    SPtr_IMC_GROUP_MAP sptr_group_map;
    size_t current_group = 0;

    IMCRegionGroupRec* rec_ptr;
    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    // locking out DB access during startup which should not be for very long
    imc_store->lock_db_access("IMCConfig::load_imc_config_overrides(1)");

    IMCRegionGroupRecStore::iterator* imc_store_iter = imc_store->new_iterator();

    log_info_p("/imc/cfg", "loading IMC Region/Group Recs from data store");

    int status;

    for (status = imc_store_iter->begin(); imc_store_iter->more(); status = imc_store_iter->next()) {
        if ( oasys::DS_NOTFOUND == status ) {
            log_warn_p("/imc/cfg", "received DS_NOTFOUND from data store - IMC Region/Group Rec <%s>",
                    imc_store_iter->cur_val().c_str());
            break;
        } else if ( oasys::DS_ERR == status ) {
            log_err_p("/imc/cfg", "error loading IMC Region/Group Rec <%s> from data store",
                    imc_store_iter->cur_val().c_str());
            break;
        }

        rec_ptr = imc_store->get(imc_store_iter->cur_val());
        SPtr_IMCRegionGroupRec sptr_rec(rec_ptr);
        
        if (sptr_rec == nullptr) {
            log_err_p("/imc/cfg", "error loading IMC Region/Group Rec <%s> from data store",
                    imc_store_iter->cur_val().c_str());
            continue;
        }

        sptr_rec->in_datastore_ = true;

        if (sptr_rec->durable_key().find(IMC_REC_TYPE_REGION_STR) == 0) {
            if (!clear_imc_region_db_on_startup_) {
                if (current_region != sptr_rec->region_or_group_num_) {
                    current_region = sptr_rec->region_or_group_num_;
                    sptr_region_map = get_region_map(current_region);
                }

                apply_override_region_rec(sptr_region_map, sptr_rec);
            }

        } else if (sptr_rec->durable_key().find(IMC_REC_TYPE_GROUP_STR) == 0) {
            if (!clear_imc_group_db_on_startup_) {
                if (current_group != sptr_rec->region_or_group_num_) {
                    current_group = sptr_rec->region_or_group_num_;
                    sptr_group_map = get_group_map(current_group);
                }

                apply_override_group_rec(sptr_group_map, sptr_rec);
            }
        } else if (sptr_rec->durable_key().find(IMC_REC_TYPE_MANUAL_JOIN_STR) == 0) {
            if (!clear_imc_manual_join_db_on_startup_) {
                apply_override_manual_join_rec(sptr_rec);
            }
        } else {
            // not a record type of interest
        }
    }

    delete imc_store_iter;

    imc_store->unlock_db_access();
}

//----------------------------------------------------------------------
void
IMCConfig::apply_override_region_rec(SPtr_IMC_REGION_MAP& sptr_region_map, SPtr_IMCRegionGroupRec& sptr_rec)
{
    ASSERT(sptr_region_map != nullptr);
    ASSERT(sptr_rec != nullptr);

    // Add each node to the region if it is not already there
    IMC_REGION_MAP::iterator iter_region_map;

    iter_region_map = sptr_region_map->find(sptr_rec->node_or_id_num_);

    if (iter_region_map == sptr_region_map->end()) {
        // rec not found so add it
        (*sptr_region_map)[sptr_rec->node_or_id_num_] = sptr_rec;
    } else {
        SPtr_IMCRegionGroupRec sptr_old_rec = iter_region_map->second;

        sptr_old_rec->operation_ = sptr_rec->operation_;
        sptr_old_rec->in_datastore_ = true;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::apply_override_group_rec(SPtr_IMC_GROUP_MAP& sptr_group_map, SPtr_IMCRegionGroupRec& sptr_rec)
{
    ASSERT(sptr_group_map != nullptr);
    ASSERT(sptr_rec != nullptr);

    // Add each node to the group if it is not already there
    IMC_GROUP_MAP::iterator iter_group_map;

    iter_group_map = sptr_group_map->find(sptr_rec->node_or_id_num_);

    if (iter_group_map == sptr_group_map->end()) {
        // rec not found so add it
        (*sptr_group_map)[sptr_rec->node_or_id_num_] = sptr_rec;
    } else {
        SPtr_IMCRegionGroupRec sptr_old_rec = iter_group_map->second;

        sptr_old_rec->operation_ = sptr_rec->operation_;
        sptr_old_rec->in_datastore_ = true;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::apply_override_manual_join_rec(SPtr_IMCRegionGroupRec& sptr_rec)
{
    ASSERT(sptr_rec != nullptr);

    char tmp_buf[128];
    snprintf(tmp_buf, sizeof(tmp_buf), "imc:%zu.%zu", 
             sptr_rec->region_or_group_num_, sptr_rec->node_or_id_num_);

    std::string imc_eid_str = tmp_buf;

    auto iter = manual_joins_map_.find(imc_eid_str);

    if (iter == manual_joins_map_.end()) {
        // rec not found so add it
        manual_joins_map_[imc_eid_str] = sptr_rec;
    } else {
        SPtr_IMCRegionGroupRec sptr_old_rec = iter->second;

        sptr_old_rec->operation_ = sptr_rec->operation_;
        sptr_old_rec->in_datastore_ = true;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::check_for_clear_region_db_at_startup()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->lock_db_access("IMCConfig::check_for_clear_region_db_at_startup");

    std::string key_str(IMC_REC_CLEAR_REGION_DB_KEY_STR);
    IMCRegionGroupRec* tmp_ptr_rec = imc_store->get(key_str);
    SPtr_IMCRegionGroupRec sptr_rec(tmp_ptr_rec);

    imc_store->unlock_db_access();

    if (clear_imc_region_db_on_startup_) {
        if (sptr_rec != nullptr) {
            if (clear_imc_region_db_id_ == 0) {
                // clear always - can delete the existing rec
                delete_from_datastore(sptr_rec->durable_key());

                sptr_clear_region_db_rec_->node_or_id_num_ = 0;
                sptr_clear_region_db_rec_->in_datastore_ = false;

            } else if (sptr_rec->node_or_id_num_ != clear_imc_region_db_id_) {
                // Clear ID is different from last saved value
                // update the rec and continue with the deletions
                sptr_clear_region_db_rec_->node_or_id_num_ = clear_imc_region_db_id_;
                sptr_clear_region_db_rec_->in_datastore_ = true;

                add_or_update_in_datastore(sptr_clear_region_db_rec_);

            } else {
                // Clear ID is same as last saved value
                // prevent the deletions
                clear_imc_region_db_on_startup_ = false;

                sptr_clear_region_db_rec_->node_or_id_num_ = clear_imc_region_db_id_;
                sptr_clear_region_db_rec_->in_datastore_ = true;
            }
            
        } else {
            if (clear_imc_region_db_id_ != 0) {
                // save this ID in the database so it will not cause deletions on restart
                sptr_clear_region_db_rec_->node_or_id_num_ = clear_imc_region_db_id_;

                add_or_update_in_datastore(sptr_clear_region_db_rec_);
            }
        }
    } else {
        // started up without clear command so remove the rec from the db
        if (sptr_rec != nullptr) {
            delete_from_datastore(sptr_rec->durable_key());
        }

        sptr_clear_region_db_rec_->node_or_id_num_ = 0;
        sptr_clear_region_db_rec_->in_datastore_ = false;
    }

    // if flag to clear db is still set then do the clear now
    if (clear_imc_region_db_on_startup_) {
        delete_all_region_recs_from_datastore();
    }

}

//----------------------------------------------------------------------
void
IMCConfig::check_for_clear_group_db_at_startup()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->lock_db_access("IMCConfig::check_for_clear_group_db_at_startup");

    std::string key_str(IMC_REC_CLEAR_GROUP_DB_KEY_STR);
    IMCRegionGroupRec* tmp_ptr_rec = imc_store->get(key_str);
    SPtr_IMCRegionGroupRec sptr_rec(tmp_ptr_rec);

    imc_store->unlock_db_access();

    if (clear_imc_group_db_on_startup_) {
        if (sptr_rec != nullptr) {
            if (clear_imc_group_db_id_ == 0) {
                // clear always - can delete the existing rec
                delete_from_datastore(sptr_rec->durable_key());

                sptr_clear_group_db_rec_->node_or_id_num_ = 0;
                sptr_clear_group_db_rec_->in_datastore_ = false;

            } else if (sptr_rec->node_or_id_num_ != clear_imc_group_db_id_) {
                // Clear ID is different from last saved value
                // update the rec and continue with the deletions
                sptr_clear_group_db_rec_->node_or_id_num_ = clear_imc_group_db_id_;
                sptr_clear_group_db_rec_->in_datastore_ = true;

                add_or_update_in_datastore(sptr_clear_group_db_rec_);

            } else {
                // Clear ID is same as last saved value
                // prevent the deletions
                clear_imc_group_db_on_startup_ = false;

                sptr_clear_group_db_rec_->node_or_id_num_ = clear_imc_group_db_id_;
                sptr_clear_group_db_rec_->in_datastore_ = true;
            }
            
        } else {
            if (clear_imc_group_db_id_ != 0) {
                // save this ID in the database so it will not cause deletions on restart
                sptr_clear_group_db_rec_->node_or_id_num_ = clear_imc_group_db_id_;

                add_or_update_in_datastore(sptr_clear_group_db_rec_);
            }
        }
    } else {
        // started up without clear command so remove the rec from the db
        if (sptr_rec != nullptr) {
            delete_from_datastore(sptr_rec->durable_key());
        }

        sptr_clear_group_db_rec_->node_or_id_num_ = 0;
        sptr_clear_group_db_rec_->in_datastore_ = false;
    }


    // if flag to clear db is still set then do the clear now
    if (clear_imc_group_db_on_startup_) {
        delete_all_group_recs_from_datastore();
    }

}

//----------------------------------------------------------------------
void
IMCConfig::check_for_clear_manual_join_db_at_startup()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->lock_db_access("IMCConfig::check_for_clear_manual_join_db_at_startup");

    std::string key_str(IMC_REC_CLEAR_MANUAL_JOIN_DB_KEY_STR);
    IMCRegionGroupRec* tmp_ptr_rec = imc_store->get(key_str);
    SPtr_IMCRegionGroupRec sptr_rec(tmp_ptr_rec);

    imc_store->unlock_db_access();

    if (clear_imc_manual_join_db_on_startup_) {
        if (sptr_rec != nullptr) {
            if (clear_imc_manual_join_db_id_ == 0) {
                // clear always - can delete the existing rec
                delete_from_datastore(sptr_rec->durable_key());

                sptr_clear_manual_join_db_rec_->node_or_id_num_ = 0;
                sptr_clear_manual_join_db_rec_->in_datastore_ = false;

            } else if (sptr_rec->node_or_id_num_ != clear_imc_manual_join_db_id_) {
                // Clear ID is different from last saved value
                // update the rec and continue with the deletions
                sptr_clear_manual_join_db_rec_->node_or_id_num_ = clear_imc_manual_join_db_id_;
                sptr_clear_manual_join_db_rec_->in_datastore_ = true;

                add_or_update_in_datastore(sptr_clear_manual_join_db_rec_);

            } else {
                // Clear ID is same as last saved value
                // prevent the deletions
                clear_imc_manual_join_db_on_startup_ = false;

                sptr_clear_manual_join_db_rec_->node_or_id_num_ = clear_imc_manual_join_db_id_;
                sptr_clear_manual_join_db_rec_->in_datastore_ = true;
            }
            
        } else {
            if (clear_imc_manual_join_db_id_ != 0) {
                // save this ID in the database so it will not cause deletions on restart
                sptr_clear_manual_join_db_rec_->node_or_id_num_ = clear_imc_manual_join_db_id_;

                add_or_update_in_datastore(sptr_clear_manual_join_db_rec_);
            }
        }
    } else {
        // started up without clear command so remove the rec from the db
        if (sptr_rec != nullptr) {
            delete_from_datastore(sptr_rec->durable_key());
        }

        sptr_clear_manual_join_db_rec_->node_or_id_num_ = 0;
        sptr_clear_manual_join_db_rec_->in_datastore_ = false;
    }


    // if flag to clear db is still set then do the clear now
    if (clear_imc_manual_join_db_on_startup_) {
        delete_all_manual_join_recs_from_datastore();
    }

}

//----------------------------------------------------------------------
SPtr_IMC_REGION_MAP
IMCConfig::get_region_map(size_t region_num)
{
    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // Find the map for this region or create it if not found
    iter_imc_region_maps = map_of_region_maps_.find(region_num);

    if (iter_imc_region_maps == map_of_region_maps_.end()) {
        sptr_region_map = std::make_shared<IMC_REGION_MAP>();
        map_of_region_maps_[region_num] = sptr_region_map;
    } else {
        sptr_region_map = iter_imc_region_maps->second;
    }

    return sptr_region_map;
}

//----------------------------------------------------------------------
SPtr_IMC_GROUP_MAP
IMCConfig::get_group_map(size_t group_num)
{
    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // Find the map for this group or create it if not found
    iter_imc_group_maps = map_of_group_maps_.find(group_num);

    if (iter_imc_group_maps == map_of_group_maps_.end()) {
        sptr_group_map = std::make_shared<IMC_GROUP_MAP>();
        map_of_group_maps_[group_num] = sptr_group_map;
    } else {
        sptr_group_map = iter_imc_group_maps->second;
    }

    return sptr_group_map;
}

//----------------------------------------------------------------------
SPtr_IMC_GROUP_MAP
IMCConfig::get_group_map_for_outer_regions(size_t group_num)
{
    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // Find the map for this group or create it if not found
    iter_imc_group_maps = map_of_group_maps_for_outer_regions_.find(group_num);

    if (iter_imc_group_maps == map_of_group_maps_for_outer_regions_.end()) {
        sptr_group_map = std::make_shared<IMC_GROUP_MAP>();
        map_of_group_maps_for_outer_regions_[group_num] = sptr_group_map;
    } else {
        sptr_group_map = iter_imc_group_maps->second;
    }

    return sptr_group_map;
}


//----------------------------------------------------------------------
size_t
IMCConfig::home_region()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    return home_region_;
}

//----------------------------------------------------------------------
void
IMCConfig::set_home_region(size_t region_num, oasys::StringBuffer& buf, bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

    if (my_ipn_node_num == 0) {
        buf.append("error - local_eid_ipn must be set before setting the imc_home_region");
        return;
    }

    bool home_region_changed = false;
    size_t old_home_region = home_region_;

    SPtr_IMC_REGION_MAP sptr_region_map;

    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;

    if ((home_region_ != 0) && (home_region_ != region_num)) {
        // remove from the old region
        home_region_changed = true;

        sptr_region_map = get_region_map(home_region_);

        // Remove the node from the region if it is a member
        iter_region_map = sptr_region_map->find(my_ipn_node_num);

        if (iter_region_map == sptr_region_map->end()) {
            // shouldn't happen?

            sptr_rec = std::make_shared<IMCRegionGroupRec>(IMC_REC_TYPE_REGION);

            sptr_rec->region_or_group_num_ = region_num;
            sptr_rec->node_or_id_num_ = my_ipn_node_num;
            sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

            (*sptr_region_map)[my_ipn_node_num] = sptr_rec;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        } else {
            sptr_rec = iter_region_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_REMOVE) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                }
            }
        }
    }

    if (home_region_ != region_num) {
        // set the new home region
        home_region_ = region_num;

        if (home_region_ != 0) {
            // add this node to the new region

            sptr_region_map = get_region_map(home_region_);

            // Remove the node from the region if it is a member
            iter_region_map = sptr_region_map->find(my_ipn_node_num);

            if (iter_region_map == sptr_region_map->end()) {
                sptr_rec = std::make_shared<IMCRegionGroupRec>(IMC_REC_TYPE_REGION);

                sptr_rec->region_or_group_num_ = region_num;
                sptr_rec->node_or_id_num_ = my_ipn_node_num;
                sptr_rec->operation_ = IMC_REC_OPERATION_ADD;
                sptr_rec->is_router_node_ = is_imc_router_node_;

                (*sptr_region_map)[my_ipn_node_num] = sptr_rec;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                }
            } else {
                sptr_rec = iter_region_map->second;

                if ((!sptr_rec->in_datastore_) ||
                    (sptr_rec->operation_ != IMC_REC_OPERATION_ADD) ||
                    (sptr_rec->is_router_node_ != is_imc_router_node_)) {

                    sptr_rec->operation_ = IMC_REC_OPERATION_ADD;
                    sptr_rec->is_router_node_ = is_imc_router_node_;

                    if (db_update && imc_router_active_) {
                        add_or_update_in_datastore(sptr_rec);
                    }
                }
            }
        }
    }

    sptr_home_region_rec_->region_or_group_num_ = home_region_;

    if (db_update && imc_router_active_) {
        // write a home region record to the database
        add_or_update_in_datastore(sptr_home_region_rec_);
    }

    if (home_region_changed) {
        buf.appendf("IMC Home Region changed from %zu to %zu for locel node #%zu",
                    old_home_region, home_region_, my_ipn_node_num);
    } else {
        buf.appendf("IMC Home Region set to %zu for locel node #%zu",
                    home_region_, my_ipn_node_num);
    }
}

//----------------------------------------------------------------------
bool
IMCConfig::imc_router_node_designation()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    return is_imc_router_node_;
}

//----------------------------------------------------------------------
void
IMCConfig::set_imc_router_node_designation(bool designation, bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // check if designation changed
    // to true:
    //    - send out briefing request
    // to false:
    //    - clear config?


    is_imc_router_node_ = designation;

    // update the home region record

    sptr_home_region_rec_->is_router_node_ = designation;

    if (db_update && imc_router_active_) {
        // write a home region record to the database
        add_or_update_in_datastore(sptr_home_region_rec_);
    }

    std::string dummy_str;
    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

    add_node_range_to_region(home_region_, is_imc_router_node_, 
                             my_ipn_node_num, my_ipn_node_num,
                             dummy_str, db_update);

//XXX/TODO    if (changed) {  ???
//    } else {
//    }
}

//----------------------------------------------------------------------
void
IMCConfig::clear_imc_region_db(size_t id_num, std::string& result_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (!imc_router_active_) {
        clear_imc_region_db_id_ = id_num;
        clear_imc_region_db_on_startup_ = true;

        result_str = "clear_imc_region_db request will be processed when the IMC Router starts up";

    } else {
        delete_all_region_recs_from_datastore();

        result_str = "clear_imc_region_db request has been processed";
    }
}

//----------------------------------------------------------------------
void
IMCConfig::clear_imc_group_db(size_t id_num, std::string& result_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (!imc_router_active_) {
        clear_imc_group_db_id_ = id_num;
        clear_imc_group_db_on_startup_ = true;

        result_str = "clear_imc_group_db request will be processed when the IMC Router starts up";

    } else {
        delete_all_group_recs_from_datastore();

        result_str = "clear_imc_group_db request has been processed";
    }
}

//----------------------------------------------------------------------
void
IMCConfig::clear_imc_manual_join_db(size_t id_num, std::string& result_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (!imc_router_active_) {
        clear_imc_manual_join_db_id_ = id_num;
        clear_imc_manual_join_db_on_startup_ = true;

        result_str = "clear_imc_manual_join_db request will be processed when the IMC Router starts up";

    } else {
        delete_all_manual_join_recs_from_datastore();

        result_str = "clear_imc_manual_join_db request has been processed";
    }
}

//----------------------------------------------------------------------
void
IMCConfig::add_node_range_to_region(size_t region_num, bool is_router_node, size_t node_num_start, 
                                    size_t node_num_end, std::string& result_str, 
                                    bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (region_num == 0) {
        result_str = "error - region number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    // Find the map for this region or create it if not found
    SPtr_IMC_REGION_MAP sptr_region_map = get_region_map(region_num);

    // Add each node to the region if it is not already there
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_region_map = sptr_region_map->find(ix);

        if (iter_region_map == sptr_region_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_REGION;
            sptr_rec->region_or_group_num_ = region_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->is_router_node_ = is_router_node;
            sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

            (*sptr_region_map)[ix] = sptr_rec;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        } else {
            sptr_rec = iter_region_map->second;

            if ((!sptr_rec->in_datastore_) ||
                (sptr_rec->operation_ != IMC_REC_OPERATION_ADD) ||
                (sptr_rec->is_router_node_ != is_router_node)) {

                sptr_rec->operation_ = IMC_REC_OPERATION_ADD;
                sptr_rec->is_router_node_ = is_router_node;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                }
            }
        }
    }

    if (node_num_end == node_num_start) {
        result_str = "Added node ";
        result_str.append(std::to_string(node_num_start));
    } else {
        result_str = "Added nodes from ";
        result_str.append(std::to_string(node_num_start));
        result_str.append(" thru ");
        result_str.append(std::to_string(node_num_end));
    }
    result_str.append(" to Region #");
    result_str.append(std::to_string(region_num));
}

//----------------------------------------------------------------------
void
IMCConfig::del_node_range_from_region(size_t region_num, size_t node_num_start, 
                                                 size_t node_num_end, std::string& result_str, 
                                                 bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (region_num == 0) {
        result_str = "error - region number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    // Find the map for this region or create it if not found
    SPtr_IMC_REGION_MAP sptr_region_map = get_region_map(region_num);

    // Remove each node from the region if it is a member
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_region_map = sptr_region_map->find(ix);

        if (iter_region_map == sptr_region_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_REGION;
            sptr_rec->region_or_group_num_ = region_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

            (*sptr_region_map)[ix] = sptr_rec;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        } else {
            sptr_rec = iter_region_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_REMOVE) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                }
            }
        }
    }

    if (node_num_end == node_num_start) {
        result_str = "Removed node ";
        result_str.append(std::to_string(node_num_start));
    } else {
        result_str = "Removed nodes from ";
        result_str.append(std::to_string(node_num_start));
        result_str.append(" thru ");
        result_str.append(std::to_string(node_num_end));
    }
    result_str.append(" from Region #");
    result_str.append(std::to_string(region_num));
}

//----------------------------------------------------------------------
bool
IMCConfig::is_node_in_home_region(size_t node_num)
{
    return is_node_in_region(node_num, home_region());
}

//----------------------------------------------------------------------
bool
IMCConfig::is_node_in_region(size_t node_num, size_t region_num)
{
    bool result = false;

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // find the single region
    iter_imc_region_maps = map_of_region_maps_.find(region_num);

    if (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;


        SPtr_IMCRegionGroupRec sptr_rec;
        IMC_REGION_MAP::iterator iter_region_map;
        iter_region_map = sptr_region_map->find(node_num);

        if (iter_region_map != sptr_region_map->end()) {
            sptr_rec = iter_region_map->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                result = true;
            }
        }
    }

    return result;
}


//----------------------------------------------------------------------
bool
IMCConfig::have_outer_region_router_nodes()
{
    bool result = false;

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;

    // loop through the region maps
    iter_imc_region_maps = map_of_region_maps_.begin();

    while (iter_imc_region_maps != map_of_region_maps_.end()) {

        if (iter_imc_region_maps->first != home_region()) {
            sptr_region_map = iter_imc_region_maps->second;

            // loop through this region map
            iter_region_map = sptr_region_map->begin();

            while (iter_region_map != sptr_region_map->end()) {
                sptr_rec = iter_region_map->second;

                if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                    result = sptr_rec->is_router_node_;
                    if (result) break;
                }

                ++iter_region_map;
            }
        }

        if (result) break;
        ++iter_imc_region_maps;
    }

    return result;
}

//----------------------------------------------------------------------
bool
IMCConfig::have_any_other_router_nodes(size_t skip_node_num)
{
    bool result = false;

    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;

    // loop through the region maps
    iter_imc_region_maps = map_of_region_maps_.begin();

    while (iter_imc_region_maps != map_of_region_maps_.end()) {

        sptr_region_map = iter_imc_region_maps->second;

        // loop through this region map
        iter_region_map = sptr_region_map->begin();

        while (iter_region_map != sptr_region_map->end()) {
            sptr_rec = iter_region_map->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                if ((sptr_rec->node_or_id_num_ != my_ipn_node_num) &&
                    (sptr_rec->node_or_id_num_ != skip_node_num)) {
                    result = sptr_rec->is_router_node_;
                    if (result) break;
                }
            }

            ++iter_region_map;
        }

        if (result) break;
        ++iter_imc_region_maps;
    }

    return result;
}


//----------------------------------------------------------------------
void
IMCConfig::imc_region_dump(size_t region_num, oasys::StringBuffer& buf)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    
    buf.appendf("Home Region = %zu\n", sptr_home_region_rec_->region_or_group_num_);

    if (sptr_clear_region_db_rec_->in_datastore_) {
        buf.appendf("Stored clear_region_db ID = %zu\n", sptr_clear_region_db_rec_->node_or_id_num_);
    }

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    if (region_num == 0) {
        // loop through all regions and dump them
        iter_imc_region_maps = map_of_region_maps_.begin();

        while (iter_imc_region_maps != map_of_region_maps_.end()) {
            region_num = iter_imc_region_maps->first;
            sptr_region_map = iter_imc_region_maps->second;
            dump_single_imc_region(region_num, sptr_region_map, buf);

            ++iter_imc_region_maps;
        }
    } else {
        // find the single region to dump
        iter_imc_region_maps = map_of_region_maps_.find(region_num);

        if (iter_imc_region_maps == map_of_region_maps_.end()) {
            buf.append("no nodes configured for IMC Region #%zu\n", region_num);
        } else {
            sptr_region_map = iter_imc_region_maps->second;
            dump_single_imc_region(region_num, sptr_region_map, buf);
        }
    }
}

//----------------------------------------------------------------------
void
IMCConfig::dump_single_imc_region(size_t region_num, SPtr_IMC_REGION_MAP& sptr_region_map, oasys::StringBuffer& buf)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    buf.appendf("Region #%zu contains %zu records (\"*\" indicates IMC Router Node):\n", region_num, sptr_region_map->size());

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_region_map->begin();

    char router_node_char = ' ';
    size_t ctr_removed = 0;
    size_t ctr = 0;
    while (iter_region_map != sptr_region_map->end()) {
        sptr_rec = iter_region_map->second;

        if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
            if ((ctr % 8) == 0) {
                // indent the line
                if (ctr != 0) {
                    buf.append("\n");
                }
                buf.appendf("    ");
            }

            ++ctr;
            router_node_char = sptr_rec->is_router_node_ ? '*' : ' ';
            buf.appendf("%7zu%c ", sptr_rec->node_or_id_num_, router_node_char);
        } else {
            ++ctr_removed;
        }

        ++iter_region_map;
    }
    buf.append("\n");

    if (ctr_removed > 0) {
        ctr = 0;
        buf.appendf("--Region #%zu configured to remove these nodes:\n", region_num);
        iter_region_map = sptr_region_map->begin();

        while (iter_region_map != sptr_region_map->end()) {
            sptr_rec = iter_region_map->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_REMOVE) {
                if ((ctr % 8) == 0) {
                    // indent the line
                    if (ctr != 0) {
                        buf.append("\n");
                    }
                    buf.appendf("    ");
                }

                ++ctr;
                buf.appendf("%7zu ", sptr_rec->node_or_id_num_);
            }

            ++iter_region_map;
        }
        buf.append("\n");
    }


    buf.append("\n");
}

//----------------------------------------------------------------------
void
IMCConfig::add_node_range_to_group(size_t group_num, size_t node_num_start, 
                                   size_t node_num_end, std::string& result_str, 
                                   bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (group_num == 0) {
        result_str = "error - group number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    bool issue_joins = false;

    // Find the map for this group or create it if not found
    SPtr_IMC_GROUP_MAP sptr_group_map = get_group_map(group_num);

    // Add each node to the group if it is not already there
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_GROUP_MAP::iterator iter_group_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_group_map = sptr_group_map->find(ix);

        if (iter_group_map == sptr_group_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_GROUP;
            sptr_rec->region_or_group_num_ = group_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

            (*sptr_group_map)[ix] = sptr_rec;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
                issue_joins = true;
            }
        } else {
            sptr_rec = iter_group_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_ADD) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                    issue_joins = true;
                }
            }
        }
    }

    // if nodes added to group via console or AMP (db_update == true) 
    // and this is a router node then issue join petition(s)
    if (issue_joins && is_imc_router_node_) {
       if (imc_issue_bp7_joins_) {
           issue_bp7_imc_join_petition(group_num); 
       }
       if (imc_issue_bp6_joins_) {
           issue_bp6_imc_join_petitions(group_num); 
       }
    }


    result_str = "Added nodes from ";
    result_str.append(std::to_string(node_num_start));
    result_str.append(" thru ");
    result_str.append(std::to_string(node_num_end));
    result_str.append(" to IMC Group #");
    result_str.append(std::to_string(group_num));
}

//----------------------------------------------------------------------
void
IMCConfig::del_node_range_from_group(size_t group_num, size_t node_num_start, 
                                     size_t node_num_end, std::string& result_str, 
                                     bool db_update)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (group_num == 0) {
        result_str = "error - group number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    // Find the map for this group or create it if not found
    SPtr_IMC_GROUP_MAP sptr_group_map = get_group_map(group_num);

    // Remove each node from the group if it is a member
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_GROUP_MAP::iterator iter_group_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_group_map = sptr_group_map->find(ix);

        if (iter_group_map == sptr_group_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_GROUP;
            sptr_rec->region_or_group_num_ = group_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

            (*sptr_group_map)[ix] = sptr_rec;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        } else {
            sptr_rec = iter_group_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_REMOVE) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

                if (db_update && imc_router_active_) {
                    add_or_update_in_datastore(sptr_rec);
                }
            }
        }
    }


    result_str = "Removed nodes from ";
    result_str.append(std::to_string(node_num_start));
    result_str.append(" thru ");
    result_str.append(std::to_string(node_num_end));
    result_str.append(" from  IMC Group #");
    result_str.append(std::to_string(group_num));
}

//----------------------------------------------------------------------
void
IMCConfig::add_node_range_to_group_for_outer_regions(size_t group_num, size_t node_num_start, 
                                                     size_t node_num_end, std::string& result_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (group_num == 0) {
        result_str = "error - group number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    // Find the map for this group or create it if not found
    SPtr_IMC_GROUP_MAP sptr_group_map = get_group_map_for_outer_regions(group_num);

    // Add each node to the group if it is not already there
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_GROUP_MAP::iterator iter_group_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_group_map = sptr_group_map->find(ix);

        if (iter_group_map == sptr_group_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_GROUP;
            sptr_rec->region_or_group_num_ = group_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

            (*sptr_group_map)[ix] = sptr_rec;

        } else {
            sptr_rec = iter_group_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_ADD) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_ADD;
            }
        }
    }


    result_str = "Added nodes from ";
    result_str.append(std::to_string(node_num_start));
    result_str.append(" thru ");
    result_str.append(std::to_string(node_num_end));
    result_str.append(" to outer regions IMC Group #");
    result_str.append(std::to_string(group_num));
}

//----------------------------------------------------------------------
void
IMCConfig::del_node_range_from_group_for_outer_regions(size_t group_num, size_t node_num_start, 
                                                       size_t node_num_end, std::string& result_str)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (group_num == 0) {
        result_str = "error - group number cannot be zero";
        return;
    }

    if (node_num_start > node_num_end) {
        result_str = "error - start node number is greater than the end node number";
        return;
    }

    // Find the map for this group or create it if not found
    SPtr_IMC_GROUP_MAP sptr_group_map = get_group_map_for_outer_regions(group_num);

    // Remove each node from the group if it is a member
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_GROUP_MAP::iterator iter_group_map;
    for (size_t ix=node_num_start; ix<=node_num_end; ++ix) {
        iter_group_map = sptr_group_map->find(ix);

        if (iter_group_map == sptr_group_map->end()) {
            sptr_rec = std::make_shared<IMCRegionGroupRec>();

            sptr_rec->rec_type_ = IMC_REC_TYPE_GROUP;
            sptr_rec->region_or_group_num_ = group_num;
            sptr_rec->node_or_id_num_ = ix;
            sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

            (*sptr_group_map)[ix] = sptr_rec;

        } else {
            sptr_rec = iter_group_map->second;

            if ((sptr_rec->operation_ != IMC_REC_OPERATION_REMOVE) || (!sptr_rec->in_datastore_)) {
                sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;
            }
        }
    }


    result_str = "Removed nodes from ";
    result_str.append(std::to_string(node_num_start));
    result_str.append(" thru ");
    result_str.append(std::to_string(node_num_end));
    result_str.append(" from  outer regions IMC Group #");
    result_str.append(std::to_string(group_num));
}

//----------------------------------------------------------------------
void
IMCConfig::imc_group_dump(size_t group_num, oasys::StringBuffer& buf)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (sptr_clear_group_db_rec_->in_datastore_) {
        buf.appendf("Stored clear_group_db ID = %zu\n", sptr_clear_group_db_rec_->node_or_id_num_);
    }

    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    if (group_num == 0) {
        // loop through all groups and dump them
        iter_imc_group_maps = map_of_group_maps_.begin();

        while (iter_imc_group_maps != map_of_group_maps_.end()) {
            group_num = iter_imc_group_maps->first;
            sptr_group_map = iter_imc_group_maps->second;
            dump_single_imc_group(group_num, sptr_group_map, buf);

            ++iter_imc_group_maps;
        }
    } else {
        // find the single group to dump
        iter_imc_group_maps = map_of_group_maps_.find(group_num);

        if (iter_imc_group_maps == map_of_group_maps_.end()) {
            buf.append("no nodes configured for IMC Group #%zu\n", group_num);
        } else {
            sptr_group_map = iter_imc_group_maps->second;
            dump_single_imc_group(group_num, sptr_group_map, buf);
        }
    }
}

//----------------------------------------------------------------------
void
IMCConfig::dump_single_imc_group(size_t group_num, SPtr_IMC_GROUP_MAP& sptr_group_map, oasys::StringBuffer& buf)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    buf.appendf("Group #%zu contains %zu records:\n", group_num, sptr_group_map->size());

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_group_map;
    iter_group_map = sptr_group_map->begin();

    size_t ctr_removed = 0;
    size_t ctr = 0;
    while (iter_group_map != sptr_group_map->end()) {
        sptr_rec = iter_group_map->second;

        if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
            if ((ctr % 8) == 0) {
                // indent the line
                if (ctr != 0) {
                    buf.append("\n");
                }
                buf.appendf("    ");
            }

            ++ctr;
            buf.appendf("%7zu ", sptr_rec->node_or_id_num_);
        } else {
            ++ctr_removed;
        }

        ++iter_group_map;
    }
    buf.append("\n");

    if (ctr_removed > 0) {
        ctr = 0;
        buf.appendf("Group #%zu configured to remove these nodes:\n", group_num);
        iter_group_map = sptr_group_map->begin();

        while (iter_group_map != sptr_group_map->end()) {
            sptr_rec = iter_group_map->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_REMOVE) {
                if ((ctr % 8) == 0) {
                    // indent the line
                    if (ctr != 0) {
                        buf.append("\n");
                    }
                    buf.appendf("    ");
                }

                ++ctr;
                buf.appendf("%7zu ", sptr_rec->node_or_id_num_);
            }

            ++iter_group_map;
        }
        buf.append("\n");
    }

    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps_for_outer_regions;
    iter_imc_group_maps_for_outer_regions = map_of_group_maps_for_outer_regions_.find(group_num);

    if (iter_imc_group_maps_for_outer_regions != map_of_group_maps_for_outer_regions_.end()) {
        SPtr_IMC_GROUP_MAP sptr_group_map_for_outer_regions;
        sptr_group_map_for_outer_regions = iter_imc_group_maps_for_outer_regions->second;
        dump_single_imc_group_for_outer_regions(group_num, sptr_group_map_for_outer_regions, buf);
    }


    buf.append("\n");
}

//----------------------------------------------------------------------
void
IMCConfig::dump_single_imc_group_for_outer_regions(size_t group_num, SPtr_IMC_GROUP_MAP& sptr_group_map, oasys::StringBuffer& buf)
{
    (void) group_num;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (sptr_group_map->size() > 0) {
        buf.appendf("--- acting as proxy for %zu nodes in outer regions:\n", sptr_group_map->size());

        SPtr_IMCRegionGroupRec sptr_rec;
        IMC_REGION_MAP::iterator iter_group_map;
        iter_group_map = sptr_group_map->begin();

        size_t ctr_removed = 0;
        size_t ctr = 0;
        while (iter_group_map != sptr_group_map->end()) {
            sptr_rec = iter_group_map->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                if ((ctr % 8) == 0) {
                    // indent the line
                    if (ctr != 0) {
                        buf.append("\n");
                    }
                    buf.appendf("    ");
                }

                ++ctr;
                buf.appendf("%7zu ", sptr_rec->node_or_id_num_);
            } else {
                ++ctr_removed;
            }

            ++iter_group_map;
        }
        buf.append("\n");
    }
}

//----------------------------------------------------------------------
void
IMCConfig::add_manual_join(size_t group_num, size_t service_num,
                                  std::string& result_str, bool db_update)
{
    char tmp_buf[128];
    snprintf(tmp_buf, sizeof(tmp_buf), "imc:%zu.%zu", group_num, service_num);
    std::string imc_eid_str = tmp_buf;


    SPtr_IMCRegionGroupRec sptr_rec;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    auto iter = manual_joins_map_.find(imc_eid_str);
    if (iter == manual_joins_map_.end()) {
        sptr_rec = std::make_shared<IMCRegionGroupRec>();

        sptr_rec->rec_type_ = IMC_REC_TYPE_MANUAL_JOIN;
        sptr_rec->region_or_group_num_ = group_num;
        sptr_rec->node_or_id_num_ = service_num;
        sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

        manual_joins_map_[imc_eid_str] = sptr_rec;

        if (db_update && imc_router_active_) {
            add_or_update_in_datastore(sptr_rec);
        }
    } else {
        sptr_rec = iter->second;

        if ((sptr_rec->operation_ != IMC_REC_OPERATION_ADD) || (!sptr_rec->in_datastore_)) {
            sptr_rec->operation_ = IMC_REC_OPERATION_ADD;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        }
    }

    if (imc_router_enabled_ && imc_router_active_) {
        if (group_num > 0) {
           add_to_local_group_joins_count(group_num);

           log_always_p("/imc/cfg", "%s: sending manual IMC join petition for registration EID: %s", 
                      __func__, tmp_buf);

           if (imc_issue_bp7_joins_) {
               issue_bp7_imc_join_petition(group_num); 
           }
           if (imc_issue_bp6_joins_) {
               issue_bp6_imc_join_petitions(group_num); 
           }
        }
    }

    result_str = "Added manual join for EID: ";
    result_str.append(imc_eid_str.c_str());
}

//----------------------------------------------------------------------
void
IMCConfig::del_manual_join(size_t group_num, size_t service_num,
                           std::string& result_str, bool db_update)
{
    char tmp_buf[128];
    snprintf(tmp_buf, sizeof(tmp_buf), "imc:%zu.%zus", group_num, service_num);
    std::string imc_eid_str = tmp_buf;

    SPtr_IMCRegionGroupRec sptr_rec;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    auto iter = manual_joins_map_.find(imc_eid_str);
    if (iter == manual_joins_map_.end()) {
        sptr_rec = std::make_shared<IMCRegionGroupRec>();

        sptr_rec->rec_type_ = IMC_REC_TYPE_MANUAL_JOIN;
        sptr_rec->region_or_group_num_ = group_num;
        sptr_rec->node_or_id_num_ = service_num;
        sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

        manual_joins_map_[imc_eid_str] = sptr_rec;

        if (db_update && imc_router_active_) {
            add_or_update_in_datastore(sptr_rec);
        }
    } else {
        sptr_rec = iter->second;

        if ((sptr_rec->operation_ != IMC_REC_OPERATION_REMOVE) || (!sptr_rec->in_datastore_)) {
            sptr_rec->operation_ = IMC_REC_OPERATION_REMOVE;

            if (db_update && imc_router_active_) {
                add_or_update_in_datastore(sptr_rec);
            }
        }
    }

    if (imc_router_enabled_ && imc_router_active_) {
        if (group_num > 0) {
            if (subtract_from_local_group_joins_count(group_num) == 0) {

                log_always_p("/imc/cfg", "%s: sending manual IMC unjoin petition for registration EID: %s", 
                           __func__, tmp_buf);

                if (imc_issue_bp7_joins_) {
                    issue_bp7_imc_unjoin_petition(group_num); 
                }
                 if (imc_issue_bp6_joins_) {
                    issue_bp6_imc_unjoin_petitions(group_num); 
                }
            }
        }
    }




    result_str = "Removed manual join for EID: ";
    result_str.append(imc_eid_str.c_str());
}

//----------------------------------------------------------------------
bool
IMCConfig::is_manually_joined_eid(SPtr_EID dest_eid)
{
    bool result = false;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    auto iter = manual_joins_map_.find(dest_eid->str());
    if (iter != manual_joins_map_.end()) {
        result = iter->second->operation_ == IMC_REC_OPERATION_ADD;
    }

    return result;
}

//----------------------------------------------------------------------
void
IMCConfig::imc_manual_join_dump(oasys::StringBuffer& buf)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    if (sptr_clear_manual_join_db_rec_->in_datastore_) {
        buf.appendf("Stored clear_manual_join_db ID = %zu \n", sptr_clear_manual_join_db_rec_->node_or_id_num_);
    }

    size_t ctr = 0;
    auto iter = manual_joins_map_.begin();
    while (iter != manual_joins_map_.end()) {
        if (iter->second->operation_ == IMC_REC_OPERATION_ADD) {
            ++ctr;
            ++iter;
        }
    }

    buf.appendf("Num manually joined IMC EIDs: %zu \n", ctr);

    if (ctr == 0) {
    } else {
        iter = manual_joins_map_.begin();
        while (iter != manual_joins_map_.end()) {
            if (iter->second->operation_ == IMC_REC_OPERATION_ADD) {
                buf.appendf("    %s \n", iter->first.c_str());
                ++iter;
            }
        }
    }
}




//----------------------------------------------------------------------
void
IMCConfig::delete_all_region_recs_from_datastore()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    log_info_p("/imc/cfg", "Deleting all IMC Region Recs from data store");

    // delete all of the region recs from the data store
    delete_rec_type_from_datastore(IMC_REC_TYPE_REGION_STR);

    // spin through the region maps to set all of the in_datastore_ flags to false
    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions and clear them
    iter_imc_region_maps = map_of_region_maps_.begin();

    while (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;
        clear_in_datastore_flags(sptr_region_map);

        ++iter_imc_region_maps;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::delete_all_group_recs_from_datastore()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    log_info_p("/imc/cfg", "Deleting all IMC Group Recs from data store");

    // delete all of the group recs from the data store
    delete_rec_type_from_datastore(IMC_REC_TYPE_GROUP_STR);

    // spin through the group maps to set all of the in_datastore_ flags to false
    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // loop through all groups and clear them
    iter_imc_group_maps = map_of_group_maps_.begin();

    while (iter_imc_group_maps != map_of_group_maps_.end()) {
        sptr_group_map = iter_imc_group_maps->second;
        clear_in_datastore_flags(sptr_group_map);

        ++iter_imc_group_maps;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::delete_all_manual_join_recs_from_datastore()
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    log_info_p("/imc/cfg", "Deleting all IMC Manual Join Recs from data store");

    // delete all of the manual_join recs from the data store
    delete_rec_type_from_datastore(IMC_REC_TYPE_MANUAL_JOIN_STR);

    // spin through the manual_join maps to set all of the in_datastore_ flags to false
    SPtr_IMCRegionGroupRec sptr_rec;
    auto iter = manual_joins_map_.begin();
    while (iter != manual_joins_map_.end()) {
        iter->second->in_datastore_ = false ;
        ++iter;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::delete_rec_type_from_datastore(const char* key_match)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    IMCRegionGroupRecStore* imc_store = IMCRegionGroupRecStore::instance();

    imc_store->lock_db_access("IMCConfig::add_or_update_in_datastore");

    IMCRegionGroupRecStore::iterator* imc_store_iter = imc_store->new_iterator();

    // Build a list of keys that need to be deleted
    std::vector<std::string> delete_list;


    int status;

    for (status = imc_store_iter->begin(); imc_store_iter->more(); status = imc_store_iter->next()) {
        if ( oasys::DS_NOTFOUND == status ) {
            log_warn_p("/imc/cfg", "received DS_NOTFOUND from data store - IMC Region/Group Rec <%s>",
                    imc_store_iter->cur_val().c_str());
            break;
        } else if ( oasys::DS_ERR == status ) {
            log_err_p("/imc/cfg", "error reading IMC Region/Group Rec <%s> from data store",
                    imc_store_iter->cur_val().c_str());
            break;
        }

        if (imc_store_iter->cur_val().find(key_match) == 0) {
            // found correct type of record
            delete_list.push_back(imc_store_iter->cur_val());
        } else {
            // wrong type of record - continue
        }
    }

    delete imc_store_iter;

    imc_store->unlock_db_access();

    // Loop through the list of keys and delete them from the datastore
    std::vector<std::string>::iterator iter_keys = delete_list.begin();

    while (iter_keys != delete_list.end()) {
        delete_from_datastore(*iter_keys);

        ++iter_keys;
    }
}


//----------------------------------------------------------------------
//
//  This works since IMC_REGION_MAP and IMC_GROUP_MAP are currently the same definition
//
void
IMCConfig::clear_in_datastore_flags(SPtr_IMC_REGION_MAP& sptr_map)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_map->begin();

    while (iter_region_map != sptr_map->end()) {
        iter_region_map->second->in_datastore_ = false ;

        ++iter_region_map;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::add_to_local_group_joins_count(size_t imc_group)
{
    if (imc_group > 0) {
        std::lock_guard<std::recursive_mutex> scop_lok(lock_);

        IMC_GROUP_JOINS_MAP::iterator iter = local_imc_group_joins_.find(imc_group);
        if (iter != local_imc_group_joins_.end()) {
            iter->second += 1;
        } else {
            local_imc_group_joins_[imc_group] = 1;
        }

        size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();
        std::string dummy_str;
        add_node_range_to_group(imc_group, my_ipn_node_num, my_ipn_node_num, dummy_str, false);
    }
}

//----------------------------------------------------------------------
size_t
IMCConfig::subtract_from_local_group_joins_count(size_t imc_group)
{
    size_t num_joins_remaining = 0;

    if (imc_group > 0) {
        std::lock_guard<std::recursive_mutex> scop_lok(lock_);

        IMC_GROUP_JOINS_MAP::iterator iter = local_imc_group_joins_.find(imc_group);
        if (iter != local_imc_group_joins_.end()) {
            if (iter->second > 0) {
                iter->second -= 1;
                num_joins_remaining = iter->second;
            }

            if (num_joins_remaining == 0) {
                local_imc_group_joins_.erase(iter);

                size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();
                std::string dummy_str;
                del_node_range_from_group(imc_group, my_ipn_node_num, my_ipn_node_num, dummy_str, false);
            }
        }
    }

    return num_joins_remaining;
}


//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // remove local node number from the working dest list
    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

    bundle->imc_dest_node_handled(my_ipn_node_num);

    // remove the previous hop node number from the working dest list
    bundle->imc_dest_node_handled(bundle->prevhop()->node_num());

    // remove the source node number from the working dest list
    size_t source_node = bundle->source()->node_num();
    bundle->imc_dest_node_handled(source_node);


    size_t group_num = bundle->dest()->node_num();

    if (!bundle->is_imc_region_processed(home_region_)) {
        // get the group number from the IMC Destination EID


        if (group_num == IMC_GROUP_0_SYNC_REQ) {
            if (is_node_in_home_region(source_node)) {
                // group 0 bundles from within home region go to all known router nodes in home region

                //dzdebug   update_bundle_imc_dest_nodes_with_all_router_nodes_in_home_region(bundle);
                update_bundle_imc_dest_nodes_with_all_router_nodes(bundle);
            } else {
                // (passageway router nodes will issue proxies to their outer regions)
                // TODO:   may not need to do anything here!!!
            }

        } else {
            // add known recipients from home region
            update_imc_dest_nodes_from_home_region_group_list(bundle, group_num);
        }
        
        bundle->add_imc_region_processed(home_region_);
    }

    if (group_num != 0) {
        if (!bundle->imc_has_proxy_been_processed_by_node(my_ipn_node_num)) {
            // always add outer region nodes that other home region IMC routers 
            // may not know about if this node is acting as a proxy for them
            update_imc_dest_nodes_from_outer_regions_group_list(bundle, group_num);

            // flag this node as having applied passageway proxies
            bundle->add_imc_processed_by_node(my_ipn_node_num);
        }
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_imc_dest_nodes_from_home_region_group_list(Bundle* bundle, size_t group_num)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // spin through the group maps to set all of the in_datastore_ flags to false
    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // find the list for this group num
    iter_imc_group_maps = map_of_group_maps_.find(group_num);

    if (iter_imc_group_maps != map_of_group_maps_.end()) {
        sptr_group_map = iter_imc_group_maps->second;

        // process each node in the group list
        SPtr_IMCRegionGroupRec sptr_rec;

        IMC_GROUP_MAP::iterator iter_group = sptr_group_map->begin();
        while (iter_group != sptr_group_map->end()) {
            sptr_rec = iter_group->second;

            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                bundle->add_imc_dest_node(sptr_rec->node_or_id_num_);
            } else {
                bundle->imc_dest_node_handled(sptr_rec->node_or_id_num_);
            }

            ++iter_group;
        }
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_imc_dest_nodes_from_outer_regions_group_list(Bundle* bundle, size_t group_num)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    // spin through the group maps to set all of the in_datastore_ flags to false
    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // find the list for this group num
    iter_imc_group_maps = map_of_group_maps_for_outer_regions_.find(group_num);

    if (iter_imc_group_maps != map_of_group_maps_for_outer_regions_.end()) {
        sptr_group_map = iter_imc_group_maps->second;

        // add each node in the group list to the bundle list of destination nodes
        IMC_GROUP_MAP::iterator iter_group = sptr_group_map->begin();
        while (iter_group != sptr_group_map->end()) {
            bundle->add_imc_dest_node(iter_group->first);

            ++iter_group;
        }

        ++iter_imc_group_maps;
    }
}


//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_all_router_nodes(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions
    iter_imc_region_maps = map_of_region_maps_.begin();

    while (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;

        // add all router nodes from this region
        update_bundle_imc_dest_nodes_with_router_nodes_from_single_region(bundle, sptr_region_map);

        ++iter_imc_region_maps;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_all_router_nodes_in_home_region(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions
    iter_imc_region_maps = map_of_region_maps_.find(home_region());

    if (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;

        // add all router nodes from this region
        update_bundle_imc_dest_nodes_with_router_nodes_from_single_region(bundle, sptr_region_map);
    }
}


//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_all_router_nodes_in_outer_regions(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions
    iter_imc_region_maps = map_of_region_maps_.begin();

    while (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;

        if (iter_imc_region_maps->first != home_region()) {
            // add all router nodes from this outer region
            update_bundle_imc_dest_nodes_with_router_nodes_from_single_region(bundle, sptr_region_map);
        }

        ++iter_imc_region_maps;
    }
}


//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_router_nodes_from_single_region(Bundle* bundle, SPtr_IMC_REGION_MAP& sptr_region_map)
{
    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();
             
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_region_map->begin();

    while (iter_region_map != sptr_region_map->end()) {
        sptr_rec = iter_region_map->second;

        if (my_ipn_node_num != sptr_rec->node_or_id_num_) {
            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                if (sptr_rec->is_router_node_) {
                    bundle->add_imc_dest_node(sptr_rec->node_or_id_num_);
                }
            }
        }

        ++iter_region_map;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_all_nodes_in_home_region(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // Find the home region map
    sptr_region_map = get_region_map(home_region_);

    if (sptr_region_map) {
        // add all router nodes from this region
        update_bundle_imc_dest_nodes_with_all_nodes_from_single_region(bundle, sptr_region_map);
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_dest_nodes_with_all_nodes_from_single_region(Bundle* bundle, SPtr_IMC_REGION_MAP& sptr_region_map)
{
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_region_map->begin();

    while (iter_region_map != sptr_region_map->end()) {
        sptr_rec = iter_region_map->second;

        if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
            bundle->add_imc_dest_node(sptr_rec->node_or_id_num_);
        }

        ++iter_region_map;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_alternate_dest_nodes_with_router_nodes_in_home_region(Bundle* bundle)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;

    // Find the home region map
    sptr_region_map = get_region_map(home_region_);

    if (sptr_region_map) {
        // add all router nodes from this region
        update_bundle_imc_alternate_dest_nodes_with_router_nodes_from_single_region(bundle, sptr_region_map);
    }
}

//----------------------------------------------------------------------
void
IMCConfig::update_bundle_imc_alternate_dest_nodes_with_router_nodes_from_single_region(Bundle* bundle, 
                                                                                       SPtr_IMC_REGION_MAP& sptr_region_map)
{
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_region_map->begin();

    while (iter_region_map != sptr_region_map->end()) {
        sptr_rec = iter_region_map->second;

        if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
            if (sptr_rec->is_router_node_) {
                bundle->add_imc_alternate_dest_node(sptr_rec->node_or_id_num_);
            }
        }

        ++iter_region_map;
    }
}


//----------------------------------------------------------------------
bool
IMCConfig::process_bpv6_imc_group_petition(Bundle* bundle,
                                            const u_char* buf_ptr, size_t buf_len)
{
    if (bundle->source() == BundleDaemon::instance()->local_eid_ipn()) {
        // no need to process the bundle we created
        return true;
    }

    size_t ipn_node_num = bundle->source()->node_num();
    size_t imc_group_num = 0;
    size_t join_or_unjoin = 0;  // 1 = Join; 0 = UnJoin

    // skip past the Admin Type code and flags (1 byte)
    buf_ptr++;
    buf_len--;

    // Next field is the IMC group num
    ssize_t sdnv_len = SDNV::decode(buf_ptr, buf_len, &imc_group_num);
    if (sdnv_len < 0) {
        log_info_p("/imc/cfg", "Error decoding BPv6 IMC Group Petition group number");
        return false;
    }
    buf_ptr += sdnv_len;
    buf_len -= sdnv_len;

    // Next field is the join/unjoin flag
    sdnv_len = SDNV::decode(buf_ptr, buf_len, &join_or_unjoin);
    if (sdnv_len < 0) {
        log_info_p("/imc/cfg", "Error decoding BPv6 IMC Group Petition join/unjoin flag");
        return false;
    }

    std::string dummy_str;
    bool db_update = false;

    if (join_or_unjoin == 0) {
        del_node_range_from_group(imc_group_num, ipn_node_num, ipn_node_num, dummy_str, db_update);
    } else {
        add_node_range_to_group(imc_group_num, ipn_node_num, ipn_node_num, dummy_str, db_update);
    }

    return true;
}


//----------------------------------------------------------------------
bool
IMCConfig::process_imc_group_petition(Bundle* bundle)
{
    if (bundle->source() == BundleDaemon::instance()->local_eid_ipn()) {
        // no need to process the bundle we created
        return true;
    }

    // Parse the payload
    int status = 0;

    size_t remote_node_num = bundle->source()->node_num();
    size_t imc_group_num = 0;
    size_t join_or_unjoin = 0;  // 1 = Join; 0 = UnJoin

    CborUtilBP7 cborutil("ImcGrpPetition");


    size_t payload_len = bundle->payload().length();
    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const uint8_t* payload_buf = 
        bundle->payload().read_data(0, payload_len, scratch.buf(payload_len));

    CborError err;
    CborParser parser;
    CborValue cvPayloadArray;
    CborValue cvPayloadElements;

    err = cbor_parser_init(payload_buf, payload_len, 0, &parser, &cvPayloadArray);
    CHECK_CBOR_DECODE_ERR

    SET_FLDNAMES("IMCGroupPetition");
    size_t block_elements;
    status = cborutil.validate_cbor_fixed_array_length(cvPayloadArray, 2, 2, block_elements);
    CHECK_CBOR_STATUS

    err = cbor_value_enter_container(&cvPayloadArray, &cvPayloadElements);
    CHECK_CBOR_DECODE_ERR

    // Group Number
    SET_FLDNAMES("IMCGroupPetition-GroupNum");
    status = cborutil.decode_uint(cvPayloadElements, imc_group_num);
    CHECK_CBOR_STATUS

    // Join or UnJoin Flag
    SET_FLDNAMES("IMCGroupPetition-JoinFlag");
    status = cborutil.decode_uint(cvPayloadElements, join_or_unjoin);
    CHECK_CBOR_STATUS

    // Apply the request
    if (remote_node_num > 0) {
        if (imc_group_num == IMC_GROUP_0_SYNC_REQ) {

            if (bundle->imc_sync_request()) { 
                // request came from a DTNME node requesting a full sync
                generate_dtnme_imc_briefing_bundle(bundle);
            } else {
                generate_bp7_ion_compatible_imc_briefing_bundle(bundle);
            }

            log_info_p("/imc/config", "Received IMC Briefing Request bundle from node: %zu",
                         remote_node_num);

        } else {

            log_info_p("/imc/config", "Received IMC Group Petition bundle for group: %zu node: %zu %s",
                         imc_group_num, remote_node_num, join_or_unjoin==1?"join":"unjoin");

            std::string dummy_str;
            bool db_update = false;  // transient join/unjoins are not saved to the DB
            size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

            if (join_or_unjoin == 0) {
                del_node_range_from_group(imc_group_num, remote_node_num, remote_node_num, dummy_str, db_update);

                //TODO: anything special needed for passageway nodes?

            } else {

                if (bundle->imc_has_proxy_been_processed_by_node(my_ipn_node_num)) {
                    // this node has already processed this proxy petition so nothing further to do

                } else if (is_node_in_home_region(remote_node_num)) {
                    // This IMC Group Join is from a node in the home region
                    // - add the node to the group list
                    // - initiate a proxy join to the outer regions

                    add_node_range_to_group(imc_group_num, remote_node_num, remote_node_num, dummy_str, db_update);

                    if (imc_router_enabled_ && is_imc_router_node_) {
                        // only need to use a proxy if outer IMC regions exist
                        if (have_outer_region_router_nodes()) {
                            // Only supporting BPv7
                            // create a join petition from the local node to send out as a proxy
                            std::unique_ptr<Bundle> qbptr (create_bp7_imc_join_petition(imc_group_num));

                            if (qbptr) {
                                // issue a group join petition as proxy to outer regions
                                update_bundle_imc_dest_nodes_with_all_router_nodes_in_outer_regions(qbptr.get());

                                // set the IMC State block to indicate this is a proxy 
                                qbptr->set_imc_is_proxy_petition(true);

                                // and copy the processed node list from the original bundle to the new proxy
                                qbptr->copy_imc_processed_by_node_list(bundle);

                                // and indicate that this node has processed it to prevent circular bundles
                                qbptr->add_imc_processed_by_node(my_ipn_node_num);
                                // and original node does not need to process it either
                                qbptr->add_imc_processed_by_node(remote_node_num);

                                // flag IMC bundles as processed so the IMC Router will not add additional dest nodes
                                qbptr->set_router_processed_imc();

                                // "Send" the bundle
                                SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                                BundleReceivedEvent* event_to_post;
                                event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
                                SPtr_BundleEvent sptr_event_to_post(event_to_post);
                                BundleDaemon::post(sptr_event_to_post);
                            } else {
                                log_err_p("imc/config", "Error creating IMC Proxy Group Petition bundle");
                            }
                        }
                    }
                } else {
                    // This IMC Group Join is from an outer region
                    // - add the node to the proxy group list

                    add_node_range_to_group_for_outer_regions(imc_group_num, remote_node_num, remote_node_num, dummy_str);

                    if (imc_router_enabled_ && is_imc_router_node_) {
                        // Add self node to the group list so other home region routers will send the IMC group bundles our way
                        add_node_range_to_group(imc_group_num, my_ipn_node_num, my_ipn_node_num, dummy_str, db_update);

                        if (have_any_other_router_nodes(remote_node_num)) {
                            // Only supporting BPv7
                            // create a join petition from the local node to send out as a proxy
                            std::unique_ptr<Bundle> qbptr (create_bp7_imc_join_petition(imc_group_num));

                            if (qbptr) {
                                // set previous hop to same as the original bundle so we might avoid sending it back to it
                                qbptr->mutable_prevhop() = bundle->prevhop();

                                // issue a group join petition for self as a proxy to all regions
                                // (assuming that since the bundle arrived here we can route back to the node)
                                update_bundle_imc_dest_nodes_with_all_router_nodes(qbptr.get());

                                // set the IMC State block to indicate this is a proxy 
                                qbptr->set_imc_is_proxy_petition(true);
                                // and copy the processed node list from the original bundle to the new proxy

                                qbptr->copy_imc_processed_by_node_list(bundle);
                                // and indicate that this node has processed it to prevent circular bundles
                                qbptr->add_imc_processed_by_node(my_ipn_node_num);
                                // and original node does not need to process it either
                                qbptr->add_imc_processed_by_node(remote_node_num);

                                // indicate that the originating node has already been handled
                                qbptr->imc_dest_node_handled(remote_node_num);

                                // flag IMC bundles as processed so the IMC Router will not add additional dest nodes
                                qbptr->set_router_processed_imc();

                                // "Send" the bundle
                                SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                                BundleReceivedEvent* event_to_post;
                                event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
                                SPtr_BundleEvent sptr_event_to_post(event_to_post);
                                BundleDaemon::post(sptr_event_to_post);
                            } else {
                                log_err_p("imc/config", "Error creating IMC Proxy Group Petition bundle");
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------
void
IMCConfig::generate_bp7_ion_compatible_imc_briefing_bundle(Bundle* orig_bundle)
{
    if (!orig_bundle->source()->is_ipn_scheme()) {
        return;
    }

    // generate standard briefing message that ION can also processes
    std::unique_ptr<Bundle> qbptr ( new Bundle(BundleProtocol::BP_VERSION_7) );

    // dest is the administrative EID of the original source node  ipn:x.0
    qbptr->mutable_dest() = BD_MAKE_EID_IPN(orig_bundle->source()->node_num(), 0);

    qbptr->set_source(BundleDaemon::instance()->local_eid_ipn());
    qbptr->set_expiration_secs(86400);  // 1 day is what ION uses - make config?
    qbptr->set_is_admin(true);

    int64_t rpt_len;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    rpt_len = encode_bp7_ion_compatible_imc_briefing(nullptr, 0);

    if (BP_FAIL == rpt_len) {
        log_err_p("/dtn/imcconfig", "Error encoding ION compatible IMC Briefing for request: *%p", orig_bundle);
        return;
    }

    // we generally don't expect the report length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;
    u_char* bp = scratch.buf(rpt_len);

    int64_t status = encode_bp7_ion_compatible_imc_briefing(bp, rpt_len);

    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/dtn/imcconfig", "Error encoding ION compatible IMC Briefing for request: *%p", orig_bundle);
        return;
    }

    // 
    // Finished generating the payload
    //
    qbptr->mutable_payload()->set_data(scratch.buf(), rpt_len);


    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
int64_t
IMCConfig::encode_bp7_ion_compatible_imc_briefing(uint8_t* buf, int64_t buflen)
{
    CborEncoder encoder;
    CborEncoder adminRecArrayEncoder;
    CborEncoder rptArrayEncoder;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);


    // create an array with 2 elements
    err = cbor_encoder_create_array(&encoder, &adminRecArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // 1st element: admin rec type
    err = cbor_encode_uint(&adminRecArrayEncoder, BundleProtocolVersion7::ADMIN_IMC_BRIEFING);
    CHECK_CBOR_ENCODE_ERR_RETURN


    // 2nd element: array of the IMC groups handled/needed by this node
    //    -- if acting as an IMC Router node then the list of groups will be all known groups
    //    -- otherwise the list of groups will just be the ones joined locally
    if (imc_router_enabled_ && is_imc_router_node_) {
        // if active router node then respond will all IMC groups it can handle
        size_t array_size = map_of_group_maps_.size();

        err = cbor_encoder_create_array(&adminRecArrayEncoder, &rptArrayEncoder, array_size);
        CHECK_CBOR_ENCODE_ERR_RETURN

        MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;
        iter_imc_group_maps = map_of_group_maps_.begin();

        while (iter_imc_group_maps != map_of_group_maps_.end()) {
            // Encode the IMC group number
            err = cbor_encode_uint(&rptArrayEncoder, iter_imc_group_maps->first);
            CHECK_CBOR_ENCODE_ERR_RETURN

            ++iter_imc_group_maps;
        }

        // close the array
        err = cbor_encoder_close_container(&encoder, &rptArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN

    } else {
        // otherwise just respond with IMC groups listening for locally
        size_t array_size = local_imc_group_joins_.size();

        err = cbor_encoder_create_array(&adminRecArrayEncoder, &rptArrayEncoder, array_size);
        CHECK_CBOR_ENCODE_ERR_RETURN

        IMC_GROUP_JOINS_MAP::iterator iter = local_imc_group_joins_.begin();
        while (iter != local_imc_group_joins_.end()) {
            // Encode the IMC group number
            err = cbor_encode_uint(&rptArrayEncoder, iter->first);
            CHECK_CBOR_ENCODE_ERR_RETURN

            ++iter;
        }

        // close the array
        err = cbor_encoder_close_container(&encoder, &rptArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN
    }

    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    return need_bytes;
}

//----------------------------------------------------------------------
bool
IMCConfig::process_ion_compatible_imc_briefing(Bundle* bundle, CborValue& cvAdminRecArray, CborUtilBP7& cborutil)
{
    // picking up processing at the 2nd element of the Admin Rec Array (1 st element was the Admin Rec Type

    size_t ipn_node_num = bundle->source()->node_num();
    size_t imc_group_num = 0;

    CborError err;
    bool is_router_node;
    bool db_update = false;  // transient info does not get saved to the database
    std::string dummy_str;

    SET_FLDNAMES("IonImcBriefing-CborArray");
    size_t array_elements;
    int status = cborutil.validate_cbor_fixed_array_length(cvAdminRecArray, 0, UINT64_MAX, array_elements);
    CHECK_CBOR_STATUS

    if (array_elements == 0) {
        // sending node is not joined to any IMC groups
        // so nothing more to do as far as parsing goes
    } else {
        CborValue cvGroupArray;
        err = cbor_value_enter_container(&cvAdminRecArray, &cvGroupArray);
        CHECK_CBOR_DECODE_ERR

        // parse the aray elements which is a list of IMC gourp numbers to which the node is joined
        SET_FLDNAMES("IMCBriefing-GroupNums");
        for (size_t ix=0; ix<array_elements; ++ix) {
            // Group Number
            status = cborutil.decode_uint(cvGroupArray, imc_group_num);
            CHECK_CBOR_STATUS

            // add thie sending ipn node num to the list of nodes joined to the group
            add_node_range_to_group(imc_group_num, ipn_node_num, ipn_node_num, dummy_str, db_update);
        }
    }

    // now check the IMC Processed Regions block info for additioonal DTNME info
    if (!bundle->imc_is_dtnme_node()) {
        // No IMC Processed Region block or no DNTME_NODE flag so this is 
        // an ION or other implemantion that does not support the additional DTNME flags
        // -- add/update this node as an IMC Router so that all join/unjoin requests get sent to it
        is_router_node = true;
        add_node_range_to_region(home_region(), is_router_node, ipn_node_num, ipn_node_num,
                                 dummy_str, db_update);

    } else {
        // This is a DTNME Node which informs whether or not it is an IMC Router node
        is_router_node = bundle->imc_is_router_node();

        add_node_range_to_region(home_region(), is_router_node, ipn_node_num, ipn_node_num,
                                 dummy_str, db_update);
    }

    // always send back a briefing of our joins
    generate_bp7_ion_compatible_imc_briefing_bundle(bundle);

    return true;
}

//----------------------------------------------------------------------
void
IMCConfig::generate_dtnme_imc_briefing_bundle(Bundle* orig_bundle)
{
    if (!orig_bundle->source()->is_ipn_scheme()) {
        return;
    }

    // generate standard briefing message that ION can also processes
    std::unique_ptr<Bundle> qbptr ( new Bundle(BundleProtocol::BP_VERSION_7) );

    // dest is the administrative EID of the original source node  ipn:x.0
    qbptr->mutable_dest() = BD_MAKE_EID_IPN(orig_bundle->source()->node_num(), 0);

    qbptr->set_source(BundleDaemon::instance()->local_eid_ipn());
    qbptr->set_expiration_secs(86400);  // 1 day is what ION uses - make config?
    qbptr->set_is_admin(true);

    // signal that this is a DTNME IMC Sync briefing as opposed to an ION compatible IMC briefing
    qbptr->set_imc_is_dtnme_node(true);
    qbptr->set_imc_sync_reply(true);
    qbptr->set_imc_briefing(true);

    // if original bundle was an IMC group 0 bundle instead of an abmin bundle then 
    // also request a sync reply back from the originating node
    if (!orig_bundle->is_admin()) {
        qbptr->set_imc_sync_request(true);
    }

    int64_t rpt_len;

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    rpt_len = encode_dtnme_imc_briefing(nullptr, 0);

    if (BP_FAIL == rpt_len) {
        log_err_p("/dtn/imcbriefing", "Error encoding IMC Briefing for Bundle *%p", qbptr.get());
        return;
    }

    oasys::ScratchBuffer<u_char*, 0> scratch;
    u_char* bp = scratch.buf(rpt_len);

    int64_t status = encode_dtnme_imc_briefing(bp, rpt_len);

    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/dtn/imcbriefing", "Error encoding IMC Briefing for Bundle *%p", qbptr.get());
        return;
    }

    // 
    // Finished generating the payload
    //
    qbptr->mutable_payload()->set_data(scratch.buf(), rpt_len);


    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
int64_t
IMCConfig::encode_dtnme_imc_briefing(uint8_t* buf, int64_t buflen)
{
    CborError err;

    CborEncoder encoder;
    CborEncoder adminRecArrayEncoder;
    CborEncoder rptArrayEncoder;

    cbor_encoder_init(&encoder, buf, buflen, 0);


    // create an Administrive Rec array with 2 elements
    err = cbor_encoder_create_array(&encoder, &adminRecArrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // 1st element: admin rec type
    err = cbor_encode_uint(&adminRecArrayEncoder, BundleProtocolVersion7::ADMIN_IMC_BRIEFING);
    CHECK_CBOR_ENCODE_ERR_RETURN


    // 2nd element: array with the IMC briefing report details
    {
        err = cbor_encoder_create_array(&adminRecArrayEncoder, &rptArrayEncoder, 3);
        CHECK_CBOR_ENCODE_ERR_RETURN

        // 1st element of the report details: home region number
        err = cbor_encode_uint(&rptArrayEncoder, home_region());
        CHECK_CBOR_ENCODE_ERR_RETURN

        // 2nd element of the report details: array of node recs in the home region 
        //              - each node rec is an array of node num and whether it is an IMC router
        err = encode_dtnme_imc_briefing_home_region_array(rptArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN


        // 3rd element of the report details: array of group arrays
        //              - each group array is a 2 element array of group num and an array of nodes in the group
        err = encode_dtnme_imc_briefing_group_arrays(rptArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN

        // close the report array
        err = cbor_encoder_close_container(&adminRecArrayEncoder, &rptArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN
    }

    // close the array of group arrays
    err = cbor_encoder_close_container(&encoder, &adminRecArrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    return need_bytes;
}

//----------------------------------------------------------------------
CborError
IMCConfig::encode_dtnme_imc_briefing_home_region_array(CborEncoder& rptArrayEncoder)
{
    // 2nd element of the report details: array of node recs in the home region 
    //              - each node rec is an array of node num and whether it is an IMC router

    CborError err;
    CborEncoder regionArrayEncoder;

    size_t array_size = 0;
    SPtr_IMC_REGION_MAP sptr_region_map = get_region_map(home_region());

    if (sptr_region_map) {
        array_size = sptr_region_map->size();
    }
    // else if no sptr_region_map found then this will just be an array with zero elements

    // create the CBOR region array
    err = cbor_encoder_create_array(&rptArrayEncoder, &regionArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

    if (sptr_region_map) {

        SPtr_IMCRegionGroupRec sptr_rec;
        CborEncoder regionrecArrayEncoder;

        IMC_REGION_MAP::iterator iter_region_map;
        iter_region_map = sptr_region_map->begin();

        while (iter_region_map != sptr_region_map->end()) {
            sptr_rec = iter_region_map->second;

            // create a CBOR region rec array for this record
            err = cbor_encoder_create_array(&regionArrayEncoder, &regionrecArrayEncoder, 2);
            CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

            if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                // 1st element: node number
                err = cbor_encode_uint(&regionrecArrayEncoder, sptr_rec->node_or_id_num_);
                CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

                // 2nd element: is_router_node? true or false
                err = cbor_encode_boolean(&regionrecArrayEncoder,sptr_rec->is_router_node_);
                CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
            } else {
                // 1st element: node number - use zero for a place holder
                err = cbor_encode_uint(&regionrecArrayEncoder, 0);
                CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

                // 2nd element: is_router_node? true or false
                err = cbor_encode_boolean(&regionrecArrayEncoder, false);
                CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
            }

            err = cbor_encoder_close_container(&regionArrayEncoder, &regionrecArrayEncoder);
            CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

            ++iter_region_map;
        }
        
        // close the region array
        err = cbor_encoder_close_container(&rptArrayEncoder, &regionArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
    }

    // close the region array
    err = cbor_encoder_close_container(&rptArrayEncoder, &regionArrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

    return err;
}


//----------------------------------------------------------------------
CborError
IMCConfig::encode_dtnme_imc_briefing_group_arrays(CborEncoder& rptArrayEncoder)
{
    // 3rd element of the report details: array of group arrays
    //              - each group array is a 2 element array of group num and an array of nodes in the group

    CborError err;
    CborEncoder arrayOfGroupsArrayEncoder;

    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    size_t array_size = map_of_group_maps_.size();

    // create the CBOR array of group arrays
    err = cbor_encoder_create_array(&rptArrayEncoder, &arrayOfGroupsArrayEncoder, array_size);
    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

    iter_imc_group_maps = map_of_group_maps_.begin();

    CborEncoder groupArrayEncoder;
    CborEncoder nodeArrayEncoder;
    while (iter_imc_group_maps != map_of_group_maps_.end()) {
        size_t group_num = iter_imc_group_maps->first;

        // create a CBOR group array
        err = cbor_encoder_create_array(&arrayOfGroupsArrayEncoder, &groupArrayEncoder, 2);
        CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

        // 1st element: group number
        err = cbor_encode_uint(&groupArrayEncoder, group_num);
        CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

        // 2nd element: array of nodes in the group
        {  // indent to show array within array
            sptr_group_map = iter_imc_group_maps->second;
            array_size = sptr_group_map->size();

            // create the CBOR node array
            err = cbor_encoder_create_array(&groupArrayEncoder, &nodeArrayEncoder, array_size);
            CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

            SPtr_IMCRegionGroupRec sptr_rec;
            IMC_GROUP_MAP::iterator iter_group_map;

            iter_group_map = sptr_group_map->begin();

            while (iter_group_map != sptr_group_map->end()) {
                sptr_rec = iter_group_map->second;

                if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
                    err = cbor_encode_uint(&nodeArrayEncoder, sptr_rec->node_or_id_num_);
                    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
                } else {
                    // use zero as a place holder for nodes that were removed from the group
                    err = cbor_encode_uint(&nodeArrayEncoder, 0);
                    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
                }
        
                ++iter_group_map;
            }

            // close the node array
            err = cbor_encoder_close_container(&groupArrayEncoder, &nodeArrayEncoder);
            CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE
        }

        // close the group array
        err = cbor_encoder_close_container(&arrayOfGroupsArrayEncoder, &groupArrayEncoder);
        CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

        ++iter_imc_group_maps;
    }

    // create the CBOR array of group arrays
    err = cbor_encoder_close_container(&rptArrayEncoder, &arrayOfGroupsArrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN_ERROR_CODE

    return err;
}



//----------------------------------------------------------------------
bool
IMCConfig::process_dtnme_imc_briefing(Bundle* bundle, CborValue& cvAdminRecArray, CborUtilBP7& cborutil)
{
    size_t remote_node_num = bundle->source()->node_num();
    
    // picking up processing at the 2nd element of the Admin Rec Array (1 st element was the Admin Rec Type)

    CborError err;
    std::string dummy_str;

    // Expecting an array of 3 elements
    SET_FLDNAMES("DtnmeImcBriefing-CborArray");
    size_t array_elements;
    int status = cborutil.validate_cbor_fixed_array_length(cvAdminRecArray, 3, 3, array_elements);
    CHECK_CBOR_STATUS

    CborValue cvRptArray;
    err = cbor_value_enter_container(&cvAdminRecArray, &cvRptArray);
    CHECK_CBOR_DECODE_ERR

    // 1st element of the report details: home region
    SET_FLDNAMES("DtnmeImcBriefing.HomeRegion");
    size_t remote_home_region = 0;
    status = cborutil.decode_uint(cvRptArray, remote_home_region);
    CHECK_CBOR_STATUS

    // 2nd element of the report details: array of node recs in the home region 
    //              - each node rec is an array of node num and whether it is an IMC router
    if (!decode_dtnme_imc_briefing_region_array(cvRptArray, cborutil, 
                                                remote_home_region, remote_node_num)) {
        return false;
    }

    // 3rd element of the report details: array of group arrays
    //              - each group array is a 2 element array of group num and an array of nodes in the group
    if (!decode_dtnme_imc_briefing_array_of_groups(cvRptArray, cborutil, 
                                                   remote_home_region, remote_node_num)) {
        return false;
    }


    // if a sync request was also requested then generate the reply
    if (bundle->imc_sync_request()) {
        // a full DTNME IMC Sync was requested
        generate_dtnme_imc_briefing_bundle(bundle);
    }

    return true;
}

//----------------------------------------------------------------------
bool
IMCConfig::decode_dtnme_imc_briefing_region_array(CborValue& cvRptArray, CborUtilBP7& cborutil, 
                                                  size_t remote_home_region, size_t remote_node_num)
{
    // 2nd element of the report details: array of node recs in the home region 
    //              - each node rec is an array of node num and whether it is an IMC router

    CborError err;
    CborValue cvRegionArray;
    CborValue cvRegionRec;

    std::string dummy_str;
    bool db_update = false;  // transient info does not get saved to the database

    SET_FLDNAMES("DtnmeImcBriefing.RegionArray");
    size_t num_region_recs = 0;
    int64_t status = cborutil.validate_cbor_fixed_array_length(cvRptArray, 0, UINT64_MAX, num_region_recs);
    CHECK_CBOR_STATUS

    err = cbor_value_enter_container(&cvRptArray, &cvRegionArray);
    CHECK_CBOR_DECODE_ERR

    for (size_t ix=0; ix<num_region_recs; ++ix) {
        // each region rec is a two element array
        SET_FLDNAMES("DtnmeImcBriefing.RegionRec");
        size_t num_rec_fields = 0;
        status = cborutil.validate_cbor_fixed_array_length(cvRegionArray, 2, 2, num_rec_fields);
        CHECK_CBOR_STATUS

        err = cbor_value_enter_container(&cvRegionArray, &cvRegionRec);
        CHECK_CBOR_DECODE_ERR

        // 1st element: node number
        size_t node_num = 0;
        SET_FLDNAMES("DtnmeImcBriefing.RegionRec.NodeNum");
        status = cborutil.decode_uint(cvRegionRec, node_num);
        CHECK_CBOR_STATUS

        // 2nd element: Router Node flag
        bool is_router_node = false;
        SET_FLDNAMES("DtnmeImcBriefing.RegionRec.IsRouterNode");
        status = cborutil.decode_boolean(cvRegionRec, is_router_node);
        CHECK_CBOR_STATUS

        err = cbor_value_leave_container(&cvRegionArray, &cvRegionRec);
        CHECK_CBOR_DECODE_ERR

        // if our home region matches that of the remote sender then add all of the 
        // nodes that the sender knows is in the region
        //     OR
        // if the sending node is in a different [outer] region then only 
        // add the remote node when we get to its record so that this node 
        // can act as a passageway to that outer region.

        if (node_num != 0) {
            if ((remote_home_region == home_region()) || (node_num == remote_node_num)) {
                add_node_range_to_region(remote_home_region, is_router_node, node_num, 
                                         node_num, dummy_str, db_update);
            }
        }
    }

    err = cbor_value_leave_container(&cvRptArray, &cvRegionArray);
    CHECK_CBOR_DECODE_ERR

    return true;
}


//----------------------------------------------------------------------
bool
IMCConfig::decode_dtnme_imc_briefing_array_of_groups(CborValue& cvRptArray, CborUtilBP7& cborutil, 
                                                     size_t remote_home_region, size_t remote_node_num)
{
    size_t my_ipn_node_num = BundleDaemon::instance()->local_ipn_node_num();

    // 3rd element of the report details: array of group arrays
    //              - each group array is a 2 element array of group num and an array of nodes in the group

    CborError err;
    CborValue cvArrayOfGroupsArray;
    CborValue cvGroupArray;
    CborValue cvNodeArray;

    std::string dummy_str;
    bool db_update = false;  // transient info does not get saved to the database

    SET_FLDNAMES("DtnmeImcBriefing.ArrayOfGroups");
    size_t num_group_arrays = 0;
    int64_t status = cborutil.validate_cbor_fixed_array_length(cvRptArray, 0, UINT64_MAX, num_group_arrays);
    CHECK_CBOR_STATUS

    err = cbor_value_enter_container(&cvRptArray, &cvArrayOfGroupsArray);
    CHECK_CBOR_DECODE_ERR

    for (size_t ix=0; ix<num_group_arrays; ++ix) {
        // each group rec is a two element array
        SET_FLDNAMES("DtnmeImcBriefing.ArrayOfGroups.GroupArray");
        size_t num_group_fields = 0;
        status = cborutil.validate_cbor_fixed_array_length(cvArrayOfGroupsArray, 2, 2, num_group_fields);
        CHECK_CBOR_STATUS

        err = cbor_value_enter_container(&cvArrayOfGroupsArray, &cvGroupArray);
        CHECK_CBOR_DECODE_ERR

        // 1st element: group number
        size_t group_num = 0;
        SET_FLDNAMES("DtnmeImcBriefing.ArrayOfGroups.GroupArray.GroupNum");
        status = cborutil.decode_uint(cvGroupArray, group_num);
        CHECK_CBOR_STATUS

        // 2nd element: array of node numbers
        SET_FLDNAMES("DtnmeImcBriefing.ArrayOfGroups.GroupArray.NodeArray");
        size_t num_nodes = 0;
        status = cborutil.validate_cbor_fixed_array_length(cvGroupArray, 0, UINT64_MAX, num_nodes);
        CHECK_CBOR_STATUS

        err = cbor_value_enter_container(&cvGroupArray, &cvNodeArray);
        CHECK_CBOR_DECODE_ERR

        bool proxy_added = false;

        for (size_t kx=0; kx<num_nodes; ++kx) {
            size_t node_num = 0;
            SET_FLDNAMES("DtnmeImcBriefing.ArrayOfGroups.GroupArray.NodeArray.NodeNum");
            status = cborutil.decode_uint(cvNodeArray, node_num);
            CHECK_CBOR_STATUS

            // Add the node number to our group list if we are in the same region
            //XXX?dz - only if in our home region???

            if ((group_num != 0) && (node_num != 0)) {
                if (remote_home_region == home_region()) {
                    // this node is in the same region as the sending node so add
                    // all known group joins

                    add_node_range_to_group(group_num, node_num, node_num, 
                                            dummy_str, db_update);

                } else if (!proxy_added) {
                    // adding a single proxy for all nodes in this group so
                    // only come through here once
                    proxy_added = true;

                    // the sending node is in an outer region so it will act
                    // as a proxy for its region and this node only needs to
                    // add the sender itself to the group
                    // only add the sender itself as joined in from an outer regions
                    
                    add_node_range_to_group_for_outer_regions(group_num, remote_node_num, remote_node_num, 
                                                              dummy_str);

                    // and then issue a proxy join for our region
                    if (imc_router_enabled_ && is_imc_router_node_) {
                        // Add self node to the group list so other home region routers 
                        // will send the IMC group bundles our way
                        add_node_range_to_group(group_num, my_ipn_node_num, my_ipn_node_num, dummy_str, db_update);

                        if (have_any_other_router_nodes(remote_node_num)) {
                            // Only supporting BPv7
                            // create a join petition from the local node to send out as a proxy
                            std::unique_ptr<Bundle> qbptr (create_bp7_imc_join_petition(group_num));

                            if (qbptr) {
                                // issue a group join petition for self as a proxy to all regions
                                // (assuming that since the bundle arrived here we can route back to the node)
                                update_bundle_imc_dest_nodes_with_all_router_nodes(qbptr.get());

                                // set the IMC State block to indicate this is a proxy 
                                qbptr->set_imc_is_proxy_petition(true);

                                // indicate that this node has processed it to prevent circular bundles
                                qbptr->add_imc_processed_by_node(my_ipn_node_num);
                                // and original node does not need to process it either
                                qbptr->add_imc_processed_by_node(remote_node_num);

                                // indicate that the originating node has already been handled
                                qbptr->imc_dest_node_handled(remote_node_num);

                                // flag IMC bundles as processed so the IMC Router will not add additional dest nodes
                                qbptr->set_router_processed_imc();

                                // "Send" the bundle
                                SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                                BundleReceivedEvent* event_to_post;
                                event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
                                SPtr_BundleEvent sptr_event_to_post(event_to_post);
                                BundleDaemon::post(sptr_event_to_post);
                            } else {
                                log_err_p("imc/config", "Error creating IMC Proxy Group Petition bundle");
                            }
                        }
                    }

                }
            }
        }

        err = cbor_value_leave_container(&cvGroupArray, &cvNodeArray);
        CHECK_CBOR_DECODE_ERR

        err = cbor_value_leave_container(&cvArrayOfGroupsArray, &cvGroupArray);
        CHECK_CBOR_DECODE_ERR

    }

    err = cbor_value_leave_container(&cvRptArray, &cvArrayOfGroupsArray);
    CHECK_CBOR_DECODE_ERR

    return true;
}

//----------------------------------------------------------------------
bool
IMCConfig::process_ion_contact_plan_sync(Bundle* bundle)
{
    // Parse the payload
    int status = 0;

    size_t region_num = 0;
    ssize_t from_time = 0;
    ssize_t to_time = 0;
    size_t from_node = 0;
    size_t to_node = 0;
    ssize_t magnitude = 0;
    ssize_t confidence = 0;

    CborUtilBP7 cborutil("IonContactPlanSync");


    size_t payload_len = bundle->payload().length();
    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const uint8_t* payload_buf = 
        bundle->payload().read_data(0, payload_len, scratch.buf(payload_len));

    CborError err;
    CborParser parser;
    CborValue cvPayloadArray;
    CborValue cvPayloadElements;

    err = cbor_parser_init(payload_buf, payload_len, 0, &parser, &cvPayloadArray);
    CHECK_CBOR_DECODE_ERR

    SET_FLDNAMES("IonContactPlanSync");
    size_t block_elements;
    status = cborutil.validate_cbor_fixed_array_length(cvPayloadArray, 7, 7, block_elements);
    CHECK_CBOR_STATUS

    err = cbor_value_enter_container(&cvPayloadArray, &cvPayloadElements);
    CHECK_CBOR_DECODE_ERR

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-RegionNum");
    status = cborutil.decode_uint(cvPayloadElements, region_num);
    CHECK_CBOR_STATUS

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-FromTime");
    status = cborutil.decode_int(cvPayloadElements, from_time);
    CHECK_CBOR_STATUS

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-ToTime");
    status = cborutil.decode_int(cvPayloadElements, to_time);
    CHECK_CBOR_STATUS

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-FromNode");
    status = cborutil.decode_uint(cvPayloadElements, from_node);
    CHECK_CBOR_STATUS


    // Region Number
    SET_FLDNAMES("IonContactPlanSync-ToNode");
    status = cborutil.decode_uint(cvPayloadElements, to_node);
    CHECK_CBOR_STATUS

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-Magnitude");
    status = cborutil.decode_int(cvPayloadElements, magnitude);
    CHECK_CBOR_STATUS

    // Region Number
    SET_FLDNAMES("IonContactPlanSync-Confidence");
    status = cborutil.decode_int(cvPayloadElements, confidence);
    CHECK_CBOR_STATUS

    // We only process contact plan management notices in order to add nodes to our home region number
    // - we ignore contact deletions since we can't tell if just a specific contact time is
    //   being removed or if the node is being removed from the region
    //   (based on ION source code in bp7/daemon/cpsd.c)
    std::string dummy_str;
    bool db_update = false;

    if ((region_num > 0) && (region_num == home_region())) {
        if ((from_time == -1) && (to_time > 0)) {
            // Registration Contact message
            // -- add both nodes to the region
            add_node_range_to_region(region_num, true, from_node, from_node, dummy_str, db_update);
            add_node_range_to_region(region_num, true, to_node, to_node, dummy_str, db_update);
            
        } else if (confidence >= 200) {
            // Contact Revision message
            // -- add both nodes to the region
            add_node_range_to_region(region_num, true, from_node, from_node, dummy_str, db_update);
            add_node_range_to_region(region_num, true, to_node, to_node, dummy_str, db_update);
            
        } else if (to_time > 0) {
            // New Contact message
            // -- add both nodes to the region
            add_node_range_to_region(region_num, true, from_node, from_node, dummy_str, db_update);
            add_node_range_to_region(region_num, true, to_node, to_node, dummy_str, db_update);
            
        }
    }

    return true;
}

//----------------------------------------------------------------------
void
IMCConfig::issue_startup_imc_group_petitions()
{
    // loop through all of the groups issuing join petitions

    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_GROUP_MAP sptr_group_map;
    MAP_OF_IMC_GROUP_MAPS::iterator iter_imc_group_maps;

    // loop through all groups and dump them
    iter_imc_group_maps = map_of_group_maps_.begin();

    while (iter_imc_group_maps != map_of_group_maps_.end()) {
        size_t group_num = iter_imc_group_maps->first;

        if (imc_issue_bp7_joins_) {
            issue_bp7_imc_join_petition(group_num); 
        }
        if (imc_issue_bp6_joins_) {
            issue_bp6_imc_join_petitions(group_num); 
        }

        ++iter_imc_group_maps;
    }
}


//----------------------------------------------------------------------
void
IMCConfig::issue_bp7_imc_join_petition(size_t imc_group)
{
    std::unique_ptr<Bundle> qbptr (create_bp7_imc_join_petition(imc_group));

    if (!qbptr) {
        return;
    }

    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
IMCConfig::issue_bp7_imc_unjoin_petition(size_t imc_group)
{
    std::unique_ptr<Bundle> qbptr (create_bp7_imc_unjoin_petition(imc_group));

    if (!qbptr) {
        return;
    }

    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}


//----------------------------------------------------------------------
Bundle*
IMCConfig::create_bp7_imc_join_petition(size_t imc_group)
{
    size_t join_or_unjoin = 1;
    return create_bp7_imc_group_petition_bundle(imc_group, join_or_unjoin);
}


//----------------------------------------------------------------------
Bundle*
IMCConfig::create_bp7_imc_unjoin_petition(size_t imc_group)
{
    size_t join_or_unjoin = 0;
    return create_bp7_imc_group_petition_bundle(imc_group, join_or_unjoin);
}

//----------------------------------------------------------------------
Bundle*
IMCConfig::create_bp7_imc_group_petition_bundle(size_t imc_group, size_t join_or_unjoin)
{

    // BPv7 IMC un/joins are sent to the imc:0.0 EID
    std::unique_ptr<Bundle> qbptr ( new Bundle(BundleProtocol::BP_VERSION_7) );

    // Destination is imc:0.0 with a limited lifetime
    qbptr->mutable_dest() = BD_MAKE_EID_IMC00();

    qbptr->set_source(BundleDaemon::instance()->local_eid_ipn());
    qbptr->set_expiration_secs(86400);  // 1 day is what ION uses - make config?

    // build the payload 
    int64_t data_len = 0;

    data_len = encode_imc_group_petition(nullptr, 0, imc_group, join_or_unjoin);

    if (BP_FAIL == data_len) {
        log_err_p("/imc/cfg", "Error encoding IMC Group Petition bundle *%p", qbptr.get());
        return nullptr;
    }

    // we generally don't expect the length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;
    u_char* bp = scratch.buf(data_len);

    int64_t status = encode_imc_group_petition(bp, data_len, imc_group, join_or_unjoin);

    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/imc/cfg", "Error encoding IMC Group Petition bundle *%p", qbptr.get());
        return nullptr;
    }

    // 
    // Finished generating the payload
    //
    qbptr->mutable_payload()->set_data(scratch.buf(), data_len);

    return qbptr.release();
}

//----------------------------------------------------------------------
int64_t
IMCConfig::encode_imc_group_petition(uint8_t* buf, int64_t buflen, 
                                     size_t imc_group, size_t join_or_unjoin)
{
    CborEncoder encoder;
    CborEncoder arrayEncoder;

    CborError err;

    cbor_encoder_init(&encoder, buf, buflen, 0);


    // Array with two elements
    err = cbor_encoder_create_array(&encoder, &arrayEncoder, 2);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // First element: IMC Group Number 
    err = cbor_encode_uint(&arrayEncoder, imc_group);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // Second element: 1 = Join or 0 = UnJoin
    err = cbor_encode_uint(&arrayEncoder, join_or_unjoin);
    CHECK_CBOR_ENCODE_ERR_RETURN

    // close the array
    err = cbor_encoder_close_container(&encoder, &arrayEncoder);
    CHECK_CBOR_ENCODE_ERR_RETURN


    // was the buffer large enough?
    int64_t need_bytes = cbor_encoder_get_extra_bytes_needed(&encoder);

    //if (0 == need_bytes)
    //{
    //    encoded_len = cbor_encoder_get_buffer_size(&encoder, buf);
    //}

    return need_bytes;
}

//----------------------------------------------------------------------
void
IMCConfig::issue_bp6_imc_join_petitions(size_t imc_group)
{
    // ION BPv6 un/joins are sent to the administrative EID of every node in its region
    // DTMME will send one to every known router node in all regions
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions
    iter_imc_region_maps = map_of_region_maps_.begin();

    size_t join_or_unjoin = 1;  // join = 1

    while (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;

        issue_bp6_imc_group_petitions_to_all_routers_in_region(sptr_region_map, imc_group, join_or_unjoin);

        ++iter_imc_region_maps;
    }
}

//----------------------------------------------------------------------
void
IMCConfig::issue_bp6_imc_unjoin_petitions(size_t imc_group)
{
    // ION BPv6 un/joins are sent to the administrative EID of every node in its region
    // DTMME will send one to every known router node in all regions
    std::lock_guard<std::recursive_mutex> scop_lok(lock_);

    SPtr_IMC_REGION_MAP sptr_region_map;
    MAP_OF_IMC_REGION_MAPS::iterator iter_imc_region_maps;

    // loop through all regions
    iter_imc_region_maps = map_of_region_maps_.begin();

    size_t join_or_unjoin = 0;   // unjoin = 0

    while (iter_imc_region_maps != map_of_region_maps_.end()) {
        sptr_region_map = iter_imc_region_maps->second;

        issue_bp6_imc_group_petitions_to_all_routers_in_region(sptr_region_map, imc_group, join_or_unjoin);

        ++iter_imc_region_maps;
    }
}



//----------------------------------------------------------------------
void
IMCConfig::issue_bp6_imc_group_petitions_to_all_routers_in_region(SPtr_IMC_REGION_MAP& sptr_region_map,
                                                                  size_t imc_group, size_t join_or_unjoin)
{
    SPtr_IMCRegionGroupRec sptr_rec;
    IMC_REGION_MAP::iterator iter_region_map;
    iter_region_map = sptr_region_map->begin();

    while (iter_region_map != sptr_region_map->end()) {
        sptr_rec = iter_region_map->second;

        if (sptr_rec->operation_ == IMC_REC_OPERATION_ADD) {
            if (sptr_rec->is_router_node_) {
                issue_bp6_imc_group_petition_to_node(sptr_rec->node_or_id_num_, imc_group, join_or_unjoin);
            }
        }

        ++iter_region_map;
    }
}


//----------------------------------------------------------------------
void
IMCConfig::issue_bp6_imc_group_petition_to_node(size_t node_num, size_t imc_group, size_t join_or_unjoin)
{
    std::unique_ptr<Bundle> qbptr ( new Bundle(BundleProtocol::BP_VERSION_6) );

    // BPv6 IMC un/joins are sent to the administrative IPN EID  (ipn:x.0)
    qbptr->mutable_dest() = BD_MAKE_EID_IPN(node_num, 0);

    qbptr->set_source(BundleDaemon::instance()->local_eid_ipn());
    qbptr->set_expiration_secs(86400);  // 1 day is what ION uses - make config?
    qbptr->set_is_admin(true);

    // build the payload:
    // 1 byte Admin Payload type and flags
    // SDNV of IMC Group Num  (len = variable)
    // SDNV of join/unjoin flag  (len = 1)
    int sdnv_group_len = SDNV::encoding_len(imc_group);
    int sdnv_join_len = SDNV::encoding_len(join_or_unjoin);

    int rpt_len = 1 + sdnv_group_len + sdnv_join_len;
    int buf_avail_len = rpt_len;

    // we generally don't expect the report length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;
    u_char* bp = scratch.buf(rpt_len);

    // Admin Type and Flags
    *bp = BundleProtocolVersion6::ADMIN_MULTICAST_PETITION << 4;
    ++bp;
    --buf_avail_len;

    // SDNV of IMC Group num
    int sdnv_encoding_len = SDNV::encode(imc_group, bp, buf_avail_len);
    ASSERT(sdnv_encoding_len > 0);
    ASSERT(sdnv_encoding_len == sdnv_group_len);
    bp += sdnv_encoding_len;
    buf_avail_len -= sdnv_encoding_len;
    ASSERT(buf_avail_len > 0);

    // SDNV of join/unjoin flag
    sdnv_encoding_len = SDNV::encode(join_or_unjoin, bp, buf_avail_len);
    ASSERT(sdnv_encoding_len > 0);
    ASSERT(sdnv_encoding_len == sdnv_join_len);
    bp += sdnv_encoding_len;
    buf_avail_len -= sdnv_encoding_len;
    ASSERT(buf_avail_len == 0);

    qbptr->mutable_payload()->set_data(scratch.buf(), rpt_len);

    // "Send" the bundle
    SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
    BundleReceivedEvent* event_to_post;
    event_to_post = new BundleReceivedEvent(qbptr.release(), EVENTSRC_ADMIN, sptr_dummy_prevhop);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

} // namespace dtn
