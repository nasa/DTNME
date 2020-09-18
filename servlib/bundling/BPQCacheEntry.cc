/*
 *    Copyright 2010-2012 Trinity College Dublin
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

#ifdef BPQ_ENABLED

#include "BPQCacheEntry.h"
#include "BPQBlock.h"

namespace dtn {

bool
BPQCacheEntry::add_response(Bundle* bundle)
{
	if ( ! bundle->is_fragment() ) {
		log_debug("add complete response to cache entry");
	} else {
		log_debug("add response fragment to cache entry");
	}

	fragments_.insert_sorted(bundle, BundleList::SORT_FRAG_OFFSET);

	return is_complete();
}

//----------------------------------------------------------------------
int
BPQCacheEntry::reassemble_fragments(Bundle* new_bundle, Bundle* meta_bundle){

	log_debug("reassembling fragments for bundle id=%u", meta_bundle->bundleid());

	// copy metadata
	new_bundle->copy_metadata(meta_bundle);
	new_bundle->set_orig_length(meta_bundle->orig_length());
	new_bundle->set_frag_offset(0);

	// copy payload
	BundleList::iterator frag_iter;
	Bundle* current_fragment;

    for (frag_iter = fragments_.begin();
		 frag_iter != fragments_.end();
         ++frag_iter) {

    	current_fragment = *frag_iter;
    	size_t fraglen = current_fragment->payload().length();

    	new_bundle->mutable_payload()->write_data(	current_fragment->payload(),
    												0,
													fraglen,
													current_fragment->frag_offset());
    }

    // copy extension blocks
	BlockInfoVec::const_iterator block_iter;

    for (block_iter =  meta_bundle->recv_blocks().begin();
    	 block_iter != meta_bundle->recv_blocks().end();
         ++block_iter) {

    	if (! new_bundle->recv_blocks().has_block( block_iter->type() ) &&
    		block_iter->type() != BundleProtocol::PRIMARY_BLOCK         &&
			block_iter->type() != BundleProtocol::PAYLOAD_BLOCK) {

    		log_debug("Adding block(%d) to fragment bundle", block_iter->type());

    		new_bundle->mutable_recv_blocks()->push_back(BlockInfo(*block_iter));
		}
    }
	return BP_SUCCESS;
}

//----------------------------------------------------------------------
bool
BPQCacheEntry::is_complete() const
{
    Bundle* fragment;
    BundleList::iterator iter;
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::is_complete");

    size_t done_up_to = 0;  // running total of completed reassembly
    size_t f_len;
    size_t f_offset;
    size_t f_origlen;

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

        if (! fragment->is_fragment() )
        	return true;

        if (f_origlen != total_len_) {
            PANIC("check_completed: error fragment orig len %zu != total %zu",
                  f_origlen, total_len_);
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
    l.unlock();

    if (done_up_to == total_len_) {
        log_debug("check_completed reassembly complete!");
        return true;
    } else {
        log_debug("check_completed reassembly not done (got %zu/%zu)",
                  done_up_to, total_len_);
        return false;
    }
}

//----------------------------------------------------------------------

bool
BPQCacheEntry::remove_bundle(Bundle* bundle)
{
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::remove_bundle");
	if (fragments_.contains(bundle)) {
		return fragments_.erase(bundle);
	}
	return false;
}

//----------------------------------------------------------------------

bool
BPQCacheEntry::is_empty(void)
{
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::is_empty");
	return fragments_.empty();
}

//----------------------------------------------------------------------

bool
BPQCacheEntry::is_bundle_in_entry(Bundle* bundle)
{
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::is_bundle_in_entry");
	return fragments_.contains(bundle);
}

//----------------------------------------------------------------------

bool
BPQCacheEntry::is_fragmented() const
{
    Bundle* bundle;
    BundleList::iterator iter;
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::is_fragmented");

    for (iter = fragments_.begin();
         iter != fragments_.end();
         ++iter)
    {
        bundle = *iter;

        if (bundle->is_fragment()){
        	l.unlock();
        	return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
size_t
BPQCacheEntry::entry_size() const
{
	size_t size = 0;
    BundleList::iterator iter;
    oasys::ScopeLock l(fragments_.lock(),
                       "BPQCacheEntry::is_fragmented");

    for (iter = fragments_.begin();
         iter != fragments_.end();
         ++iter) {
        size += (*iter)->payload().length();
    }

    return size;
}

} // namespace dtn

#endif /* BPQ_ENABLED */
