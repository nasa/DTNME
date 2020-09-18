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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "APIRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemonStorage.h"

namespace dtn {

//----------------------------------------------------------------------
RegistrationTable::RegistrationTable()
    : Logger("RegistrationTable", "/dtn/registration/table")
{
}

//----------------------------------------------------------------------
RegistrationTable::~RegistrationTable()
{
    while (! reglist_.empty()) {
        delete reglist_.front();
        reglist_.pop_front();
    }
}

//----------------------------------------------------------------------
bool
RegistrationTable::find(u_int32_t regid, RegistrationList::iterator* iter)
{
    Registration* reg;

    for (*iter = reglist_.begin(); *iter != reglist_.end(); ++(*iter)) {
        reg = *(*iter);

        if (reg->regid() == regid) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
Registration*
RegistrationTable::get(u_int32_t regid) const
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    RegistrationList::iterator iter;

    // the const_cast lets us use the same find method for get as we
    // use for add/del
    if (const_cast<RegistrationTable*>(this)->find(regid, &iter)) {
        return *iter;
    }
    return NULL;
}

//----------------------------------------------------------------------
Registration*
RegistrationTable::get(const EndpointIDPattern& eid) const
{
    Registration* reg;
    RegistrationList::const_iterator iter;
    
    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;
        if (reg->endpoint().equals(eid)) {
            return reg;
        }
    }
    
    return NULL;
}

//----------------------------------------------------------------------
Registration*
RegistrationTable::get(const EndpointIDPattern& eid, u_int64_t reg_token) const
{
    Registration* reg;
    RegistrationList::const_iterator iter;
    

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;
        if (reg->endpoint().equals(eid)) {
            APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
            if (api_reg == NULL) {
                continue;
            }
            if ( api_reg->reg_token()==reg_token ) {
                return reg;
            }
        }
    }
    
    return NULL;
}

//----------------------------------------------------------------------
bool
RegistrationTable::add(Registration* reg, bool add_to_store)
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    // put it in the list
    reglist_.push_back(reg);

    // don't store (or log) default registrations 
    if (!add_to_store || reg->regid() <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    // now, all we should get are API registrations
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                reg->regid());
        return false;
    }

    api_reg->set_add_to_datastore(true);

    log_info("adding registration %d/%s", reg->regid(),
             reg->endpoint().c_str());

    BundleDaemonStorage::instance()->registration_add_update(api_reg);
    
    return true;
}

//----------------------------------------------------------------------
bool
//RegistrationTable::del(u_int32_t regid)
RegistrationTable::del(Registration* reg)
{
    u_int32_t regid = reg->durable_key();
    log_info("removing registration %d", regid);

  
    reg->stop_initial_load_thread();

    RegistrationList::iterator iter;

    oasys::ScopeLock l(&lock_, "RegistrationTable");
    
    if (! find(regid, &iter)) {
        log_err("error removing registration %d: no matching registration",
                regid);
        return false;
    }

    reglist_.erase(iter);

    // Store (or log) default registrations and not pushed to persistent store
    if (regid <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    return true;
}

//----------------------------------------------------------------------
bool
RegistrationTable::update(Registration* reg) const
{
    log_debug("updating registration %d/%s",
             reg->regid(), reg->endpoint().c_str());

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                reg->regid());
        return false;
    }
    
    BundleDaemonStorage::instance()->registration_add_update(api_reg);

    return true;
}

//----------------------------------------------------------------------
int
RegistrationTable::get_matching(const EndpointID& demux,
                                RegistrationList* reg_list) const
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    int count = 0;
    
    RegistrationList::const_iterator iter;
    Registration* reg;

    log_debug("get_matching %s", demux.c_str());

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;

        if (reg->endpoint().match(demux)) {
            log_debug("matched registration %d %s",
                      reg->regid(), reg->endpoint().c_str());
            count++;
            reg_list->push_back(reg);
        }
    }

    log_debug("get_matching %s: returned %d matches", demux.c_str(), count);
    return count;
}


void
RegistrationTable::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    RegistrationList::const_iterator i;
    for (i = reglist_.begin(); i != reglist_.end(); ++i)
    {
        buf->appendf("*%p\n", *i);
    }
}

/**
 * Return the routing table.  Asserts that the RegistrationTable
 * spin lock is held by the caller.
 */
const RegistrationList *
RegistrationTable::reg_list() const
{
    ASSERTF(lock_.is_locked_by_me(),
            "RegistrationTable::reg_list must be called while holding lock");
    return &reglist_;
}

} // namespace dtn
