/* 
 * 
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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <map>

#include "../util/StringBuffer.h"
#include "DataStoreProxy.h"
#include "ExternalDurableStore.h"
#include "../serialize/MarshalSerialize.h"
#include "../serialize/StringPairSerialize.h"

#include "DS.h"

using namespace std;
using namespace dsmessage;
using namespace oasys;

/*!
 * set the connection to zero; allocate the connection on init()
 */
DataStoreProxy::DataStoreProxy(const char *logpath, 
                               const char *specific_class) :
    DataStore(logpath, specific_class),
    st_(UNDEFINED),
    worker_(0)
{
    log_debug("%s: allocated", __func__);
    return;
}

/*!
 * Tell the worker thread to shut down. This will block and wait for
 * the queue to flush.
 */
DataStoreProxy::~DataStoreProxy()
{
    // delete if allocated
    if (worker_ != 0) {
        worker_->shutdown();    // wait for it to shut down...
        delete worker_;         // ... then delete it
        worker_ = 0;
    }

    log_debug("%s: deallocated", __func__);
    return;
}

/*!
 * initialize the proxy
 */
int DataStoreProxy::init(const StorageConfig &config, string &handle)
{
    log_debug("%s called", __func__);

    // make sure we are running single threaded 
    ASSERT(oasys::Thread::start_barrier_enabled());

    // make sure they specified a schema
    if (config.schema_.length() == 0) {
        log_warn("no XML schema file specified for external data store");
        return -1;              // XXX DS_OPEN_FAILED?
    }

    log_info("Using XML schema file %s", config.schema_.c_str());

    // allocate the worker, which opens the TCP connection for us
    worker_ = new Worker(logpath_, config);
    if (worker_->valid() == false) {
        return -1;              // XXX DS_OPEN_FAILED?
    }

    // copy over variables we need
    // cred_ = config.cred;

    // learn what kind of store it is
    vector<string> languages;
    string st;
    bool does_triggers;
    int ret = ds_caps(languages, st, does_triggers);
    if (ret != 0) {
        log_debug("%s: ds_caps failed", __func__);
        return ret;
    }

    // there must be a better way to do this. Can we somehow use the
    // strings that are in DS.c?

    // determine what kind of store this is
    if (st == "pair")
        st_ = PAIR; 
    else if (st == "field")
        st_ = FIELD;
    else if (st == "advanced")
        st_ = ADVANCED;
    ASSERT(st_ != UNDEFINED);
    log_debug("%s: this is a %s data store", __func__, st.c_str());

    // create it if requested to
    if (config.init_) {
        log_debug("%s: creating data store \"%s\"", __func__, config.dbname_.c_str());
        ds_create(config.dbname_, true, 0, cred_);
        if (ret != 0) {
            log_debug("%s: data store creation failed", __func__);
            return ret;
        }
    }

    // open it
    ds_open(config.dbname_, 0, cred_, handle_);
    if (ret != 0) {
        log_debug("%s: open failed", __func__);
        return ret;
    }
    handle = handle_;           // pass back to caller

    // note: don't start the worker here. Wait until we've gone
    // multithreaded, and at that point the first request will cause
    // the worker to get kicked off.
    // start the worker thread
    // was: worker_->start();

    // success
    init_ = true;
    log_debug("%s: initialization succeeded", __func__);
    return 0;
}

//----------------------------------------------------------------

/*!
 * constructor for the worker class, nothing to do here
 */
DataStoreProxy::Worker::Worker(const char *logpath, 
                               const StorageConfig &config) :
    Thread("DataStoreProxy", 0),
    Logger("DataStoreProxy::Worker", logpath),
    flag_mutex_(logpath),
    shutdown_notifier_(logpath),
    valid_(false),
    cur_cookie(0),
    parser_(true, config.schema_.c_str())
{
    log_debug("%s: allocated worker", __func__);

    // create the TCPClient connection
    // we don't need to set any of the IPSocket params, defaults are good
    conn_ = new TCPClient(logpath_, true);

    log_debug("%s: allocated tcp client", __func__);

    // configure the socket
    int ret = conn_->connect(htonl(INADDR_LOOPBACK), config.server_port_);
    if (ret != 0) {
        log_err("%s: could not connect to server on port %d", __func__, config.server_port_);
        return;
    }

    // allocate the message queue
    mq_ = new MsgQueue < DataStoreProxy::RequestReply * >(logpath_);

    valid_ = true;
    return;
}

