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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include "Registration.h"
#include "RegistrationTable.h"
#include "RegistrationInitialLoadThread.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "storage/GlobalStore.h"

namespace dtn {

//----------------------------------------------------------------------
const char*
Registration::failure_action_toa(failure_action_t action)
{
    switch(action) {
    case DROP:  return "DROP";
    case DEFER:    return "DEFER";
//dzdebug    case EXEC:  return "EXEC";
    }

    return "__INVALID__";
}

//----------------------------------------------------------------------
const char*
Registration::replay_action_toa(replay_action_t action)
{
    switch(action) {
    case NEW:  return "NEW";
    case NONE: return "NONE";
    case ALL:  return "ALL";
    }

    return "__INVALID__";
}

Registration::Registration(u_int32_t regid,
                 const SPtr_EIDPattern& sptr_pattern,
                 u_int32_t failure_action,
                 u_int32_t replay_action,
                 u_int32_t session_flags,
                 u_int32_t expiration,
                 bool delivery_acking,
                 const std::string& script)
    : Logger("Registration", "/dtn/registration/%d", regid),
      regid_(regid),
      failure_action_(failure_action),
      replay_action_(replay_action),
      session_flags_(session_flags),
      delivery_acking_(delivery_acking),
      script_(script),
      expiration_(expiration),
      expiration_timer_(nullptr),
      active_(false),
      expired_(false),
      in_datastore_(false),
      add_to_datastore_(false),
      in_storage_queue_(false),
      delivery_cache_(std::string(logpath()) + "/delivery_cache", 1024),
      initial_load_thread_(nullptr)
      
{
    sptr_endpoint_ = sptr_pattern;

    struct timeval now;
    ::gettimeofday(&now, 0);
    creation_time_ = now.tv_sec;
    
    init_expiration_timer();

    reg_list_type_str_ = "RegistrationBase";
}


Registration::Registration(u_int32_t regid,
                 const SPtr_EID& sptr_eid,
                 u_int32_t failure_action,
                 u_int32_t replay_action,
                 u_int32_t session_flags,
                 u_int32_t expiration,
                 bool delivery_acking,
                 const std::string& script)
    : Logger("Registration", "/dtn/registration/%d", regid),
      regid_(regid),
      failure_action_(failure_action),
      replay_action_(replay_action),
      session_flags_(session_flags),
      delivery_acking_(delivery_acking),
      script_(script),
      expiration_(expiration),
      expiration_timer_(nullptr),
      active_(false),
      expired_(false),
      in_datastore_(false),
      add_to_datastore_(false),
      in_storage_queue_(false),
      delivery_cache_(std::string(logpath()) + "/delivery_cache", 1024),
      initial_load_thread_(nullptr)
      
{
    sptr_endpoint_ = BD_MAKE_PATTERN(sptr_eid->str());

    struct timeval now;
    ::gettimeofday(&now, 0);
    creation_time_ = now.tv_sec;
    
    init_expiration_timer();

    reg_list_type_str_ = "RegistrationBase";
}


//----------------------------------------------------------------------
Registration::Registration(const oasys::Builder&)
    : Logger("Registration", "/dtn/registration"),
      regid_(0),
      failure_action_(DEFER),
      replay_action_(NEW),
      session_flags_(0),
      delivery_acking_(false),
      script_(),
      expiration_(0),
      creation_time_(0),
      expiration_timer_(nullptr),
      active_(false),
      expired_(false),
      in_datastore_(false),
      add_to_datastore_(false),
      in_storage_queue_(false),
      delivery_cache_(std::string(logpath()) + "/delivery_cache", 1024),
      initial_load_thread_(nullptr)
{
    sptr_endpoint_ = BD_MAKE_PATTERN_NULL();

    reg_list_type_str_ = "RegistrationBase";
}


//----------------------------------------------------------------------
Registration::~Registration()
{
    cleanup_expiration_timer();

    delivery_cache_.evict_all();
}

//----------------------------------------------------------------------
void
Registration::set_reg_list_type_str(const char* type_str)
{
    reg_list_type_str_ = type_str;
}

//----------------------------------------------------------------------
int
Registration::deliver_if_not_duplicate(Bundle* bundle, SPtr_Registration& sptr_reg)
{

    if (BundleDaemon::params_.dzdebug_reg_delivery_cache_enabled_) {
        if (! delivery_cache_.add_entry(bundle, bundle->bundleid())) {
            log_debug("suppressing duplicate delivery of bundle *%p - %s", bundle, bundle->gbofid_str().c_str());
            return false;
        }
    }

    log_debug("delivering bundle *%p - %s", bundle, bundle->gbofid_str().c_str());

    return deliver_bundle(bundle, sptr_reg);
}

//----------------------------------------------------------------------
void
Registration::session_notify(Bundle* bundle)
{
    (void)bundle;
    PANIC("generic session_notify implementation should never be called");
}

//----------------------------------------------------------------------
void
Registration::force_expire()
{
    ASSERT(active_);
    
    cleanup_expiration_timer();
    set_expired(true);
}

//----------------------------------------------------------------------
void
Registration::cleanup_expiration_timer()
{
    if (expiration_timer_ != nullptr) {
        // try to cancel the expiration timer. if it is still pending,
        // then the timer will clean itself up when it eventually
        // fires. otherwise, assert that we have actually expired and
        // delete the timer itself.
        oasys::SPtr_Timer base_sptr = expiration_timer_;
        bool pending = expiration_timer_->cancel_timer(base_sptr);
        
        if (! pending) {
            ASSERT(expired_);
        }
        
        expiration_timer_ = nullptr;
    }
}

//----------------------------------------------------------------------
void
Registration::serialize(oasys::SerializeAction* a)
{
    std::string tmp_eid = sptr_endpoint_->str();

    a->process("endpoint", &tmp_eid);
    a->process("regid", &regid_);
    a->process("failure_action", &failure_action_);
    a->process("replay_action", &replay_action_);
    a->process("session_flags", &session_flags_);
    a->process("delivery_acking", &delivery_acking_);
    a->process("script", &script_);
    a->process("creation_time", &creation_time_);
    a->process("expiration", &expiration_);
    a->process("delivery_acking", &delivery_acking_);

    // finish constructing the object after unserialization
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        sptr_endpoint_ = BD_MAKE_PATTERN(tmp_eid);
        init_expiration_timer();
    }

