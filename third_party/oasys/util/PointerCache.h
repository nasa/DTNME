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

#ifndef __POINTERCACHE_H__
#define __POINTERCACHE_H__

#include <map>

#include "../debug/DebugUtils.h"

namespace oasys {

/*!
 * A cache of pointers with a at_limit() predicate that is called when
 * with each new allocation to decide which pointer to
 * evict. Reinstate is called to revive evicted objects.
 */
template <
    typename _Name,            // namespace for the cache
    typename _PtrType          // type of pointer being stored
>
class PointerCache {
public:
    /*!
     * Adds the pointer to the cache.
     */
    PointerCache() : ptr_(0) {
        // NOTE! because of the fact that virtual functions cannot be
        // called from the constructor, this line of code:
        //
        // set_ptr(ptr);
        // 
        // which really should be here needs to be put in the
        // constructor of the derived class.
    }
    
    /*!
     * Unregisters the contained pointer from the cache.
     */
    virtual ~PointerCache() {
        // NOTE! because of the fact that virtual functions cannot be
        // called from the destructor, this line of code:
        //
        // set_ptr(0);
        // 
        // which really should be here needs to be put in the
        // destructor of the derived class.
    }

    //! @{ referencing operations will resurrect pointers if need be
    _PtrType& operator*() const {
        restore_and_update_ptr();       
        return *ptr_;
    }    
    _PtrType* operator->() const {
        restore_and_update_ptr();       
        return ptr_;
    }
    //! @}

    /*! 
     * Set the contained pointer to something else. Replaces the
     * contained pointer in the cache with the new pointer.
     */
    PointerCache& operator=(_PtrType* ptr) {
        set_ptr(ptr);
        return *this;
    }

    //! Not implemented to make this illegal
    PointerCache& operator=(const PointerCache&); 
    
    //! @return Original pointer
    _PtrType* ptr() { 
        restore_and_update_ptr();       
        return ptr_; 
    }
    
protected:
    _PtrType*     ptr_;
    
    virtual void restore_and_update_ptr() = 0;
    virtual bool at_limit(_PtrType* ptr)  = 0;
    virtual void evict()                  = 0;

    virtual void register_ptr(_PtrType* ptr)   = 0;
    virtual void unregister_ptr(_PtrType* ptr) = 0;

    void set_ptr(_PtrType* ptr) {
        if (ptr == ptr_) {
            return;
        }

        if (ptr_ != 0) {
            unregister_ptr(ptr_);
            ASSERT(ptr_ == 0);
        }

        if (ptr != 0) {
            while (at_limit(ptr)) {
                evict();
            }
            register_ptr(ptr);
        }

        ptr_ = ptr;
    }
};

} // namespace oasys

#endif /* __POINTERCACHE_H__ */