DataStoreProxy::Worker::~Worker()
{
    // clean up
    delete conn_;
    delete mq_;
    return;
}

int DataStoreProxy::Worker::shutdown()
{
    // set the flag that tells the thread to stop, then kick it so
    // it wakes up
    set_should_stop();
    mq_->notify();

    // wait for the worker to shut down
    // XXX this hangs on shutdown! 
    shutdown_notifier_.wait();

    // all done
    return 0;
}

void DataStoreProxy::Worker::send(ds_request_type &request)
{
    // convert request a string and transmit to the server
    xml_schema::namespace_infomap map;
    ostringstream oss;

    // generate printable xml
    ds_request(oss, request, map);

    const string buf(oss.str());

    // capture the length of the string
    StringBuffer len;
    len.appendf("%08zu", buf.length());

    log_debug("%s: sending to server \"%s%s\"", __func__, len.c_str(), buf.c_str()); 

    // write out length, followed by the XML itself
    conn_->writeall(const_cast< char * >(len.c_str()), len.length());
    conn_->writeall(const_cast< char * >(buf.c_str()), buf.length());

    return;
}

// #include <iostream>

void DataStoreProxy::Worker::recv(ds_reply_type_p &reply)
{
    reply = 0;                  // to be safe

    // read the length first
    int l;
    char lenchars[9];
    int len = 8;
    l = conn_->readall(lenchars, len);
    if (l != len) {
        log_err("%s: short read on header, expected %d, got %d", __func__, len, l);
        return;
    }
    lenchars[8] = '\0';
    len = atol(lenchars);
    log_debug("%s: expecting %d-byte (\"%s\") XML message", __func__, len, lenchars);
    // now read the buffer
    char buf[len + 1];
    l = conn_->readall(buf, len);
    if (l != len) {
        log_err("%s: short read on body, expected %d, got %d", __func__, len, l);
        return;
    }
    buf[len] = '\0';
    log_debug("%s: received XML from server:\n%s", __func__, buf); 

    // parse the buffer into an xml object
    parser_.reset_error();
    const xercesc::DOMDocument *document = parser_.doc(buf);
    if (document == 0) {
        log_debug("unable to parse document");
        return;                 // XXX
    }

    try {
        auto_ptr<ds_reply_type> rp(ds_reply(*document));
        log_debug("%s: parse succeeded, object is at location %p", __func__, rp.get());
        {
            xml_schema::namespace_infomap map;
            ostringstream oss;
            string foo(oss.str());
            ds_reply(oss, *rp, map);
            log_debug("%s: received %s", __func__, foo.c_str());
        }
        reply = rp.release();
        log_debug("%s: released auto_ptr, ptr is %p, error is %s",
                  __func__, reply, reply->error().c_str());
    } catch (const xml_schema::exception &e) {
        log_debug("%s: parse failed: %s\n", __func__, e.what());
    }
    return;
}

