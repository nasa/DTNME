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

#ifndef __FRAGMENT_MANAGER_H__
#define __FRAGMENT_MANAGER_H__

#include <string>
#include <oasys/debug/Log.h>
#include <oasys/util/StringUtils.h>

namespace oasys {
class SpinLock;
}

namespace dtn {

class Link;
class Bundle;
class BundleList;
class BlockInfoVec;
class FragmentState;
class BlockInfoPointerList;

// XXX/demmer should change the overall flow of the reassembly so all
// arriving bundle fragments are enqueued onto the appropriate
// reassembly state list immediately -- in fact all the fragmentation
// and reassembly stuff needs rework

/**
 * The Fragment Manager maintains state for all of the fragmentary
 * bundles, reconstructing whole bundles from partial bundles.
 *
 * It also implements the routine for creating bundle fragments from
 * larger bundles.
 */
class FragmentManager : public oasys::Logger {
public:
    /**
     * Constructor.
     */
    FragmentManager();
    
    /**
     * Create a bundle fragment from another bundle.
     *
     * @param bundle
     *   the source bundle from which we create the
     *   fragment. Note: the bundle may itself be a fragment
     *
     * @param offset
     *   the offset relative to this bundle (not the
     *   original) for the for the new fragment. note that if this
     *   bundle is already a fragment, the offset into the original
     *   bundle will be this bundle's frag_offset + offset
     *
     * @param length
     *   the length of the fragment we want
     * 
     * @return
     *   pointer to the newly created bundle
     */
    Bundle* create_fragment(Bundle* bundle,
                            const BlockInfoVec &blocks,
                            const BlockInfoVec *xmit_blocks,
                            size_t offset,
                            size_t length,
                            bool first=false,
                            bool last=false);

    /**
     * Turn a bundle into a fragment. Note this is used just for
     * reactive fragmentation on a newly received partial bundle and
     * therefore the offset is implicitly zero (unless the bundle was
     * already a fragment).
     */
    void convert_to_fragment(Bundle* bundle, size_t length);

    /**
     * Given the given fragmentation threshold, determine whether the
     * given bundle should be split into several smaller bundles. If
     * so, this returns true and generates a bunch of bundle received
     * events for the individual fragments.
     *
     * Return the number of fragments created or zero if none were
     * created.
     */
    FragmentState* proactively_fragment(Bundle* bundle, 
                                        const LinkRef& link,
                                        size_t max_length);
    
    FragmentState* get_fragment_state(Bundle* bundle);
    
    void erase_fragment_state(FragmentState* fragment);

    /**
     * If only part of the given bundle was sent successfully, split
     * it into two. The original bundle
     *
     * Return true if a fragment was created
     */
    bool try_to_reactively_fragment(Bundle* bundle, BlockInfoVec *blocks,
                                    size_t bytes_sent);

    /**
     * Convert a partially received bundle into a fragment.
     *
     * Return true if a fragment was created
     */
    bool try_to_convert_to_fragment(Bundle* bundle);
    
    /**
     * Given a newly arrived bundle fragment, append it to the table
     * of fragments and see if it allows us to reassemble the bundle.
     *
     * If it does, a ReassemblyCompletedEvent will be posted.
     */
    void process_for_reassembly(Bundle* fragment);

    /**
     * Delete any fragments that are no longer needed given the incoming (non-fragment) bundle.
     */
    void delete_obsoleted_fragments(Bundle* fragment);

    /**
     * Delete reassembly state for a bundle.
     */
    void delete_fragment(Bundle* fragment);
    
 protected:
    /**
     * Calculate a hash table key from a bundle
     */
    void get_hash_key(const Bundle*, std::string* key);

    /**
     * Check if the bundle has been completely reassembled.
     */
    //bool check_completed(FragmentState* state);

    /// Table of partial bundles
    typedef oasys::StringHashMap<FragmentState*> FragmentTable;
    FragmentTable fragment_table_;
};

} // namespace dtn

#endif /* __FRAGMENT_MANAGER_H__ */
