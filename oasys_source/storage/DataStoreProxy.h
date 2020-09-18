/* 
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the pache License, Version 2.0 (the "License"); you may not
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

#ifndef __DATASTOREPROXY_H__
#define __DATASTOREPROXY_H__

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include <map>
#include <utility>
#include <vector>
#include <memory>

#include "../thread/Thread.h"
#include "../thread/Mutex.h"
#include "../thread/MsgQueue.h"
#include "../io/TCPClient.h"
#include "../serialize/XercesXMLSerialize.h"

#include "DS.h"
#include "DataStore.h"
#include "StorageConfig.h"

namespace oasys {

/*!
 * a remote data store proxy
 */
class DataStoreProxy: public DataStore {
    friend class ExternalDurableTableImpl;
    friend class ExternalDurableTableIterator;

public:
    class RequestReply;
    class Worker;

    typedef dsmessage::ds_reply_type *ds_reply_type_p;

    /*!
     * constructor: needs to allocate the TCPClient that will talk
     * to the server
     */
    DataStoreProxy(const char *logpath, 
                   const char *specific_class = "DataStoreProxy");


    /*!
     * destructor: needs to deallocate the TCPClient if it exists
     */
    virtual ~DataStoreProxy();
    
    /*!
     * do object initialization
     */
    virtual int init(const StorageConfig &config, 
                     std::string &handle);

    /*!
     * ds_caps: capabilities of this data store server
     * OUT vector<std::string> languages: languages supported
     *              by this ds 
     * OUT std::string storetype: "pair", "field", "advanced"
     * OUT bool supports_triggers
     * OUT ret: return code
     */
    virtual int ds_caps(std::vector<std::string> &languages, 
                        std::string &storetype, 
                        bool &supports_triggers);

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
                          const credentials_t &cred);

    /*!
     * ds_delete: delete a data store
     * IN std::string name: name of data store
     * IN credentials_t: credentials (TBD)
     * OUT ret: return code
     */
    virtual int ds_del(const std::string &dsname, 
                       const credentials_t &cred);

    /*!
     * ds_open: open a data store
     * IN std::string name: name of data store
     * IN int lease: lease time in seconds (zero means forever)
     * IN credentials_t: credentials (TBD)
     * OUT handle: ref to open db
     * OUT ret: return code
     */
    virtual int ds_open(const std::string &dsname, 
                        int lease, 
                        const credentials_t &cred, 
                        std::string &handle);

    /*!
     * ds_stat: information about a data store
     * IN const std::string handle
     * OUT vector<std::string> tables: names of the tables
     * OUT ret: return code
     */
    virtual int ds_stat(const std::string &handle,
                        std::vector<std::string> &tables);

    /*!
     * ds_close: drop connection to data store
     * OUT ret: return code
     */
    virtual int ds_close(const std::string &handle);

    /*!
     * table_create: create a named table with the specified fields
     * IN const std::string handle
     * IN const std::string tablename: name of table to create
     * IN const std::string keyname: which field is the key
     * IN const std::vector<std::string> fieldnames: list of fields
     *              to create
     */
    virtual int table_create(const std::string &handle,
                             const std::string &tablename,
                             const std::string &keyname,
                             const std::string &key_type,
                             const std::vector<StringPair> &fieldinfo);

    /*!
     * table_del: delete a named table and all of its elements
     * IN const std::string tablename: name of table to delete
     */
    virtual int table_del(const std::string &handle,
                          const std::string &tablename);

    /*!
     * table_keys
     * IN const std::string handle
     * IN const std::string tablename
     * OUT std::vector<std::string> keys
     */
    virtual int table_keys(const std::string &handle,
                           const std::string &tablename,
                           const std::string &keyname,
                           std::vector<std::string> &keys);

    /*!
     * table_stat
     * IN const std::string tablename: name of table to delete
     * OUT std::vector<std::string> fields: field names
     */
    virtual int table_stat(const std::string &handle,
                           const std::string &tablename,
                           std::string &key,
                           std::string &key_type,
                           std::vector<std::string> &fieldnames,
                           std::vector<std::string> &fieldtypes,
                           u_int32_t &count);

    /*!
     * put: insert a row (element) into a table
     * IN const std::string handle
     * IN std::string tablename: name of table
     * IN std::vector<keyval_t> fields: the field values 
     */
    virtual int put(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &key,
                    const std::vector<StringPair> &fields);

    /*!
     * get: get a row (element) from a table
     * IN std::string tablename: name of table
     * IN std::string key value
     * OUT vector<StringPair>: <field, value> pairs
     */
    virtual int get(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &keyval,
                    std::vector<StringPair> &fields);

    /*!
     * delete: remove a row (element) from a table
     * IN std::string tablename: name of table
     * IN std::string key value
     */
    virtual int del(const std::string &handle,
                    const std::string &tablename, 
                    const std::string &keyname,
                    const std::string &keyval);

    /*!
     * field_select: return 
     * IN const std::string handle
     * IN std::string tablename: name of table
     * IN std::vector<keyval_t> &constraints: (field,value) pairs to match
     * IN std::vector<std::string> get_fields: names of fields to get
     * IN int32_t howmany: how many to get
     * OUT std::vector<std::vector<std::string> > values: 
     *              values for those fields, for matching elts
     */
    virtual int select(const std::string &handle,
                       const std::string &tablename,
                       const std::vector<StringPair> &constraints,
                       const std::vector<std::string> &get_fields,
                       u_int32_t howmany,
                       std::vector<std::vector<std::string> > &values);

    /*!
     * eval: execute an arbitrary expression
     * IN const std::string handle
     * IN std::string language: language the expr is written
     * OUT std::string result: result from evaluation of the expr
     */
    virtual int eval(const std::string &handle,
                     const std::string &language, 
                     const std::string &expr,
                     std::string &result);

    /*!
     * trigger is not supported by the C++ binding
     */
    virtual int trigger(const std::string &handle,
                        const std::string &language, 
                        const std::string &expr,
                        std::string &result);

    // what kind of store is this
    enum storetype_t storetype() { return st_; }

    // helper functions for durable tables, which defer to this object
    // put an element
    int do_put(const std::string &tablename, 
               const std::string &keyname,
               const SerializableObject &key, 
               const SerializableObject *data);

    // get an element. note that if data == 0, the object
    // is not returned to the caller; this can be used to 
    // see if an object with that key already exists.
    int do_get(const std::string &tablename, 
               const std::string &keyname,
               const SerializableObject &key, 
               SerializableObject *data);

    // delete an element
    int do_del(const std::string &tablename, 
               const std::string &keyname,
               const SerializableObject &key);

    // get the number of elements
    int do_count(const std::string tablename, u_int32_t &count);

    // create a table
    int do_table_create(const std::string &tablename, 
                        const std::string &keyname,
                        const SerializableObject &obj);

