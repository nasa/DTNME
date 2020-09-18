/*
 *    Copyright 2007 Intel Corporation
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

#include <map>
#include <string>

using namespace std;

typedef map<unsigned int, dtn_handle_t> HandleMap;

HandleMap    Handles;
static unsigned int HandleID = 1;

//----------------------------------------------------------------------
int
dtn_open()
{
    dtn_handle_t ret = 0;
    int err = dtn_open(&ret);
    if (err != DTN_SUCCESS) {
        return -1;
    }

    unsigned int i = HandleID++;
    Handles[i] = ret;
    return i;
}

//----------------------------------------------------------------------
static dtn_handle_t
find_handle(int i)
{
    HandleMap::iterator iter = Handles.find(i);
    if (iter == Handles.end())
        return NULL;
    return iter->second;
}

//----------------------------------------------------------------------
void
dtn_close(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return;
    dtn_close(h);
}

//----------------------------------------------------------------------
int
dtn_errno(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return DTN_EINVAL;
    return dtn_errno(h);
}

//----------------------------------------------------------------------
string
dtn_build_local_eid(int handle, const char* service_tag)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return "";
    
    dtn_endpoint_id_t eid;
    memset(&eid, 0, sizeof(eid));
    dtn_build_local_eid(h, &eid, service_tag);
    return string(eid.uri);
}

//----------------------------------------------------------------------
static int
build_reginfo(dtn_reg_info_t* reginfo,
              const string&   endpoint,
              unsigned int    flags,
              unsigned int    expiration,
              bool            init_passive,
              const string&   script)
{
    memset(reginfo, 0, sizeof(dtn_reg_info_t));
    
    strcpy(reginfo->endpoint.uri, endpoint.c_str());
    reginfo->flags             = flags;
    reginfo->expiration        = expiration;
    reginfo->init_passive      = init_passive;
    reginfo->script.script_len = script.length();
    reginfo->script.script_val = (char*)script.c_str();

    return 0;
}
    
//----------------------------------------------------------------------
int
dtn_register(int           handle,
             const string& endpoint,
             unsigned int  flags,
             int           expiration,
             bool          init_passive,
             const string& script)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_reg_info_t reginfo;
    build_reginfo(&reginfo, endpoint, flags, expiration,
                  init_passive, script);
        
    dtn_reg_id_t regid = 0;
    int ret = dtn_register(h, &reginfo, &regid);
    if (ret != DTN_SUCCESS) {
        return -1;
    }
    return regid;
}

//----------------------------------------------------------------------
int
dtn_unregister(int handle, dtn_reg_id_t regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    
    return dtn_unregister(h, regid);
}

//----------------------------------------------------------------------
int
dtn_find_registration(int handle, const string& endpoint)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_endpoint_id_t eid;
    strcpy(eid.uri, endpoint.c_str());

    dtn_reg_id_t regid = 0;
    
    int err = dtn_find_registration(h, &eid, &regid);
    if (err != DTN_SUCCESS) {
        return -1;
    }

    return regid;
}

//----------------------------------------------------------------------
int
dtn_change_registration(int           handle,
                        dtn_reg_id_t  regid,
                        const string& endpoint,
                        unsigned int  action,
                        int           expiration,
                        bool          init_passive,
                        const string& script)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_reg_info_t reginfo;
    build_reginfo(&reginfo, endpoint, action, expiration,
                  init_passive, script);
        
    return dtn_change_registration(h, regid, &reginfo);
}

//----------------------------------------------------------------------
int
dtn_bind(int handle, int regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    return dtn_bind(h, regid);
}

//----------------------------------------------------------------------
int
dtn_unbind(int handle, int regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    return dtn_unbind(h, regid);
}

//----------------------------------------------------------------------
struct dtn_bundle_id {
    string       source;
    unsigned int creation_secs;
    unsigned int creation_seqno;
};

//----------------------------------------------------------------------
dtn_bundle_id*
dtn_send(int           handle,
         int           regid,
         const string& source,
         const string& dest,
         const string& replyto,
         unsigned int  priority,
         unsigned int  dopts,
         unsigned int  expiration,
         unsigned int  payload_location,
         const string& payload_data,
         const string& sequence_id = "",
         const string& obsoletes_id = "")
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    
    strcpy(spec.source.uri, source.c_str());
    strcpy(spec.dest.uri, dest.c_str());
    strcpy(spec.replyto.uri, replyto.c_str());
    spec.priority   = (dtn_bundle_priority_t)priority;
    spec.dopts      = dopts;
    spec.expiration = expiration;

    if (sequence_id.length() != 0) {
        spec.sequence_id.data.data_val = const_cast<char*>(sequence_id.c_str());
        spec.sequence_id.data.data_len = sequence_id.length();
    }

    if (obsoletes_id.length() != 0) {
        spec.obsoletes_id.data.data_val = const_cast<char*>(obsoletes_id.c_str());
        spec.obsoletes_id.data.data_len = obsoletes_id.length();
    }

    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    switch (payload_location) {
    case DTN_PAYLOAD_MEM:
        payload.location    = DTN_PAYLOAD_MEM;
        payload.buf.buf_val = (char*)payload_data.data();
        payload.buf.buf_len = payload_data.length();
        break;
    case DTN_PAYLOAD_FILE:
        payload.location = DTN_PAYLOAD_FILE;
        payload.filename.filename_val = (char*)payload_data.data();
        payload.filename.filename_len = payload_data.length();
        break;
    case DTN_PAYLOAD_TEMP_FILE:
        payload.location = DTN_PAYLOAD_TEMP_FILE;
        payload.filename.filename_val = (char*)payload_data.data();
        payload.filename.filename_len = payload_data.length();
        break;
    default:
        dtn_set_errno(h, DTN_EINVAL);
        return NULL;
    }

    dtn_bundle_id_t id;
    memset(&id, 0, sizeof(id));
    int err = dtn_send(h, regid, &spec, &payload, &id);
    if (err != DTN_SUCCESS) {
        return NULL;
    }

    dtn_bundle_id* ret = new dtn_bundle_id();
    ret->source         = id.source.uri;
    ret->creation_secs  = id.creation_ts.secs;
    ret->creation_seqno = id.creation_ts.seqno;
    
    return ret;
}

//----------------------------------------------------------------------
int
dtn_cancel(int handle, const dtn_bundle_id& id)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    
    dtn_bundle_id_t id2;
    strcpy(id2.source.uri, id.source.c_str());
    id2.creation_ts.secs  = id.creation_secs;
    id2.creation_ts.seqno = id.creation_seqno;
    return dtn_cancel(h, &id2);
}

//----------------------------------------------------------------------
struct dtn_status_report {
    dtn_bundle_id bundle_id;
    unsigned int  reason;
    unsigned int  flags;
    unsigned int  receipt_ts_secs;
    unsigned int  receipt_ts_seqno;
    unsigned int  custody_ts_secs;
    unsigned int  custody_ts_seqno;
    unsigned int  forwarding_ts_secs;
    unsigned int  forwarding_ts_seqno;
    unsigned int  delivery_ts_secs;
    unsigned int  delivery_ts_seqno;
    unsigned int  deletion_ts_secs;
    unsigned int  deletion_ts_seqno;
    unsigned int  ack_by_app_ts_secs;
    unsigned int  ack_by_app_ts_seqno;
};

//----------------------------------------------------------------------
string
dtn_status_report_reason_to_str(unsigned int reason)
{
    return dtn_status_report_reason_to_str((dtn_status_report_reason_t)reason);
}

//----------------------------------------------------------------------
struct dtn_bundle {
    string       source;
    string       dest;
    string       replyto;
    unsigned int priority;
    unsigned int dopts;
    unsigned int expiration;
    unsigned int creation_secs;
    unsigned int creation_seqno;
    unsigned int delivery_regid;
    string       sequence_id;
    string       obsoletes_id;
    string       payload;
    dtn_status_report* status_report;
};

//----------------------------------------------------------------------
dtn_bundle*
dtn_recv(int handle, unsigned int payload_location, int timeout)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    
    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    dtn_bundle_payload_location_t location =
        (dtn_bundle_payload_location_t)payload_location;

    int err = dtn_recv(h, &spec, location, &payload, timeout);
    if (err != DTN_SUCCESS) {
        return NULL;
    }
    
    dtn_bundle* bundle     = new dtn_bundle();
    bundle->source         = spec.source.uri;
    bundle->dest           = spec.dest.uri;
    bundle->replyto        = spec.replyto.uri;
    bundle->priority       = spec.priority;
    bundle->dopts          = spec.dopts;
    bundle->expiration     = spec.expiration;
    bundle->creation_secs  = spec.creation_ts.secs;
    bundle->creation_seqno = spec.creation_ts.seqno;
    bundle->delivery_regid = spec.delivery_regid;

    switch(location) {
    case DTN_PAYLOAD_MEM:
        bundle->payload.assign(payload.buf.buf_val,
                               payload.buf.buf_len);
        break;
    case DTN_PAYLOAD_FILE:
    case DTN_PAYLOAD_TEMP_FILE:
        bundle->payload.assign(payload.filename.filename_val,
                               payload.filename.filename_len);
        break;
    default:
        dtn_set_errno(h, DTN_EINVAL);
        return NULL;
    }

    if (payload.status_report) {
        dtn_status_report* sr_dst = new dtn_status_report();
        dtn_bundle_status_report_t* sr_src = payload.status_report;

        sr_dst->bundle_id.source         = sr_src->bundle_id.source.uri;
        sr_dst->bundle_id.creation_secs  = sr_src->bundle_id.creation_ts.secs;
        sr_dst->bundle_id.creation_seqno = sr_src->bundle_id.creation_ts.seqno;
        sr_dst->reason                   = sr_src->reason;
        sr_dst->flags                    = sr_src->flags;
        sr_dst->receipt_ts_secs          = sr_src->receipt_ts.secs;
        sr_dst->receipt_ts_seqno         = sr_src->receipt_ts.seqno;
        sr_dst->custody_ts_secs          = sr_src->custody_ts.secs;
        sr_dst->custody_ts_seqno         = sr_src->custody_ts.seqno;
        sr_dst->forwarding_ts_secs       = sr_src->forwarding_ts.secs;
        sr_dst->forwarding_ts_seqno      = sr_src->forwarding_ts.seqno;
        sr_dst->delivery_ts_secs         = sr_src->delivery_ts.secs;
        sr_dst->delivery_ts_seqno        = sr_src->delivery_ts.seqno;
        sr_dst->deletion_ts_secs         = sr_src->deletion_ts.secs;
        sr_dst->deletion_ts_seqno        = sr_src->deletion_ts.seqno;
        sr_dst->ack_by_app_ts_secs       = sr_src->ack_by_app_ts.secs;
        sr_dst->ack_by_app_ts_seqno      = sr_src->ack_by_app_ts.seqno;

        bundle->status_report = sr_dst;
    } else {
        bundle->status_report = NULL;
    }

    return bundle;
}

//----------------------------------------------------------------------
dtn_bundle*
dtn_peek(int handle, unsigned int payload_location, int timeout)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    
    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    dtn_bundle_payload_location_t location =
        (dtn_bundle_payload_location_t)payload_location;

    int err = dtn_recv(h, &spec, location, &payload, timeout);
    if (err != DTN_SUCCESS) {
        return NULL;
    }
    
    dtn_bundle* bundle     = new dtn_bundle();
    bundle->source         = spec.source.uri;
    bundle->dest           = spec.dest.uri;
    bundle->replyto        = spec.replyto.uri;
    bundle->priority       = spec.priority;
    bundle->dopts          = spec.dopts;
    bundle->expiration     = spec.expiration;
    bundle->creation_secs  = spec.creation_ts.secs;
    bundle->creation_seqno = spec.creation_ts.seqno;
    bundle->delivery_regid = spec.delivery_regid;

    switch(location) {
    case DTN_PAYLOAD_MEM:
        bundle->payload.assign(payload.buf.buf_val,
                               payload.buf.buf_len);
        break;
    case DTN_PAYLOAD_FILE:
    case DTN_PAYLOAD_TEMP_FILE:
        bundle->payload.assign(payload.filename.filename_val,
                               payload.filename.filename_len);
        break;
    default:
        dtn_set_errno(h, DTN_EINVAL);
        return NULL;
    }

    if (payload.status_report) {
        dtn_status_report* sr_dst = new dtn_status_report();
        dtn_bundle_status_report_t* sr_src = payload.status_report;

        sr_dst->bundle_id.source         = sr_src->bundle_id.source.uri;
        sr_dst->bundle_id.creation_secs  = sr_src->bundle_id.creation_ts.secs;
        sr_dst->bundle_id.creation_seqno = sr_src->bundle_id.creation_ts.seqno;
        sr_dst->reason                   = sr_src->reason;
        sr_dst->flags                    = sr_src->flags;
        sr_dst->receipt_ts_secs          = sr_src->receipt_ts.secs;
        sr_dst->receipt_ts_seqno         = sr_src->receipt_ts.seqno;
        sr_dst->custody_ts_secs          = sr_src->custody_ts.secs;
        sr_dst->custody_ts_seqno         = sr_src->custody_ts.seqno;
        sr_dst->forwarding_ts_secs       = sr_src->forwarding_ts.secs;
        sr_dst->forwarding_ts_seqno      = sr_src->forwarding_ts.seqno;
        sr_dst->delivery_ts_secs         = sr_src->delivery_ts.secs;
        sr_dst->delivery_ts_seqno        = sr_src->delivery_ts.seqno;
        sr_dst->deletion_ts_secs         = sr_src->deletion_ts.secs;
        sr_dst->deletion_ts_seqno        = sr_src->deletion_ts.seqno;
        sr_dst->ack_by_app_ts_secs       = sr_src->ack_by_app_ts.secs;
        sr_dst->ack_by_app_ts_seqno      = sr_src->ack_by_app_ts.seqno;

        bundle->status_report = sr_dst;
    } else {
        bundle->status_report = NULL;
    }

    return bundle;
}

//----------------------------------------------------------------------
struct dtn_session_info {
    unsigned int status;
    string       session;
};

//----------------------------------------------------------------------
dtn_session_info*
dtn_session_update(int handle, int timeout)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;

    unsigned int status = 0;
    dtn_endpoint_id_t session;
    memset(&session, 0, sizeof(session));
    
    int err = dtn_session_update(h, &status, &session, timeout);
    if (err != DTN_SUCCESS) {
        return NULL;
    }

    dtn_session_info* s = new dtn_session_info();
    s->status = status;
    s->session = session.uri;

    return s;
}

//----------------------------------------------------------------------
int
dtn_poll_fd(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return DTN_EINVAL;

    return dtn_poll_fd(h);
}

//----------------------------------------------------------------------
int
dtn_begin_poll(int handle, int timeout)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return DTN_EINVAL;

    return dtn_begin_poll(h, timeout);
}

//----------------------------------------------------------------------
int
dtn_cancel_poll(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return DTN_EINVAL;

    return dtn_cancel_poll(h);
}
