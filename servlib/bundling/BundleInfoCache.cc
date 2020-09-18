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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleInfoCache.h"
#include "BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
BundleInfoCache::BundleInfoCache(const std::string& logpath, size_t capacity)
    : cache_(logpath, CacheCapacityHelper(capacity),
             false /* no reorder on get() */ )
{
}

//----------------------------------------------------------------------
bool
BundleInfoCache::add_entry(const Bundle* bundle, const EndpointID& prevhop)
{
    Cache::Handle h;
    std::string id = bundle->gbofid_str();
    bool ok = cache_.put_and_pin(id, prevhop, &h);
    if (!ok) {
        return false;
    }
    
    h.unpin();

    return true;
}

//----------------------------------------------------------------------
void
BundleInfoCache::remove_entry(const Bundle* bundle)
{
    cache_.evict(bundle->gbofid_str());
}

//----------------------------------------------------------------------
bool
BundleInfoCache::lookup(const Bundle* bundle, EndpointID* prevhop)
{
    return cache_.get(bundle->gbofid_str(), prevhop);
}

//----------------------------------------------------------------------
void
BundleInfoCache::evict_all()
{
    cache_.evict_all();
}

} // namespace dtn
