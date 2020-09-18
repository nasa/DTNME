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

#ifndef __BPQ_CACHE_ENTRY__
#define __BPQ_CACHE_ENTRY__

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include <oasys/debug/Log.h>

#include "Bundle.h"
#include "BundleRef.h"
#include "BundleList.h"

namespace dtn {

class Bundle;

class BPQCacheEntry : public oasys::Logger {
public:
	BPQCacheEntry(size_t len, BundleTimestamp ts, const EndpointID& eid) :
        Logger("BPQCacheEntry", "/dtn/bundle/bpq"),
        total_len_(len),
        creation_ts_(ts.seconds_, ts.seqno_),
        source_(eid),
        fragments_("cache_entry") {}



	/**
	 * Insert the fragment in sorted order
	 * @return  true if the new fragment completed the cache entry
     * 			false otherwise
	 */
	bool add_response(Bundle* bundle);

	/**
	 * As fragments may have different bundle ids and destinations
	 * when they are coming from the cache, choose the correct destination eid
	 * to deliver to.
	 */
	int reassemble_fragments(Bundle* new_bundle, Bundle* meta_bundle);

	/**
	 * Delete a bundle (which might be a fragment) from the cache entry
	 * if it actually exists - may not have been inserted
	 * @return true if bundle deleted, false otherwise
	 */
	bool remove_bundle(Bundle* bundle);

	/**
	 * Report if entry has any contents
	 * @return true if entry has no bundles, false otherwise
	 */
	bool is_empty(void);

	/**
	 * Report if entry contains a particular bundle
	 * @return true if entry has bundle, false otherwise
	 */
	bool is_bundle_in_entry(Bundle* bundle);

	bool 					is_complete()	const;
	bool 					is_fragmented()	const;
	size_t					entry_size() 	const;

	/// accessors
	size_t					total_len()			  {	return total_len_; }
	const BundleTimestamp& 	creation_ts()  	const { return creation_ts_; }
	const EndpointID& 		source()        const { return source_; }
	BundleList& 			fragment_list()       { return fragments_; }


private:
	size_t total_len_;				///< Complete payload size
    BundleTimestamp creation_ts_;	///< Original Creation Timestamp
    EndpointID source_;				///< Original Source EID
    BundleList fragments_;  		///< List of partial fragments
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* __BPQ_CACHE_ENTRY__ */
