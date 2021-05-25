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

#include "../util/StringBuffer.h"

template <typename _DataType>
class DurableObjectCache : public Logger {
public:
    enum CachePolicy_t {
        CAP_BY_SIZE,
        CAP_BY_COUNT
    };

    /**
     * Constructor.
     */
    DurableObjectCache(const char*   logpath,
                       size_t        capacity,
                       CachePolicy_t policy = CAP_BY_SIZE);

    /**
     * Destructor.
     */
    ~DurableObjectCache();

    /**
     * Add a new object to the cache, initially putting it on the live
     * object list. Note that this may cause some other object(s) to
     * be evicted from the cache.
     */
    int put(const SerializableObject& key, const _DataType* data, int flags);
    
    /**
     * Look up a given object in the cache.
     */
    int get(const SerializableObject& key, _DataType** data);

    /**
     * Return whether or not the key is currently live in in the cache.
     */
    bool is_live(const SerializableObject& key);

    /**
     * Release the given object, making it eligible for eviction.
     */
    int release(const SerializableObject& key, const _DataType* data);
    
    /**
     * Forcibly remove an object from the cache and delete it.
     */
    int del(const SerializableObject& key);

    /**
     * Remove the given entry from the cache but don't delete it.
     */
    int remove(const SerializableObject& key, const _DataType* data);

    /**
     * Flush all evictable (i.e. not live) objects from the cache.
     *
     * @return the number of objects evicted
     */
    size_t flush();

    /// @{
    /// Accessors
    size_t size()   const { return size_; }
    size_t count()  const { return cache_.size(); }
    size_t live()   const { return cache_.size() - lru_.size(); }
    int hits()      const { return hits_; }
    int misses()    const { return misses_; }
    int evictions() const { return evictions_; }
    /// @}

    /**
     * Get a string representation of the stats in the given string buffer.
     */
    void get_stats(StringBuffer* buf);

    /**
     * Reset the cache statistics.
     */
    void reset_stats()
    {
        hits_      = 0;
        misses_    = 0;
        evictions_ = 0;
    }

protected:
    /**
     * Build a std::string to index the hash map.
     */
    void get_cache_key(std::string* cache_key, const SerializableObject& key);
    
    /**
     * @return Whether or not the added item of size puts the cache
     * over the capacity.
     */
    bool is_over_capacity(size_t size);
    
    /**
     * Kick the least recently used element out of the cache.
     */
    void evict_last();

    /**
     * The LRU list just stores the key for the object in the main
     * cache table.
     */
    typedef LRUList<std::string> CacheLRUList;
    
    /**
     * Type for the cache table elements. 
     */
    struct CacheElement {
        CacheElement(const _DataType* object, size_t object_size,
                     bool live, CacheLRUList::iterator lru_iter)
            
            : object_(object),
              object_size_(object_size),
              live_(live),
              lru_iter_(lru_iter) {}

        const _DataType*       object_;
        size_t                 object_size_;
        bool                   live_;
        CacheLRUList::iterator lru_iter_;
    };

    /**
     * The cache table.
     */
    class CacheTable : public StringHashMap<CacheElement*> {};
    typedef std::pair<typename CacheTable::iterator, bool> CacheInsertRet;

    size_t size_;	///< The current size of the cache
    size_t capacity_;	///< The maximum size of the cache
    int hits_;		///< Number of times the cache hits
    int misses_;	///< Number of times the cache misses
    int evictions_;	///< Number of times the cache evicted an object
    CacheLRUList lru_;	///< The LRU List of objects
    CacheTable cache_;	///< The object cache table
    SpinLock* lock_;	///< Lock to protect the in-memory cache
    CachePolicy_t policy_; ///< Cache policy (see enum above)

public:
    /**
     * Class to represent a cache iterator and still hide the
     * implementation details of the cache table structure.
     */
    class iterator {
    public:
        iterator() {}
        iterator(const typename CacheTable::iterator& i) : iter_(i) {}
        
        const std::string& key()         { return iter_->first; }
        bool               live()        { return iter_->second->live_; }
        const _DataType*   object()      { return iter_->second->object_; }
        size_t             object_size() { return iter_->second->object_size_; }
        
        const iterator& operator++()
        {
            iter_++;
            return *this;
        }

        bool operator==(const iterator& other)
        {
            return iter_ == other.iter_;
        }
        
        bool operator!=(const iterator& other)
        {
            return iter_ != other.iter_;
        }
        
    protected:
        friend class DurableObjectCache;
        typename CacheTable::iterator iter_;
    };

    /// Return an iterator at the beginning of the cache.
    iterator begin()
    {
        return iterator(cache_.begin());
    }
    
    /// Return an iterator at the end of the cache.
    iterator end()
    {
        return iterator(cache_.end());
    }

};
