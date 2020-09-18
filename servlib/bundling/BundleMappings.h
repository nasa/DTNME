/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _BUNDLE_MAPPING_H_
#define _BUNDLE_MAPPING_H_

#include <tr1/memory>
#include <vector>

#include "BundleListBase.h"

namespace dtn {

class BundleMapping;

// Shorthand for the shared pointers
typedef std::tr1::shared_ptr<void> SPV;
typedef std::tr1::shared_ptr<BundleMapping> SPBMapping;



/**
 * Structure stored in a list along with each bundle to keep a
 * "backpointer" to any bundle lists that the bundle is queued on to
 * make searching the lists more efficient.
 *
 * Relies on the fact that BundleList::iterator remains valid through
 * insertions and removals to other parts of the list.
 */
class BundleMapping {
public:
    BundleMapping() : list_(NULL), position_() {}
    
    BundleMapping(BundleListBase* list, SPV& position)
        : list_(list), position_(position) {}

    BundleListBase* list() const { return list_; }
    const SPV position()   const { return position_; }

protected:
    /// Pointer to the BundleList on which the bundle is held
    BundleListBase* list_;

    /// Position of the bundle in a BundleList list
    SPV position_;
};

/**
 * Class to define the set of mappings.
 *
 * Currently the code just uses a vector to store the mappings to make
 * it compact in memory and because the number of queues for each
 * bundle is likely small.
 */
class BundleMappings : public std::vector< SPBMapping > {
public:
    /**
     * Return an iterator at the mapping to the given list, or end()
     * if the mapping is not present.
     */
    iterator find(const BundleListBase* list);

    /**
     * Syntactic sugar for finding whether or not a mapping exists for
     * the given list.
     */
    bool contains(const BundleListBase* list) { return find(list) != end(); }
};

} // namespace dtn

#endif /* _BUNDLE_MAPPING_H_ */
