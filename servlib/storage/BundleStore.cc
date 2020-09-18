/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <inttypes.h>

#include "BundleStore.h"
#include "bundling/Bundle.h"

#include <oasys/storage/DurableStore.h>

namespace oasys {
    template <> dtn::BundleStore* oasys::Singleton<dtn::BundleStore, false>::instance_ = 0;
}

namespace dtn {

bool BundleStore::using_aux_table_ = false;

//----------------------------------------------------------------------
BundleStore::BundleStore(const DTNStorageConfig& cfg)
    : cfg_(cfg),
      bundles_("BundleStore", "/dtn/storage/bundles",
               "bundle", "bundles"),
      payload_fdcache_("/dtn/storage/bundles/fdcache",
                       cfg.payload_fd_cache_size_),
#ifdef LIBODBC_ENABLED
      bundle_details_("BundleStoreExtra", "/dtn/storage/bundle_details",
               "BundleDetail", "bundles_aux"),
#endif
                       total_size_(0)
{
}

//----------------------------------------------------------------------
int
BundleStore::init(const DTNStorageConfig& cfg,
                  oasys::DurableStore*    store)
{
    int err;
    if (instance_ != NULL) {
        PANIC("BundleStore::init called multiple times");
    }
    instance_ = new BundleStore(cfg);
    err = instance_->bundles_.do_init(cfg, store);
#ifdef LIBODBC_ENABLED
    // Auxiliary tables only available with ODBC/SQL
    if ((err == 0) && store->aux_tables_available()) {
    	err = instance_->bundle_details_.do_init_aux(cfg, store);
    	if (err == 0) {
    		using_aux_table_ = true;
    		log_debug_p("/dtn/storage/bundle",
    				    "BundleStore::init initialised auxiliary bundle details table.");
    	}
    } else {
    	log_debug_p("/dtn/storage/bundle", "BundleStore::init skipping auxiliary table initialisation.");
    }
#endif
    return err;
}

//----------------------------------------------------------------------
bool
BundleStore::add(Bundle* bundle)
{
    bool ret;

    lock_.lock("BundleStore::add");

    ret = bundles_.add(bundle);

    lock_.unlock();

    if (ret) {
        //total_size_ += bundle->durable_size();
        reserve_payload_space(bundle);

#ifdef LIBODBC_ENABLED
        if (using_aux_table_) {
            lock_.lock("BundleStore::add (detail)");   // ?? needed here
       	    BundleDetail *details = new BundleDetail(bundle);
       	    ret = bundle_details_.add(details);
            lock_.unlock();
       	    delete details;
        }
#endif
    }
    return ret;
}

//----------------------------------------------------------------------
// All the information about the bundle is in the main bundles table.
// The auxiliary details table does not (currently) need to be accessed.
// NOTE (elwyn/20111231): The auxiliary table 'get' code is essentially
// untested.  The main purpose of the auxiliary table is to allow external
// processes to see the totality of the bundle cache, rather than accessing
// individual bundles, and/or having them delivered.  To this end the auxiliary
// table is 'write only' as far as DTNME is concerned at present.
Bundle*
BundleStore::get(bundleid_t bundleid)
{
    oasys::ScopeLock l(&lock_, "BundleStore::get");
    return bundles_.get(bundleid);
}
    
//----------------------------------------------------------------------
// Whether update has to touch the auxiliary table depends on the items
// placed into the auxiliary details table.  In most cases it is expected
// that the items will be non-mutable over the life of the bundle, so it will
// not be necessary to update the auxiliary table.  The code below can be enabled
// if this situation changes.
bool
BundleStore::update(Bundle* bundle)
{
    int ret;

    lock_.lock("BundleStore::update");

    ret = bundles_.update(bundle);

    lock_.unlock();

#ifdef LIBODBC_ENABLED
#if 0
    if ((ret == 0) && using_aux_table_) {
    	BundleDetail *details = new BundleDetail(bundle);
    	ret = bundle_details_.update(details);
    	delete details;
    }
#endif
#endif

    return ret;
}

//----------------------------------------------------------------------
// When an auxiliary table is configured (only in ODBC/SQL databases), deletion
// of corresponding rows from the auxiliary table is handled by triggers in the
// database.  No action is needed here to keep the tables consistent.
bool
BundleStore::del(Bundle* bundle)
{
    bool ret;

    lock_.lock("BundleStore::del");

    ret = bundles_.del(bundle->bundleid());

    lock_.unlock();

    if (ret) {
        total_size_lock_.lock("del1");
        ASSERT(total_size_ >= bundle->durable_size());
        total_size_ -= bundle->durable_size();
        total_size_lock_.unlock();
    }
    return ret;
}

//----------------------------------------------------------------------
// When an auxiliary table is configured (only in ODBC/SQL databases), deletion
// of corresponding rows from the auxiliary table is handled by triggers in the
// database.  No action is needed here to keep the tables consistent.
bool
BundleStore::del(bundleid_t bundleid, u_int64_t durable_size)
{
    bool ret;

    lock_.lock("BundleStore::del");

    ret = bundles_.del(bundleid);

    lock_.unlock();

    if (ret) {
        total_size_lock_.lock("del2");

        ASSERT(total_size_ >= durable_size);
        total_size_ -= durable_size;
        total_size_lock_.unlock();
    }
    return ret;
}

//----------------------------------------------------------------------
void 
BundleStore::reserve_payload_space(Bundle* bundle)
{
    if (!bundle->payload_space_reserved())
    {
        total_size_lock_.lock("reserve_payload_space");

        total_size_ += bundle->durable_size();
        total_size_lock_.unlock();
        bundle->set_payload_space_reserved();
    }
}

//----------------------------------------------------------------------
bool
BundleStore::try_reserve_payload_space(u_int64_t durable_size)
{
    bool result = true;

    total_size_lock_.lock("try_reserve_payload_space");

    if ((cfg_.payload_quota_ != 0) &&
        ((total_size_ + durable_size) > cfg_.payload_quota_))
    {
        result = false;
    }
    else
    {
        // good to go - reserve the space
        total_size_ += durable_size;
    }
    total_size_lock_.unlock();

    return result;
}

//----------------------------------------------------------------------
void
BundleStore::release_payload_space(u_int64_t durable_size, bundleid_t bundleid)
{
    (void) bundleid;
    total_size_lock_.lock("release_payload_space");

    ASSERT(total_size_ >= durable_size);
    total_size_ -= durable_size;
    total_size_lock_.unlock();
}

//----------------------------------------------------------------------
BundleStore::iterator*
BundleStore::new_iterator()
{
    oasys::ScopeLock l(&lock_, "BundleStore::new_iterator");
    return bundles_.new_iterator();
}
        
//----------------------------------------------------------------------
void
BundleStore::close()
{
    oasys::ScopeLock l(&lock_, "BundleStore::close");
    bundles_.close();
#ifdef LIBODBC_ENABLED
    // Auxiliary tables only available with ODBC/SQL
    if (using_aux_table_)
    {
    	bundle_details_.close();
    }
#endif
}


} // namespace dtn

