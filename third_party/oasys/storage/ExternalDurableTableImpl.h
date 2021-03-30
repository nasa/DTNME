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
#ifndef __EXTERNALDURABLETABLEIMPL_H__
#define __EXTERNALDURABLETABLEIMPL_H__

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include <map>
#include <utility>
#include <vector>

#include "../debug/Logger.h"
#include "../thread/Mutex.h"
#include "../thread/SpinLock.h"
#include "../thread/Timer.h"

#include "DurableStore.h"
#include "ExternalDurableStore.h"

namespace oasys {

class ExternalDurableTableImpl: public DurableTableImpl, public Logger {
    friend class ExternalDurableTableIterator;
        
public:
    ExternalDurableTableImpl(std::string table_name, 
                             bool multitype, 
                             ExternalDurableStore &owner,
                             bool exists);
    virtual ~ExternalDurableTableImpl() { }

    virtual int get(const SerializableObject &key, 
                    SerializableObject *data);

    virtual int put(const SerializableObject &key,
                    TypeCollection::TypeCode_t typecode,
                    const SerializableObject *data,
                    int flags);

    virtual int del(const SerializableObject &key);
    virtual size_t size() const;
    virtual DurableIterator* itr();
private:
    ExternalDurableStore &owner_;
    std::string keyname_;
    bool exists_;               /* does it exist or do we need to create it? */
};
} // oasys

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
#endif // __EXTERNALDURABLETABLEIMPL_H__
