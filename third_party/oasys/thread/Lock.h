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


#ifndef _OASYS_LOCK_H_
#define _OASYS_LOCK_H_

#include "Atomic.h"
#include "Thread.h"

#include "../debug/Logger.h"
#include "../util/Pointers.h"
#include "../compat/inttypes.h"

namespace oasys {

/**
 * Abstract Lock base class.
 */
class Lock : public Logger {
public:
    /**
     * Default Lock constructor.
     */
    Lock(const char* lock_class) 
        : Logger("Lock", "/lock"),
          lock_count_(0),
          lock_holder_(0),
          lock_holder_name_(0), 
          class_(lock_class),
          scope_lock_count_(0) 
    {}

    /**
     * Lock destructor. Asserts that the lock is not locked by another
     * thread or by a scope lock.
     */
    virtual ~Lock()
    {
        /* XXX/bowei -- This really should be nothing. */
        /*
        if (is_locked()) 
        {
            ASSERT(is_locked_by_me());
            ASSERT(scope_lock_count_ == 0);
        }
        */
    }
    
    /**
     * Acquire the lock.
     *
     * @return 0 on success, -1 on error
     */
    virtual int lock(const char* lock_user) = 0;

    /**
     * Release the lock.
     *
     * @return 0 on success, -1 on error
     */
    virtual int unlock() = 0;

    /**
     * Try to acquire the lock.
     *
     * @return 0 on success, 1 if already locked, -1 on error.
     */
    virtual int try_lock(const char* lock_user) = 0;

    /**
     * Check for whether the lock is locked or not.
     *
     * @return true if locked, false otherwise
     */
    bool is_locked()
    {
        return (lock_count_.value != 0);
    }

    /**
     * Check for whether the lock is locked or not by the calling
     * thread.
     *
     * @return true if locked by the current thread, false otherwise
     */
    bool is_locked_by_me()
    {
        return is_locked() &&
            pthread_equal(lock_holder_, Thread::current());
    }

    /**
     * Name of the current holder of the lock.
     */
    const char* lock_holder_name() 
    {
        return lock_holder_name_;
    }

    /**
     * Class of the lock.
     */
    const char* lock_class()
    {
        return class_;
    }

protected:
    friend class ScopeLock;
    friend class ScopeLockIf;

    /**
     * Stores a count of the number of locks currently held, needed
     * for recursive locking. Note that 0 means the lock is currently
     * not locked.
     */
    atomic_t lock_count_;
    
    /**
     * Stores the pthread thread id of the current lock holder. It is
     * the responsibility of the derived class to set this in lock()
     * and unset it in unlock(), since the accessors is_locked() and
     * is_locked_by_me() depend on it.
     *
     * Note that this field must be declared volatile to ensure that
     * readers of the field make sure to go to memory (which might
     * have been updated by another thread).
     */
    volatile pthread_t lock_holder_;

    /**
     * Lock holder name for debugging purposes. Identifies call site
     * from which lock has been held.
     */ 
    const char* lock_holder_name_;

    /*!
     * The class of the lock. Defaults to generic.
     */
    const char* class_;

    /**
     * Stores a count of the number of ScopeLocks holding the lock.
     * This is checked in the Lock destructor to avoid strange crashes
     * if you delete the lock object and then the ScopeLock destructor
     * tries to unlock it.
     */
    int scope_lock_count_;
};

/**
 * Scope based lock created from a Lock. Holds the lock until the
 * object is destructed. Example of use:
 *
 * \code
 * {
 *     Mutex m;
 *     ScopeLock lock(&m);
 *     // protected code
 *     ...
 * }
 * \endcode
 */
class ScopeLock {
public:
    ScopeLock()
        : lock_(NULL)
    {
    }
    
    ScopeLock(Lock*       l,
              const char* lock_user)
        : lock_(l)
    {
        do_lock(lock_user);
    }

    ScopeLock(const Lock* l,
              const char* lock_user)
        : lock_(const_cast<Lock*>(l))
    {
        do_lock(lock_user);
    }

    ScopeLock(oasys::ScopePtr<Lock> l,
              const char*           lock_user)
        : lock_(l.ptr())
    {
        do_lock(lock_user);
    }

    void set_lock(Lock* l, const char* lock_user)
    {
        lock_ = l;
        do_lock(lock_user);
    }

    void do_lock(const char* lock_user) {
        ASSERT(lock_ != NULL);
        int ret = lock_->lock(lock_user);
        ASSERT(ret == 0);
        lock_->scope_lock_count_++;
    }

    void unlock() {
        if (lock_ != NULL) {
            lock_->scope_lock_count_--;
            lock_->unlock();
            lock_ = NULL;
        }
    }

    ~ScopeLock()
    {
        if (lock_) {
            unlock();
        }
    }
    
protected:
    Lock* lock_;
};

/**
 * Same as ScopeLock from above, but with a boolean predicate. Only
 * locks if true. Useful for cases where locking is optional, but
 * putting things in an if scope will null the scope of the ScopeLock.
 */
class ScopeLockIf {
public:
    ScopeLockIf(Lock*       l,
                const char* lock_user,
                bool        use_lock)
        : lock_(l), use_lock_(use_lock)
    {
        do_lock(lock_user);
    }

    ScopeLockIf(const Lock* l,
                const char* lock_user,
                bool        use_lock)
        : lock_(const_cast<Lock*>(l)), use_lock_(use_lock)
    {
        do_lock(lock_user);
    }

    ScopeLockIf(oasys::ScopePtr<Lock> l,
                const char*           lock_user,
                bool                  use_lock)
        : lock_(l.ptr()), use_lock_(use_lock)
    {
        do_lock(lock_user);
    }
    
    void do_lock(const char* lock_user) {
        if (use_lock_) 
        {
            int ret = lock_->lock(lock_user);       
            ASSERT(ret == 0);                       
            lock_->scope_lock_count_++;
        }
    }
    
    void unlock() {
        if (use_lock_) 
        {
            lock_->scope_lock_count_--;
            lock_->unlock();
            lock_ = 0;
        }
    }
    
    ~ScopeLockIf()
    {
        if (use_lock_ && lock_) {
            unlock();
        }
    }
    
protected:
    Lock* lock_;
    bool  use_lock_;
};

/*!
 * ScopeLock without the initial locking.
 */
class ScopeUnlock {
public:
    ScopeUnlock(Lock* lock)
        : lock_(lock)
    {
        ASSERT(lock_->is_locked_by_me());
    }

    ~ScopeUnlock()
    {
        lock_->unlock();
    }
    
private:
    Lock* lock_;
};

} // namespace oasys

#endif /* LOCK_h */
