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

#include <oasys/storage/DurableStore.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/MD5.h>

#include "GlobalStore.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemonStorage.h"
#include "reg/APIRegistration.h"

namespace dtn {

//----------------------------------------------------------------------
const u_int32_t GlobalStore::CURRENT_VERSION = 3;
static const char* GLOBAL_TABLE = "globals";
static const char* GLOBAL_KEY   = "global_key";

//----------------------------------------------------------------------
class Globals : public oasys::SerializableObject
{
public:
    Globals() {}
    Globals(const oasys::Builder&) {}

    u_int32_t version_;         ///< on-disk copy of CURRENT_VERSION
    bundleid_t next_bundleid_;	///< running serial number for bundles
    u_int32_t next_regid_;	///< running serial number for registrations
    bundleid_t last_custodyid_;	///< running serial number for custody IDs
    u_int32_t next_pacsid_;	///< running serial number for pending ACS IDs
    u_char digest_[oasys::MD5::MD5LEN];	///< MD5 digest of all serialized fields
    
    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);
};

//----------------------------------------------------------------------
void
Globals::serialize(oasys::SerializeAction* a)
{
    a->process("version",       &version_);
    a->process("next_bundleid", &next_bundleid_);
    a->process("next_regid",    &next_regid_);
    a->process("last_custodyid", &last_custodyid_);
    a->process("next_pacsid",   &next_pacsid_);
    a->process("digest",	digest_, 16);
}

//----------------------------------------------------------------------
GlobalStore* GlobalStore::instance_;

//----------------------------------------------------------------------
GlobalStore::GlobalStore()
    : Logger("GlobalStore", "/dtn/storage/%s", GLOBAL_TABLE),
      globals_(NULL), store_(NULL),
      needs_update_(false)
{
    lock_ = new oasys::Mutex(logpath_,
                             oasys::Mutex::TYPE_RECURSIVE,
                             true /* quiet */);
}

//----------------------------------------------------------------------
int
GlobalStore::init(const oasys::StorageConfig& cfg, 
                  oasys::DurableStore*        store)
{
    if (instance_ != NULL) 
    {
        PANIC("GlobalStore::init called multiple times");
    }
    
    instance_ = new GlobalStore();
    return instance_->do_init(cfg, store);
}

//----------------------------------------------------------------------
int
GlobalStore::do_init(const oasys::StorageConfig& cfg, 
                     oasys::DurableStore*        store)
{
    int flags = 0;

    // Global table has a single row using the (serialized) string key GLOBAL_KEY.
    // Just tell the database its a variable length string since it doesn't have to do
    // serious indexing.
    flags = (0 & KEY_LEN_MASK) << KEY_LEN_SHIFT;

    if (cfg.init_) {
        flags |= oasys::DS_CREATE;
    }

    int err = store->get_table(&store_, GLOBAL_TABLE, flags);

    if (err != 0) {
        log_err("error initializing global store: %s",
                (err == oasys::DS_NOTFOUND) ?
                "table not found" :
                "unknown error");
        return err;
    }

    // if we're initializing the database for the first time, then we
    // prime the values accordingly and sync the database version
    if (cfg.init_) 
    {
        log_info("initializing global table");

        globals_ = new Globals();

        globals_->version_          = CURRENT_VERSION;
        globals_->next_bundleid_    = 0;
        globals_->next_regid_       = Registration::MAX_RESERVED_REGID + 1;
        globals_->next_pacsid_      = 0;
        globals_->last_custodyid_   = 0;
        calc_digest(globals_->digest_);

        // store the new value
        err = store_->put(oasys::StringShim(GLOBAL_KEY), globals_,
                          oasys::DS_CREATE | oasys::DS_EXCL);
        
        if (err == oasys::DS_EXISTS) 
        {
            // YUCK
            log_err_p("/dtnd", "Initializing datastore which already exists.");
            exit(1);
        } else if (err != 0) {
            log_err_p("/dtnd", "unknown error initializing global store");
            return err;
        }
        
        loaded_ = true;
        
    } else {
        loaded_ = false;
    }

    GlobalStoreDBUpdateThread::init();

    return 0;
}

//----------------------------------------------------------------------
GlobalStore::~GlobalStore()
{
    delete store_;
    delete globals_;
    delete lock_;
}

//----------------------------------------------------------------------
bundleid_t
GlobalStore::next_bundleid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_bundleid");
    
    ASSERT(globals_->next_bundleid_ != BUNDLE_ID_MAX);
    //log_debug("next_bundleid %" PRIbid " -> %" PRIbid,
    //          globals_->next_bundleid_,
    //          globals_->next_bundleid_ + 1);
    
    bundleid_t ret = globals_->next_bundleid_++;

    update();

    return ret;
}
    
