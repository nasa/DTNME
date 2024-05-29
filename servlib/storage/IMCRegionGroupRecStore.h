/*
 *    Copyright 2015 United States Government as represented by NASA
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


#ifndef _IMC_REGION_GROUP_REC_STORE_H_
#define _IMC_REGION_GROUP_REC_STORE_H_


#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/serialize/TypeShims.h>
#include <third_party/oasys/storage/InternalKeyDurableTable.h>
#include <third_party/oasys/util/Singleton.h>

namespace dtn {

class IMCRegionGroupRec;
class GlobalStore;

/**
 * Convenience typedef for the oasys adaptor that implements the IMCRegionGroupRec 
 * durable store.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::StringShim, std::string, IMCRegionGroupRec> IMCRegionGroupRecStoreImpl;

/**
 * The class for link storage.
 */
class IMCRegionGroupRecStore : public oasys::Singleton<IMCRegionGroupRecStore, false>,
                  public IMCRegionGroupRecStoreImpl {
public:
    /**
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store);
    
    /**
     * Constructor.
     */
    IMCRegionGroupRecStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance() != NULL); }

    /**
     * Lock to synchronize database access between BundleDaemonStorage and GlobalStore
     */
     void lock_db_access(const char* lockedby);

    /**
     * Release lock to synchronize database access
     */
    void unlock_db_access();

    /**
     * Begin a transaction in the underlying database system.
     * 
     * There can be at most one open transaction at a time.
     *
     * @param txid       Output of implementation handle for created transaction
     * @return
     */
    int begin_transaction(void **txid = nullptr);

    /*
     * End a transaction.
     *
     * There can be at most one open transaction at a time.
     */
    int end_transaction();


protected:
    /// Provide the DurableStore object to allow controlling transactions
    void set_durable_store(oasys::DurableStore* store);

protected:

    /// Pointer to the DurableStore object used to begin/end transactions
    oasys::DurableStore* durable_store_ = nullptr;

    /// Pointer to the GlobaStore object used to serialize access to the datastore
    GlobalStore* global_store_ = nullptr;

};

} // namespace dtn


#endif /* _IMC_REGION_GROUP_REC_STORE_H_ */
