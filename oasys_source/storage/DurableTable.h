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
#error DurableTable.h must only be included from within DurableStore.h
#endif

/**
 * Object that encapsulates a single durable table. The interaction
 * with the underlying storage implementation is handled by the
 * DurableTableImpl class.
 */
template <typename _Type>
class DurableTable {
public:
    DurableTable(DurableTableImpl*   impl,
                 const std::string&  name,
                 DurableObjectCache<_Type>* cache)
        : impl_(impl), name_(name), cache_(cache) {}
    
    ~DurableTable()
    {
        delete impl_;
    }

    /**
     * Delete a (key,data) pair from the database
     *
     * @return DS_OK, DS_NOTFOUND if key is not found
     */
    int del(const SerializableObject& key);

    /**
     * Return the number of elements in the table.
     */
    size_t size();

    /**
     * Return a newly allocated iterator for the table. Caller has the
     * responsibility to delete it once finished.
     */
    DurableIterator* itr() { return impl_->itr(); }

    /**
     * Return the underlying table implementation.
     */
    DurableTableImpl* impl() { return impl_; }

    /**
     * Return table name.
     */
    std::string name() { return name_; }

    /**
     * Return pointer to the cache (if any).
     */
    DurableObjectCache<_Type>* cache() { return cache_; }

protected:
    DurableTableImpl*   impl_;
    std::string         name_;
    DurableObjectCache<_Type>* cache_;

    int cleanup_put_flags(int flags);

private:
    DurableTable();
    DurableTable(const DurableTable&);
};

/**
 * Class for a durable table that only stores one type of object,
 * represented by the template parameter _DataType.
 */
template <typename _DataType>
class SingleTypeDurableTable : public DurableTable<_DataType> {
public:
    /**
     * Constructor
     */
    SingleTypeDurableTable(DurableTableImpl*   impl,
                           const std::string&  name,
                           DurableObjectCache<_DataType>* cache)
        : DurableTable<_DataType>(impl, name, cache) {}

    /** 
     * Update the value of the key, data pair in the database. It
     * should already exist.
     *
     * @param key   Key object
     * @param data  Data object
     * @param flags Bit vector of DurableStoreFlags_t values.
     * @return DS_OK, DS_NOTFOUND, DS_ERR
     */
    int put(const SerializableObject& key,
            const _DataType* data,
            int flags);

    /**
     * Get the data for key, possibly creating a new object of the
     * template type _DataType. Note that the given type must match
     * the actual type that was stored in the database, or this will
     * return undefined garbage.
     *
     * @param key  Key object
     * @param data Data object
     * @param from_cache == true if the object retrieved from
     *     the cache
     *
     * @return DS_OK, DS_NOTFOUND if key is not found
     */
    int get(const SerializableObject& key,
            _DataType**               data,
            bool*                     from_cache = 0);

    /**
     * Get variant which can take a blank object down into the get
     * function. 
     */
    int get_copy(const SerializableObject& key,
                 _DataType* data);
    
private:
    // Not implemented on purpose -- can't copy
    SingleTypeDurableTable(const SingleTypeDurableTable&);
};

/**
 * Class for a durable table that can store various objects, each a
 * subclass of _BaseType which must in turn be or be a subclass of
 * TypedSerializableObject, and that has a type code defined in the
 * template parameter _Collection.
 */
template <typename _BaseType, typename _Collection>
class MultiTypeDurableTable : public DurableTable<_BaseType> {
public:
    /**
     * Constructor
     */
    MultiTypeDurableTable(DurableTableImpl*   impl,
                          const std::string&  name,
                          DurableObjectCache<_BaseType>* cache)
        : DurableTable<_BaseType>(impl, name, cache) {}
    
    /** 
     * Update the value of the key, data pair in the database. It
     * should already exist.
     *
     * @param key   Key object
     * @param type  Type code for the object
     * @param data  Data object
     * @param flags Bit vector of DurableStoreFlags_t values.
     * @return DS_OK, DS_NOTFOUND, DS_ERR
     */
    int put(const SerializableObject& key,
            TypeCollection::TypeCode_t type,
            const _BaseType* data,
            int flags);

    /**
     * Get the data for key, possibly creating a new object of the
     * given template type _Type (or some derivative), using the
     * multitype collection specified by _Collection. _Type therefore
     * must be a valid superclass for the object identified by the
     * type code in the database.
     *
     * @param key  Key object
     * @param data Data object
     * @param from_cache == true if the object retrieved from
     *     the cache
     *
     * @return DS_OK, DS_NOTFOUND if key is not found
     */
    int get(const SerializableObject& key,
            _BaseType**               data,
            bool*                     from_cache = 0);

    /**
     * Object allocation callback that is handed to the implementation
     * to allow it to properly create an object once it extracts the
     * type code.
     */
    static int new_object(TypeCollection::TypeCode_t typecode,
                          SerializableObject** generic_object);

private:
    // Not implemented on purpose -- can't copy
    MultiTypeDurableTable(const MultiTypeDurableTable&);
};

/**
 * Class for a durable table that can store objects which share no
 * base class and have no typecode. This is used for tables (such as a
 * property table) which have specified compile time programmer
 * defined types and have no need for creating a class hierarchy of
 * serializable types to handle. (e.g. a table containing both strings
 * and sequence #s)
 *
 * The underlying DurableObjectCache is specialized with 
 * SerializableObject.
 */
class StaticTypedDurableTable : public DurableTable< SerializableObject > {
public:
    /**
     * Constructor - We don't support caches for now. These tables are
     * usually small in size and have contents which need to be
     * immediately durable, so this should not be a problem.
     */
    StaticTypedDurableTable(DurableTableImpl*   impl,
                            const std::string&  name)
        : DurableTable< SerializableObject >(impl, name, 0) {}
    
    
    template<typename _Type>
    int put(const SerializableObject& key, const _Type* data, int flags);

    template<typename _Type>
    int get(const SerializableObject& key, _Type** data);

private:
    // Not implemented on purpose -- can't copy
    StaticTypedDurableTable(const StaticTypedDurableTable&);
};
