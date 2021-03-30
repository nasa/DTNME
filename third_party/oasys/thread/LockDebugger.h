/*
 *    Copyright 2007 Intel Corporation
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

#ifndef __LOCKDEBUGGER_H__
#define __LOCKDEBUGGER_H__

#include <vector>

#include "../debug/Formatter.h"

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_DEBUG_LOCKING_ENABLED

namespace oasys {

class Lock;

class LockDebugger : Formatter {
public:
    struct Ent {
        Ent(Lock* lock, int count)
            : lock_(lock), count_(count) {}

        Lock* lock() const { return lock_; }
        
        Lock* lock_;
        int   count_;
    };

    typedef std::vector<Ent> LockVector;
    
    /*!
     * Register a lock as being held.
     */ 
    void add_lock(Lock* lock);
    
    /*!
     * Remove the lock as being held.
     */
    void remove_lock(Lock* lock);
    
    /*!
     * Debugging information about the held locks.
     */
    int format(char* buf, size_t len) const;
    
    /*!
     * @return True if all o the specified locks are held and no other
     * locks.
     */
    bool check_n(unsigned int n, ...);
    
    //! @{ lock_check for various numbers of locks
    bool check();
    bool check(Lock* l1);
    bool check(Lock* l1, Lock* l2);
    bool check(Lock* l1, Lock* l2, Lock* l3);
    bool check(Lock* l1, Lock* l2, Lock* l3, Lock* l4);
    bool check(Lock* l1, Lock* l2, Lock* l3, Lock* l4, Lock* l5);
    bool check(Lock* l1, Lock* l2, Lock* l3, Lock* l4, Lock* l5, Lock* l6);
    //! @}

    /*!
     * @return True if all o the specified locks are held and no other
     * locks.
     */
    bool check_class(const char* c);
    
private:
    LockVector locks_held_;
};

} // namespace oasys

#include "../thread/Thread.h"

#define LOCKED_BY_ME(args...)                                   \
do {                                                            \
    if (! ::oasys::Thread::lock_debugger()->check(args))        \
    {                                                           \
                                                                \
        fprintf(stderr, "LOCK ASSERTION FAILED at %s:%d\n",     \
                __FILE__, __LINE__);                            \
        ::oasys::Breaker::break_here();                         \
        ::oasys::FatalSignals::die();                           \
    }                                                           \
} while (0);

#define CLASS_LOCKED_BY_ME(_the_class)                          \
do {                                                            \
    if (! ::oasys::Thread::lock_debugger()->check_class(_the_class))    \
    {                                                           \
        fprintf(stderr, "Lock class %s not locked at %s:%d\n",  \
                _the_class, __FILE__, __LINE__);                \
        ::oasys::Breaker::break_here();                         \
        ::oasys::FatalSignals::die();                           \
    }                                                           \
} while(0);

#define CLASS_NOT_LOCKED_BY_ME(_the_class)                              \
do {                                                                    \
    if (Thread::lock_debugger()->check_class(_the_class))               \
    {                                                                   \
        fprintf(stderr, "Lock class %s should not be locked at %s:%d\n", \
                _the_class, __FILE__, __LINE__);                        \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    }                                                                   \
} while(0);

#else /* ! OASYS_DEBUG_LOCKING_ENABLED */

#define LOCKED_BY_ME(args...)
#define CLASS_LOCKED_BY_ME(_the_class)
#define CLASS_NOT_LOCKED_BY_ME(_the_class)

#endif /* OASYS_DEBUG_LOCKING_ENABLED */
#endif /* __LOCKDEBUGGER_H__ */