// main body of worker thread
void DataStoreProxy::Worker::run()
{
    using std::map;             // just used in this function

    log_debug("%s: enter", __func__);

    // keep track of pending requests. Since there won't be very many
    // of them, just use an vector
    typedef map<string, RequestReply *> pending_t;
    pending_t pending;

    // this code adapted from ExternalRouter::ModuleServer::run()

    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[2];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = mq_->read_fd();
    event_poll->events = POLLIN;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = conn_->fd();
    sock_poll->events = POLLIN;

    while (1) {
        // we don't stop on a dime; we keep going until all pending
        // requests are taken care of  
        if (should_stop() && pending.empty()) {
            break;
        }

        log_debug("%s: blocking on %d and %d", __func__, event_poll->fd, sock_poll->fd);

        // block waiting until a request or I/O arrives
        int ret = oasys::IO::poll_multiple(pollfds, 2, -1);

        if (should_stop()) {
            if (pending.empty()) {
                log_debug("%s: stopping", __func__);
                break;
            }
            log_debug("%s: told to stop, still %zu pending messages", __func__, pending.size());
        }

        log_debug("%s: back from poll_multiple", __func__);

        if (ret == oasys::IOINTR) {
            log_debug("%s: data store server interrupted", __func__);
            set_should_stop();
            continue;
        }
        else if (ret == oasys::IOERROR) {
            log_debug("%s: data store server error", __func__);
            set_should_stop();
            continue;
        }

        // have we been sent a request from a thread? forward it
        // to the external server.
        if (event_poll->revents & POLLIN) {
            log_debug("%s: event_poll fired", __func__);
            RequestReply *event;
            if (mq_->try_pop(&event)) {
                // sanity checking
                ASSERT(event != 0);
                log_debug("%s: Worker::run popped event (client->server request)", __func__);
                // save this, waiting for a response
                string cookie = gen_cookie();
                event->request_.cookie(cookie);
                pending[cookie] = event;

                // send it out
                log_debug("%s: sending request with cookie %s", __func__, cookie.c_str());
                send(event->request_);
            } else {
                log_debug("%s: could not pop event!", __func__);
            }
        }

        // have we been sent a response from the external server?
        // forward it to the waiting thread.
        if (sock_poll->revents & POLLIN) {
            log_debug("%s: sock_poll fired (server->client response)", __func__);

            ds_reply_type_p guy;
            recv(guy);
            if (guy != 0) {
                // find the matching request
                string cookie = guy->cookie();
                log_debug("%s: receieved response for message with cookie %s",
                          __func__, cookie.c_str());
                pending_t::iterator i = pending.find(cookie);
                if (i != pending.end()) {
                    // fill in the response and tell waiter it's ready 
                    RequestReply *p = i->second;
                    p->reply_ = guy;
                    p->notify();
                    pending.erase(i); // drop it from the map
                } else {
                    // it's not found, we don't need the guy any more
                    delete guy;
                    log_debug("%s: no matching request found for cookie %s, ignoring", 
                              __func__, cookie.c_str());
                }
            }
        }
    }

    // tell the parent that we're all done
    shutdown_notifier_.notify();
    return;
}

//----------------------------------------------------------------

string DataStoreProxy::Worker::gen_cookie()
{
    ostringstream oss;
    oss << "asynch-" << ++cur_cookie;
    return oss.str();
}

// this assumes the root object is in root, reply should go onto
// an auto_ptr reply_var, stored in reply_var ## _type
#define EVAL(root, reply, reply_field) \
        log_debug("%s: looking for a %s",  __func__, #reply_field); \
        ds_reply_type_p root_repl;\
        execute(root, root_repl);\
        log_debug("%s: returned object at %p, should be a %s",  __func__,  root_repl, #reply_field "_type"); \
        if (root_repl->reply_field().present() == false) {\
                log_err("%s: wrong return type", __func__); \
                delete root_repl;\
                return ERR_INTERNAL; \
        } \
        ret = atoi(root_repl->error().c_str()); \
        if (ret != 0) { \
                delete root_repl;\
                return ret;\
        }\
        reply_field ## _type &reply = root_repl->reply_field().get(); \
        (void)reply                     /* use this */

/*!
 * Ds_caps: capabilities of this data store server
 */
int DataStoreProxy::ds_caps(vector<string> &languages, 
                            string &storetype, 
                            bool &supports_trigger)
{
    int ret;
    ds_caps_request_type req;
    ds_request_type root("");
    root.ds_caps().set(req);

    EVAL(root, caps_repl, ds_caps_reply);

    storetype = caps_repl.storetype();
    supports_trigger = caps_repl.supports_trigger();
    for (ds_caps_reply_type::language::const_iterator i(caps_repl.language().begin());
         i != caps_repl.language().end();
         ++i) {
        languages.push_back(i->language());
    }
    return 0;
}

/*!
 * ds_create: create a data store
 */
int DataStoreProxy::ds_create(const string &dsname, 
                              const bool clear, 
                              const int quota, 
                              const credentials_t &cred)
{
    int ret;
    ds_create_request_type req(dsname);

    req.user().set(cred.user);
    xml_schema::base64_binary pw(cred.password.data(), cred.password.length());
    req.password().set(pw);
    req.clear().set(clear);
    req.quota().set(quota);

    ds_request_type root("");
    root.ds_create().set(req);

    EVAL(root, reply, ds_create_reply);
    return 0;
}

/*!
 * ds_del: delete a data store
 */
int DataStoreProxy::ds_del(const string &dsname, 
			   const credentials_t &cred)
{
    int ret;
    ds_del_request_type req(dsname);

    req.user().set(cred.user);
    xml_schema::base64_binary pw(cred.password.data(), cred.password.length());
    req.password().set(pw);
    
    ds_request_type root("");
    root.ds_del().set(req);

    EVAL(root, reply, ds_del_reply);
    return 0;
}

/*!
 * ds_open: open a data store
 */
int DataStoreProxy::ds_open(const string &dsname, 
                            int lease, 
                            const credentials_t &cred, 
                            string &handle)
{
    int ret;
    ds_open_request_type req(dsname);
    req.lease().set(lease);

    req.user().set(cred.user);
    xml_schema::base64_binary pw(cred.password.data(), cred.password.length());
    req.password().set(pw);

    ds_request_type root("");
    root.ds_open().set(req);

    EVAL(root, reply, ds_open_reply);
    handle = reply.handle().get();
    return 0;
}

/*!
 * ds_stat: information about a data store
 */
int DataStoreProxy::ds_stat(const string &handle,
                            vector<string> &tables)
{
    int ret;
    if (!init_) 
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    ds_stat_request_type req(handle);
    ds_request_type root("");
    root.ds_stat().set(req);

    // evaluate it 
    EVAL(root, reply, ds_stat_reply);
    for (ds_stat_reply_type::table::const_iterator i(reply.table().begin());
         i != reply.table().end(); ++i) {
        tables.push_back(i->table());
    }
    return 0;
}

/*!
 * ds_close: drop connection to data store
 */
int DataStoreProxy::ds_close(const string &handle)
{
    int ret;
    if (!init_) 
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    ds_close_request_type req(handle);
    ds_request_type root("");
    root.ds_close().set(req);

    // evaluate it 
    EVAL(root, reply, ds_close_reply);
    return 0;
}

/*!
 * table_create: create a named table with the specified fields
 */
int DataStoreProxy::table_create(const string &handle,
                                 const string &tablename,
                                 const string &key,
                                 const string &keytype,
                                 const vector<StringPair> &fieldinfo)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    // create the base request
    table_create_request_type req(handle, tablename, key, keytype);

    // add the fields and their types
    for (vector<StringPair>::const_iterator info = fieldinfo.begin();
         info != fieldinfo.end();
         ++info) {
        req.field().push_back(fieldInfoType(info->first, 
                                            fieldType(info->second)));
    }

    // create the root
    ds_request_type root("");
    root.table_create().set(req);

    // evaluate it
    EVAL(root, reply, table_create_reply);
    return 0;
}

/*!
 * table_del: delete a named table and all of its elements
 */
int DataStoreProxy::table_del(const string &handle,
			      const string &tablename)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    table_del_request_type req(handle, tablename);
    ds_request_type root("");
    root.table_del().set(req);
    EVAL(root, reply, table_del_reply);
    return 0;
}

