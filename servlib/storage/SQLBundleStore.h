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

#ifndef _SQL_BUNDLE_STORE_H_
#define _SQL_BUNDLE_STORE_H_

#include <sys/time.h>
#include "BundleStore.h"

namespace dtn {

class SQLImplementation;
class SQLStore;

/**
 * Implementation of a BundleStore that uses an underlying SQL
 * database.
 */

class SQLBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods. The table_name identifies the table in which all bundles
     * will be stored
     */
    SQLBundleStore(oasys::SQLImplementation* db, const char* table_name = "bundles");
    
    /**
     * Virtual methods inheritied from BundleStore
     */
    Bundle* get(int bundle_id) ;
    bool    insert(Bundle* bundle);
    bool    update(Bundle* bundle);
    bool    del(int bundle_id) ;
    int     delete_expired(const time_t now);
    bool    is_custodian(int bundle_id);
    
private:
    
    /**
     * The SQLStore instance used to store all the bundles.
     */
    SQLStore* store_;
};
} // namespace dtn

#endif /* _SQL_BUNDLE_STORE_H_ */
