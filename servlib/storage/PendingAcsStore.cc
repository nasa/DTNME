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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef ACS_ENABLED

#include "PendingAcsStore.h"
#include "bundling/AggregateCustodySignal.h"

#include <oasys/storage/DurableStore.h>

namespace oasys {
    template<> dtn::PendingAcsStore* oasys::Singleton<dtn::PendingAcsStore, false>::instance_ = NULL;
}


namespace dtn {

PendingAcsStore::PendingAcsStore()
    : PendingAcsStoreImpl("PendingAcsStore", "/dtn/storage/pendingacs", 
                          "pendingacs", "pendingacs")
{
}

//----------------------------------------------------------------------
int
PendingAcsStore::init(const oasys::StorageConfig& cfg,
                  oasys::DurableStore*    store)
{
    if (instance_ != NULL) {
        PANIC("PendingAcsStore::init called multiple times");
    }
    instance_ = new PendingAcsStore();
    return instance_->do_init(cfg, store);
}

    

} // namespace dtn

#endif // ACS_ENABLED
