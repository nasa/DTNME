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

#ifndef _DTPC_DATA_PDU_COLLECTOR_STORE_H_
#define _DTPC_DATA_PDU_COLLECTOR_STORE_H_

#ifdef DTPC_ENABLED

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/storage/InternalKeyDurableTable.h>
#include <oasys/util/Singleton.h>

#include "DtpcDataPduCollector.h"

namespace dtn {

/**
 * Convenience typedef for the oasys adaptor that implements the DtpcDataPduCollector 
 * durable store.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::StringShim, std::string, DtpcDataPduCollector> DtpcDataPduCollectorStoreImpl;

/**
 * The class for DTPC DataPduCollector storage.
 */
class DtpcDataPduCollectorStore : public oasys::Singleton<DtpcDataPduCollectorStore, false>,
                                  public DtpcDataPduCollectorStoreImpl 
{
public:
    /**
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store) 
    {
        if (instance_ != NULL) {
            PANIC("DtpcDataPduCollectorStore::init called multiple times");
        }
        instance_ = new DtpcDataPduCollectorStore();
        return instance_->do_init(cfg, store);
    }
    
    /**
     * Constructor.
     */
    DtpcDataPduCollectorStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance() != NULL); }
};

} // namespace dtn

#endif // DTPC_ENABLED

#endif /* _DTPC_DATA_PDU_COLLECTOR_STORE_H_ */
