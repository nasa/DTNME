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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if SQL_ENABLED

#include "SQLBundleStore.h"
#include "SQLStore.h"
#include "StorageConfig.h"
#include "bundling/Bundle.h"

namespace dtn {

/******************************************************************************
 *
 * SQLBundleStore
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLBundleStore::SQLBundleStore(oasys::SQLImplementation* db, const char* table_name)
    : BundleStore()
{
    Bundle tmpobj(this);

    store_ = new SQLStore(table_name, db);
    store_->create_table(&tmpobj);
    store_->set_key_name("bundleid");
}

/**
 * Create a new bundle instance, then make a generic call into the
 * bundleStore to look up the bundle and fill in it's members if
 * found.
 */

Bundle*
SQLBundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle();
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

/**
 * Store the given bundle in the persistent store.
 */
bool
SQLBundleStore::insert(Bundle* bundle)
{
    return store_->insert(bundle) == 0;
}

/**
 * Update the given bundle in the persistent store.
 */
bool
SQLBundleStore::update(Bundle* bundle)
{
    return store_->update(bundle, bundle->bundleid_) == 0;
}

/**
 * Delete the given bundle from the persistent store.
 */
bool
SQLBundleStore::del(int bundle_id)
{
    return store_->del(bundle_id) == 0;
}



int 
SQLBundleStore::delete_expired(const time_t now) 
{
    const char* field = "expiration";
    oasys::StringBuffer query ;
    query.appendf("DELETE FROM %s WHERE %s > %lu", store_->table_name(), field, now);
    
    int retval = store_->exec_query(query.c_str());
    return retval;
}
     


bool
SQLBundleStore::is_custodian(int bundle_id) 
{
    NOTIMPLEMENTED ; 

    /**
     * One possible implementation. Currently b.is_custodian() 
     * function does not exist
     * Bundle b = get(bundle_id);
     * if (b == NULL) return 0;
     * return (b.is_custodian()) ;
     */
}

} // namespace dtn

#endif /* SQL_ENABLED */
