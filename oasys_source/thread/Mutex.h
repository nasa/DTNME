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


#ifndef _OASYS_MUTEX_H_
#define _OASYS_MUTEX_H_

#include "Lock.h"
#include "../debug/Logger.h"

namespace oasys {

/// Mutex wrapper class for pthread_mutex_t.
class Mutex : public Lock {
    friend class Monitor; // Monitor needs access to mutex_.
    
public:
    /// Different kinds of mutexes offered by Linux, distinguished by
    /// their response to a single thread attempting to acquire the
    /// same lock more than once.
    ///
    /// - FAST: No error checking. The thread will block forever.
    /// - RECURSIVE: Double locking is safe.
    ///
    enum lock_type_t {
        TYPE_FAST = 1,
        TYPE_RECURSIVE
    };

    /// Creates a mutex. By default, we create a TYPE_RECURSIVE.
    Mutex(const char* logbase,
          lock_type_t type = TYPE_RECURSIVE,
          bool keep_quiet = false,
          const char* classname = "GENERIC");

    ~Mutex();

    //! @{ Virtual from Lock
    int lock(const char* lock_user);
    int unlock();
    int try_lock(const char* lock_user);
    //! @}

protected:
    pthread_mutex_t mutex_;        ///< the underlying mutex
    lock_type_t     type_;         ///< the mutex type
    bool	    keep_quiet_;   ///< no logging
};

} // namespace oasys

#endif /* _OASYS_MUTEX_H_ */

