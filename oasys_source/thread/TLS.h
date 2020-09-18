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

#ifndef __TLS_H__
#define __TLS_H__

#include <pthread.h>

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#if HAVE_PTHREAD_SETSPECIFIC

namespace oasys {

template<typename _Type>
class TLS {
public:
    /*!
     * Allocate a key for (key,_Type)
     */
    static void init() {
        ::pthread_key_create(&key_, &destroy);
    }

    /*!
     * Clean up routine for the key on thread exit.
     */
    static void destroy(void* self) {
        _Type* obj = reinterpret_cast<_Type*>(self);
        delete obj;
    }
    
    /*!
     * Sets the key to obj. Thread will take ownership of the memory
     * deallocation.
     */
    static void set(_Type* obj) {
        ::pthread_setspecific(key_, obj);
    }
    
    /*!
     * Get the thread local object.
     */
    static _Type* get() {
        _Type* obj = reinterpret_cast<_Type*>(::pthread_getspecific(key_));
        return obj;
    }

private:
    static pthread_key_t key_;
};

} // namespace oasys

#endif /* HAVE_PTHREAD_SETSPECIFIC */

#endif /* __TLS_H__ */
