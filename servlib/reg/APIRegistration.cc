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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include <oasys/thread/SpinLock.h>
#include <oasys/serialize/SerializableVector.h>

#include "APIRegistration.h"
#include "reg/RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "session/Session.h"

namespace dtn {

//----------------------------------------------------------------------
APIRegistration::APIRegistration(const oasys::Builder& builder)
    : Registration(builder)
{
    bundle_list_ = new BlockingBundleList(logpath_);
    session_notify_list_ = NULL;
    unacked_bundle_list_ = new BundleList(logpath_);
    acked_bundle_list_   = new BundleList(logpath_);
}
    
//----------------------------------------------------------------------
APIRegistration::APIRegistration(u_int32_t regid,
                                 const EndpointIDPattern& endpoint,
                                 failure_action_t failure_action,
                                 replay_action_t replay_action,
                                 u_int32_t session_flags,
                                 u_int32_t expiration,
                                 bool delivery_acking,
                                 u_int64_t reg_token,
                                 const std::string& script)
    : Registration(regid, endpoint, failure_action, replay_action,
                   session_flags, expiration, delivery_acking, script)
{
    memcpy(&reg_token_, &reg_token, sizeof(u_int64_t));

    bundle_list_ = new BlockingBundleList(logpath_);
    if (session_flags & Session::CUSTODY) {
        session_notify_list_ = new BlockingBundleList(logpath_);
        session_notify_list_->logpath_appendf("/session_notify");
    } else {
        session_notify_list_ = NULL;
    }
    unacked_bundle_list_ = new BundleList(logpath_);
    acked_bundle_list_   = new BundleList(logpath_);
}

//----------------------------------------------------------------------
int
APIRegistration::format(char *buf, size_t sz) const {
    Registration::format(buf, sz);
    snprintf(&buf[strlen(buf)], sz-strlen(buf)-1,
             " ready %zu unacked %zu acked %zu",
             bundle_list_->size(),
             unacked_bundle_list_->size(), 
             acked_bundle_list_->size());
    return strlen(buf);
}

//----------------------------------------------------------------------
void
APIRegistration::serialize(oasys::SerializeAction* a)
{
    oasys::ScopeLock l(&lock_, "serializing bundle");

    Registration::serialize(a);

    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        bundle_list_->serialize(a);
        unacked_bundle_list_->serialize(a);
        acked_bundle_list_->serialize(a);
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL &&
        (session_flags_ & Session::CUSTODY))
    {
        session_notify_list_ = new BlockingBundleList(logpath_);
        session_notify_list_->logpath_appendf("/session_notify");
    }
    log_debug("APIRegistration::serialize -- done.");
}

//----------------------------------------------------------------------
APIRegistration::~APIRegistration()
{
    delete bundle_list_;
    if (session_notify_list_) {
        delete session_notify_list_;
    }
    delete unacked_bundle_list_;
    delete acked_bundle_list_;
}

//----------------------------------------------------------------------
void
APIRegistration::deliver_bundle(Bundle* bundle)
{
    if (!active() && (failure_action_ == DROP)) {
        log_info("deliver_bundle: "
                 "dropping bundle id %" PRIbid " for passive registration %d (%s)",
                 bundle->bundleid(), regid_, endpoint_.c_str());
        
        // post an event saying we "delivered" it
        BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
        return;
    }
    
    if (!active() && (failure_action_ == EXEC)) {
        // this sure seems like a security hole, but what can you
        // do -- it's in the spec
        log_info("deliver_bundle: "
                 "running script '%s' for registration %d (%s)",
                 script_.c_str(), regid_, endpoint_.c_str());
        
        system(script_.c_str());
        // fall through
    }

    log_info("deliver_bundle: queuing bundle id %" PRIbid " for %s delivery to %s",
             bundle->bundleid(),
             active() ? "active" : "deferred",
             endpoint_.c_str());

    oasys::ScopeLock l(&lock_, "adding bundle to bundle_list");
    if (active() || (replay_action() == Registration::NEW)) {
        if (BundleDaemon::instance()->params_.test_permuted_delivery_) {
            bundle_list_->insert_random(bundle);
        } else {
            bundle_list_->push_back(bundle);
        }
    } else {
        // application is not connected
        // either the replay action is NONE, which is an implicit ack, or
        // this bundle will be included in an ALL playback on reconnect

        // XXX/dz bundle used to be flagged as delivered before the actual
        // delivery so we need to issue the delivered event to mark it
        if (replay_action() == Registration::NONE) {
            BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
        }

        acked_bundle_list_->push_back(bundle);
    }
    
    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        update();
    }
}

