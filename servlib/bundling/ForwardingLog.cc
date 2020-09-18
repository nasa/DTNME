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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/Serialize.h>
#include "ForwardingLog.h"
#include "conv_layers/ConvergenceLayer.h"
#include "reg/Registration.h"
#include "BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
ForwardingLog::ForwardingLog(oasys::SpinLock* lock, Bundle* bundle)
    : Logger("ForwardingLog", "/dtn/bundle/forwardingLog:"),
      lock_(lock), bundle_(bundle)
{
}

//----------------------------------------------------------------------
bool
ForwardingLog::get_latest_entry(const LinkRef& link, ForwardingInfo* info) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::get_latest_state");

    // iterate backwards through the vector to get the latest entry
    Log::const_reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->link_name() == link->name_str())
        {
            // This assertion holds as long as the mapping of link
            // name to remote eid is persistent. This may need to be
            // revisited once link tables are serialized to disk.
        	// xxx/Elwyn: (hopefully) correctly persistent link names
        	// across restarts are now implemented and this assertion
        	// has been forced to hold.
        	// See ContactManager::new_opportunistic_link.
        	// (as at March 2012).


            if (!BundleDaemon::params_.persistent_links_) {
                if ((iter->remote_eid() != EndpointID::NULL_EID()) &&
                    (iter->remote_eid() != link->remote_eid())) {
                    // XXX/dz - had issues with persistent links probably due
                    //          to different IP addresses from VPN sessions
                    // when reloading at startup the link may not have a remote_eid yet
                    return false;
                }
            } else {
                ASSERT((iter->remote_eid() == EndpointID::NULL_EID()) ||
                       (iter->remote_eid() == link->remote_eid()));
            }
            *info = *iter;
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
ForwardingLog::state_t
ForwardingLog::get_latest_entry(const LinkRef& link) const
{
    ForwardingInfo info;
    if (! get_latest_entry(link, &info)) {
        return ForwardingInfo::NONE;
    }

    return info.state();
}

//----------------------------------------------------------------------
bool
ForwardingLog::get_latest_entry(const Registration* reg,
                                ForwardingInfo*     info) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::get_latest_state");

    // iterate backwards through the vector to get the latest entry
    Log::const_reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->regid() == reg->regid())
        {
            // This assertion holds as long as the mapping of
            // registration id to registration eid is persistent,
            // which will need to be revisited once the forwarding log
            // is serialized to disk.
            ASSERT(iter->remote_eid() == EndpointID::NULL_EID() ||
                   iter->remote_eid() == reg->endpoint());
            *info = *iter;
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
ForwardingLog::state_t
ForwardingLog::get_latest_entry(const Registration* reg) const
{
    ForwardingInfo info;
    if (! get_latest_entry(reg, &info)) {
        return ForwardingInfo::NONE;
    }

    return info.state();
}

//----------------------------------------------------------------------
bool
ForwardingLog::get_latest_entry(state_t state, ForwardingInfo* info) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::get_latest_state");

    // iterate backwards through the vector to get the latest entry
    Log::const_reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->state() == state)
        {
            *info = *iter;
            return true;
        }
    }
    
    return false;
}
    
