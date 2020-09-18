/*
 *    Copyright 2006 Intel Corporation
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

#ifndef __OPENFDCACHE_H__
#define __OPENFDCACHE_H__

#include <map>

#include "../debug/Logger.h"
#include "../thread/SpinLock.h"
#include "../io/IO.h"
#include "../util/LRUList.h"

namespace oasys {

struct OpenFdCacheClose {
    static void close(int fd) {
//        log_debug("/OpenFdCacheClose", "closed %d", fd);
        IO::close(fd);
    }
};

/*!
 * Maintains a cache of open files to get rid of calls to syscall
 * open().
 */ 
template<typename _Key, typename _CloseFcn = OpenFdCacheClose>
class OpenFdCache : Logger {
public:
    /*!
     * File descriptor LRU list entry.
     */
    struct FdListEnt {
        FdListEnt(const _Key& key,
                  int fd        = -1,
                  int pin_count = 0)
            : key_(key), fd_(fd), pin_count_(pin_count)
        {}
        
        _Key key_;
        int  fd_;
        int  pin_count_;
    };

    typedef LRUList<FdListEnt>                        FdList;
    typedef std::map<_Key, typename FdList::iterator> FdMap;

    OpenFdCache(const char* logpath,
                size_t      max)
        : Logger("OpenFdCache", "%s/%s", logpath, "cache"),
          max_(max)
    {}

    /*!
     * @return -1 if the fd is not in the cache, otherwise the fd of
     * the open file. Also pin the fd so it can't be closed.
     */
    int get_and_pin(const _Key& key) 
    {
        ScopeLock l(&lock_, "OpenFdCache::get_and_pin");

        typename FdMap::iterator i = open_fds_map_.find(key);
        if (i == open_fds_map_.end()) 
        {
            return -1;
        }
        
        open_fds_.move_to_back(i->second);
        ++(i->second->pin_count_);

        log_debug("Got entry fd=%d pin_count=%d size=%u", 
                  i->second->fd_, 
                  i->second->pin_count_,
                  (u_int)open_fds_map_.size());

        ASSERT(i->second->fd_ != -1);

        return i->second->fd_;
    }

    /*!
     * Unpin the fd referenced by _Key.
     */
    void unpin(const _Key& key) 
    {
        ScopeLock l(&lock_, "OpenFdCache::unpin");

        typename FdMap::iterator i = open_fds_map_.find(key);
        ASSERT(i != open_fds_map_.end());
        
        --(i->second->pin_count_);

        log_debug("Unpin entry fd=%d pin_count=%d size=%u", 
                  i->second->fd_, 
                  i->second->pin_count_,
                  (u_int)open_fds_map_.size());
    }

    /*!
     * Helper class to unpin a file descriptor at the end of a scope.
     */
    class ScopedUnpin {
    public:
        ScopedUnpin(OpenFdCache* cache, const _Key& key)
            : cache_(cache), key_(key) {}

        ~ScopedUnpin()
        {
            cache_->unpin(key_);
        }

    private:
        OpenFdCache* cache_;
        _Key         key_;
    };

    /*!
     * Put an fd in the file cache which may evict unpinned fds. Also
     * pin the fd that was just put into the cache.
     * 
     * N.B. There could be race when you are putting a newly opened
     * file (and someone else is trying to do the same) into the
     * cache. You need to check the return from put_and_pin as to
     * whether you will need to close the file or not.
     *
     * @return Fd 
     */
    int put_and_pin(const _Key& key, int fd) 
    {
        ScopeLock l(&lock_, "OpenFdCache::put_and_pin");

        ASSERT(fd != -1);

        typename FdMap::iterator i = open_fds_map_.find(key);
        if (i != open_fds_map_.end()) 
        {
            ++(i->second->pin_count_);
            log_debug("Added entry but already there fd=%d pin_count=%d size=%u", 
                      i->second->fd_, 
                      i->second->pin_count_,
                      (u_int)open_fds_map_.size());

            return i->second->fd_;
        }

        while (open_fds_map_.size() + 1> max_) 
        {
            if (evict() == -1) 
            {
                break;
            }
        }
        
        // start off with pin count 1
        typename FdList::iterator new_ent = open_fds_.insert(open_fds_.end(),
                                                             FdListEnt(key, fd, 1));
        log_debug("Added entry fd=%d pin_count=%d size=%u", 
                  new_ent->fd_, 
                  new_ent->pin_count_,
                  (u_int)open_fds_map_.size());

        
        open_fds_map_.insert(typename FdMap::value_type(key, new_ent));

        return fd;
    }

