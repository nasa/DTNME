/*
 *    Copyright 2010-2011 Trinity College Dublin
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

#ifndef __BPQ_CACHE__
#define __BPQ_CACHE__

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include "Bundle.h"
#include <oasys/debug/Log.h>
#include <oasys/util/StringUtils.h>
#include <oasys/thread/SpinLock.h>
#include "../reg/Registration.h"
#include "../reg/RegistrationTable.h"


namespace dtn {

class BPQBlock;
class BPQCacheEntry;
class EndpointID;
class BPQResponse;
class RegistrationList;
class RegistrationTable;

class BPQCache : public oasys::Logger {
public:
	BPQCache() :
        Logger("BPQCache", "/dtn/bundle/bpq"),
        cache_size_(0) {}

	/**
	 * Add a new BPQ response to the to the cache
	 * @return true if entry successfully added to cache,
	 *         false otherwise.
	 */
    bool add_response_bundle(Bundle* bundle, BPQBlock* block);

    /**
     * Try to answer a BPQ query with a response in the cache
     * @return  true if the query was successfully answered in full
     * 			false otherwise
     */
    bool answer_query(Bundle* bundle, BPQBlock* block);

    /**
     * Check if a bundle is in the cache and remove it if so
     * Delete the cache entry altogether if no relevant bundle (fragments) left.
     */
    void check_and_remove(Bundle* bundle);

    /**
     * Check if a bundle is in the cache
     * @return true if in cache, false otherwise
     */
   bool bundle_in_bpq_cache(Bundle* bundle);

    /**
     * Display keys in cache
     */
    void get_keys(oasys::StringBuffer* buf);

    /**
     * Display ordered list of keys in LRU list
     */
    void get_lru_list(oasys::StringBuffer* buf);

    /**
     * Number of bundles in the cache
     */
    size_t size() { return bpq_table_.size();}

    static 			bool  cache_enabled_;
    static 			u_int max_cache_size_;
    static const 	u_int MAX_KEY_SIZE = 4096;

protected:

    /**
     * Build a new BPQCcacheEntry from this bundle.
     * Copy the bundle into the fragment list
     * @return  true if the new cache entry was created
     *   		false otherwise (eg if bundle is larger than cache)
     */
    bool create_cache_entry(Bundle* bundle, BPQBlock* block, std::string key);

    /**
	 * Remove existing cache entry along with all bundle fragments
	 * and create a new entry
     * @return  true if the new cache entry was created
     *   		false otherwise (eg if bundle is larger than cache)
	 */
    bool replace_cache_entry(BPQCacheEntry* entry, Bundle* bundle,
    						 BPQBlock* block, std::string key);

    void remove_cache_entry(BPQCacheEntry* entry, std::string key);
    /**
     * Add received bundle fragment to the cache entry
     * @return  true if the new fragment completed the cache entry
     * 			false otherwise
     */
    bool append_cache_entry(BPQCacheEntry* entry, Bundle* bundle, std::string key);



    bool bpq_requires_fragment(BPQBlock* block, Bundle* fragment);
    int  update_bpq_block(Bundle* bundle, BPQBlock* block);
    bool try_to_deliver(BPQCacheEntry* entry);

    void update_lru_keys(std::string key);

    /**
     * Calculate a hash table key from a bundle
     * This is a  SHA256 hash of the concatenation of:
     * 		Matching Rule and Query Value
     */
    void get_hash_key(Bundle* bundle, std::string* key);
    void get_hash_key(BPQBlock* block, std::string* key);


    /**
     * Table of partial BPQ bundles
     */
    typedef oasys::StringHashMap<BPQCacheEntry*> Cache;
    Cache bpq_table_;

    /**
     * Lock for bpq_table_
     */
    oasys::SpinLock lock_;

    std::list<std::string> lru_keys_;
    size_t cache_size_;
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* __BPQ_CACHE__ */
