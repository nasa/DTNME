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


#ifndef __BERKELEY_TABLE_STORE_H__
#define __BERKELEY_TABLE_STORE_H__

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#if LIBDB_ENABLED

#include <map>
#include <db.h>

#if (DB_VERSION_MAJOR != 4) && (DB_VERSION_MAJOR != 5)
#error "must use Berkeley DB major version 4 or 5"
#endif

#include "../debug/Logger.h"
#include "../thread/Mutex.h"
#include "../thread/SpinLock.h"
#include "../thread/Timer.h"

#include "DurableStore.h"

namespace oasys {

// forward decls
class BerkeleyDBStore;
class BerkeleyDBTable;
class BerkeleyDBIterator;
struct StorageConfig;

/**
 * Interface for the generic datastore
 */
class BerkeleyDBStore : public DurableStoreImpl {
    friend class BerkeleyDBTable;
    
public:
    BerkeleyDBStore(const char* logpath);

    // Can't copy or =, don't implement these
    BerkeleyDBStore& operator=(const BerkeleyDBStore&);
    BerkeleyDBStore(const BerkeleyDBStore&);

    ~BerkeleyDBStore();

    //! @{ Virtual from DurableStoreImpl
    //! Initialize BerkeleyDBStore
    int init(const StorageConfig& cfg);

    //! Primitive support for transactions (assumes ALL
    //! transactionalized accesses are single-threaded).
    int   begin_transaction(void **txid);
    int   end_transaction(void *txid, bool be_durable);

    //! Allow access to the underlying DB implementation
    void* get_underlying();

    int get_table(DurableTableImpl** table,
                  const std::string& name,
                  int                flags,
                  PrototypeVector&   prototypes);

    int del_table(const std::string& name);
    int get_table_names(StringVector* names);
    std::string get_info() const;
    /// @}

private:
    bool        init_;        //!< Initialized?
    std::string db_name_;     ///< Name of the database file
    DB_ENV*     dbenv_;       ///< database environment for all tables
    bool	sharefile_;   ///< share a single db file

    SpinLock    ref_count_lock_;
    RefCountMap ref_count_;   ///< Ref. count for open tables.

    /// Id that represents the metatable of tables
    static const std::string META_TABLE_NAME;

    /// Get meta-table
    int get_meta_table(BerkeleyDBTable** table);
    
    /// @{ Changes the ref count on the tables, used by
    /// BerkeleyDBTable
    int acquire_table(const std::string& table);
    int release_table(const std::string& table);
    /// @}

    /// DB internal error log callback (unfortunately, the function
    /// signature changed between 4.2 and 4.3)

#if (DB_VERSION_MINOR >= 3) || (DB_VERSION_MAJOR >= 5)
    static void db_errcall(const DB_ENV* dbenv,
                           const char* errpfx,
                           const char* msg);
#else
    static void db_errcall(const char* errpfx, char* msg);
#endif
    
    /// DB internal panic callback
    static void db_panic(DB_ENV* dbenv, int errval);

    /**
     * Timer class used to periodically check for deadlocks.
     */
    class DeadlockTimer : public oasys::Timer, public oasys::Logger {
    public:
        DeadlockTimer(const char* logbase, DB_ENV* dbenv, int frequency)
            : Logger("BerkeleyDBStore::DeadlockTimer",
                     "%s/%s", logbase, "deadlock_timer"),
              dbenv_(dbenv), frequency_(frequency) {}

        void reschedule();
        virtual void timeout(const struct timeval& now);

    protected:
        DB_ENV* dbenv_;
        int     frequency_;
    };

    DeadlockTimer* deadlock_timer_;
};

/**
 * Object that encapsulates a single table. Multiple instances of
 * this object represent multiple uses of the same table.
 */
class BerkeleyDBTable : public DurableTableImpl, public Logger {
    friend class BerkeleyDBStore;
    friend class BerkeleyDBIterator;

public:
    ~BerkeleyDBTable();

    /// @{ virtual from DurableTableImpl
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
    DB*              db_;
    DBTYPE	     db_type_;
    BerkeleyDBStore* store_;

    //! Only BerkeleyDBStore can create BerkeleyDBTables
    BerkeleyDBTable(const char* logpath,
                    BerkeleyDBStore* store, 
                    const std::string& table_name,
                    bool multitype,
                    DB* db, DBTYPE type);

    /// Whether a specific key exists in the table.
    int key_exists(const void* key, size_t key_len);
};

/**
 * Wrapper around a DBT that correctly handles memory management.
 */
class DBTRef {
public:
    /// Initialize an empty key with the DB_DBT_REALLOC flag
    DBTRef()
    {
        memset(&dbt_, 0, sizeof(dbt_));
        dbt_.flags = DB_DBT_REALLOC;
    }

    /// Initialize a key with the given data/len and the
    /// DB_DBT_USERMEM flag
    DBTRef(void* data, size_t size)
    {
        memset(&dbt_, 0, sizeof(dbt_));
        dbt_.data  = data;
        dbt_.size  = size;
        dbt_.flags = DB_DBT_USERMEM;
    }

    /// If any data was malloc'd in the key, free it
    ~DBTRef()
    {
        if (dbt_.flags == DB_DBT_MALLOC ||
            dbt_.flags == DB_DBT_REALLOC)
        {
            if (dbt_.data != NULL) {
                free(dbt_.data);
                dbt_.data = NULL;
            }
        }
    }

    /// Return a pointer to the underlying DBT structure
    DBT* dbt() { return &dbt_; }

    /// Convenience operator overload
    DBT* operator->() { return &dbt_; }

protected:
    DBT dbt_;
};

/**
 * Iterator class for Berkeley DB tables.
 */
class BerkeleyDBIterator : public DurableIterator, public Logger {
    friend class BerkeleyDBTable;

private:
    /**
     * Create an iterator for table t. These should not be called
     * except by BerkeleyDBTable.
     */
    BerkeleyDBIterator(BerkeleyDBTable* t);

public:
    virtual ~BerkeleyDBIterator();

    /// @{ virtual from DurableIteratorImpl
    int next();
    int get_key(SerializableObject* key);
    /// @}

protected:
    DBC* cur_;          ///< Current database cursor
    bool valid_;        ///< Status of the iterator

    DBTRef key_;	///< Current element key
    DBTRef data_;	///< Current element data
};

}; // namespace oasys

#endif // LIBDB_ENABLED

#endif //__BERKELEY_TABLE_STORE_H__
