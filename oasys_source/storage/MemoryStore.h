/*
 *    Copyright 2005-2006 Intel Corporation
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


#ifndef __MEMORY_STORE_H__
#define __MEMORY_STORE_H__

#include <map>

#include "../debug/Logger.h"
#include "../thread/SpinLock.h"
#include "../util/ScratchBuffer.h"
#include "../util/StringUtils.h"

#include "DurableStore.h"

namespace oasys {

// forward decls
class MemoryStore;
class MemoryTable;
class MemoryIterator;
struct StorageConfig;

/**
 * Object that encapsulates a single table. Multiple instances of
 * this object represent multiple uses of the same table.
 */
class MemoryTable : public DurableTableImpl, public Logger {
    friend class MemoryStore;
    friend class MemoryIterator;

public:
    ~MemoryTable();

    /// @{ virtual from DurableTableInpl
    int get(const SerializableObject& key,
            SerializableObject* data);
    
    int get(const SerializableObject& key,
            SerializableObject** data,
            TypeCollection::Allocator_t allocator);
    
    int put(const SerializableObject& key,
            TypeCollection::TypeCode_t typecode,
            const SerializableObject* data,
            int flags);
    
    int del(const SerializableObject& key);

    size_t size() const;
    
    DurableIterator* itr();
    /// @}

private:
    SpinLock lock_;

    struct Item {
        ScratchBuffer<u_char*>	   key_;
        ScratchBuffer<u_char*>	   data_;
        TypeCollection::TypeCode_t typecode_;
    };

    typedef StringMap<Item*>   ItemMap;
    ItemMap* items_;
    
    oasys::ScratchBuffer<u_char*> scratch_;

    //! Only MemoryStore can create MemoryTables
    MemoryTable(const char* logpath, ItemMap* items,
                const std::string& name, bool multitype);
};

/**
 * Fake durable store that just uses RAM. 
 *
 * N.B: This is not durable unless you have a bunch of NVRAM.
 *
 */
class MemoryStore : public DurableStoreImpl {
    friend class MemoryTable;

public:
    MemoryStore(const char* logpath);

    // Can't copy or =, don't implement these
    MemoryStore& operator=(const MemoryStore&);
    MemoryStore(const MemoryStore&);

    ~MemoryStore();

    //! @{ Virtual from DurableStoreImpl
    //! Initialize MemoryStore
    int init(const StorageConfig& cfg);

    int get_table(DurableTableImpl** table,
                  const std::string& name,
                  int                flags,
                  PrototypeVector&   prototypes);

    int del_table(const std::string& name);
    int get_table_names(StringVector* names);
    std::string get_info() const;

    //! Memory Store doesn't do transactions, so
    //! begin_transaction, end_transaction are not implemented.
    //! get_underlying can't return anything useful either.
    /// @}

private:
    bool init_;        //!< Initialized?

    typedef StringMap<MemoryTable::ItemMap> TableMap;
    TableMap tables_;
};
 
/**
 * Iterator class for Memory tables.
 */
class MemoryIterator : public DurableIterator, public Logger {
    friend class MemoryTable;

private:
    /**
     * Create an iterator for table t. These should not be called
     * except by MemoryTable.
     */
    MemoryIterator(const char* logpath, MemoryTable* t);

public:
    virtual ~MemoryIterator();
    
    /// @{ virtual from DurableIteratorImpl
    int next();
    int get_key(SerializableObject* key);
    /// @}

protected:
    MemoryTable* table_;
    bool first_;
    MemoryTable::ItemMap::iterator iter_;
};

}; // namespace oasys

#endif //__MEMORY_STORE_H__
