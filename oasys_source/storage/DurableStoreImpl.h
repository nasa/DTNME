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


#ifndef __OASYS_DURABLE_STORE_INTERNAL_HEADER__
#error DurableStoreImpl.h must only be included from within DurableStore.h
#endif

struct StorageConfig;

/**
 * Storage implementation specific pieces of the data store.
 */
class DurableStoreImpl : public Logger {
public:
    //! Map used for ref counting tables
    typedef std::map<std::string, int> RefCountMap;

    /**
     * Typedef for the list of objects passed to get_table.
     */
    typedef std::vector<SerializableObject*> PrototypeVector;

    /**
     * Constructor (initializes the log path).
     */
    DurableStoreImpl(const char* classname, const char* logpath)
        : Logger(classname, "%s", logpath) {}
    
    /**
     * Destructor
     */
    virtual ~DurableStoreImpl() {}

    /*!
     * Real initialization function.
     */
    virtual int init(const StorageConfig& config) = 0;

    /*!
     * Function to do any additional storage initialization once tables have been
     * set up.  May be empty.  Useful if any inter-table connections (such as
     * SQL triggers need to be added).  Default does nothing.
     */
    virtual int create_finalize(const StorageConfig& config) { (void)config; return 0; }

    /**
     * Hook to get or create the implementation-specific components of
     * a durable table.
     *
     */
    virtual int get_table(DurableTableImpl** table,
                          const std::string& db_name,
                          int                flags,
                          PrototypeVector&   prototypes) = 0;

    /**
     * Begin a transaction.  If the implementation doesn't support
     * transactions, then a simple counter of unmatched calls to
     * begin_transaction is implemented by this base class.
     */
    virtual int begin_transaction(void **txid);

    /*
     * end an ongoing transaction, identified by txid if the
     * database implementation wants such an ID.  If be_durable
     * is true, durably save (e.g. to disk) storage state.
     */
    virtual int end_transaction(void *txid, bool be_durable = false);

    /*
     * Return a pointer to the 'Base' of the underlying database
     * implementation.  For BekeleyDB, for example, this would be
     * a pointer to the DB_ENV
     */
    virtual void *get_underlying();

    /**
     * Hook to remove a table (by name) from the data store.
     */
    virtual int del_table(const std::string& db_name) = 0;

    /**
     * Hook to get all of the names of the tables in the store.
     */
    virtual int get_table_names(StringVector* names) = 0;

    /**
     * @return Text description of database configuration.
     */
    virtual std::string get_info() const = 0;
    
    /**
     * Indicates if auxiliary tables are available and configured for use
     * Depends on type of storage (only implemented in ODBC/SQL currently) and
     * configuration setting of odbc_use_aux_tables.
     *
     * @return true or false (logical and of flags)
     */
    virtual bool aux_tables_available();

protected:

    /**
     * Check for the db directory
     * @param db_dir     Directory to check
     * @param dir_exists To be set if directory exists.
     */
    int check_db_dir(const char* db_dir,
                     bool*       dir_exists);

    /**
     * Create database directory.
     */
    int create_db_dir(const char* db_dir);
    
    /**
     * Remove the given directory, after waiting the specified
     * amount of time.
     */
    void prune_db_dir(const char* db_dir,
                      int         tidy_wait);

};

//----------------------------------------------------------------------------
/**
 * Storage implementation specific piece of a table.
 */
class DurableTableImpl {
public:
    DurableTableImpl(std::string table_name, bool multitype)
        : table_name_(table_name), multitype_(multitype) {}

    virtual ~DurableTableImpl() {}

    /**
     * Get the data for the given key from the datastore and
     * unserialize into the given data object.
     *
     * @param key  Key object
     * @param data Data object
     * @return DS_OK, DS_NOTFOUND if key is not found
     */
    virtual int get(const SerializableObject& key, 
                    SerializableObject*       data) = 0;

    /** 
     * For a multi-type table, get the data for the given key, calling
     * the provided allocator function to create the object.
     *
     * Note that a default implementation (that panics) is provided
     * such that subclasses need not support multi-type tables.
     *
     * @param key 	Key object
     * @param data 	Data object 
     * @param allocator Type allocator class
     * @return DS_OK, DS_ERR
     */
    virtual int get(const SerializableObject&   key,
                    SerializableObject**        data,
                    TypeCollection::Allocator_t allocator);
                    
    /**
     * Put data for key in the database
     *
     * @param key      Key object
     * @param typecode Typecode (if multitype)
     * @param data     Data object
     * @param flags    Bit vector of DurableStoreFlags_t values.
     * @return DS_OK, DS_ERR // XXX/bowei
     */
    virtual int put(const SerializableObject&  key,
                    TypeCollection::TypeCode_t typecode,
                    const SerializableObject*  data,
                    int flags) = 0;
    
    /**
     * Delete a (key,data) pair from the database
     * @return DS_OK, DS_NOTFOUND if key is not found
     */
    virtual int del(const SerializableObject& key) = 0;

    /**
     * Return the number of elements in the table.
     */
    virtual size_t size() const = 0;
    
    /**
     * Get an iterator to this table. 
     *
     * @return The new iterator. Caller deletes this pointer.
     */
    virtual DurableIterator* itr() = 0;

    /**
     * Return the name of this table.
     */
    const char* name() const { return table_name_.c_str(); }

protected:
    /**
     * Helper method to flatten a serializable object into a buffer.
     */
    size_t flatten(const SerializableObject& key, 
                   u_char* key_buf, size_t size);
    
    template<size_t _size>
    size_t flatten(const SerializableObject&      key,
                   ScratchBuffer<u_char*, _size>* scratch);
    
    std::string table_name_;	///< Name of the table
    bool multitype_;		///< Whether single or multi-type table
};

//----------------------------------------------------------------------------
// Implementation of the templated method must be in the header
template<size_t _size>
size_t
DurableTableImpl::flatten(const SerializableObject&      key,
                          ScratchBuffer<u_char*, _size>* scratch)
{
    Marshal marshaller(Serialize::CONTEXT_LOCAL, scratch);
    marshaller.action(&key);
    return scratch->len();
}
