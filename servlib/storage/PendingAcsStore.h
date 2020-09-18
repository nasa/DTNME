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


#ifndef _PENDING_ACS_STORE_H_
#define _PENDING_ACS_STORE_H_

#ifdef ACS_ENABLED

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/storage/InternalKeyDurableTable.h>
#include <oasys/util/Singleton.h>

namespace dtn {

class PendingAcs;

/**
 * Convenience typedef for the oasys adaptor that implements the PendingAcs 
 * durable store.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::StringShim, std::string, PendingAcs> PendingAcsStoreImpl;

/**
 * The class for link storage.
 */
class PendingAcsStore : public oasys::Singleton<PendingAcsStore, false>,
                  public PendingAcsStoreImpl {
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
    PendingAcsStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance() != NULL); }
};

} // namespace dtn

#endif // ACS_ENABLED

#endif /* _PENDING_ACS_STORE_H_ */
