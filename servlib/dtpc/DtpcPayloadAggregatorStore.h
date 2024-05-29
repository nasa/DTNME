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

#ifndef _DTPC_PAYLOAD_AGGREGATOR_STORE_H_
#define _DTPC_PAYLOAD_AGGREGATOR_STORE_H_

#ifdef DTPC_ENABLED

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/serialize/TypeShims.h>
#include <third_party/oasys/storage/InternalKeyDurableTable.h>
#include <third_party/oasys/util/Singleton.h>

#include "DtpcPayloadAggregator.h"

namespace dtn {

/**
 * Convenience typedef for the oasys adaptor that implements the DtpcPayloadAggregator 
 * durable store.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::StringShim, std::string, DtpcPayloadAggregator> DtpcPayloadAggregatorStoreImpl;

/**
 * The class for DTPC PayloadAggregator storage.
 */
class DtpcPayloadAggregatorStore : public oasys::Singleton<DtpcPayloadAggregatorStore, false>,
                  public DtpcPayloadAggregatorStoreImpl {
public:
    /**
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store) ;
    
    /**
     * Constructor.
     */
    DtpcPayloadAggregatorStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance() != nullptr); }
};

} // namespace dtn

#endif // DTPC_ENABLED

#endif /* _DTPC_PAYLOAD_AGGREGATOR_STORE_H_ */
