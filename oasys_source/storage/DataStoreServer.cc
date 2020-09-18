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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)

#include <iostream>
#include <iomanip>              // for formatted i/o
#include <string>
#include "DataStoreServer.h"
#include "../debug/DebugUtils.h"
#include "../serialize/XercesXMLSerialize.h"

using namespace std;
using namespace dsmessage;

namespace oasys {

DataStoreServer::DataStoreServer(DataStore &ds,
                                 const char *schema,
                                 const char *logpath, 
                                 const char *specific_class) :
    Logger(specific_class, logpath),
    schema_(schema),
    parser_(0),
    ds_(ds)
{
    // nothing
}


DataStoreServer::~DataStoreServer()
{
    if (parser_ != 0) delete parser_;
}

//----------------------------------------------------------------
// iostream versions
//----------------------------------------------------------------
// server loop: just calls recv and send repeatedly, until one or the
// other closes
int DataStoreServer::serve(istream &is, ostream &os)
{
    ds_request_type_p req;
    ds_reply_type_p reply;

    while (recv(is, req) == 0) {
        if (process(*req, reply) == 0 &&
            send(os, *reply) != 0)
            break;
    }
    return -1;                      // somebody failed
}

// recv: read a request from the stream, unmarshal, and return
int DataStoreServer::recv(istream &is, ds_request_type_p &req)
{
    // read the length first
    char lenchars[9];
    is.read(lenchars, 8);
    if (is.gcount() != 8) {
        log_debug("%s: short read on header, expected 8, got %d", __func__, is.gcount());
        return -1;
    }
    lenchars[8] = '\0';
    int xml_len = atol(lenchars);

    // now we know how much we're going to buffer
    char buf[xml_len];

    // read the body
    is.read(buf, xml_len);
    if (is.gcount() != xml_len) {
        log_err("%s: short read on body, expected %d, got %d", __func__, xml_len, is.gcount());
        return -1;
    }

    string xml(buf, xml_len);
    return unmarshal(xml, req);
}


// send: inverse of recv, take a reply, marshal it, and send it to the
// out stream
int DataStoreServer::send(ostream &os, ds_reply_type &repl)
{
    string buf;
    marshal(repl, buf);

    // capture the length of the string
    ostringstream os_len;
    os_len << setfill('0') << setw(8) << buf.length();
    string len(os_len.str());

    // write out length, followed by the XML itself
    os << len.c_str() << buf.c_str();

    return 0;
}

//----------------------------------------------------------------
// file descriptor versions
//----------------------------------------------------------------
// server loop: just calls recv and send repeatedly, until one or the
// other closes
int DataStoreServer::serve(int infd, int outfd)
{
    ds_request_type_p req;
    ds_reply_type_p reply;

    while (this->recv(infd, req) == 0) {
        if (process(*req, reply) != 0) {
            log_debug("%s: could not process request", __func__);
            break;
        }
        if (this->send(outfd, *reply) != 0) {
            log_debug("%s: could not process request", __func__);
            break;
        }
    }
    return -1;                      // somebody failed
}

// recv: read a request from the stream, unmarshal, and return
int DataStoreServer::recv(int fd, ds_request_type_p &req)
{
    int count;

    // read the length first
    char lenchars[9];
    count = read(fd, lenchars, 8);
    if (count != 8) {
        log_debug("%s: short read on header, expected 8, got %d", __func__, count);
        return -1;
    }
    lenchars[8] = '\0';
    int xml_len = atol(lenchars);

    // now we know how much we're going to buffer
    char buf[xml_len];

    // read the body
    count = read(fd, buf, xml_len);
    if (count != xml_len) {
        log_err("%s: short read on body, expected %d, got %d", __func__, xml_len, count);
        return -1;
    }

    string xml(buf, xml_len);
    return unmarshal(xml, req);
}


// send: inverse of recv, take a reply, marshal it, and send it to the
// out stream
int DataStoreServer::send(int fd, ds_reply_type &repl)
{
    size_t count;
    string buf;
    marshal(repl, buf);

    // capture the length of the string
    ostringstream os_len;
    os_len << setfill('0') << setw(8) << buf.length();

    string res = os_len.str() + buf;
    log_debug("%s: sending %zu bytes:\n%s", __func__, res.length(), res.c_str());
    count = write(fd, res.data(), res.length());
    if (count != res.length())      // short write?
        return -1;

    return 0;
}
//----------------------------------------------------------------
// unmarshal: take a string containing a request and unmarshal it into
// an object
int DataStoreServer::unmarshal(string &buf_str, ds_request_type_p &req)
{
    log_debug("%s: %s", __func__, buf_str.c_str());

    // late, late binding: create the parser
    if (parser_ == 0) {
        parser_ = new XercesXMLUnmarshal(true, schema_.c_str());
    }

    // parse the body
    parser_->reset_error();
    const xercesc::DOMDocument *document = parser_->doc(buf_str.c_str());
    if (document == 0) {
        log_debug("unable to parse document");
        return -1;
    }

    // XXX do we need to delete document?
    try {
        req = ds_request(*document).release();
    } catch (const xml_schema::exception &e) {
        log_debug("%s: parse failed: %s\n", __func__, e.what());
        return -1;
    }
    
    // parse was successful
    return 0;
}

// marshal: take a reply and generate an xml string
int DataStoreServer::marshal(ds_reply_type &reply, string &xml)
{
    xml_schema::namespace_infomap map;
    ostringstream os;

    // linearize
    ds_reply(os, reply, map);

    // grab string
    xml = string(os.str());

    log_debug("%s: %s", __func__, xml.c_str());

    return 0;
}

// process: take a request object, process the request embedded
// within, return a reply object. Note that we assume at most one
// request in each request object. 
int DataStoreServer::process(ds_request_type &req, ds_reply_type_p &reply)
{
    int ret;
    reply = 0;

    // this is basically a big case statement, where each arm takes
    // the in the object, dispatches to the worker function, then
    // copies the results into a result object which is returned.

    // by default the error return is "0"
    reply = new ds_reply_type(req.cookie(), "0");

    // ds-caps
    if (req.ds_caps().present()) {
        // copy in
        // ...

        // dispatch
        vector<string> languages;
        string storetype;
        bool does_triggers;
        if ((ret = ds_.ds_caps(languages, storetype, does_triggers)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        ds_caps_reply_type r(storeType(storetype), does_triggers);

        for (vector<string>::iterator i = languages.begin();
             i != languages.end(); 
             ++i)
        {
            r.language().push_back(languageName(*i));
        }

        // set
        reply->ds_caps_reply().set(r);

        // return
        return 0;
    }

    // ds-create
    if (req.ds_create().present()) {
        // copy in
        DataStore::credentials_t cred;

        ds_create_request_type &m = req.ds_create().get();

        if (m.user().present())
            cred.user = m.user().get();
        if (m.password().present())
            cred.password = string(m.password().get().data(), 
                                   m.password().get().size());
        // XXX we don't pass keys yet

        // dispatch
        ret = ds_.ds_create(m.ds(), 
                            m.clear().present() ? m.clear().get() : false,
                            m.quota().present() ? m.quota().get() : 0,
                        cred);
        if (ret != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        ds_create_reply_type r;

        // set
        reply->ds_create_reply().set(r);

        // return
        return 0;
    }

    // ds-delete
    if (req.ds_del().present()) {
        // copy in
        DataStore::credentials_t cred;

        ds_del_request_type &m = req.ds_del().get();

        if (m.user().present())
            cred.user = m.user().get();
        if (m.password().present())
            cred.password = string(m.password().get().data(), 
                                   m.password().get().size());
        // XXX we don't pass keys yet

        // dispatch
        if ((ret = ds_.ds_del(m.ds(), cred)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        ds_del_reply_type r;

        // set
        reply->ds_del_reply().set(r);

        // return
        return 0;
    }

    // ds-open
    if (req.ds_open().present()) {
        // copy in
        DataStore::credentials_t cred;
        string handle;

        ds_open_request_type &m = req.ds_open().get();

        if (m.user().present())
            cred.user = m.user().get();
        if (m.password().present())
            cred.password = string(m.password().get().data(), 
                                   m.password().get().size());
        // XXX we don't pass keys yet

        ds_open_reply_type r;

        // dispatch
        if ((ret = ds_.ds_open(m.ds(), 
                               m.lease().present() ? m.lease().get() : 0,
                               cred, 
                               handle)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        } else {
            r.handle().set(handle);
        }

        // set
        reply->ds_open_reply().set(r);

        // return
        return 0;
    }

    // ds-stat
    if (req.ds_stat().present()) {
        // copy in
        ds_stat_request_type &m = req.ds_stat().get();

        // dispatch
        vector<string> tables;
        if ((ret = ds_.ds_stat(m.handle(), tables)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        ds_stat_reply_type r;
        for (vector<string>::iterator i = tables.begin();
             i != tables.end();
             ++i) {
            r.table().push_back(tableInfoType(*i));
        }

        // set
        reply->ds_stat_reply().set(r);

        // return
        return 0;
    }

    // ds-close
    if (req.ds_close().present()) {
        // copy in
        ds_close_request_type &m = req.ds_close().get();

        // dispatch
        if ((ret = ds_.ds_close(m.handle())) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        ds_close_reply_type r;

        // set
        reply->ds_close_reply().set(r);

        // return
        return 0;
    }
    if (req.table_create().present()) {
        // copy in
        table_create_request_type &m = req.table_create().get();

        vector<StringPair> fieldinfo;
        vector<string> fieldtypes;
        for (table_create_request_type::field::iterator i = m.field().begin();
             i != m.field().end();
             ++i) {
            fieldinfo.push_back(StringPair(i->field(), i->type()));
        }

        // dispatch
        if ((ret = ds_.table_create(m.handle(),
                                    m.table(),
                                    m.key(),
                                    m.keytype(),
                                    fieldinfo)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        table_create_reply_type r;

        // set
        reply->table_create_reply().set(r);

        // return
        return 0;
    }
    if (req.table_del().present()) {
        // copy in
        table_del_request_type &m = req.table_del().get();

        // dispatch
        if ((ret = ds_.table_del(m.handle(), m.table())) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        table_del_reply_type r;

        // set
        reply->table_del_reply().set(r);

        // return
        return 0;
    }
    if (req.table_stat().present()) {
        // copy in
        table_stat_request_type &m = req.table_stat().get();

        // dispatch
        vector<string> fieldnames;
        vector<string> fieldtypes;
        string key, key_type;
        u_int32_t count;

        if ((ret = ds_.table_stat(m.handle(),
                                  m.table(),
                                  key,
                                  key_type,
                                  fieldnames,
                                  fieldtypes,
                                  count)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        table_stat_reply_type r(key, key_type, count);

        vector<string>::iterator name = fieldnames.begin();
        vector<string>::iterator ty = fieldtypes.begin();
        for ( ;
              name != fieldnames.end() && ty != fieldtypes.end();
              ++name, ++ty) {

            fieldInfoType fit(*name, fieldType(*ty));
            r.field().push_back(fit);
        }
        ASSERT(name == fieldnames.end() && ty == fieldtypes.end());

        // set
        reply->table_stat_reply().set(r);

        // return
        return 0;
    }
    if (req.table_keys().present()) {
        // copy in
        table_keys_request_type &m = req.table_keys().get();

        // dispatch
        vector<string> keys;
        if ((ret = ds_.table_keys(m.handle(), m.table(), m.keyname(), keys)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        table_keys_reply_type r;
        for (vector<string>::iterator key = keys.begin();
             key != keys.end();
             ++key) {
            r.key().push_back(xml_schema::base64_binary(key->data(), 
                                                        key->length()));
        }

        // set
        reply->table_keys_reply().set(r);

        // return
        return 0;

    }
    if (req.put().present()) {
        // copy in
        put_request_type &m = req.put().get();

        string key(m.key().data(), m.key().size());
        vector<StringPair> pairs;
        for (put_request_type::set::iterator i = m.set().begin();
             i != m.set().end();
             ++i) {
            string &field = i->field();
            string value = string(i->value().data(), i->value().size());
            pairs.push_back(StringPair(field, value));
        }

        // dispatch
        if ((ret = ds_.put(m.handle(), m.table(), m.keyname(), key, pairs)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        put_reply_type r;

        // set
        reply->put_reply().set(r);

        // return
        return 0;
    }
    if (req.get().present()) {
        // copy in
        get_request_type &m = req.get().get();
        string key(m.key().data(), m.key().size());

        // dispatch
        vector<StringPair> values;
        if ((ret = ds_.get(m.handle(), m.table(), m.keyname(), key, values)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        get_reply_type r;

        for (vector<StringPair>::iterator i = values.begin();
             i != values.end();
             ++i) {
            xml_schema::base64_binary value(i->second.data(), i->second.length());
	    // yes, args backwards are they. sigh. 
            r.field().push_back(fieldNameValue(value, i->first));
        }

        // set
        reply->get_reply().set(r);

        // return
        return 0;
    }
    if (req.del().present()) {
        // copy in
        del_request_type &m = req.del().get();
        string key(m.key().data(), m.key().size());

        // dispatch
        if ((ret = ds_.del(m.handle(), m.table(), m.keyname(), key)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        del_reply_type r;

        // set
        reply->del_reply().set(r);

        // return
        return 0;
    }
    if (req.select().present()) {
        log_debug("%s: select message received, not yet coded", __func__);

        ostringstream oss;
        oss << DataStore::ERR_NOTSUPPORTED;
        reply->error(oss.str());
        xml_schema::base64_binary value("", 0);
        eval_reply_type r(value, "");
        reply->eval_reply().set(r);
    }
    if (req.eval().present()) {
        // copy in
        eval_request_type &m = req.eval().get();
        string expr(m.expr().data(), m.expr().size());
        string result;

        // dispatch
        if ((ret = ds_.eval(m.handle(), m.language(), expr, result)) != 0) {
            ostringstream oss;
            oss << ret;
            reply->error(oss.str());
        }

        // copy out
        xml_schema::base64_binary value(result.data(), result.length());
        eval_reply_type r(value, m.language());

        // set
        reply->eval_reply().set(r);

        // return
        return 0;
    }
    if (req.trigger().present()) {
        log_debug("%s: trigger message not implemented in C++ interface", __func__);

        ostringstream oss;
        oss << DataStore::ERR_NOTSUPPORTED;
        reply->error(oss.str());
        xml_schema::base64_binary value("", 0);
        eval_reply_type r(value, "");
        reply->eval_reply().set(r);
        return 0;
    }
    return 0;
}

} // namespace oasys
#endif
