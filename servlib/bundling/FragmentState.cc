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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "Bundle.h"
#include "FragmentState.h"

namespace dtn {

//----------------------------------------------------------------------
void
FragmentState::add_fragment(Bundle* fragment)
{
    fragments_.insert_sorted(fragment, BundleList::SORT_FRAG_OFFSET);
}

//----------------------------------------------------------------------
bool
FragmentState::erase_fragment(Bundle* fragment) 
{
    return fragments_.erase(fragment);
}

//----------------------------------------------------------------------
bool
FragmentState::check_completed() const
{
    Bundle* fragment;
    BundleList::iterator iter;
    oasys::ScopeLock l(fragments_.lock(),
                       "FragmentState::check_completed");
    
    size_t done_up_to = 0;  // running total of completed reassembly
    size_t f_len;
    size_t f_offset;
    size_t f_origlen;

    size_t total_len = bundle_->payload().length();
    
    int fragi = 0;
    int fragn = fragments_.size();
    (void)fragn; // in case NDEBUG is defined

    for (iter = fragments_.begin();
         iter != fragments_.end();
         ++iter, ++fragi)
    {
        fragment = *iter;

        f_len = fragment->payload().length();
        f_offset = fragment->frag_offset();
        f_origlen = fragment->orig_length();
        
        ASSERT(fragment->is_fragment());
        
        if (f_origlen != total_len) {
            PANIC("check_completed: error fragment orig len %zu != total %zu",
                  f_origlen, total_len);
            // XXX/demmer deal with this
        }

        if (done_up_to == f_offset) {
            /*
             * fragment is adjacent to the bytes so far
             * bbbbbbbbbb
             *           fff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(perfect fit)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            done_up_to += f_len;
        }

        else if (done_up_to < f_offset) {
            /*
             * there's a gap
             * bbbbbbb ffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(found a hole)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            return false;

        }

        else if (done_up_to > (f_offset + f_len)) {
            /* fragment is completely redundant, skip
             * bbbbbbbbbb
             *      fffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(redundant fragment)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            continue;
        }
        
        else if (done_up_to > f_offset) {
            /*
             * there's some overlap, so reduce f_len accordingly
             * bbbbbbbbbb
             *      fffffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(overlapping fragment, reducing len to %zu)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to,
                      (f_len - (done_up_to - f_offset)));
            
            f_len -= (done_up_to - f_offset);
            done_up_to += f_len;
        }

        else {
            // all cases should be covered above
            NOTREACHED;
        }
    }

    if (done_up_to == total_len) {
        log_debug("check_completed reassembly complete!");
        return true;
    } else {
        log_debug("check_completed reassembly not done (got %zu/%zu)",
                  done_up_to, total_len);
        return false;
    }
}

} // namespace dtn
    
