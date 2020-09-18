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

#ifndef __FRAGMENT_STATE_H__
#define __FRAGMENT_STATE_H__

#include <oasys/debug/Log.h>

#include "BundleRef.h"
#include "BundleList.h"

namespace dtn {

class Bundle;
class BlockInfoPointerList;

class FragmentState : public oasys::Logger {
public:
    FragmentState() : 
        Logger("FragmentState", "/dtn/bundle/fragmentation"),
        bundle_(new Bundle(), "fragment_state"), 
        fragments_("fragment_state") {}
    
    FragmentState(Bundle* bundle) :
        Logger("FragmentState", "/dtn/bundle/fragmentation"),
        bundle_(bundle, "fragment_state"), 
        fragments_("fragment_state") { }
    
    void add_fragment(Bundle* fragment);
    bool erase_fragment(Bundle* fragment);
    bool check_completed() const;
    size_t num_fragments() const { return fragments_.size(); }
    BundleRef& bundle() { return bundle_; }
    BundleList& fragment_list() { return fragments_; }
    
private:
    BundleRef  bundle_; ///< The bundle to eb 
    BundleList fragments_;  ///< List of partial fragments
};

} // namespace dtn

#endif
