/*
 *    Copyright 2007-2008 Intel Corporation
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

#ifndef _BUNDLEINFOCACHE_H_
#define _BUNDLEINFOCACHE_H_

#include <oasys/debug/Logger.h>
#include <oasys/util/Cache.h>
#include <oasys/util/CacheCapacityHelper.h>
#include "Bundle.h"
#include "GbofId.h"

namespace dtn {

/**
 * Utility class for maintain a cache of recently received bundles,
 * indexed by GbofId. Used for routers to detect redundant BundleInfos
 * and to avoid duplicate deliveries to registrations.
 */
class BundleInfoCache {
public:
    /**
     * Constructor that takes the logpath and the number of entries to
     * maintain in the cache.
     */
    BundleInfoCache(const std::string& logpath, size_t capacity);
    
    /**
     * Try to add the bundle to the cache. If it already exists in the
     * cache, adding it again fails, and the method returns false.
     */
    bool add_entry(const Bundle* bundle, const EndpointID& prevhop);

    /**
     * Remove an entry from the cache.
     */
    void remove_entry(const Bundle* bundle);

    /**
     * Check if the given bundle is in the cache, returning the EID of
     * the node from which it arrived (if known).
     */
    bool lookup(const Bundle* bundle, EndpointID* prevhop);

    /**
     * Flush the cache.
     */
    void evict_all();

protected:
    typedef oasys::CacheCapacityHelper<std::string, EndpointID> CacheCapacityHelper;
    typedef oasys::Cache<std::string, EndpointID, CacheCapacityHelper> Cache;
    Cache cache_;
};

} // namespace dtn


#endif /* _BUNDLEINFOCACHE_H_ */