    logpathf("/dtn/registration/%d", regid_);
}

//----------------------------------------------------------------------
int
Registration::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz,
                    "id %u [%s]: %s %s (%s%s %s %s) [expiration %d]",
                    regid(),
                    reg_list_type_str_.c_str(),
                    active() ? "active" : "passive",
                    endpoint()->c_str(),
                    failure_action_toa(failure_action()),

                    "",
//dzdebug                    failure_action() == Registration::EXEC ?
//dzdebug                      script().c_str() : "",

                    replay_action_toa(replay_action()),
                    delivery_acking_ ? "ACK_Bndl_Dlvry" : "Auto-ACK_Bndl_Dlvry",
                    expiration()
        );
}

//----------------------------------------------------------------------
void
Registration::init_expiration_timer()
{
    if (expiration_ != 0) {
        struct timeval when, now;
        when.tv_sec  = creation_time_ + expiration_;
        when.tv_usec = 0;

        ::gettimeofday(&now, 0);

        long int in_how_long = TIMEVAL_DIFF_MSEC(when, now);
        if (in_how_long < 0) {
            log_warn("scheduling IMMEDIATE expiration for registration id %d: "
                     "[creation_time %u, expiration %u, now %u]", 
                     regid_, creation_time_, expiration_, (u_int)now.tv_sec);
        } else {
            log_debug("scheduling expiration for registration id %d at %u.%u "
                      "(in %ld seconds): ", regid_,
                      (u_int)when.tv_sec, (u_int)when.tv_usec,
                      in_how_long / 1000);
        }
        
        const RegistrationTable* reg_table = BundleDaemon::instance()->reg_table();
        SPtr_Registration sptr_reg = reg_table->get(regid_);
        if (sptr_reg) {
            expiration_timer_ = std::make_shared<ExpirationTimer>(sptr_reg);
            oasys::SPtr_Timer base_sptr = expiration_timer_;
            expiration_timer_->schedule_at(&when, base_sptr);
        }

    } else {
        set_expired(true);
    }
}

//----------------------------------------------------------------------
void
Registration::ExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;
    
    sptr_reg_->set_expired(true);
                      
    if (! sptr_reg_->active()) {
        RegistrationExpiredEvent* event_to_post;
        event_to_post = new RegistrationExpiredEvent(sptr_reg_);
        SPtr_BundleEvent sptr_event_to_post(event_to_post);
        BundleDaemon::post(sptr_event_to_post);
    } 
}

//----------------------------------------------------------------------
void
Registration::set_initial_load_thread(RegistrationInitialLoadThread* loader)
{
    initial_load_thread_ = loader;
}

//----------------------------------------------------------------------
void 
Registration::clear_initial_load_thread(RegistrationInitialLoadThread* loader)
{
    if ( initial_load_thread_ == loader ) {
        initial_load_thread_ = nullptr;
    }
}

//----------------------------------------------------------------------
void 
Registration::stop_initial_load_thread()
{
    if ( nullptr != initial_load_thread_ ) {
        initial_load_thread_->set_should_stop();
        initial_load_thread_ = nullptr;
    }
}



} // namespace dtn