/*!
 * table_keys
 */
int DataStoreProxy::table_keys(const string &handle,
                               const string &tablename,
			       const string &keyname,
                               vector<string> &keys)
{
    int ret;
    if (!init_) 
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    keys.empty();

    table_keys_request_type req(handle, tablename, keyname);
    ds_request_type root("");
    root.table_keys().set(req);

    // evaluate it 
    EVAL(root, reply, table_keys_reply);

    for (table_keys_reply_type::key::const_iterator i(reply.key().begin());
         i != reply.key().end(); 
         ++i) {
        keys.push_back(string(i->data(), i->size()));
    }
    return 0;
}

/*!
 * table_stat
 */
int DataStoreProxy::table_stat(const string &handle,
                               const string &tablename,
                               string &keyname,
                               string &keytype,
                               vector<string> &fieldnames,
                               vector<string> &fieldtypes,
                               u_int32_t &count)
{
    int ret;
    if (!init_) 
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    table_stat_request_type req(handle, tablename);
    ds_request_type root("");
    root.table_stat().set(req);

    // evaluate it 
    EVAL(root, reply, table_stat_reply);
    for (table_stat_reply_type::field::const_iterator i(reply.field().begin());
         i != reply.field().end(); ++i) {
        fieldInfoType info = *i;
        fieldnames.push_back(info.field());
        fieldtypes.push_back(info.type());
    }
    keyname = reply.keyname();
    keytype = reply.keytype();
    count = reply.count();
    return 0;
}

/*!
 * put: insert a row (element) into a table
 */
int DataStoreProxy::put(const string &handle,
                        const string &tablename, 
			const string &keyname,
                        const string &key,
                        const vector<StringPair> &fields)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    // need to set some fields
    if (fields.size() == 0)
        return ERR_INVALID;

    xml_schema::base64_binary k(key.data(), key.length());
    put_request_type req(k, handle, tablename, keyname);

    for (vector<StringPair>::const_iterator i = fields.begin();
         i != fields.end();
         ++i) {
        xml_schema::base64_binary value(i->second.data(), i->second.length());
        req.set().push_back(fieldNameValue(value, i->first));
    }

    ds_request_type root("");
    root.put().set(req);
    EVAL(root, reply, put_reply);
    return 0;
}


