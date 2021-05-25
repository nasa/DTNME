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
#ifndef __EXTERNALDATASTORE_H__
#define __EXTERNALDATASTORE_H__

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include <map>
#include <utility>
#include <vector>

#include "../debug/Logger.h"
#include "../thread/Mutex.h"
#include "../thread/SpinLock.h"
#include "../thread/Timer.h"

#include "DurableStore.h"

namespace oasys {

using namespace std;

class DataStoreProxy;   /* forward decl */

/*!
 * an ExternalDataStore works just like an internal data store,
 * but delegates to a DataStoreProxy.
 */
class ExternalDurableStore : public DurableStoreImpl {
    friend class ExternalDurableTableImpl;
    friend class ExternalDurableTableIterator;

private:
    // we don't permit these
    ExternalDurableStore &operator=(const ExternalDurableStore &);
    ExternalDurableStore(const ExternalDurableStore &);

public:
    ExternalDurableStore(const char *logpath);
    ~ExternalDurableStore();

    // the following are inherited from DurableStoreImpl
    virtual int init(const StorageConfig &config);


    /**
     * Get a new handle on a single-type table.
     *
     * @param table      Pointer to the table to be created
     * @param table_name Name of the table
     * @param flags      Options for creating the table
     *
     * @return DS_OK, DS_NOTFOUND, DS_EXISTS, DS_ERR
     */
    virtual int get_table(DurableTableImpl  **table,
                          const std::string &table_name,
                          int               flags,
                          PrototypeVector       &prototypes);

    virtual int del_table(const std::string &name);
    virtual int get_table_names(StringVector *names);
    virtual std::string get_info() const;

    static std::string schema; /* XXX? xml schema */

private:
    /*!
     * the DataStoreProxy instance does the talking
     */
    DataStoreProxy *proxy_;
    std::string handle_;
};

} // namespace oasys

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED

#endif /* __EXTERNALDATASTORE_H__ */
