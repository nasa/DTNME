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

#ifdef BPQ_ENABLED

#include "BPQCache.h"
#include "BPQCacheEntry.h"
#include "BPQBlock.h"
#include "BPQResponse.h"
#include "BPQCacheEntry.h"
#include "BundleDaemon.h"
//#include <openssl/sha.h>

namespace dtn {

//----------------------------------------------------------------------
bool  BPQCache::cache_enabled_	= false;
u_int BPQCache::max_cache_size_ = 1073741824;	// 1 GB

//----------------------------------------------------------------------
bool
BPQCache::add_response_bundle(Bundle* bundle, BPQBlock* block)
{
	ASSERT( block->kind() == BPQBlock::KIND_RESPONSE ||
			block->kind() == BPQBlock::KIND_RESPONSE_DO_NOT_CACHE_FRAG ||
			block->kind() == BPQBlock::KIND_PUBLISH );

	std::string key;
	get_hash_key(block, &key);

	oasys::ScopeLock l(&lock_, "BPQCache::add_reponse_bundle");

	Cache::iterator iter = bpq_table_.find(key);

	if ( iter == bpq_table_.end() ) {
		log_debug("no response found in cache, create new cache entry");

		return create_cache_entry(bundle, block, key);

	} else {
		log_debug("response found in cache");
		BPQCacheEntry* entry = iter->second;
		bool entry_complete = entry->is_complete();

		if ( entry_complete && ! bundle->is_fragment() ) {
			log_debug("cache complete & bundle complete: "
					  "accept the newer copy");

			if ( entry->creation_ts() < bundle->creation_ts() ){
				log_debug("received bundle is newer than cached one: "
						  "replace cache entry");

				return replace_cache_entry(entry, bundle, block, key);

			} else {
				log_debug("cached bundle is newer than received one: "
										  "do nothing");
				return false;
			}

		} else if ( entry_complete && bundle->is_fragment() ) {
			log_debug("cache complete & bundle incomplete: "
					  "not accepting new fragments");
			return false;

		} else if ( ! entry_complete && ! bundle->is_fragment() ) {
			log_debug("cache incomplete & bundle complete: "
					  "replace cache entry");

			replace_cache_entry(entry, bundle, block, key);
			return true;

		} else if ( ! entry_complete && bundle->is_fragment() ) {
			log_debug("cache incomplete & bundle incomplete: "
					  "append cache entry");

			entry_complete = append_cache_entry(entry, bundle, key);

			// if this completes the bundle and if it is destined for this node
			// if so, it should be reconstructed and delivered.
			if (entry_complete){
				try_to_deliver(entry);
			}

			return true;
		} else {
			NOTREACHED;
		}
	}
	return false;
}

//----------------------------------------------------------------------
bool
BPQCache::answer_query(Bundle* bundle, BPQBlock* block)
{
	ASSERT(block->kind() == BPQBlock::KIND_QUERY);

	// first see if the bundle exists
	std::string key;
	get_hash_key(block, &key);

	oasys::ScopeLock l1(&lock_, "BPQCache::answer_query");

	Cache::iterator cache_iter = bpq_table_.find(key);

	if ( cache_iter == bpq_table_.end() ) {
		log_debug("no response found in cache for query");
		return false;
	}

	log_debug("response found in cache");
	BPQCacheEntry* entry = cache_iter->second;
	EndpointID local_eid = BundleDaemon::instance()->local_eid();


	bool is_complete = false;
	Bundle* current_bundle;
	BundleList::iterator frag_iter;
	oasys::ScopeLock l2(entry->fragment_list().lock(), "BPQCache::answer_query");

	for (frag_iter  = entry->fragment_list().begin();
		 frag_iter != entry->fragment_list().end();
		 ++frag_iter) {

		current_bundle = *frag_iter;

		// if the current bundle is not a fragment
		// just return it and break out
		if ( ! current_bundle->is_fragment() ) {
			Bundle* new_response = new Bundle();
			BPQResponse::create_bpq_response(new_response,
											 bundle,
											 current_bundle,
											 local_eid);

			ASSERT(new_response->is_fragment() == current_bundle->is_fragment());

			BundleReceivedEvent* e = new BundleReceivedEvent(new_response, EVENTSRC_CACHE);
			BundleDaemon::instance()->post(e);

			is_complete = true;
			break;
		}

		size_t total_len = entry->total_len();
		size_t frag_off = current_bundle->frag_offset();
		size_t frag_len = current_bundle->payload().length();

		if ( block->fragments().requires_fragment(total_len, frag_off, frag_off + frag_len )) {
			Bundle* new_response = new Bundle();
			BPQResponse::create_bpq_response(new_response,
											 bundle,
											 current_bundle,
											 local_eid);

			ASSERT(new_response->is_fragment() == current_bundle->is_fragment());

			BundleReceivedEvent* e = new BundleReceivedEvent(new_response, EVENTSRC_CACHE);
			BundleDaemon::instance()->post(e);

			block->add_fragment(new BPQFragment(frag_off, frag_len));

			if (block->fragments().is_complete(total_len)) {
				is_complete = true;
				break;
			}
		}
	}
	l2.unlock();

	if ( is_complete ) {
		return true;
	} else {
		update_bpq_block(bundle, block);
		return false;
	}
}

//----------------------------------------------------------------------
void
BPQCache::check_and_remove(Bundle* bundle)
{
	/* Check if bundle has a BPQ block - nothing to do if it doesn't have */
	const BlockInfo* bi_bpq;
	BPQBlock* block;
    if( ((bi_bpq = bundle->recv_blocks().find_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) != NULL) ||
        ((bi_bpq = ((const_cast<Bundle*>(bundle))->api_blocks()->
                       find_block(BundleProtocol::QUERY_EXTENSION_BLOCK))) != NULL) ) {

        log_debug("bundle %d has BPQ block - removing from cache if present", bundle->bundleid());

        // now check if there is a cache entry for this key
        block = dynamic_cast<BPQBlock *>(bi_bpq->locals());
        ASSERT (block != NULL);
    	std::string key;
    	get_hash_key(block, &key);

    	oasys::ScopeLock l(&lock_, "BPQCache::check_and_remove");

    	Cache::iterator cache_iter = bpq_table_.find(key);

    	if ( cache_iter == bpq_table_.end() ) {
    		log_debug("no entry found in cache for query %s", key.c_str());
    		return;
    	}

    	// Remove bundle from cache entry and remove cache entry if no bundles left
    	if (cache_iter->second->remove_bundle(bundle)) {
    		cache_size_ -= bundle->payload().length();
    		if (cache_iter->second->is_empty()) {
    			log_debug("Deleting cache item key: %s", cache_iter->first.c_str());
    			int i = bpq_table_.erase(key);
    			log_debug("%d entries removed from bpq_table)", i);
    			lru_keys_.remove(key);
    		}
    	}
    }

}
//----------------------------------------------------------------------
bool
BPQCache::bundle_in_bpq_cache(Bundle* bundle)
{
	/* Check if bundle has a BPQ block - nothing to do if it doesn't have */
	const BlockInfo* bi_bpq;
	BPQBlock* block;
    if( ((bi_bpq = bundle->recv_blocks().find_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) != NULL) ||
        ((bi_bpq = ((const_cast<Bundle*>(bundle))->api_blocks()->
                       find_block(BundleProtocol::QUERY_EXTENSION_BLOCK))) != NULL) ) {

        log_debug("bundle %d has BPQ block - maybe prevent early expiry", bundle->bundleid());

        // now check if there is a cache entry for this key
        block = dynamic_cast<BPQBlock *>(bi_bpq->locals());
        ASSERT (block != NULL);
    	std::string key;
    	get_hash_key(block, &key);

    	oasys::ScopeLock l(&lock_, "BPQCache::bundle_in_bpq_cache");

    	Cache::iterator cache_iter = bpq_table_.find(key);

    	if ( cache_iter == bpq_table_.end() ) {
    		log_debug("no entry found in cache for query %s", key.c_str());
    		return false;
    	}

    	// There is a cache entry that matches this bundle BUT the bundle
    	// that is cached might not be this one as any generated response
    	// bundles will contain a BPQ block copied from the cached bundle
    	// so check if this is actually the cached bundle or another one.
    	return cache_iter->second->is_bundle_in_entry(bundle);
    }

    // Bundle doesn't have BPQ block so can't be in cache
    return false;
}

//----------------------------------------------------------------------
void
BPQCache::get_keys(oasys::StringBuffer* buf)
{

	oasys::ScopeLock l1(&lock_, "BPQCache::get_keys");

	buf->appendf("Currently cached keys:\n");

	for (Cache::iterator cache_iter = bpq_table_.begin();
			cache_iter != bpq_table_.end();
			cache_iter++) {
		buf->appendf("%s with bundle(s) ", cache_iter->first.c_str());
		BundleList& frags = cache_iter->second->fragment_list();
		BundleList::iterator frag_iter, frag_iter_first;
		oasys::ScopeLock l2(frags.lock(), "BPQCache::get_keys");

		frag_iter_first = frags.begin();

		for (frag_iter  = frag_iter_first;
			 frag_iter != frags.end();
			 ++frag_iter) {
			buf->appendf("%s%d", ((frag_iter == frag_iter_first) ? "" : ", "),
					             (*frag_iter)->bundleid());
		}
		buf->append("\n");
		l2.unlock();
	}
}

//----------------------------------------------------------------------
void
BPQCache::get_lru_list(oasys::StringBuffer* buf)
{

	oasys::ScopeLock l(&lock_, "BPQCache::get_lru_list");

	buf->appendf("Current LRU list (most recent first):\n");

	for (std::list<std::string>::iterator lru_iter = lru_keys_.begin();
			lru_iter != lru_keys_.end();
			lru_iter++) {
		buf->appendf("%s\n", lru_iter->c_str());
	}
}

//----------------------------------------------------------------------
bool
BPQCache::create_cache_entry(Bundle* bundle, BPQBlock* block, std::string key)
{
	if (max_cache_size_ < bundle->payload().length()) {
		log_warn("bundle too large to add to cache {max cache size: %u, bundle size: %zu}",
				max_cache_size_, bundle->payload().length());

		return false;
	}

	if ( bundle->is_fragment() ) {
		log_debug("creating new cache entry for bundle fragment "
				  "{key: %s, offset: %u, length: %zu}",
				  key.c_str(), bundle->frag_offset(),
				  bundle->payload().length());
	} else {
		log_debug("creating new cache entry for complete bundle "
				  "{key: %s, length: %zu}",
				  key.c_str(), bundle->payload().length());
	}

	// Step 1: 	No in-network reassembly
	//			State bundle only contains metadata
	//			The fragment list contains all the payload data

	BPQCacheEntry* entry = new BPQCacheEntry(bundle->orig_length(),
											 block->creation_ts(),
											 block->source());

	entry->add_response(bundle);

	bpq_table_[key] = entry;
	cache_size_ += entry->entry_size();
	update_lru_keys(key);

	return true;
}

//----------------------------------------------------------------------
bool
BPQCache::replace_cache_entry(BPQCacheEntry* entry, Bundle* bundle,
							  BPQBlock* block, std::string key)
{
	ASSERT ( ! bundle->is_fragment() );
	log_debug("Remove existing cache entry");

	remove_cache_entry(entry, key);

	log_debug("Create new cache entry");
	return create_cache_entry(bundle, block, key);
}

//----------------------------------------------------------------------
void
BPQCache::remove_cache_entry(BPQCacheEntry* entry, std::string key)
{
	oasys::ScopeLock l1(&lock_, "BPQCache::remove_cache_entry");
	oasys::ScopeLock l2(entry->fragment_list().lock(),
						   "BPQCache::remove_cache_entry");

	log_debug("remove_cache_entry called for key %s", key.c_str());

	cache_size_ -= entry->entry_size();
	while (! entry->fragment_list().empty()) {
		BundleDaemon::post(
			new BundleDeleteRequest(entry->fragment_list().pop_back(),
			BundleProtocol::REASON_NO_ADDTL_INFO) );
	}

	ASSERT(entry->fragment_list().size() == 0);
	l2.unlock();

	bpq_table_.erase(key);
	lru_keys_.remove(key);
}
//----------------------------------------------------------------------
bool
BPQCache::append_cache_entry(BPQCacheEntry* entry, Bundle* bundle, std::string key)
{
	ASSERT( bundle->is_fragment() );

	if (max_cache_size_ < bundle->payload().length()) {
		log_warn("bundle too large to add to cache {max cache size: %u, bundle size: %zu}",
				max_cache_size_, bundle->payload().length());

		return false;
	}

	log_debug("appending received bundle fragment to cache {offset: %u, length: %zu}",
			  bundle->frag_offset(), bundle->payload().length());

	cache_size_ += bundle->payload().length();
	bool is_complete = entry->add_response(bundle);
	update_lru_keys(key);


	if ( is_complete ) {
		log_info("appending received bundle completed cache copy "
				"{number of frags: %zu}", entry->fragment_list().size());

	} else {
		log_debug("appending received bundle has not completed cache copy "
				"{number of frags: %zu}", entry->fragment_list().size());
	}

	return is_complete;
}

//----------------------------------------------------------------------
int
BPQCache::update_bpq_block(Bundle* bundle, BPQBlock* block)
{
	BlockInfo* block_info = NULL;

    if( bundle->recv_blocks().
        has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {

    	block_info = const_cast<BlockInfo*>
        			 (bundle->recv_blocks().find_block(
        					 BundleProtocol::QUERY_EXTENSION_BLOCK));

    } else if( bundle->api_blocks()->
               has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {

    	block_info = const_cast<BlockInfo*>
    	        	 (bundle->api_blocks()->find_block(
    	        			 BundleProtocol::QUERY_EXTENSION_BLOCK));

    } else {
        log_err("BPQ Block not found in bundle");
        NOTREACHED;
        return BP_FAIL;
    }

    ASSERT (block != NULL);

    u_int32_t new_len = block->length();
    block_info->set_data_length(new_len);

    BlockInfo::DataBuffer* contents = block_info->writable_contents();
	contents->reserve(block_info->data_offset() + new_len);
	contents->set_len(block_info->data_offset() + new_len);

	// Set our pointer to the right offset.
	u_char* buf = contents->buf() + block_info->data_offset();

    // now write contents of BPQ block into the block
    if ( block->write_to_buffer(buf, new_len) == -1 ) {
        log_err("Error writing BPQ block to buffer");
        return BP_FAIL;
    }

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
bool
BPQCache::try_to_deliver(BPQCacheEntry* entry)
{
	if (!entry->is_complete())
		return false;

	BundleList::iterator frag_iter;
	Bundle* current_fragment;
	const RegistrationTable* reg_table = BundleDaemon::instance()->reg_table();

	RegistrationList::iterator reg_iter;


	oasys::ScopeLock l(entry->fragment_list().lock(), "BPQCache::try_to_deliver");

	for (frag_iter  = entry->fragment_list().begin();
		 frag_iter != entry->fragment_list().end();
		 ++frag_iter) {

		current_fragment = *frag_iter;
		RegistrationList reg_list;

		int mathces = reg_table->get_matching(current_fragment->dest(), &reg_list);

		if (mathces > 0) {
			Bundle* new_bundle = new Bundle();
			entry->reassemble_fragments(new_bundle, current_fragment);

			BundleReceivedEvent* e = new BundleReceivedEvent(new_bundle, EVENTSRC_CACHE);
			BundleDaemon::instance()->post(e);
		}
	}

	l.unlock();

	return false;
}

//----------------------------------------------------------------------
void
BPQCache::update_lru_keys(std::string key)
{
	lru_keys_.remove(key);
	lru_keys_.push_front(key);

	while (cache_size_ > BPQCache::max_cache_size_) {
		std::string lru = lru_keys_.back();

		// don't remove the key that's being updated
		if (lru == key)
			break;

		Cache::iterator cache_iter = bpq_table_.find(lru);

		if ( cache_iter != bpq_table_.end() ) {
			remove_cache_entry( cache_iter->second, lru );
		}

		lru_keys_.pop_back();
	}
}

//----------------------------------------------------------------------
void
BPQCache::get_hash_key(Bundle* bundle, std::string* key)
{
    BPQBlock block(bundle);
    get_hash_key(&block, key);
}

//----------------------------------------------------------------------
void
BPQCache::get_hash_key(BPQBlock* block, std::string* key)
{
//    u_char hash[SHA256_DIGEST_LENGTH];
//    char buf[3];
    key->clear();

    // concatenate matching rule and query value
    std::string input;
    char matching_rule = (char)block->matching_rule();
    input.append(&matching_rule);
    input.append((char*)block->query_val());

    key->append(input);

//    SHA256_CTX sha256;
//    SHA256_Init(&sha256);
//    SHA256_Update(&sha256, input.c_str(), input.length());
//    SHA256_Final(hash, &sha256);
//
//    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
//    {
//        snprintf(buf, 2, "%02x", hash[i]);
//        key->append(buf);
//    }
}

} // namespace dtn

#endif /* BPQ_ENABLED */
