/*
 *    Copyright 2011 Trinity College Dublin
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

#include <oasys/thread/SpinLock.h>

#include "BPQFragmentList.h"

namespace dtn {

//----------------------------------------------------------------------
BPQFragmentList::BPQFragmentList(const std::string& name, oasys::SpinLock* lock)
    : Logger("BPQFragmentList", "/dtn/bpq-frag/list/%s", name.c_str()),
      name_(name)
{
    if (lock != NULL) {
        lock_     = lock;
        own_lock_ = false;
    } else {
        lock_     = new oasys::SpinLock();
        own_lock_ = true;
    }
}

//----------------------------------------------------------------------
BPQFragmentList::~BPQFragmentList()
{
    clear();
    if (own_lock_) {
        delete lock_;
    }
    lock_ = NULL;
}

//----------------------------------------------------------------------
void
BPQFragmentList::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtn/bpq-frag/list/%s", name.c_str());
}

//----------------------------------------------------------------------
void
BPQFragmentList::insert_sorted(BPQFragment* new_fragment)
{
	oasys::ScopeLock l(lock_, "BPQFragmentList::insert_sorted");

    iterator iter;
    for (iter  = list_.begin();
    	 iter != list_.end();
    	 ++iter) {

    	if ((*iter)->offset() > new_fragment->offset()) {
			break;
		}
    }
    list_.insert(iter, new_fragment);
}

//----------------------------------------------------------------------
bool
BPQFragmentList::is_complete(size_t total_len) const
{
	oasys::ScopeLock l(lock_, "BPQFragmentList::is_complete");

	size_t gap_start = 0;
	size_t gap_end   = 0;

    const_iterator iter;
    for (iter  = list_.begin();
    	 iter != list_.end();
    	 ++iter) {

    	gap_end = (*iter)->offset();

    	if ( gap_end - gap_start != 0) {
    		return false;
    	}

    	gap_start = (*iter)->offset() + (*iter)->length();
    }

    gap_end = total_len;

	if ( gap_end - gap_start != 0) {
		return false;
	} else {
		return true;
	}
}

//----------------------------------------------------------------------
bool
BPQFragmentList::requires_fragment (
		size_t total_len,
		size_t frag_start,
		size_t frag_end) const
{
	oasys::ScopeLock l(lock_, "BPQFragmentList::requires_fragment");

	size_t gap_start = 0;
	size_t gap_end   = 0;

    const_iterator iter;
    for (iter  = list_.begin();
    	 iter != list_.end();
    	 ++iter) {

    	BPQFragment* fragment = *iter;
    	gap_end = fragment->offset();

    	if ( (gap_start < frag_start && frag_start < gap_end) ||
			 (gap_start < frag_end	 && frag_end   < gap_end) ) {
    		return true;
    	}

    	gap_start = fragment->offset() + fragment->length();
    }

    gap_end = total_len;

	if ( (gap_start < frag_start && frag_start < gap_end) ||
		 (gap_start < frag_end   && frag_end   < gap_end) ) {
		return true;
	} else {
		return false;
	}
}

//----------------------------------------------------------------------
void
BPQFragmentList::clear()
{
    oasys::ScopeLock l(lock_, "BPQFragmentList::clear");

    iterator iter;
    for (iter  = list_.begin();
    	 iter != list_.end();
    	 ++iter) {

    	delete *iter;
    }
}

} // namespace dtn

#endif /* BPQ_ENABLED */