//----------------------------------------------------------------------
void
APIRegistration::delete_bundle(Bundle* bundle)
{
    oasys::ScopeLock l(&lock_, "deleting bundle");
    if (bundle_list_->erase(bundle))
        return;

    if (unacked_bundle_list_->erase(bundle))
        return;

    if (acked_bundle_list_->erase(bundle))
        return;

    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        update();
    }
}

//----------------------------------------------------------------------
void
APIRegistration::session_notify(Bundle* bundle)
{
    oasys::ScopeLock l(&lock_, "session_notify");
    log_debug("session_notify *%p", bundle);
    session_notify_list_->push_back(bundle);

    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        update();
    }
}

//----------------------------------------------------------------------
void
APIRegistration::set_active_callback(bool a)
{
    // If application has gone inactive, do nothing
    if (! a) return;

    // Application is active

    oasys::ScopeLock l(&lock_, "set_active_callback");
    // Regardless of replay configuration, if the unacked
    // bundle list has bundles, we try again immediately
    // (move after an ALL bundle playback to preserve ordering?)
    if (! (unacked_bundle_list_->empty())) {
        unacked_bundle_list_->move_contents(bundle_list_);
        if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
            update();
        }
    }

    if ((replay_action() == Registration::NONE) ||
        (replay_action() == Registration::NEW))
        return;

    ASSERT(replay_action() == Registration::ALL);

    if (! (acked_bundle_list_->empty())) {
        acked_bundle_list_->move_contents(bundle_list_);
        if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
            update();
        }
    }
}

//----------------------------------------------------------------------
BundleRef
APIRegistration::deliver_front()
{
    oasys::ScopeLock l(&lock_, "deliver_front");

    BundleRef bref("APIRegistration::deliver_front");
    bref = bundle_list_->pop_front();

    Bundle* b = bref.object();
    //dz debug ASSERT(b != NULL);
    // possible that bundle expired between calls in APIServer::handle_recv??
    if (b == NULL) {
        return bref;
    }

    if (delivery_acking()) {
        // must wait for an explicit ack from the app
        unacked_bundle_list_->push_back(b);
    } else {
        // auto-acking enabled
        acked_bundle_list_->push_back(b);
    }

    // Update registration on disk
    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        update();
    }

    return bref;
}

//----------------------------------------------------------------------
void
APIRegistration::save(Bundle *b)
{
    log_debug("YOU SHOULDN'T SEE THIS.");

    if (delivery_acking()) {
        // must wait for an explicit ack from the app
        unacked_bundle_list_->push_back(b);
    } else {
        // auto-acking enabled
        acked_bundle_list_->push_back(b);
    }
}

//----------------------------------------------------------------------
void
APIRegistration::update()
{
    const RegistrationTable* reg_table = BundleDaemon::instance()->reg_table();
    reg_table->update(this);
}

//----------------------------------------------------------------------
void
APIRegistration::bundle_ack(const EndpointID& source_eid,
                            const BundleTimestamp& creation_ts)
{
    BundleRef b("APIRegistration::bundle_ack");
    b = unacked_bundle_list_->find(source_eid, creation_ts);
    if (b.object() == NULL) {
        log_debug("acknowledgement of unknown bundle");
        return;
    }

    oasys::ScopeLock l(&lock_, "bundle_ack");

    acked_bundle_list_->push_back(b.object());
    unacked_bundle_list_->erase(b.object());

    log_info("registration %d (%s) acknowledged delivery of bundle %" PRIbid,
             regid_, endpoint_.c_str(), (b.object())->bundleid());

    if (BundleDaemon::instance()->params_.serialize_apireg_bundle_lists_) {
        update();
    }
}

} // namespace dtn
