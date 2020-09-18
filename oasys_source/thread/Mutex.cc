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
#  include <oasys-config.h>
#endif

#include <unistd.h>
#include <errno.h>

#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif

#include "../debug/DebugUtils.h"
#include "../debug/Log.h"
#include "../thread/LockDebugger.h"

#include "Mutex.h"

namespace oasys {

Mutex::Mutex(const char* logbase,
             lock_type_t type,
             bool        keep_quiet,
             const char* classname)
    : Lock(classname),
      type_(type), 
      keep_quiet_(keep_quiet)
{
    logpathf("%s/lock", logbase);

    // Set up the type attribute
    pthread_mutexattr_t attrs;
    memset(&attrs, 0, sizeof(attrs));
    
    if (pthread_mutexattr_init(&attrs) != 0) {
        PANIC("fatal error in pthread_mutexattr_init: %s", strerror(errno));
    }

    int mutex_type;
    switch(type_) {
    case TYPE_FAST:
        mutex_type = PTHREAD_MUTEX_NORMAL;
        break;
    case TYPE_RECURSIVE:
        mutex_type = PTHREAD_MUTEX_RECURSIVE;
        break;
    default:
        NOTREACHED; // needed to avoid uninitialized variable warning
        break;
    }

    if (pthread_mutexattr_settype(&attrs, mutex_type) != 0) {
        PANIC("fatal error in pthread_mutexattr_settype: %s", strerror(errno));
    }

    memset(&mutex_, 0, sizeof(mutex_));
    if (pthread_mutex_init(&mutex_, &attrs) != 0) {
        PANIC("fatal error in pthread_mutex_init: %s", strerror(errno));
    }

    if (pthread_mutexattr_destroy(&attrs) != 0) {
        PANIC("fatal error in pthread_mutexattr_destroy: %s", strerror(errno));
    }
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex_);
    if ((keep_quiet_ == false) && (logpath_[0] != 0))
        log_debug("destroyed");
}

int
Mutex::lock(const char* lock_user)
{
    int err = pthread_mutex_lock(&mutex_);

#if OASYS_DEBUG_LOCKING_ENABLED
    if (Thread::lock_debugger() != NULL ) {
        Thread::lock_debugger()->add_lock(this); // Only do this if the calling
                                                 // thread is an oasys thread.
    }
#endif

    if (err != 0) {
        PANIC("error in pthread_mutex_lock: %s", strerror(errno));
    }

    ++lock_count_.value;
    
    if ((keep_quiet_ == false) && (logpath_[0] != 0))
        log_debug("locked (count %u)", lock_count_.value);
    
    lock_holder_      = Thread::current();
    lock_holder_name_ = lock_user;
    
    return err;
}

int
Mutex::unlock()
{
    ASSERT(is_locked_by_me());

    if (--lock_count_.value == 0) {
        lock_holder_      = 0;
        lock_holder_name_ = 0;
    }
    
    int err = pthread_mutex_unlock(&mutex_);

#if OASYS_DEBUG_LOCKING_ENABLED
    if (Thread::lock_debugger() != NULL ) {
        Thread::lock_debugger()->remove_lock(this); // Only do this if the calling
                                                 // thread is an oasys thread.
    }
#endif
    
    if (err != 0) {
        PANIC("error in pthread_mutex_unlock: %s", strerror(errno));
    }

    if ((keep_quiet_ == false) && (logpath_[0] != 0))
        log_debug("unlocked (count %u)", lock_count_.value);
    
    return err;
}

int
Mutex::try_lock(const char* lock_user)
{
    int err = pthread_mutex_trylock(&mutex_);

    if (err == EBUSY) {
        if ((keep_quiet_ == false) && (logpath_[0] != 0)) {
            log_debug("try_lock busy");
        }
        return 1;
    } else if (err != 0) {
        PANIC("error in pthread_mutex_trylock: %s", strerror(errno));
    }

#if OASYS_DEBUG_LOCKING_ENABLED
    if (Thread::lock_debugger() != NULL ) {
        Thread::lock_debugger()->add_lock(this); // Only do this if the calling
                                                 // thread is an oasys thread.
    }
#endif

    ++lock_count_.value;
    if ((keep_quiet_ == false) && (logpath_[0] != 0))
        log_debug("try_lock locked (count %u)", lock_count_.value);
    lock_holder_      = Thread::current();
    lock_holder_name_ = lock_user;
    return 0;
}


} // namespace oasys