protected:
    bool init_;             /* has it been initialized successfully? */
    storetype_t st_;        /* store type */
    credentials_t cred_;
    std::string handle_;    /* handle to the open store */

    // helper functions for the do_* helper functions
    static int do_serialize(const SerializableObject &obj, std::string &str);
    static int do_serialize(const SerializableObject &obj, 
                            std::vector<StringPair> &fields,
                            bool get_schema = false);
    static int do_unserialize(const std::string &str, SerializableObject *obj);
    static int do_unserialize(const std::vector<StringPair> &fields, 
                              SerializableObject *obj);
        

private:
    void execute(dsmessage::ds_request_type &request, 
                 ds_reply_type_p &reply);
    Worker *worker_;
    bool do_init_;
    std::string dbname_;
};

/*!
 * This is what the code for the class above adds to the worker's message queue.
 * As each is a notifier, the caller can wait on the object itself.
 */
class DataStoreProxy::RequestReply: public oasys::Notifier {
public:
    RequestReply(const char *logpath, 
                 dsmessage::ds_request_type &request) :
        Notifier(logpath), 
        request_(request), 
        reply_(0) { };

    dsmessage::ds_request_type &request_;
    ds_reply_type_p reply_;
};

/*!
 * The worker that handles communication with the server. In some
 * sense, this is the proxy itself.  The worker is never
 * manipulated directly by client code; let a DataStoreProxy
 * do it for you.
 */
class DataStoreProxy::Worker: public Thread, public Logger {
public:
    Worker(const char *logpath, const StorageConfig &);
    virtual ~Worker();

    /* main loop code */
    virtual void run();

    /* called to halt the worker, blocks until worker finishes */
    int shutdown();

    /* communication between my proxy and myself */
    MsgQueue<RequestReply *> *mq_;

    /* or, send and recv things synchronously if we're in startup*/
    void send(dsmessage::ds_request_type &request);
    void recv(ds_reply_type_p &reply);

    // was it correctly constructed?
    bool valid() { return valid_; }

private:
    /* connection to the server */
    TCPClient *conn_;

    Mutex flag_mutex_;
    Notifier shutdown_notifier_;
    bool valid_;            /* did construction succeed? */

    // generate a unique cookie
    std::string gen_cookie(void);
    u_int64_t cur_cookie;

    // parser for the xml that comes back
    XercesXMLUnmarshal parser_;

};

} // namespace oasys

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
#endif // __DATASTOREPROXY_H__
