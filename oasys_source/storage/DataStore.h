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

#ifndef _DATASTORE_H_
#define _DATASTORE_H_

#include <string>
#include <vector>
#include "../debug/Logger.h"
#include "../util/StringUtils.h" /* for StringPair */
#include "DS.h"

//
// DataStore abstract base class (interface), the functions
// appropriate for an implementation of a data store.
//
// Derive from this, write code for each virtual function. Implement
// each message you handle, return errors from the ones you don't
// handle.


namespace oasys {

// for brevity
typedef dsmessage::ds_request_type *ds_request_type_p;
typedef dsmessage::ds_reply_type *ds_reply_type_p;

class DataStore: public Logger {
public:
    enum storetype_t {
        UNDEFINED = 0,
        PAIR,
        FIELD,
        ADVANCED 
    };

    enum errorcodes_t {
        ERR_NONE = 0,
        ERR_INTERNAL = 1,
        ERR_BADHANDLE = 2,
        ERR_BADTABLE = 3,
        ERR_EXISTS = 4,
        ERR_CANTCREATE = 5,
        ERR_CANTDELETE = 6,
        ERR_CANTOPEN = 7,
        ERR_INVALID = 8,
        ERR_NOTFOUND = 9,
        ERR_NOTSUPPORTED = 10,
        ERR_NOTOPEN = 11,
    };

    /*!
     * credentials passed to the server -- not yet used
     */
    struct credentials_t {
        std::string user;
        std::string password;
        std::vector<std::string> keys;
    };


    /*
     * helper functions for serializing keys and data
     */
        
    // by convention, a pair data store uses these for the field
    // names of its two fields.
    static const char *pair_key_field_name; // == "key"
    static const char *pair_data_field_name; // == "data"

    // need a default constructor
    DataStore(const char *logpath, const char *specific_class = "DataStore") : 
        Logger(specific_class, logpath) 
        {
            // nothing
        }

    // need a virtual destructor
    virtual ~DataStore() { }

    // the functions that implement each of the messages, need to be
    // implemented by the real DSServer. each should return 0 on
    // success. 

    /*!
     * ds_caps: capabilities of this data store server
     * OUT vector<std::string> languages: languages supported
     *          by this ds 
     * OUT std::string storetype: "pair", "field", "advanced"
     * OUT bool supports_triggers
     */
    virtual int ds_caps(std::vector<std::string> &languages, 
                        std::string &storetype, 
                        bool &supports_triggers) = 0;

    /*!
     * ds_create: create a data store
     * IN std::string dsname: data store name
     * IN bool clear: clear out existing store by this name
     * IN int quota: quota in MB
     * IN credentials_t: credentials (TBD)
     * OUT ret: return code
     */
    virtual int ds_create(const std::string &dsname, 
                          const bool clear, 
                          const int quota, 
                          const credentials_t &cred) = 0;

    /*!
     * ds_del delete a data store
     * IN std::string name: name of data store
     * IN credentials_t: credentials (TBD)
     * OUT ret: return code
     */
    virtual int ds_del(const std::string &dsname, 
                       const credentials_t &cred) = 0;

    /*!
     * ds_open: open a data store
     * IN std::string name: name of data store
     * IN int lease: lease time in seconds (zero means forever)
     * IN credentials_t: credentials (TBD)
     * OUT ret: return code
     */
    virtual int ds_open(const std::string &dsname, 
                        int lease, 
                        const credentials_t &cred, 
                        std::string &handle) = 0;

    /*!
     * ds_stat: information about a data store
     * OUT vector<std::string> tables: names of the tables
     * OUT ret: return code
     */
    virtual int ds_stat(const std::string &handle,
                        std::vector<std::string> &tables) = 0;

    /*!
     * ds_close: drop connection to data store
     * OUT ret: return code
     */
    virtual int ds_close(const std::string &handle) = 0;

    /*!
     * table_create: create a named table with the specified fields
     * IN const std::string tablename: name of table to create
     * IN const std::string keyname: which field is the key
     * IN const std::vector<std::string> fieldnames: list of fields
     *          to create
     * OUT int ret: return code
     */
    virtual int table_create(const std::string &handle,
                             const std::string &tablename,
                             const std::string &keyname,
                             const std::string &key_type,
                             const std::vector<StringPair> &fieldinfo) = 0;

    /*!
     * table_del: delete a named table and all of its elements
     * IN const std::string tablename: name of table to delete
     * OUT int ret: return code
     */
    virtual int table_del(const std::string &handle,
                          const std::string &tablename) = 0;

    /*!
     * table_keys
     * IN const std::string &handle
     * IN const std::string tablename
     * OUT std::vector<std::string> keys: all keys of the table
     * OUT int ret: return code
     */
    virtual int table_keys(const std::string &handle,
                           const std::string &tablename,
                           const std::string &keyname,
                           std::vector<std::string> &keys) = 0;


    /*!
     * table_stat: retrieve information about the table
     * IN std::string &handle
     * IN std::string tablename
     * OUT string key: the key for the table
     * OUT vector<string> fieldnames: all the fieldnames
     * OUT vector<string> fieldtypes: all the fieldtypes
     * OUT count: number of elements in the table
     * OUT int ret: return code
     */
    virtual int table_stat(const std::string &handle,
                           const std::string &tablename,
                           std::string &key,
                           std::string &key_type,
                           std::vector<std::string> &fieldnames,
                           std::vector<std::string> &fieldtypes,
                           u_int32_t &count) = 0;

    /*!
     * del: delete a blob with the given key
     * IN std::string tablename
     * IN std::string key
     * OUT int ret: return code
     */
    virtual int del(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &key) = 0;

    /*!
     * put: insert a row (element) into a table
     * IN std::string tablename: name of table
     * IN std::vector<keyval_t> fields: the field values 
     * OUT int ret: return code
     */
    virtual int put(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &key,
                    const std::vector<StringPair> &fields) = 0;

    /*!
     * get: get a row (element) from a table
     * IN std::string tablename: name of table
     * IN std::string key value
     * OUT vector<StringPair>: <field, value> pairs
     * OUT int ret: return code
     */
    virtual int get(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &key,
                    std::vector<StringPair> &fields) = 0;

    /*!
     * select: return 
     * IN std::string tablename: name of table
     * IN std::vector<keyval_t> &constraints: (field,value) pairs to match
     * IN std::vector<std::string> get_fields: names of fields to get
     * IN int32_t howmany: how many to get
     * OUT std::vector<std::vector<std::string> > values: 
     *          values for those fields, for matching elts
     * OUT int ret: return code
     */
    virtual int select(const std::string &handle,
                       const std::string &tablename,
                       const std::vector<StringPair> &constraints,
                       const std::vector<std::string> &get_fields,
                       u_int32_t howmany,
                       std::vector<std::vector<std::string> > &values) = 0;

    /*!
     * eval: execute an arbitrary expression
     * IN std::string language: language the expr is written
     * OUT std::string result: result from evaluation of the expr
     * OUT int ret: return code
     */
    virtual int eval(const std::string &handle,
                     const std::string &language, 
                     const std::string &expr,
                     std::string &result) = 0;

    /*!
     * trigger: not yet well defined
     */
    virtual int trigger(const std::string &handle,
                        const std::string &language, 
                        const std::string &expr,
                        std::string &result) = 0;
};
}
#endif /* _DATASTORE_H_ */

