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

#ifndef __POINTERHANDLE_H__
#define __POINTERHANDLE_H__

#include <map>

#include "../debug/DebugUtils.h"

namespace oasys {

/*!
 * Implements a handle which can be invalidated. This then can be used
 * to create a set of handles which have some caching/invalidation
 * policy.
 */
template <
    typename _PtrType          // type of pointer being stored
>
class PointerHandle {
public:
    /*!
     * Adds the pointer to the cache.
     */
    PointerHandle() : ptr_(0) {}
    
    /*!
     * Unregisters the contained pointer from the cache.
     */
    virtual ~PointerHandle() { ptr_ = 0; }

    //! @{ referencing operations will resurrect pointers if need be
    _PtrType& operator*() const {
        restore_and_update();
        return *ptr_;
    }    
    _PtrType* operator->() const {
        restore_and_update();
        return ptr_;
    }
    //! @}

    //! @return Original pointer
    _PtrType* ptr() const { 
        restore_and_update();
        return ptr_; 
    }
    
protected:
    mutable _PtrType* ptr_;

    /*!
     * Invalidate and free the resources associated with this pointer.
     * Invalid pointers are null pointers.
     */ 
    virtual void invalidate() const = 0;
    
    /*!
     * Restore the resources associated with this pointer. Assumes the
     * pointer is invalid.
     */
    virtual void restore() const = 0;

    /*!
     * Update usage information (e.g. for LRU)
     */
    virtual void update() const = 0;

    void restore_and_update() const {
        if (ptr_ == 0) {
            restore();
            ASSERT(ptr_ != 0);
        }
        update();
    }
};

} // namespace oasys

#endif /* __POINTERHANDLE_H__ */
