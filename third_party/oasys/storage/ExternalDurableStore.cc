/* 
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include "ExternalDurableStore.h"
#include "DataStoreProxy.h"
#include "ExternalDurableTableIterator.h"
#include "StorageConfig.h"

#include "../util/ExpandableBuffer.h"
#include "../serialize/KeySerialize.h"
#include "../serialize/TypeCollection.h"
#include "../io/FileUtils.h"
#include "../io/IO.h"

namespace oasys {

// constructor
ExternalDurableStore::ExternalDurableStore(const char *logpath)
    : DurableStoreImpl("ExternalDurableStore", logpath),
      proxy_(0)
{
    // don't do any setup yet
    log_notice("ExternalDurableStore allocated");
}

ExternalDurableStore::~ExternalDurableStore()
{
    log_notice("ExternalDurableStore deallocated");
    delete proxy_;
}

int ExternalDurableStore::init(const StorageConfig &config)
{
    ASSERT(proxy_ == 0);

    // assume that config.type_ == "external" for now. 
    // In the future we might have internal data store
    // clients that use this interface
    if (config.type_ == "external") {
        proxy_ = new DataStoreProxy(logpath_);
        log_debug("DataStoreProxy allocated");
#if 0
    } else {
        proxy_ = new InternalDataStoreProxy(logpath_);
#endif
    }
    return proxy_->init(config, handle_);
}

int ExternalDurableStore::get_table(DurableTableImpl **table,
                                    const std::string &name,
                                    int                flags,
                                    PrototypeVector   &/* prototypes */)
{
    vector<string> fieldnames;
    vector<string> fieldtypes;
    string key, key_type;
    u_int32_t count;            // how many elements
    int ret;                    // return code
    bool exists = false;

    log_debug("looking up table %s", name.c_str());

    if (flags & oasys::DS_CREATE) {
        ret = proxy_->table_del(handle_, name);
    } else {
        // see if the table exists
        ret = proxy_->table_stat(handle_, name, key, key_type, fieldnames,
                                 fieldtypes, count);
        exists = (ret == 0);
    }

    *table = new ExternalDurableTableImpl(name, false, *this, exists);
    return 0;
}

int ExternalDurableStore::del_table(const std::string &name)
{
    log_debug("deleting table %s", name.c_str());
    return proxy_->table_del(handle_, name);
}

int ExternalDurableStore::get_table_names(StringVector *names)
{
    return proxy_->ds_stat(handle_, *names);
}

std::string ExternalDurableStore::get_info() const
{
    // XXX
    return "external";
}

// class static data
// XXX ???
std::string ExternalDurableStore::schema;


} // namespace oasys

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED

