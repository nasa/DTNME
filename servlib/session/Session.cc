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

#ifdef HAVE_CONFIG_H
#include <dtn-config.h>
#endif

#include <oasys/thread/Atomic.h>

#include "Session.h"

namespace dtn {

//----------------------------------------------------------------------
Session::Session(const EndpointID& eid)
    : Logger("Session", "/dtn/session"),
      eid_(eid),
      bundles_(std::string("session-") + eid.str()),
      resubscribe_timer_(NULL)
{
    static oasys::atomic_t next_session_id = 0;
    id_ = oasys::atomic_incr_ret(&next_session_id);

    logpath_appendf("/%d", id_);
    log_info("created new session %u for eid %s", id_, eid.c_str());
}

//----------------------------------------------------------------------
Session::~Session()
{
}

//----------------------------------------------------------------------
int
Session::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "session id %u [%s]", id(), eid().c_str());
}

//----------------------------------------------------------------------
const char*
Session::flag_str(u_int flags)
{
    // XXX/demmer not really flags, but you know...
    switch (flags) {
    case 0:           return "Session::NONE";
    case SUBSCRIBE:   return "Session::SUBSCRIBE";
    case RESUBSCRIBE: return "Session::RESUBSCRIBE";
    case UNSUBSCRIBE: return "Session::UNSUBSCRIBE";
    case PUBLISH:     return "Session::PUBLISH";
    case CUSTODY:     return "Session::CUSTODY";
    default: {
        static char buf[256];
        snprintf(buf, sizeof(buf), "Session::UNKNOWN(0x%x)", flags);
        return buf;
    }
    }
}

//----------------------------------------------------------------------
void
Session::set_upstream(const Subscriber& upstream)
{
    upstream_ = upstream;
    log_info("set_upstream(*%p)", &upstream_);

    add_subscriber(upstream);
}

//----------------------------------------------------------------------
void
Session::add_subscriber(const Subscriber& subscriber)
{
    if (std::find(subscribers_.begin(), subscribers_.end(), subscriber) !=
        subscribers_.end())
    {
        log_info("add_subscriber(*%p): already subscribed",
                 &subscriber);
        return;
    }
    
    subscribers_.push_back(subscriber);
    log_info("add_subscriber(*%p)", &subscriber);
}

} // namespace dtn
