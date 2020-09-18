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

#ifndef _GLOBAL_STORE_H_
#define _GLOBAL_STORE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>

// forward decl
namespace oasys {
template<typename _Type> class SingleTypeDurableTable;
class DurableStore;
class Mutex;
struct StorageConfig;
}

namespace dtn {

class Globals;

/**
 * Class for those elements of the router that need to be persistently
 * stored but are singleton global values. Examples include the
 * running sequence number for bundles and registrations, as well as
 * any other persistent configuration settings.
 */
class GlobalStore : public oasys::Logger {
public:
    static const u_int32_t CURRENT_VERSION;
    
    /**
     * Singleton instance accessor.
     */
    static GlobalStore* instance() {
        if (instance_ == NULL) {
            PANIC("GlobalStore::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer
     *
     */
    static int init(const oasys::StorageConfig& cfg, 
                    oasys::DurableStore*        store);

    /**
     * Constructor.
     */
    GlobalStore();

    /**
     * Real initialization method.
     */
    int do_init(const oasys::StorageConfig& cfg, 
                oasys::DurableStore*        store);
    
    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Destructor.
     */
    ~GlobalStore();

    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    bundleid_t next_bundleid();
    
    /**
     * Get a new unique registration id, updating the running value in
     * the persistent table.
     *
     * (was db_new_regID, db_update_registration_id,
     * db_retable_registration_id)
     */
    u_int32_t next_regid();

    /**
     * Get a new unique Pending ACS ID
     */
    u_int32_t next_pacsid();
    
    /**
     * Get a new Custody ID
     */
    bundleid_t next_custodyid();

    /**
     * Get the last custody id
     */
    bundleid_t last_custodyid();
    
    /**
     * Load in the globals.
     */
    bool load();

    /**
     * Close (and flush) the data store.
     */
    void close();

    /**
     * Lock to synchronize database access between BundleDaemonStorage and GlobalStore
     */
     void lock_db_access(const char* lockedby);

    /**
     * Release lock to synchronize database access
     */
    void unlock_db_access();

    /**
     * Indicates if a database update is needed and makes a snapshot
     * of the current Globals to store in the database
     */
    bool needs_update(Globals* globals_copy);

    /**
     * Makes the actual update to the database and returns a status code
     */
    int update_database(Globals* globals_copy);

protected:
    /**
     * Update the globals in the store.
     */
    void update();

    /**
     * Calculate a digest of on-disk serialized objects.
     */
    void calc_digest(u_char* digest);

    bool loaded_;
    Globals* globals_;
    oasys::SingleTypeDurableTable<Globals>* store_;

    oasys::Mutex* lock_;

    /// flag indicating if a database update is needed
    bool needs_update_;

    /// control db access between the BundleDaemonStorage and the GlobalStore
    mutable oasys::SpinLock db_access_lock_;

    static GlobalStore* instance_; ///< singleton instance
};


/**
 * A simple thread class that drives the TimerSystem implementation.
 */
class GlobalStoreDBUpdateThread : public oasys::Thread {
public:
    static void init();
    static GlobalStoreDBUpdateThread* instance()
    {
        return instance_;
    }

private:
    GlobalStoreDBUpdateThread() : Thread("GlobalStoreDBUpdateThread") {}
    void run();
    void update();

    Globals* globals_copy_;    
    static GlobalStoreDBUpdateThread* instance_;
};

} // namespace dtn

#endif /* _GLOBAL_STORE_H_ */