//----------------------------------------------------------------------
bundleid_t
GlobalStore::next_custodyid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_custodyid");

    //next_bundleid() will abort before this does
    //so don't worry about wrapping around
    ASSERT(globals_->last_custodyid_ != BUNDLE_ID_MAX);

#ifndef ACS_DBG
    // increment before returning value so that the value 0 is not a valid 
    // id and can be used to determine that the bundle was not received 
    // while ACS was enabled
    bundleid_t ret = ++globals_->last_custodyid_;
#else
    // debug assigning ids out of order to test various cases
    int delta = globals_->next_custodyid_ % 10;
    switch (delta) {
        case 0: delta = 7; break;
        case 1: delta = 9; break;
        case 2: delta = 3; break;
        case 3: delta = 5; break;
        case 4: delta = -3; break;
        case 5: delta = 4; break;
        case 6: delta = -2; break;
        case 7: delta = -4; break;
        case 8: delta = -6; break;
        case 9: delta = -3; break;
    }

    bundleid_t ret = globals_->last_custodyid_ += delta;
#endif

    //log_debug("last_custodyid: %" PRIu64, globals_->last_custodyid_);

    update();

    return ret;
}
    
//----------------------------------------------------------------------
bundleid_t
GlobalStore::last_custodyid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::last_custodyid");
#ifndef ACS_DBG
    return globals_->last_custodyid_;
#else
    // if debug mode and the id is jumbled then return a 
    // value that is larger than the highest value issued so far
    return globals_->last_custodyid_ + 10;
#endif
}

//----------------------------------------------------------------------
u_int32_t
GlobalStore::next_regid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_regid");
    
    ASSERT(globals_->next_regid_ != 0xffffffff);
    //log_debug("next_regid %d -> %d",
    //          globals_->next_regid_,
    //          globals_->next_regid_ + 1);

    u_int32_t ret = globals_->next_regid_++;

    update();

    return ret;
}

//----------------------------------------------------------------------
u_int32_t
GlobalStore::next_pacsid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_pacsid");
   
    // NOTE: it is safe to wrap around the pacsid counter 
    //log_debug("next_pcasid %d -> %d",
    //          globals_->next_pacsid_,
    //          globals_->next_pacsid_ + 1);

    u_int32_t ret = globals_->next_pacsid_++;

    update();

    return ret;
}

//----------------------------------------------------------------------
void
GlobalStore::calc_digest(u_char* digest)
{
    // We create dummy objects for all serialized objects, then take
    // their serialized form and MD5 it, so adding or deleting a
    // serialized field will change the digest
    Bundle b(oasys::Builder::builder());
    APIRegistration r(oasys::Builder::builder());


    oasys::StringSerialize s(oasys::Serialize::CONTEXT_LOCAL,
                             oasys::StringSerialize::INCLUDE_NAME |
                             oasys::StringSerialize::INCLUDE_TYPE |
                             oasys::StringSerialize::SCHEMA_ONLY);

    s.action(&b);
    s.action(&r);

    oasys::MD5 md5;
    md5.update(s.buf().data(), s.buf().length());
    md5.finalize();

    log_debug("calculated digest %s for serialize string '%s'",
              md5.digest_ascii().c_str(), s.buf().c_str());

    memcpy(digest, md5.digest(), oasys::MD5::MD5LEN);
}

//----------------------------------------------------------------------
bool
GlobalStore::load()
{
    log_debug("loading global store");

    oasys::StringShim key(GLOBAL_KEY);

    if (globals_ != NULL) {
        delete globals_;
        globals_ = NULL;
    }

    if (store_->get(key, &globals_) != 0) {
        log_crit("error loading global data");
        return false;
    }
    ASSERT(globals_ != NULL);

    if (globals_->version_ != CURRENT_VERSION) {
        log_crit("datastore version mismatch: "
                 "expected version %d, database version %d",
                 CURRENT_VERSION, globals_->version_);
        return false;
    }

    u_char digest[oasys::MD5::MD5LEN];
    calc_digest(digest);

    if (memcmp(digest, globals_->digest_, oasys::MD5::MD5LEN) != 0) {
        u_char digest_with_fwd_logs[oasys::MD5::MD5LEN] = { 0xbc, 0xde, 0xe3, 0x61, 0x02, 0x92, 0x1c, 0x5c, 0xec, 0x56, 0xb6, 0x3b, 0x3d, 0xe6, 0xd6, 0x49 };
        u_char digest_without_fwd_logs[oasys::MD5::MD5LEN] = { 0x61, 0x0f, 0xc7, 0x20, 0x6c, 0x9b, 0xd8, 0x7e, 0x4a, 0xb1, 0x16, 0x73, 0x54, 0x79, 0x69, 0x6a };

        if ((memcmp(globals_->digest_, digest_with_fwd_logs, oasys::MD5::MD5LEN) == 0) &&
            (memcmp(digest, digest_without_fwd_logs, oasys::MD5::MD5LEN) == 0)) {
            log_always("Converting database to not store forwarding logs - there is no going back");
            memcpy(globals_->digest_, digest, oasys::MD5::MD5LEN);
        } else {
            log_crit("datastore digest mismatch: "
                     "expected %s, database contains %s",
                     oasys::hex2str(digest, oasys::MD5::MD5LEN).c_str(),
                     oasys::hex2str(globals_->digest_, oasys::MD5::MD5LEN).c_str());
            log_crit("(implies serialized schema change)");
            return false;
        }
    }

    loaded_ = true;
    return true;
}

