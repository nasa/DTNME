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

#include "DataStoreProxy.h"
#include "ExternalDurableTableIterator.h"

namespace oasys {

ExternalDurableTableIterator::ExternalDurableTableIterator(ExternalDurableTableImpl &table) :
    Logger("RemoteDataStoreIterator", table.logpath()),
    table_(table)
{
    // cache the keys
    int ret;

    // note that the table may not actually exist, if they create an
    // iterator on a table they haven't put anything into yet
    ret = table_.owner_.proxy_->table_keys(table_.owner_.proxy_->handle_,
                                           table_.table_name_, 
                                           DataStore::pair_key_field_name,
                                           keys_);
    cur_ = 0;
}


ExternalDurableTableIterator::~ExternalDurableTableIterator()
{
    // make sure we're called, so appropriate element destructors are called
}

// move to the next element, return 0 on success, DS_NOTFOUND on end
int ExternalDurableTableIterator::next()
{
    if (cur_ >= keys_.size())
        return DS_NOTFOUND;
    return 0;
}

// get the next key
int ExternalDurableTableIterator::get_key(SerializableObject *key)
{
    if (cur_ >= keys_.size())
        return DS_NOTFOUND;
    string &guy = keys_[cur_];
    Unmarshal unmarshal(Serialize::CONTEXT_LOCAL, 
                        reinterpret_cast<const u_char *>(guy.data()), 
                        guy.length());

    if (unmarshal.action(key) != 0) {
        log_err("DB: error unserializing data object");
        return DS_ERR;
    }
    return 0;
}

} // namespace oasys
#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