/*!
 * put: insert a row (element) into a table
 */
int DataStoreProxy::get(const string &handle,
                        const string &tablename, 
			const string &keyname,
                        const string &keyval,
                        vector<StringPair> &fields)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    xml_schema::base64_binary k(keyval.data(), keyval.length());
    get_request_type req(k, handle, tablename, keyname);
    ds_request_type root("");
    root.get().set(req);
    EVAL(root, reply, get_reply);

    for (get_reply_type::field::iterator f = reply.field().begin();
	 f != reply.field().end();
	 ++f) {
        string value(f->value().data(), f->value().size());
        fields.push_back(StringPair(f->field(), value));
    }

    return 0;
}


/*!
 * del: remove an item with the given key
 */
int DataStoreProxy::del(const string &handle,
                        const string &tablename, 
			const string &keyname,
                        const string &keyval)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    xml_schema::base64_binary k(keyval.data(), keyval.length());
    del_request_type req(k, handle, tablename, keyname);
    ds_request_type root("");
    root.del().set(req);
    EVAL(root, reply, del_reply);
    return 0;
}

/*!
 * select: 
 */
int DataStoreProxy::select(const string &handle,
                           const string &tablename,
                           const vector<StringPair> &constraints,
                           const vector<string> &get_fields,
                           u_int32_t howmany,
                           vector<vector<string> > &values)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    select_request_type req(handle, tablename);

    // how many to get
    req.count().set(howmany);

    // which fields to get
    for (vector<string>::const_iterator i = get_fields.begin();
         i != get_fields.end();
         ++i) {
        req.get().push_back(fieldName(*i));
    }

    // constraints
    for (vector<StringPair>::const_iterator i = constraints.begin();
         i != constraints.end();
         ++i) {
        xml_schema::base64_binary value(i->second.data(), i->second.length());
        req.where().push_back(fieldNameValue(value, i->first));
    }

    ds_request_type root("");
    root.select().set(req);
    EVAL(root, reply, select_reply);

    // copy values back into values
    howmany = reply.row().size();
    for (select_reply_type::row::iterator guy = reply.row().begin();
         guy != reply.row().end();
         ++guy) {
        vector<string> this_row;
        for (rowType::field::iterator i = guy->field().begin();
             i != guy->field().end();
             ++i) {
            string value(i->value().data(), i->value().size());
            this_row.push_back(value);
        }
        values.push_back(this_row);
    }
    return 0;
}

/*!
 * eval an arbitrary expression
 */
int DataStoreProxy::eval(const string &handle,
                         const string &language, 
                         const string &expr,
                         string &res)
{
    int ret;
    if (!init_)
        return ERR_NOTOPEN;
    ASSERT(handle == handle_);

    xml_schema::base64_binary ex(expr.data(), expr.length());
    eval_request_type req(ex, handle, language);
    ds_request_type root("");
    root.eval().set(req);
    EVAL(root, reply, eval_reply);
    res = string(reply.value().data(), reply.value().size());
    return 0;
}

/*!
 * trigger is not supported by the C++ binding
 */
int DataStoreProxy::trigger(const string & /*handle*/,
                            const string &/*language*/, 
                            const string &/*expr*/,
                            string &/*res*/)
{
    return ERR_NOTSUPPORTED;
}

//----------------------------------------------------------------*

/*!
 * execute this command -- send it to the remote server, wait for a
 * response, parse the response into an XML object. Note that we have
 * to pass the xml to the remote server, as it annotates the object
 * (with an id number) before serializing and sending it
 */
void DataStoreProxy::execute(ds_request_type &request, ds_reply_type_p &reply)
{
    log_debug("%s: executing a request", __func__);

    // this is a hack!
    if (Thread::start_barrier_enabled()) {
        // do it synchronously
        log_debug("%s: evaluating synchronously", __func__);

        // give it a unique cookie
        static int num_synch = 0;
        ostringstream oss;
        oss << "synchronous-" << ++num_synch;
        request.cookie(oss.str());

        worker_->send(request);
        worker_->recv(reply);
    } else {
        // we've gone multithreaded -- start the worker thread
        if (!worker_->started())
            worker_->start();
        RequestReply guy(logpath_, request);
        log_debug("%s: created RequestReply", __func__);
        worker_->mq_->push_back(&guy); // queue it for the worker
        log_debug("%s: evaluating asynchronously", __func__);
        guy.wait();             // wait for the response to show up
        reply = guy.reply_;
        log_debug("%s: returned", __func__);
    }
    return;
}