//----------------------------------------------------------------------
void
GlobalStore::update()
{
    ASSERT(lock_->is_locked_by_me());
    
    //log_debug("updating global store");

    // make certain we don't attempt to write out globals before
    // load() has had a chance to load them from the database
    ASSERT(loaded_);

    needs_update_ = true;
}

//----------------------------------------------------------------------
bool
GlobalStore::needs_update(Globals* globals_copy)
{
    bool result = false;

    oasys::ScopeLock l(lock_, "GlobalStore::needs_update");

    if ( needs_update_ )
    {
        needs_update_ = false;
        result = true;
    
        // TODO - move this into the Globals object    
        globals_copy->version_         = globals_->version_;
        globals_copy->next_bundleid_   = globals_->next_bundleid_;
        globals_copy->next_regid_      = globals_->next_regid_;
        globals_copy->last_custodyid_  = globals_->last_custodyid_;
        globals_copy->next_pacsid_     = globals_->next_pacsid_;
        memcpy(globals_copy->digest_, globals_->digest_, oasys::MD5::MD5LEN);
    }

    return result;
}

//----------------------------------------------------------------------
int
GlobalStore::update_database(Globals* globals_copy)
{
    int err = store_->put(oasys::StringShim(GLOBAL_KEY), globals_copy, 0);
    return err;
}

//----------------------------------------------------------------------
void
GlobalStore::close()
{
    // stop the update thread and issue one final db update
    GlobalStoreDBUpdateThread::instance()->set_should_stop();
    while ( ! GlobalStoreDBUpdateThread::instance()->is_stopped() )
    {
        usleep(2);
    }
    // we prevent the potential for shutdown race crashes by leaving
    // the global store locked after it's been closed so other threads
    // will simply block, not crash due to a null store
    lock_->lock("GlobalStore::close");
   
    // issue the one final database update 
    update_database(globals_);

    delete store_;
    store_ = NULL;

    delete instance_;
    instance_ = NULL;
}

//----------------------------------------------------------------------
void 
GlobalStore::lock_db_access(const char* lockedby )
{
  db_access_lock_.lock(lockedby);
}

//----------------------------------------------------------------------
void
GlobalStore::unlock_db_access()
{
  db_access_lock_.unlock();
}


//----------------------------------------------------------------------
void
GlobalStoreDBUpdateThread::run()
{
    char threadname[16] = "GlobalStorage";
    pthread_setname_np(pthread_self(), threadname);
   

    globals_copy_ = new Globals();

    uint32_t idle_count = 0;
    while ( !should_stop() )
    {
        if ( !BundleDaemonStorage::params_.db_storage_enabled_ ) {
            usleep(100000);
            continue;
        }

        if ( GlobalStore::instance()->needs_update(globals_copy_) )
        {
            idle_count = 0;
            update();
        }

        if (++idle_count >= 10) {
            usleep(1000);
        } else {
            usleep(100);
            //Thread::spin_yield();
        }
    }
}

//----------------------------------------------------------------------
void
GlobalStoreDBUpdateThread::update()
{
    //log_debug_p("/dtn/storage/globals/dbupdatethread", "updating global store");

    oasys::DurableStore *store = oasys::DurableStore::instance();

    GlobalStore::instance()->lock_db_access("GlobalStoreDBUpdateThread::update");

    store->begin_transaction();
    int err = GlobalStore::instance()->update_database(globals_copy_);

    store->end_transaction();

    GlobalStore::instance()->unlock_db_access();

    if (err != 0) {
        PANIC("GlobalStoreDBUpdateThread::update fatal error updating database: %s",
              oasys::durable_strerror(err));
    }
}

//----------------------------------------------------------------------
void
GlobalStoreDBUpdateThread::init()
{
    ASSERT(instance_ == NULL);
    instance_ = new GlobalStoreDBUpdateThread();
    instance_->start();
}

GlobalStoreDBUpdateThread* GlobalStoreDBUpdateThread::instance_ = NULL;

} // namespace dtn