//----------------------------------------------------------------------
size_t
ForwardingLog::get_count(unsigned int states,
                         unsigned int actions) const
{
    size_t ret = 0;
    
    oasys::ScopeLock l(lock_, "ForwardingLog::get_count");
    
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        if ((iter->state()  & states) != 0 &&
            (iter->action() & actions) != 0)
        {
            ++ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
size_t
ForwardingLog::get_count(const EndpointID& eid,
                         unsigned int states,
                         unsigned int actions) const
{
    size_t ret = 0;

    oasys::ScopeLock l(lock_, "ForwardingLog::get_count");
    
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        if ((iter->remote_eid() == EndpointIDPattern::WILDCARD_EID() ||
             iter->remote_eid() == eid) &&
            (iter->state()  & states)  != 0 &&
            (iter->action() & actions) != 0)
        {
            ++ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
void
ForwardingLog::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::dump");
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        const ForwardingInfo* info = &(*iter);
        
        buf->appendf("\t%s -> %s [%s] %s at %u.%u "
                     "[custody min %d pct %d max %d]\n",
                     ForwardingInfo::state_to_str(info->state()),
                     info->link_name().c_str(),
                     info->remote_eid().c_str(),
                     ForwardingInfo::action_to_str(info->action()),
                     info->timestamp().sec_,
                     info->timestamp().usec_,
                     info->custody_spec().min_,
                     info->custody_spec().lifetime_pct_,
                     info->custody_spec().max_);
    }
}
    
//----------------------------------------------------------------------
void
ForwardingLog::add_entry(const LinkRef& link,
                         ForwardingInfo::action_t action,
                         state_t state,
                         const CustodyTimerSpec& custody_timer)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::add_entry");
    
    log_.push_back(ForwardingInfo(state, action, link->name_str(), 0xffffffff,
                                  link->remote_eid(), custody_timer));

    link->set_used_in_fwdlog();

    if (BundleDaemon::params_.persistent_fwd_logs_) {
        BundleDaemon* daemon = BundleDaemon::instance();
        daemon->actions()->store_update(bundle_);
    }
}

//----------------------------------------------------------------------
void
ForwardingLog::add_entry(const LinkRef& link,
                         ForwardingInfo::action_t action,
                         state_t state)
{
    CustodyTimerSpec default_spec;
    add_entry(link, action, state, default_spec);
}

//----------------------------------------------------------------------
void
ForwardingLog::add_entry(const Registration* reg,
                         ForwardingInfo::action_t action,
                         state_t state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::add_entry");

    oasys::StringBuffer name("registration-%d", reg->regid());
    CustodyTimerSpec spec;
    
    log_.push_back(ForwardingInfo(state, action, name.c_str(), reg->regid(),
                                  reg->endpoint(), spec));
    if (BundleDaemon::params_.persistent_fwd_logs_) {
        BundleDaemon* daemon = BundleDaemon::instance();
        daemon->actions()->store_update(bundle_);
    }
}

//----------------------------------------------------------------------
void
ForwardingLog::add_entry(const EndpointID&        eid,
                         ForwardingInfo::action_t action,
                         state_t                  state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::add_entry");

    oasys::StringBuffer name("eid-%s", eid.c_str());
    CustodyTimerSpec custody_timer;
    
    log_.push_back(ForwardingInfo(state, action, name.c_str(), 0xffffffff,
                                  eid, custody_timer));
    if (BundleDaemon::params_.persistent_fwd_logs_) {
        BundleDaemon* daemon = BundleDaemon::instance();
        daemon->actions()->store_update(bundle_);
    }
}

//----------------------------------------------------------------------
bool
ForwardingLog::update(const LinkRef& link, state_t state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::update");
    
    Log::reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->link_name() == link->name_str())
        {
            // This assertion holds as long as the mapping of link
            // name to remote eid is persistent. This may need to be
            // revisited once link tables are serialized to disk.
            ASSERT(iter->remote_eid() == EndpointID::NULL_EID() ||
                   iter->remote_eid() == link->remote_eid());
            iter->set_state(state);

            if (BundleDaemon::params_.persistent_fwd_logs_) {
                BundleDaemon* daemon = BundleDaemon::instance();
                daemon->actions()->store_update(bundle_);
            }

            return true;
        }
    }
    
    return false;
}

//----------------------------------------------------------------------
bool
ForwardingLog::update(Registration* reg,
                      state_t state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::update");
    
    Log::reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->regid() == reg->regid())
        {
            iter->set_state(state);

            if (BundleDaemon::params_.persistent_fwd_logs_) {
                BundleDaemon* daemon = BundleDaemon::instance();
                daemon->actions()->store_update(bundle_);
            }

            return true;
        }
    }
    return false;
}
    
//----------------------------------------------------------------------
void
ForwardingLog::update_all(state_t old_state, state_t new_state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::update_all");
    bool found = false;

    Log::reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->state() == old_state)
        {
            iter->set_state(new_state);
            found = true;
        }
    }
    if (found) {
        if (BundleDaemon::params_.persistent_fwd_logs_) {
            BundleDaemon* daemon = BundleDaemon::instance();
            daemon->actions()->store_update(bundle_);
        }
    }
}

//----------------------------------------------------------------------
void
ForwardingLog::serialize(oasys::SerializeAction *a)
{
    log_debug("Serializing the forwarding log");
    //a->process(log_);
    log_.serialize(a);
}

//----------------------------------------------------------------------
void
ForwardingLog::clear()
{
    oasys::ScopeLock l(lock_, "ForwardingLog::clear");
    log_.clear();

    if (BundleDaemon::params_.persistent_fwd_logs_) {
        BundleDaemon* daemon = BundleDaemon::instance();
        daemon->actions()->store_update(bundle_);
    }
}

} // namespace dtn
