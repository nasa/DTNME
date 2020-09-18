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

ExternalDurableTableImpl::ExternalDurableTableImpl(string table_name,
                                                   bool multitype,
                                                   ExternalDurableStore &owner,
                                                   bool exists)
    : DurableTableImpl(table_name, multitype),
      Logger("ExternalDurableTableImpl", owner.logpath_),
      owner_(owner),
      exists_(exists)
{
    log_debug("allocated ExternalDurableTableImpl");
    ASSERT(multitype == false);
}

int ExternalDurableTableImpl::get(const SerializableObject &key, 
                                  SerializableObject *data) 
{
    int ret;

    // if the table doesn't exist yet, well, we won't find this
    if (!exists_) {
        return DS_NOTFOUND;
    }

    ret = owner_.proxy_->do_get(table_name_, 
                                DataStore::pair_key_field_name, 
                                key, data);
    if (ret == DataStore::ERR_NOTFOUND)
        return DS_NOTFOUND;
    else if (ret != 0)
        return DS_ERR;          // XXX
    else
        return 0;
}


int ExternalDurableTableImpl::put(const SerializableObject &key,
                                  TypeCollection::TypeCode_t /* typecode */,
                                  const SerializableObject *data,
                                  int flags)
{
    // if the table doesn't exist yet, now we can create it because
    // we can get schema information
    if (!exists_) {
        if (owner_.proxy_->do_table_create(table_name_, 
                                           DataStore::pair_data_field_name, 
                                           *data) != 0) {
            // if we couldn't creat it, bail
            return DS_ERR;      // XXX
        }
        exists_ = true;
    }

    // if !DS_CREATE, don't create new entries
    if ((flags & DS_CREATE) == 0) {
        if (owner_.proxy_->do_get(table_name_, 
                                  DataStore::pair_key_field_name, 
                                  key, 
                                  0) != 0)
            return DS_NOTFOUND;
    }
    if (owner_.proxy_->do_put(table_name_, 
                              DataStore::pair_key_field_name, 
                              key, 
                              data) != 0)
        return DS_ERR;          // XXX
    return 0;
}

int ExternalDurableTableImpl::del(const SerializableObject &key) 
{
    // if the table doesn't exist, well, the record doesn't either
    if (!exists_) {
        return DS_NOTFOUND;
    }

    int ret;
    ret = owner_.proxy_->do_del(table_name_, 
                                DataStore::pair_key_field_name, 
                                key);
    if (ret == DataStore::ERR_CANTDELETE || 
        ret == DataStore::ERR_NOTFOUND)
        return DS_NOTFOUND;
    else if (ret != 0)
        return DS_ERR;          // XXX
    return 0;
}

size_t ExternalDurableTableImpl::size() const 
{
    u_int32_t count;            // how many elements
    
    // if the table doesn't exist yet, well, we know the size
    if (!exists_) {
        return 0;
    }

    if (owner_.proxy_->do_count(table_name_, count) != 0)
        count = 0;              // failed, say zero
    return count;
}

    
/**
 * Get an iterator to this table. 
 *
 * @return The new iterator. Caller deletes this pointer.
 */
DurableIterator* ExternalDurableTableImpl::itr() 
{
    return new ExternalDurableTableIterator(*this);
}

} // oasys

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
