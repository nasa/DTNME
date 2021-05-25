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

#ifndef _OASYS_NOLOCK_H_
#define _OASYS_NOLOCK_H_

#include "Lock.h"

namespace oasys {

/**
 * Class that implements the function signature of the Lock class but
 * doesn't actually do anything. Note, however, that is_locked() and
 * is_locked_by_me() will never actually return true.
 *
 * Useful in cases where you want to decide at runtime whether or not
 * to enable some locking code by either creating a real lock or this
 * class, but then you don't have to update all the call sites with an
 * if statement.
 */
class NoLock : public Lock {
public:
    NoLock() : Lock("") {}
    int lock(const char* lock_user);
    int unlock();
    int try_lock(const char* lock_user);
};

} // namespace oasys

#endif /* _OASYS_NOLOCK_H_ */
