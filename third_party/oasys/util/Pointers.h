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

#ifndef __POINTERS_H__
#define __POINTERS_H__

#include "../debug/DebugUtils.h"

namespace oasys {

/**
 * NOTE: Used only for things that are created which need to be
 * deleted when exiting a scope. Note how the * operator returns a
 * _reference_ to the stored pointer. This template is mainly used to
 * clean up call site regions where the functions allocate memory and
 * take a double pointer as an argument, which makes it impossible to
 * use a std::auto_ptr.
 */
template <typename _Class>
class ScopePtr {
public:
    ScopePtr() : ptr_(0) {}
    ScopePtr(_Class* obj) : ptr_(obj) {}
    ~ScopePtr() { if(ptr_) { delete ptr_; ptr_ = 0; } }

    _Class& operator*()  const { return *ptr_; }
    _Class* operator->() const { return ptr_; }

    /**
     * Assignment operator that ensures there is no currently assigned
     * pointer before claiming the given one.
     */
    ScopePtr& operator=(_Class* ptr) {
        ASSERT(ptr_ == NULL);
        ptr_ = ptr;
        return *this;
    }
    
    /**
     * This construction basically allows you to pass the ptr_ to a
     * double pointer taking function, cleaning up the syntax quite a
     * bit.
     */
    _Class*& ptr() { return ptr_; }

    /**
     * Not implemented on purpose. Don't handle assignment to another
     * ScopePtr
     */
    ScopePtr& operator=(const ScopePtr&); 
    
private:
    _Class* ptr_;
};

/**
 * Similar idea but for a malloc'd buffer.
 */
class ScopeMalloc {
public:
    ScopeMalloc(void* ptr = 0) : ptr_(ptr) {}
    
    ~ScopeMalloc() {
        if (ptr_) {
            free(ptr_);
            ptr_ = 0;
        }
    }

    /**
     * Assignment operator that ensures there is no currently assigned
     * pointer before claiming the given one.
     */
    ScopeMalloc& operator=(void* ptr) {
        ASSERT(ptr_ == NULL);
        ptr_ = ptr;
        return *this;
    }
    
    /**
     * This construction basically allows you to pass the ptr_ to a
     * double pointer taking function, cleaning up the syntax quite a
     * bit.
     */
    void*& ptr() { return ptr_; }

    /**
     * Not implemented on purpose. Don't handle assignment to another
     * ScopeMalloc
     */
    ScopeMalloc& operator=(const ScopeMalloc&); 
    
private:
    void* ptr_;
};


} // namespace oasys

#endif //__POINTERS_H__
