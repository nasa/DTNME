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


#ifndef _OASYS_TEMPREF_H_
#define _OASYS_TEMPREF_H_

#include "../debug/DebugUtils.h"

namespace oasys {

/**
 * For functions that want to return an ObjectRef, it's irritating to
 * have to go through a series of add_ref and del_ref calls to deal
 * with the C++ temporary objects that are created.
 *
 * Therefore, this class holds a pointer to a reference counted object
 * but doesn't up the reference count, and enforces that the pointer
 * is released before the class is destroyed since the temporary
 * destructor will PANIC if the pointer is non-NULL;
 *
 */
template <typename _Type>
class TempRef {
public:
    /**
     * Constructor that initializes the pointer to be empty.
     */
    TempRef(const char* what1, const char* what2 = "")
        : object_(NULL), what1_(what1), what2_(what2)
    {
    }
    
    /**
     * Constructor that takes an initial pointer for assignment.
     */
    TempRef(_Type* object, const char* what1, const char* what2 = "")
        : object_(object), what1_(what1), what2_(what2)
    {
    }

    /**
     * Copy constructor.
     */
    TempRef(const TempRef& other)
    {
        object_ = other.object();
        other.release();
    }
    
    /**
     * Destructor that asserts the pointer was claimed.
     */
    ~TempRef() {
        if (object_ != NULL) {
            PANIC("TempRef %s %s destructor fired but object still exists!!",
                  what1_, what2_);
        }
    }

    /**
     * Assignment operator.
     */
    TempRef& operator=(const TempRef<_Type>& other)
    {
        object_ = other.object();
        other.release();
        return *this;
    }

    /**
     * Assignment operator.
     */
    TempRef& operator=(_Type* object)
    {
        ASSERTF(object_ == NULL,
                "TempRef can only assign to null reference");
        object_ = object;
        return *this;
    }

    /**
     * Accessor for the object.
     */
    _Type* object() const {
        return object_;
    }
    
    /**
     * Operator overload for pointer access.
     */
    _Type* operator->() const
    {
        return object_;
    }

    /**
     * Operator overload for pointer access.
     */
    _Type& operator*() const
    {
        return *object_;
    }

    /**
     * Release the reference to the object. Note this is declared
     * const even though it technically modifies the object.
     */
    void release() const {
        object_ = NULL;
    }

    /**
     * Equality operator.
     */
    bool operator==(_Type* o)
    {
        return (object_ == o);
    }
    
    /**
     * Inequality operator.
     */
    bool operator!=(_Type* o)
    {
        return (object_ != o);
    }

    const char* what1() const { return what1_; } 
    const char* what2() const { return what2_; }
    
private:
    /**
     * The object pointer
     */
    mutable _Type* object_;

    /**
     * Debugging strings.
     */
    const char *what1_, *what2_;
};

} // namespace oasys

#endif /* _OASYS_TEMPREF_H_ */
