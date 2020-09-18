/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _INTERNAL_KEY_DURABLE_TABLE_H_
#define _INTERNAL_KEY_DURABLE_TABLE_H_

#include "../debug/Logger.h"
#include "../debug/DebugUtils.h"
#include "DurableStore.h"
#include "StorageConfig.h"

namespace oasys {

/**
 * Single type durable table adapter interface used to simplify cases
 * in which the data objects to be stored contain at least one field
 * that is the unique identifier and is wrapped in the table by one of
 * the TypeShims.
 *
 * This interface provides simple hooks for add(), get(), del(), and
 * update() that take only a pointer to the class, not a secondary
 * argument that is the id. The class also implements an alternative
 * iterator interface wherein the iterator stores the current element,
 * rather than forcing the caller to have a local temporary.
 *
 * To fulfill the contract required by the template, the stored class
 * must implement a function called durable_key() that returns the
 * unique key value, suitable to be passed to the _ShimType
 * constructor.
 *
 * Finally, to cover the most common (so far) use cases for this
 * class, it implements logging and assertion handlers to cover
 * unexpected cases in the interface, e.g. logging a warning on a call
 * to get() for an id that's not in the table, PANIC on internal
 * database errors, etc.
 */
template <typename _ShimType, typename _KeyType, typename _DataType>
class InternalKeyDurableTable : public Logger {
public:
    InternalKeyDurableTable(const char* classname,
                            const char* logpath,
                            const char* datatype,
                            const char* table_name);

    virtual ~InternalKeyDurableTable();
    
    /**
     * Real initialization methods.
     */
    int do_init(const StorageConfig& cfg,
                DurableStore*        store);
    int do_init_aux(const StorageConfig& cfg,
                DurableStore*        store);

    /**
     * Close and flush the table.
     */
    void close();
    
    bool add(_DataType* data);
    
    _DataType* get(_KeyType id);
    
    bool update(_DataType* data);

    bool del(_KeyType id);

    /**
     * STL-style iterator.
     */
    class iterator {
    public:
        typedef class InternalKeyDurableTable<_ShimType,
                                              _KeyType,
                                              _DataType> table_t;

        virtual ~iterator();

        /**
         * Advances the iterator.
         *
         * @return DS_OK, DS_NOTFOUND if no more elements, DS_ERR if
         * an error occurred while iterating.
         */
        int next();

        /**
         * Alternate hook to next() for starting iterating.
         */
        int begin() { return next(); }

        /**
         * Return true if iterating is done.
         */
        bool more() { return !done_; }

        /**
         * Accessor for the value.
         */
        _KeyType cur_val() { return cur_val_.value(); }

    private:
        friend class InternalKeyDurableTable<_ShimType,
                                             _KeyType,
                                             _DataType>;

        iterator(table_t* table, DurableIterator* iter);

        table_t*  table_;	///< Pointer to the containing table
        DurableIterator* iter_;	///< The underlying iterator
        _ShimType cur_val_;	///< Current field value
        bool      done_;	///< Flag indicating if at end
    };
    
    /**
     * Return a new iterator. The caller has the responsibility of
     * deleting it once done.
     */
    iterator* new_iterator()
    {
        return new iterator(this, table_->itr());
    }

protected:
    SingleTypeDurableTable<_DataType>* table_;
    const char* datatype_;
    const char* table_name_;
};

#include "InternalKeyDurableTable.tcc"

} // namespace oasys

#endif /* _INTERNAL_KEY_DURABLE_TABLE_H_ */
