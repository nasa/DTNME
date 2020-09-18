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

#ifndef _REGISTRATION_TABLE_H_
#define _REGISTRATION_TABLE_H_

#include <string>
#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringBuffer.h>

#include "Registration.h"

namespace dtn {

/**
 * Class for the in-memory registration table. All changes to the
 * table are made persistent via the RegistrationStore.
 */
class RegistrationTable : public oasys::Logger {
public:
    /**
     * Constructor
     */
    RegistrationTable();

    /**
     * Destructor
     */
    virtual ~RegistrationTable();

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false if there's another
     * registration with the same {endpoint,regid}.
     *
     * The flag controls whether or not the registration is added to
     * the persistent store, which is only done for registrations
     * added from the RPC interface.
     */
    bool add(Registration* reg, bool add_to_store = true);

    /**
     * Look up a matching registration.
     */
    Registration* get(u_int32_t regid) const;

    /**
     * Look up the first matching registration for the exact endpoint
     * id pattern given.
     */
    Registration* get(const EndpointIDPattern& eid) const;

    /**
     * Look up the first matching registration for the exact endpoint
     * id pattern given and with the given reg_token
     */
    Registration* get(const EndpointIDPattern& eid, u_int64_t reg_token) const;

    /**
     * Remove the registration from the database, returns true if
     * successful, false if the registration didn't exist.
     */
    //bool del(u_int32_t regid);
    bool del(Registration* reg);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false on error.
     */
    bool update(Registration* reg) const;
    
    /**
     * Populate the given reglist with all registrations with an
     * endpoint id that matches the bundle demux string.
     *
     * Returns the count of matching registrations.
     */
    int get_matching(const EndpointID& eid, RegistrationList* reg_list) const;
    
    /**
     * Delete any expired registrations
     *
     * (was sweepOldRegistrations)
     */
    int delete_expired(const time_t now);

    /**
     * Dump out the registration database.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Return the routing table.  Asserts that the RegistrationTable
     * spin lock is held by the caller.
     */
    const RegistrationList *reg_list() const;

    /**
     * Accessor for the RouteTable internal lock.
     */
    oasys::Lock* lock() const { return &lock_; }

protected:
    /**
     * Internal method to find the location of the given registration.
     */
    bool find(u_int32_t regid, RegistrationList::iterator* iter);

    /**
     * All registrations are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_registrations.
     */
    RegistrationList reglist_;

    /**
     * Lock to protect internal data structures.
     */
    mutable oasys::SpinLock lock_;
};

} // namespace dtn

#endif /* _REGISTRATION_TABLE_H_ */
