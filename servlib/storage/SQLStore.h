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

#ifndef _SQL_STORE_H_
#define _SQL_STORE_H_

#include <vector>
#include <sys/time.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/SQLSerialize.h>
#include "BundleStore.h"
#include "PersistentStore.h"

namespace dtn {

/**
 * Implementation of a StorageManager with an underlying SQL
 * database.
 */
class SQLStore : public PersistentStore, public oasys::Logger  {
public:
    
    /**
     * Create a SQLStore with specfied table_name.
     * Parameter db, points to the actual implementation to which
     * different queries are forwarded
     */
    SQLStore(const char* table_name, oasys::SQLImplementation *db);
    
    /// @{

    /**
     * Close the table.
     */
    int close();
    
    /**
     *  Get an obj (identified by key) from the sql store. 
     *  @return 0 if success, -1 on error
     */
    int get(oasys::SerializableObject* obj, const int key);
 
    /**
     * Store the object with the given key.
     */
    int put(oasys::SerializableObject* obj, const int key);

    /**
     *  Put an obj in the sql store. 
     *  @return 0 if success, -1 on error
     */
    int add(oasys::SerializableObject* obj, const int key);
    
    /**
     *  Update the object's state in the sql store.
     *  @return number updated on success, -1 on error
     */
    int update(oasys::SerializableObject* obj, const int key);
    
    /**
     *  Delete an obj (identified by key)
     *  @return 0 if success, -1 on error
     */
    int del(const int key);

    /**
     *  Return number of elements in the store.
     *  @return number of elements if success, -1 on error
     */
    int num_elements();

    /**
     *  Return list of keys for all elements in the store.
     *  @return 0 if success, -1 on error
     */
    void keys(std::vector<int> * l);

    /**
     * 
     */
    int exists(const int id);
    
    /**
     *  Extract all elements from the store, potentially matching the
     *  "where" clause. For each matching element, initialize the
     *  corresponding object in the vector with the values from the
     *  database.
     *
     *  @return count of extracted elements, or -1 on error
     */
    int elements(oasys::SerializableObjectVector* elements);

    /**
     * Returns the table name associated with this store
     */
    const char* table_name();

    /**
     * Checks if the table already exists.
     * @return true if table exits, false otherwise
     */
    bool has_table(const char *name);

    /**
     * Creates table a table in the store for objects which has the same type as obj. 
     * Checks if the table already exists.
     * @return 0 if success, -1 on error
     */
    int create_table(oasys::SerializableObject* obj);


    /**
     * Executes the given query. Essentially forwards the query to data_base_pointer_
     * @return 0 if success, -1 on error
     */
    int exec_query(const char* query);

    /**
     * Set's the key name. The key name is used in all functions which need
     * to create a query which refers to key. Must be initialized for proper
     * operation.
     */
    void set_key_name(const char* name);

private:

    /**
     * Name of the table in the database to which this SQLStore corresponds
     */
    const char* table_name_;    
    
    /**
     * Field name by which the objects are keyed. 
     */
    const char*  key_name_; 

    oasys::SQLImplementation*  sql_impl_;
};

} // namespace dtn

#endif /* _SQL_STORE_H_ */