//----------------------------------------------------------------

int DataStoreProxy::do_serialize(const SerializableObject &obj, string &str)
{
    MarshalSize sizer(Serialize::CONTEXT_LOCAL);

    sizer.action(&obj);
    int need_size = sizer.size();
    u_char buf[need_size];
    Marshal marshaller(Serialize::CONTEXT_LOCAL, buf, need_size);
    marshaller.action(&obj);
    str = hex2str(buf, need_size); // XXX asymmetry -- this will bite me I'm sure
    return 0;
}

int DataStoreProxy::do_serialize(const SerializableObject &obj, 
                                 vector<StringPair> &fields,
                                 bool get_schema)
{
    if (get_schema) {
        StringPairSerialize::Info marshaller(&fields);
        return marshaller.action(const_cast<SerializableObject *>(&obj));
    } else {
        StringPairSerialize::Marshal marshaller(&fields);
        return marshaller.action(const_cast<SerializableObject *>(&obj));
    }
}

int DataStoreProxy::do_unserialize(const string &str, SerializableObject *obj)
{
    Unmarshal unmarshaller(Serialize::CONTEXT_LOCAL, (const u_char *)str.data(), str.length());
    if (unmarshaller.action(obj) != 0) {
        return DS_ERR;
    }
    return 0;
}

int DataStoreProxy::do_unserialize(const vector<StringPair> &fields, SerializableObject *obj)
{
    StringPairSerialize::Unmarshal unmarshaller(const_cast<vector<StringPair> *>(&fields));
    return unmarshaller.action(obj);
}

// grab number of elements in this table
int DataStoreProxy::do_count(const string tablename, u_int32_t &count)
{
    // ignore key, fieldnames, fieldtypes
    string keyname, keytype;
    vector<string> fieldnames, fieldtypes;
    return table_stat(handle_, tablename, keyname, keytype, fieldnames, 
                      fieldtypes, count);
}

int DataStoreProxy::do_del(const string &tablename,
			   const std::string &keyname,
                           const SerializableObject &key)
{
    string keyval;
    do_serialize(key, keyval);

    return del(handle_, tablename, keyname, keyval);
}

int DataStoreProxy::do_get(const string &tablename,
			   const string &keyname,
                           const SerializableObject &key,
                           SerializableObject *data)
{
    int ret;
    string keyval;

    // always serialize the key to a single string
    if ((ret = do_serialize(key, keyval)) != 0) {
        return ret;
    }

    ASSERT(st_ != UNDEFINED);
    vector<StringPair> data_fields;
    if ((ret = get(handle_, tablename, keyname, keyval, data_fields)) == 0 &&
        data != 0) {
        if (st_ == PAIR) {
            ret = do_unserialize(data_fields[0].second, data);
        } else {
            ret = do_unserialize(data_fields, data);
        }
    }

    return ret;
}

int DataStoreProxy::do_put(const string &tablename,
			   const string &keyname,
                           const SerializableObject &key,
                           const SerializableObject *data)
{
    int ret;
    vector<StringPair> data_fields;
    string keyval;

    if ((ret = do_serialize(key, keyval)) != 0) {
        return ret;
    }
    if (st_ == PAIR) {
        string dataval;
        if ((ret = do_serialize(*data, dataval)) != 0) {
            return ret;
        }
        data_fields.push_back(StringPair(pair_data_field_name, dataval));
    } else {
        if ((ret = do_serialize(*data, data_fields)) != 0) {
            return ret;
        }
    }
    return put(handle_, tablename, keyname, keyval, data_fields);
}

int DataStoreProxy::do_table_create(const string &tablename,
				    const string &keyname,
                                    const SerializableObject &obj)
{
    int ret;
    string keytype = "string";
    vector<StringPair> fieldinfo;

    if (st_ == PAIR) {
        fieldinfo.push_back(StringPair(keyname, "string"));
    } else {
        if ((ret = do_serialize(obj, fieldinfo, true)) != 0) {
            return ret;
        }
    }

    return table_create(handle_, tablename, keyname, keytype, fieldinfo);
}

#endif // EXTERNAL_DS_ENABLED && XERCES_C_ENABLED
