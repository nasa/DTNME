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

#include "APIRegistration.h"
#include "Registration.h"
#include "RegistrationTable.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
RegistrationTable::RegistrationTable()
    : Logger("RegistrationTable", "/dtn/registration/table")
{
}

//----------------------------------------------------------------------
RegistrationTable::~RegistrationTable()
{
    oasys::ScopeLock l(&lock_, __func__); 
//dzdebug    while (! reglist_.empty()) {
//dzdebug        delete reglist_.front();
//dzdebug        reglist_.pop_front();
//dzdebug    }


    reglist_.clear();
}

//----------------------------------------------------------------------
bool
RegistrationTable::find(u_int32_t regid, RegistrationList::iterator* iter)
{
    oasys::ScopeLock l(&lock_, __func__); 

    SPtr_Registration sptr_reg;

    for (*iter = reglist_.begin(); *iter != reglist_.end(); ++(*iter)) {
        sptr_reg = *(*iter);

        if (sptr_reg->regid() == regid) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
SPtr_Registration
RegistrationTable::get(u_int32_t regid) const
{
    SPtr_Registration sptr_reg = nullptr;

    oasys::ScopeLock l(&lock_, __func__); 

    RegistrationList::iterator iter;

    // the const_cast lets us use the same find method for get as we
    // use for add/del
    if (const_cast<RegistrationTable*>(this)->find(regid, &iter)) {
        sptr_reg = *iter;
    }
    return sptr_reg;
}

//----------------------------------------------------------------------
SPtr_Registration
RegistrationTable::get(const SPtr_EIDPattern& sptr_eid) const
{
    SPtr_Registration sptr_reg;
    RegistrationList::const_iterator iter;
    
    oasys::ScopeLock l(&lock_, __func__); 

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        if (sptr_eid->match((*iter)->endpoint())) {
            sptr_reg = *iter;
            break;
        }
    }
   
    return sptr_reg;
}

//----------------------------------------------------------------------
SPtr_Registration
RegistrationTable::get(const SPtr_EIDPattern& sptr_eid, u_int64_t reg_token) const
{
    SPtr_Registration sptr_reg;
    RegistrationList::const_iterator iter;
    
    oasys::ScopeLock l(&lock_, __func__); 

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        if (sptr_eid->match((*iter)->endpoint())) {
            APIRegistration* api_reg = dynamic_cast<APIRegistration*>((*iter).get());
            if (api_reg == NULL) {
                continue;
            }
            if ( api_reg->reg_token()==reg_token ) {
                sptr_reg = *iter;
                break;
            }
        }
    }
    
    return sptr_reg;
}

//----------------------------------------------------------------------
bool
RegistrationTable::add(SPtr_Registration& sptr_reg, bool add_to_store)
{
    oasys::ScopeLock l(&lock_, __func__); 

    // put it in the list
    reglist_.push_back(sptr_reg);

    // don't store (or log) default registrations 
    if (!add_to_store || sptr_reg->regid() <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    // now, all we should get are API registrations
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(sptr_reg.get());
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                sptr_reg->regid());
        return false;
    }

    //dzdebug api_reg->set_add_to_datastore(true);

    log_info("adding registration %d/%s", sptr_reg->regid(),
             sptr_reg->endpoint()->c_str());

    //dzdebug BundleDaemon::instance()->registration_add_update_in_storage(sptr_reg);
    
    return true;
}

//----------------------------------------------------------------------
bool
//RegistrationTable::del(u_int32_t regid)
RegistrationTable::del(SPtr_Registration& sptr_reg)
{
    u_int32_t regid = sptr_reg->durable_key();
    log_info("removing registration %d", regid);

  
    sptr_reg->stop_initial_load_thread();

    RegistrationList::iterator iter;

    oasys::ScopeLock l(&lock_, __func__); 
    
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
    (void) reg;
    //XXX/dz strip out registration saving to database for DTNME???
    return true;

#if 0
    log_debug("updating registration %d/%s",
             sptr_reg->regid(), sptr_reg->endpoint()->c_str());

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(sptr_reg.get());
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                sptr_reg->regid());
        return false;
    }
    
    BundleDaemon::instance()->registration_add_update_in_storage(api_reg);

    return true;
#endif
}

//----------------------------------------------------------------------
int
RegistrationTable::get_matching(const SPtr_EID& sptr_eid,
                                RegistrationList* reg_list) const
{
    oasys::ScopeLock l(&lock_, __func__); 

    int count = 0;
    
    RegistrationList::const_iterator iter;
    SPtr_Registration sptr_reg;

    log_debug("get_matching %s", sptr_eid->c_str());

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        sptr_reg = *iter;

        if (sptr_reg->endpoint()->match(sptr_eid)) {
            log_debug("matched registration %d %s",
                      sptr_reg->regid(), sptr_reg->endpoint()->c_str());
            count++;
            reg_list->push_back(sptr_reg);
        }
    }

    log_debug("get_matching %s: returned %d matches", sptr_eid->c_str(), count);
    return count;
}


void
RegistrationTable::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, __func__); 

    RegistrationList::const_iterator i;
    for (i = reglist_.begin(); i != reglist_.end(); ++i)
    {
        buf->appendf("*%p\n", (*i).get());
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
