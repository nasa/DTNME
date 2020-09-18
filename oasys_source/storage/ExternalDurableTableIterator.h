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

#ifndef __EXTERNALDURABLETABLEITERATOR_H__
#define __EXTERNALDURABLETABLEITERATOR_H__

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include "../debug/Logger.h"
#include "DurableStore.h"
#include "ExternalDurableTableImpl.h"
#include "DataStoreProxy.h"

namespace oasys {

class ExternalDurableTableIterator: public DurableIterator, public Logger {
 
   friend class ExternalDurableTableImpl;

private:
    /**
     * Create an iterator for table t. These should not be called
     * except by ExternalDurableTableImpl
     */
    ExternalDurableTableIterator(ExternalDurableTableImpl &t);

public:
    virtual ~ExternalDurableTableIterator();

    // see comments in class DurableIterator for usage

    virtual int next();
    virtual int get_key(SerializableObject *key);
private:
    vector<string>::size_type cur_; /* current index */
    vector<string> keys_;	/* the keys themselves */
    ExternalDurableTableImpl &table_; /* the table we iter over */

    void iter_init();		/* initialize the above */
};

} /* namespace oasys */

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
#endif // __EXTERNALDURABLETABLEITERATOR_H__
