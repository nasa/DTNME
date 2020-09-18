/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef DTPC_ENABLED

#ifndef __STDC_FORMAT_MACROS
#    define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleRef.h"
#include "bundling/SDNV.h"
#include "naming/IPNScheme.h"

#include "DtpcDaemon.h"
#include "DtpcPayloadAggregationTimer.h"
#include "DtpcProtocolDataUnit.h"
#include "DtpcProfile.h"
#include "DtpcProfileTable.h"
#include "DtpcTopic.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcProtocolDataUnit::DtpcProtocolDataUnit(const EndpointID& remote_eid, 
                                           u_int32_t profile_id,
                                           u_int64_t seq_ctr)
    : Logger("DtpcProtocolDataUnit", "/dtpc/pdu"),
      key_(""),
      collector_key_(""),
      key_is_set_(false),
      ion_alt_key_(""),
      has_ion_alt_key_(false),
      profile_id_(profile_id),
      seq_ctr_(seq_ctr),
      retransmit_count_(0),
      retransmit_timer_(NULL),
      buf_(NULL),
      creation_ts_(0),
      expiration_ts_(0),
      app_ack_(false),
      topic_block_index_(0),
      in_datastore_(false),
      queued_for_datastore_(false)
{
    set_remote_eid(remote_eid);
    set_local_eid(EndpointID::NULL_EID());

    log_debug("Created ProtocolDataUnit for Remote EID: %s Profile: %d",
              remote_eid_.c_str(), profile_id_);
}


//----------------------------------------------------------------------
DtpcProtocolDataUnit::DtpcProtocolDataUnit(const oasys::Builder&)
    : Logger("DtpcProtocolDataUnit", "/dtpc/payload/agg"),
      key_(""),
      collector_key_(""),
      key_is_set_(false),
      ion_alt_key_(""),
      has_ion_alt_key_(false),
      profile_id_(0),
      seq_ctr_(0),
      retransmit_count_(0),
      retransmit_timer_(NULL),
      buf_(NULL),
      creation_ts_(0),
      expiration_ts_(0),
      app_ack_(false),
      topic_block_index_(0),
      in_datastore_(false),
      queued_for_datastore_(false)
{
    set_remote_eid(EndpointID::NULL_EID());
    set_local_eid(EndpointID::NULL_EID());
}

//----------------------------------------------------------------------
DtpcProtocolDataUnit::~DtpcProtocolDataUnit () 
{
    delete buf_;
}


//----------------------------------------------------------------------
void
DtpcProtocolDataUnit::serialize(oasys::SerializeAction* a)
{
    // make sure key is initialized
    key();

    a->process("key", &key_);
    a->process("collector_key", &collector_key_);
    a->process("ion_alt_key", &ion_alt_key_);
    a->process("has_ion_alt_key", &has_ion_alt_key_);
    a->process("key_is_set", &key_is_set_);
    a->process("profile_id", &profile_id_);
    a->process("remote_eid", &remote_eid_);
    a->process("local_eid", &remote_eid_);
    a->process("seq_ctr", &seq_ctr_);
    a->process("retransmit_count", &retransmit_count_);
    a->process("retransmit_ts", &retransmit_ts_);
    a->process("app_ack", &app_ack_);
    a->process("topic_block_index", &topic_block_index_);
    a->process("creation_ts", &creation_ts_);
    a->process("expiration_ts", &expiration_ts_);

    u_char* buf_ptr = NULL;
    size_t buf_size;

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        a->process("buf_size", &buf_size);
        buf_ = new DtpcPayloadBuffer(buf_size);
    } else {
        buf_size = size();
        a->process("buf_size", &buf_size);
    }

    buf_ptr = buf()->buf();
    a->process("buf", (u_char*)buf_ptr, buf_size);
}

//----------------------------------------------------------------------
void
DtpcProtocolDataUnit::set_profile_id(u_int32_t t)
{ 
    oasys::ScopeLock l(&lock_, "set_profile_id");

    key_is_set_ = false; 
    profile_id_ = t; 
}

//----------------------------------------------------------------------
void
DtpcProtocolDataUnit::set_remote_eid(const EndpointID& eid)
{
    oasys::ScopeLock l(&lock_, "set_remote_eid");

    key_is_set_ = false; 
    remote_eid_.assign(eid);
}

//----------------------------------------------------------------------
void
DtpcProtocolDataUnit::set_local_eid(const EndpointID& eid)
{
    local_eid_.assign(eid);
}

//----------------------------------------------------------------------
void 
DtpcProtocolDataUnit::set_seq_ctr(u_int64_t t)
{ 
    oasys::ScopeLock l(&lock_, "set_seq_ctr");

    key_is_set_ = false; 
    seq_ctr_ = t; 
}

//----------------------------------------------------------------------
void
DtpcProtocolDataUnit::set_key()
{
    if (!key_is_set_) {
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "~%"PRIi32"~%"PRIu64, profile_id_, seq_ctr_);

        key_.clear();
        key_.append(remote_eid_.c_str());
        key_.append(tmp);

        // create an alternate key for ION compatibility -
        // we send to ipn:x.129 and expect ACK back from that address
        // but ION sends it with a source ipn:x.128 so we compensate
        // with the ion_alt_key_
        if (remote_eid_.scheme() == IPNScheme::instance()) {
            u_int64_t node = 0;
            u_int64_t service = 0;
            if (! IPNScheme::parse(remote_eid_, &node, &service)) {
                ion_alt_key_ = key_;
                has_ion_alt_key_ = false;
            } else {
                EndpointID tmp_eid;
                service = DtpcDaemon::params_.ipn_receive_service_number;
                IPNScheme::format(&tmp_eid, node, service);
                ion_alt_key_.clear();
                ion_alt_key_.append(tmp_eid.c_str());
                ion_alt_key_.append(tmp);
                has_ion_alt_key_ = true;
            }
        } else {
            ion_alt_key_ = key_;
            has_ion_alt_key_ = false;
        }

        snprintf(tmp, sizeof(tmp), "~%"PRIi32, profile_id_);
        collector_key_.clear();
        collector_key_.append(remote_eid_.c_str());
        collector_key_.append(tmp);

        key_is_set_ = true;
    }
}

//----------------------------------------------------------------------
std::string
DtpcProtocolDataUnit::key()
{
    lock_.lock("key");

    set_key();
    std::string key = key_;

    lock_.unlock();

    return key;
}

// An alternate key for ION compatibility:
// we send to ipn:x.129 and expect ACK back from that address
// but ION sends it with a source ipn:x.128 so we compensate
// with the ion_alt_key_
//----------------------------------------------------------------------
std::string
DtpcProtocolDataUnit::ion_alt_key()
{
    lock_.lock("ion_alt_key");

    set_key();
    std::string key = ion_alt_key_;

    lock_.unlock();

    return key;
}

// An alternate key for ION compatibility:
// we send to ipn:x.129 and expect ACK back from that address
// but ION sends it with a source ipn:x.128 so we compensate
// with the ion_alt_key_
//----------------------------------------------------------------------
bool
DtpcProtocolDataUnit::has_ion_alt_key()
{
    lock_.lock("has_ion_alt_key");

    set_key();
    bool result = has_ion_alt_key_;

    lock_.unlock();

    return result;
}

//----------------------------------------------------------------------
std::string
DtpcProtocolDataUnit::collector_key()
{
    lock_.lock("collector_key");

    set_key();
    std::string key = collector_key_;

    lock_.unlock();

    return key;
}

} // namespace dtn

#endif // DTPC_ENABLED
