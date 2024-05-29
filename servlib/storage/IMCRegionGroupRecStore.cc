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

#include "IMCRegionGroupRecStore.h"
#include "routing/IMCRegionGroupRec.h"
#include "storage/GlobalStore.h"

#include <third_party/oasys/storage/DurableStore.h>

namespace oasys {
    template<> dtn::IMCRegionGroupRecStore* oasys::Singleton<dtn::IMCRegionGroupRecStore, false>::instance_ = NULL;
}


namespace dtn {

IMCRegionGroupRecStore::IMCRegionGroupRecStore()
    : IMCRegionGroupRecStoreImpl("IMCRegionGroupRecStore", "/dtn/storage/imcrgngrp", 
                                 "imcrgngrp", "imcrgngrp")
{
    global_store_ = GlobalStore::instance();
}

//----------------------------------------------------------------------
int
IMCRegionGroupRecStore::init(const oasys::StorageConfig& cfg,
                             oasys::DurableStore*    store)
{
    if (instance_ != NULL) {
        PANIC("IMCRegionGroupRecStore::init called multiple times");
    }
    instance_ = new IMCRegionGroupRecStore();
    int result = instance_->do_init(cfg, store);

    instance_->set_durable_store(store);

    return result;
}


//----------------------------------------------------------------------
void
IMCRegionGroupRecStore::set_durable_store(oasys::DurableStore* store)
{
    durable_store_ = store;
}
    

//----------------------------------------------------------------------
void
IMCRegionGroupRecStore::lock_db_access(const char* lockedby)
{
    global_store_->lock_db_access(lockedby);
}

//----------------------------------------------------------------------
void
IMCRegionGroupRecStore::unlock_db_access()
{
    global_store_->unlock_db_access();
}

//----------------------------------------------------------------------
int
IMCRegionGroupRecStore::begin_transaction(void **txid)
{
    return durable_store_->begin_transaction(txid);
}


//----------------------------------------------------------------------
int
IMCRegionGroupRecStore::end_transaction()
{
    return durable_store_->end_transaction();
}

} // namespace dtn

