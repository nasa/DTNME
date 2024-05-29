/*
 *    Copyright 2005-2006 Intel Corporation
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


#ifndef __OASYS_DURABLE_STORE_INTERNAL_HEADER__
#error DurableObjectCache.h must only be included from within DurableStore.h
#endif

//----------------------------------------------------------------------------
template <typename _DataType>
DurableObjectCache<_DataType>::DurableObjectCache(const char*   logpath,
                                                  size_t        capacity,
                                                  CachePolicy_t policy)
    : Logger("DurableObjectCache", logpath),
      size_(0), 
      capacity_(capacity),
      hits_(0), 
      misses_(0), 
      evictions_(0), 
      lock_(new SpinLock()),
      policy_(policy)
{
    log_debug("init capacity=%zu", capacity);
}

//----------------------------------------------------------------------------
template <typename _DataType>
DurableObjectCache<_DataType>::~DurableObjectCache()
{
    delete(lock_);
    lock_ = 0;
}

//----------------------------------------------------------------------------
template <typename _DataType>
void
DurableObjectCache<_DataType>::get_stats(StringBuffer* buf)
{
    buf->appendf("%zu bytes -- "
                 "%zu elements -- "
                 "%zu live -- "
                 "%u hits -- "
                 "%u misses -- "
                 "%u evictions",
                 size(),
                 count(),
                 live(),
                 hits(),
                 misses(),
                 evictions());
}
                                             
//----------------------------------------------------------------------------
template <typename _DataType>
void
DurableObjectCache<_DataType>::get_cache_key(std::string* cache_key,
                                             const SerializableObject& key)
{
    StringSerialize serialize(Serialize::CONTEXT_LOCAL,
                              StringSerialize::DOT_SEPARATED);
    if (serialize.action(&key) != 0) {
        PANIC("error sizing key");
    }

    cache_key->assign(serialize.buf().data(), serialize.buf().length());
}

//----------------------------------------------------------------------------
template <typename _DataType>
bool 
DurableObjectCache<_DataType>::is_over_capacity(size_t size)
{
    switch (policy_) 
    {
    case CAP_BY_SIZE:
        return (size_ + size) > capacity_;
    case CAP_BY_COUNT:
        return (count() + 1) > capacity_;
    }
    
    NOTREACHED;
}

//----------------------------------------------------------------------------
template <typename _DataType>
void
DurableObjectCache<_DataType>::evict_last()
{
    typename CacheTable::iterator cache_iter;
    CacheLRUList::iterator lru_iter;

    ASSERT(lock_->is_locked_by_me());

    ASSERT(!lru_.empty());
    
    lru_iter = lru_.begin();
    ASSERT(lru_iter != lru_.end());

    cache_iter = cache_.find(*lru_iter);
    ASSERT(cache_iter != cache_.end());

    CacheElement* cache_elem = cache_iter->second;
    ASSERT(cache_elem->object_ != NULL);
    ASSERT(cache_elem->lru_iter_ == lru_iter);
    ASSERT(!cache_elem->live_);

    log_debug("cache (capacity %zu/%zu) -- "
              "evicting key '%s' object %p size %zu",
              size_, capacity_, lru_iter->c_str(),
              cache_elem->object_, cache_elem->object_size_);
    
    cache_.erase(cache_iter);
    lru_.pop_front();

    size_ -= cache_elem->object_size_;
    
    evictions_++;
    
    delete cache_elem->object_;
    delete cache_elem;
}

//----------------------------------------------------------------------------
template <typename _DataType>
int
DurableObjectCache<_DataType>::put(const SerializableObject& key,
                                   const _DataType* object,
                                   int flags)
{
    ScopeLock l(lock_, "DurableObjectCache::put");
    
    CacheElement* cache_elem;
    
    typename CacheTable::iterator cache_iter;
    CacheLRUList::iterator lru_iter;
    
    std::string cache_key;
    get_cache_key(&cache_key, key);

    // first check if the object exists in the cache
    cache_iter = cache_.find(cache_key);
    
    if (cache_iter != cache_.end()) {
        cache_elem = cache_iter->second;

        if (flags & DS_EXCL) {
            log_debug("put(%s): object already exists and DS_EXCL set",
                      cache_key.c_str());
            return DS_EXISTS;
        }
        
        if (cache_elem->object_ == object) {
            log_debug("put(%s): object already exists", cache_key.c_str());
            return DS_OK;

        } else {
            PANIC("put(%s): cannot handle different objects %p %p for same key",
                  cache_key.c_str(), object, cache_elem->object_);
        }
    }

    // note that we ignore the DS_CREATE flag since and always create
    // the item (the underlying data store should have already ensured
    // that the item doesn't exist if the DS_CREATE flag isn't set)
    
    // figure out the size of the new object
    MarshalSize sizer(Serialize::CONTEXT_LOCAL);
    if (sizer.action(object) != 0) {
        PANIC("error in MarshalSize");
    }
    size_t object_size = sizer.size();

    log_debug("put(%s): object %p size %zu",
              cache_key.c_str(), object, object_size);

    // now try to evict elements if the new object will put us over
    // the cache capacity
    while (is_over_capacity(object_size)) 
    {
        if (lru_.empty()) 
        {
            switch (policy_) {
            case CAP_BY_SIZE:
                log_warn("cache already at capacity "
                         "(size %zu, object_size %zu, capacity %zu) "
                         "but all %zu elements are live",
                         size_, object_size, capacity_, cache_.size());
                break;
            case CAP_BY_COUNT:
                log_warn("cache already at capacity "
                         "(count %zu, capacity %zu) "
                         "but all %zu elements are live",
                         count(), capacity_, cache_.size());
                break;
            default:
                NOTREACHED;
            }
            break;
        }

        evict_last();
    }

    // now cons up an element linking to the object and put it in the
    // cache, but not the LRU list since the object is assumed to be
    // live
    cache_elem = new CacheElement(object, object_size, true, lru_iter);
    typename CacheTable::value_type val(cache_key, cache_elem);
    CacheInsertRet ret = cache_.insert(val);

    ASSERT(ret.second == true);
    ASSERT(cache_.find(cache_key) != cache_.end());

    size_ += object_size;

    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _DataType>
int
DurableObjectCache<_DataType>::get(const SerializableObject& key,
                                   _DataType** objectp)
{
    ScopeLock l(lock_, "DurableObjectCache::get");

    std::string cache_key;
    get_cache_key(&cache_key, key);

    typename CacheTable::iterator cache_iter = cache_.find(cache_key);
    
    if (cache_iter == cache_.end()) {
        log_debug("get(%s): no match", cache_key.c_str());
        ++misses_;
        return DS_NOTFOUND;
    } 

    ++hits_;

    CacheElement* cache_elem = cache_iter->second;

    if (! cache_elem->live_) {
        cache_elem->live_ = true;
        CacheLRUList::iterator lru_iter = cache_elem->lru_iter_;
        ASSERT(lru_iter != lru_.end());
        ASSERT(*lru_iter == cache_key);
        lru_.erase(lru_iter);
    }
    
    *objectp = const_cast<_DataType*>(cache_elem->object_);
    log_debug("get(%s): match %p", cache_key.c_str(), *objectp);

    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _DataType>
bool
DurableObjectCache<_DataType>::is_live(const SerializableObject& key)
{
    ScopeLock l(lock_, "DurableObjectCache::is_live");
    
    std::string cache_key;
    get_cache_key(&cache_key, key);

    typename CacheTable::iterator cache_iter = cache_.find(cache_key);
    
    if (cache_iter == cache_.end()) {
        log_debug("is_live(%s): no element", cache_key.c_str());
        return false;
    } 

    CacheElement* cache_elem = cache_iter->second;
    if (cache_elem->live_) {
        log_debug("is_live(%s): live", cache_key.c_str());
        return true;
    } else {
        log_debug("is_live(%s): not live", cache_key.c_str());
        return false;
    }
}

//----------------------------------------------------------------------------
template <typename _DataType>
int
DurableObjectCache<_DataType>::release(const SerializableObject& key,
                                       const _DataType* data)
{
    ScopeLock l(lock_, "DurableObjectCache::release");

    std::string cache_key;
    get_cache_key(&cache_key, key);
    
    typename CacheTable::iterator cache_iter = cache_.find(cache_key);

    if (cache_iter == cache_.end()) {
        log_err("release(%s): no match for object %p",
                cache_key.c_str(), data);
        return DS_ERR;
    }

    CacheElement* cache_elem = cache_iter->second;
    ASSERT(cache_elem->object_ == data);

    if (! cache_elem->live_) {
        log_err("release(%s): release object %p already on LRU list!!",
                cache_key.c_str(), data);
	Breaker::break_here();
        lru_.move_to_back(cache_elem->lru_iter_);
    } else {
        log_debug("release(%s): release object %p", cache_key.c_str(), data);
        lru_.push_back(cache_key);
        cache_elem->live_ = false;
        cache_elem->lru_iter_ = --lru_.end();
        ASSERT(*cache_elem->lru_iter_ == cache_key);
    }

    if (is_over_capacity(0)) 
    {
        log_debug("release while over capacity, evicting stale object");
        // ASSERT(lru_.size() == 1);
        evict_last();
    }
    
    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _DataType>
int
DurableObjectCache<_DataType>::del(const SerializableObject& key)
{
    ScopeLock l(lock_, "DurableObjectCache::del");

    std::string cache_key;
    get_cache_key(&cache_key, key);
    
    typename CacheTable::iterator cache_iter = cache_.find(cache_key);

    if (cache_iter == cache_.end()) {
        log_debug("del(%s): no match for key", cache_key.c_str());
        return DS_NOTFOUND;
    }
    
    CacheElement* cache_elem = cache_iter->second;
    
    if (cache_elem->live_) {
        log_err("del(%s): can't remove live object %p size %zu from cache",
                cache_key.c_str(), cache_elem->object_,
                cache_elem->object_size_);
        return DS_ERR;
        
    } else {
        lru_.erase(cache_elem->lru_iter_);
        log_debug("del(%s): removing non-live object %p size %zu from cache",
                  cache_key.c_str(), cache_elem->object_,
                  cache_elem->object_size_);
    }
    
    cache_.erase(cache_iter);
    size_ -= cache_elem->object_size_;

    delete cache_elem->object_;
    delete cache_elem;
    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _DataType>
int
DurableObjectCache<_DataType>::remove(const SerializableObject& key,
                                      const _DataType* data)
{
    (void)data;
    
    ScopeLock l(lock_, "DurableObjectCache::remove");

    std::string cache_key;
    get_cache_key(&cache_key, key);
    
    typename CacheTable::iterator cache_iter = cache_.find(cache_key);

    if (cache_iter == cache_.end()) {
        log_debug("remove(%s): no match for key", cache_key.c_str());
        return DS_NOTFOUND;
    }
    
    CacheElement* cache_elem = cache_iter->second;
    
    if (cache_elem->live_) {
        log_debug("del(%s): removing live object %p size %zu from cache",
                  cache_key.c_str(), cache_elem->object_,
                  cache_elem->object_size_);
        
    } else {
        lru_.erase(cache_elem->lru_iter_);
        log_debug("del(%s): removing non-live object %p size %zu from cache",
                  cache_key.c_str(), cache_elem->object_,
                  cache_elem->object_size_);
    }
    
    cache_.erase(cache_iter);
    size_ -= cache_elem->object_size_;

    delete cache_elem;
    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _DataType>
size_t
DurableObjectCache<_DataType>::flush()
{
    ScopeLock l(lock_, "DurableObjectCache::flush");
    size_t count = 0;
    while (!lru_.empty()) {
        evict_last();
        ++count;
    }
    return count;
}

