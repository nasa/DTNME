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

#include "SQLGlobalStore.h"
#include "SQLStore.h"
#include "StorageConfig.h"

namespace dtn {

static const char* TABLENAME = "globals";

SQLGlobalStore::SQLGlobalStore(oasys::SQLImplementation* impl)
{
    store_ = new SQLStore(TABLENAME, impl);
    store_->create_table(this);
}

SQLGlobalStore::~SQLGlobalStore()
{
    NOTREACHED;
}

bool
SQLGlobalStore::load()
{
    log_debug("loading global store");

    oasys::SerializableObjectVector elements;
    elements.push_back(this);
    
    int cnt = store_->elements(&elements);
    
    if (cnt == 1) {
        log_debug("loaded next bundle id %d next reg id %d",
                  next_bundleid_, next_regid_);

    } else if (cnt == 0 && StorageConfig::instance()->tidy_) {
        log_info("globals table does not exist... initializing it");
        
        next_bundleid_ = 0;
        next_regid_ = 0;
        
        if (store_->insert(this) != 0) {
            log_err("error initializing globals table");
            exit(1);
        }

    } else {
        log_err("error loading globals table");
        exit(1);
    }

    return true;
}

bool
SQLGlobalStore::update()
{
    log_debug("updating global store");
    
    if (store_->update(this, -1) == -1) {
        log_err("error updating global store");
        return false;
    }
    
    return true;
}

} // namespace dtn

#endif /* SQL_ENABLED */