    /*
     * Sync a particular fd to disk.
     */
    void sync(const _Key& key)
    {
        ScopeLock l(&lock_, "OpenFdCache::close");

        typename FdMap::iterator i = open_fds_map_.find(key);

        if (i == open_fds_map_.end())
        {
        	log_warn("sync failed; Key not found");
            return;
        }

        fsync(i->second->fd_);
    }

    /*!
     * Close and release all of the cached fds.
     */
    void sync_all() {
        ScopeLock l(&lock_, "OpenFdCache::sync_all");

        log_debug("There were %zu open fds upon sync_all.", open_fds_.size());

        for (typename FdList::iterator i = open_fds_.begin();
             i != open_fds_.end(); ++i)
        {
            log_debug("Syncing fd=%d", i->fd_);
            fsync(i->fd_);
        }
    }

    /*!
     * Close a file fd and remove it from the cache.
     */
    void close(const _Key& key) 
    {
        ScopeLock l(&lock_, "OpenFdCache::close");
        
        typename FdMap::iterator i = open_fds_map_.find(key);

        if (i == open_fds_map_.end()) 
        {
            return;
        }

        ASSERT(i->second->pin_count_ == 0);

        _CloseFcn::close(i->second->fd_);
        log_debug("Closed %d size=%u", i->second->fd_, (u_int)open_fds_map_.size());

        open_fds_.erase(i->second);
        open_fds_map_.erase(i);       
    }
    
    /*!
     * Close and release all of the cached fds.
     */
    void close_all() {
        ScopeLock l(&lock_, "OpenFdCache::close_all");
        
        log_debug("There were %u open fds upon close.", open_fds_.size());
        
        for (typename FdList::iterator i = open_fds_.begin(); 
             i != open_fds_.end(); ++i)
        {
            if (i->pin_count_ > 0) {
                log_warn("fd=%d was busy", i->fd_);
            }

            log_debug("Closing fd=%d", i->fd_);
            _CloseFcn::close(i->fd_);
        }

        open_fds_.clear();
        open_fds_map_.clear();
    }

private:
    SpinLock lock_;

    FdList open_fds_;
    FdMap  open_fds_map_;

    size_t max_;

    /*!
     * Search from the beginning of the list and throw out a single,
     * unpinned fd.
     *
     * @return 0 if evict succeed or -1 we are totally pinned and
     * can't do anything.
     */
    int evict()
    {
        bool found = false;
        typename FdList::iterator i;
        for (i = open_fds_.begin(); i != open_fds_.end(); ++i)
        {
            if (i->pin_count_ == 0) {
                found = true;
                break;
            }
        }
        
        if (found) 
        {
            ASSERT(i->fd_ < 8*1024);
            
            log_debug("Evicting fd=%d size=%u", i->fd_, (u_int)open_fds_map_.size());
            _CloseFcn::close(i->fd_);
            open_fds_map_.erase(i->key_);
            open_fds_.erase(i);
        }
        else
        {
            log_warn("All of the fds are busy! size=%u", (u_int)open_fds_map_.size());
            return -1;
        }

        return 0;
    }
};

} // namespace oasys

#endif /* __OPENFDCACHE_H__ */
