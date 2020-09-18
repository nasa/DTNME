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

#ifndef _API_REGISTRATION_H_
#define _API_REGISTRATION_H_

#include <list>
#include <map>
#include "Registration.h"

namespace dtn {

class BlockingBundleList;

/**
 * Registration class to represent an actual attached application over
 * the client api.
 */
class APIRegistration : public Registration {
public:
    /**
     * Constructor for deserialization
     */
    APIRegistration(const oasys::Builder& builder);

    /**
     * Constructor.
     */
    APIRegistration(u_int32_t regid,
                    const EndpointIDPattern& endpoint,
                    failure_action_t failure_action,
                    replay_action_t replay_action,
                    u_int32_t session_flags,
                    u_int32_t expiration,
                    bool delivery_acking = false,
                    u_int64_t reg_token = 0,
                    const std::string& script = "");

    ~APIRegistration();

    /// Virtual from SerializableObject
    void serialize(oasys::SerializeAction* a);
    
    /// Virtual from Registration
    int format(char *buf, size_t sz) const;
    void deliver_bundle(Bundle* bundle);
    void delete_bundle(Bundle* bundle);
    void session_notify(Bundle* bundle);
    void set_active_callback(bool a);

    /*
     * Return front bundle and move from bundle_list to appropriate
     * secondary list (unacked or acked)
     */
    BundleRef deliver_front();

    /**
     * Record delivery attempts to the appropriate history list
     */
    void save(Bundle *b);

    /*
     * Accessor for lock on bundle lists.
     */
    oasys::SpinLock* lock() { return &lock_; }

    /**
     * Update registration on disk.
     */
    void update();

    /**
     * Handle application bundle delivery acknowledgements
     */
    void bundle_ack(const EndpointID& source_eid,
                    const BundleTimestamp& creation_ts);
    
    /**
     * Accessor for the queue of bundles for the registration.
     */
    BlockingBundleList* bundle_list() { return bundle_list_; }

    /**
     * Accessor for notification of session subscribers /
     * unsubscribers (currently just the subscription bundles).
     */
    BlockingBundleList* session_notify_list() { return session_notify_list_; }
    
    u_int64_t reg_token() { return reg_token_; }

#ifdef DTPC_ENABLED
    u_int32_t dtpc_topic_id()            {return dtpc_topic_id_; }
    void set_dtpc_topic_id(u_int32_t t)  { dtpc_topic_id_ = t; }
#endif

protected:
    /// App-supplied token identifying the registration
    u_int64_t reg_token_;

    /// Queue of bundles for the registration
    BlockingBundleList* bundle_list_;

    /// Queue of subscription notification bundles
    BlockingBundleList* session_notify_list_;

    /// Queue of delivered bundles that haven't yet been acked
    BundleList* unacked_bundle_list_;

    /// Queue of bundles deliver to and acked by the application
    BundleList* acked_bundle_list_;

    /// Lock to be taken by those modifying or marshaling the registration.
    oasys::SpinLock lock_;

#ifdef DTPC_ENABLED
    /// DTPC Topic ID
    u_int32_t dtpc_topic_id_;
#endif
};

/**
 * Typedef for a list of APIRegistrations.
 */
class APIRegistrationList : public std::list<APIRegistration*> {};

} // namespace dtn

#endif /* _API_REGISTRATION_H_ */
