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

#ifndef _SQL_GLOBAL_STORE_H_
#define _SQL_GLOBAL_STORE_H_

#include "GlobalStore.h"

namespace oasys {
class SQLImplementation;
}

namespace dtn {

class SQLStore;

/**
 * Implementation of GlobalStore that uses an underlying SQL database.
 */
class SQLGlobalStore : public GlobalStore {
public:
    /**
     * Constructor
     */
    SQLGlobalStore(oasys::SQLImplementation* impl);

    /**
     * Destructor
     */
    virtual ~SQLGlobalStore();

    /**
     * Load in the values from the database.
     */
    virtual bool load();

    /**
     * Update the values in the database.
     */
    virtual bool update();

protected:
    SQLStore* store_;
};
} // namespace dtn

#endif /* _SQL_GLOBAL_STORE_H_ */
