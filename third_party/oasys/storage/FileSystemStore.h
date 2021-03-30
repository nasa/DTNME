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


#ifndef __FILESTORE_H__
#define __FILESTORE_H__

#include <sys/types.h>
#include <dirent.h>

#include "../debug/Logger.h"
#include "../thread/SpinLock.h"
#include "../util/ScratchBuffer.h"
#include "../util/OpenFdCache.h"

#include "DurableStore.h"

namespace oasys {

struct ExpandableBuffer;

struct StorageConfig;

class FileSystemStore;
class FileSystemTable;
class FileSystemIterator;

/*!
 * The most obvious layering of backing store -- use the file system
 * directly.
 *
 * NEW: Now with a level of indirection!
 */
class FileSystemStore : public DurableStoreImpl {
    friend class FileSystemTable;

    typedef oasys::OpenFdCache<std::string> FdCache;

public:
    FileSystemStore(const char* logpath);

    // Can't copy or =, don't implement these
    FileSystemStore& operator=(const FileSystemStore&);
    FileSystemStore(const FileSystemStore&);

    ~FileSystemStore();

    //! @{ virtual from DurableStoreImpl
    int init(const StorageConfig& cfg);
    int get_table(DurableTableImpl** table,
                  const std::string& name,
                  int                flags,
                  PrototypeVector&   prototypes);
    int del_table(const std::string& name);
    int get_table_names(StringVector* names);
    std::string get_info() const;

    //! FileSystemStore doesn't really do transactions, so
    //! begin_transaction is not implemented, but
    //! end_transaction forces an fsync of all the open
    //! file descriptors used for tables (in the cache_).
    //! get_underlying can't return anything useful either.
    int end_transaction (void *txid, bool be_durable);
    //! @}

private:
    bool        init_;
    std::string db_dir_;     //!< parent directory for the db
    std::string tables_dir_; //!< directory where the tables are stored

    RefCountMap ref_count_;     // XXX/bowei -- not used for now
    int         default_perm_;  //!< Default permissions on database files
    
    FdCache*    fd_cache_;

    //! Check for the existance of databases. @return 0 on no error.
    //! @return -2 if the database file doesn't exist. Otherwise -1.
    int check_database();

    //! Create the database. @return 0 on no error.
    int init_database();
    
    //! Wipe the database. @return 0 on no error.
    void tidy_database();

    /// @{ Changes the ref count on the tables
    // XXX/bowei -- implement me
    int acquire_table(const std::string& table);
    int release_table(const std::string& table);
    /// @}
};

class FileSystemTable : public DurableTableImpl, public Logger {
    friend class FileSystemStore;
public:
    ~FileSystemTable();

    //! @{ virtual from DurableTableInpl
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
    //! @}

private:
    std::string path_;

    /*!
     * Shared Fd cache.
     */
    FileSystemStore::FdCache* cache_;

    FileSystemTable(const char*               logpath,
                    const std::string&        table_name,
                    const std::string&        path,
                    bool                      multitype,
                    FileSystemStore::FdCache* cache);

    int get_common(const SerializableObject& key,
                   ExpandableBuffer* buf);
};

class FileSystemIterator : public DurableIterator {
    friend class FileSystemTable;
private:
    /**
     * Create an iterator for table t. These should not be called
     * except by FileSystemTable.
     */
    FileSystemIterator(const std::string& directory);

public:
    virtual ~FileSystemIterator();
    
    //! @{ virtual from DurableIteratorImpl
    int next();
    int get_key(SerializableObject* key);
    //! @}

protected:
    struct dirent* ent_;
    DIR*           dir_;
};

} // namespace oasys

#endif /* __FILESTORE_H__ */
