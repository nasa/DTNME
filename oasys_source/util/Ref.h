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

#ifndef _OASYS_REF_H_
#define _OASYS_REF_H_

#include "../debug/DebugUtils.h"
#include "TempRef.h"

namespace oasys {

/**
 * Smart pointer class to maintain reference counts on objects.
 *
 * The reference template expects the instatiating _Type to implement
 * methods for adding and deleting references that fit the following
 * signature:
 *
 * <code>
 * void add_ref(const char* what1, const char* what2);
 * void del_ref(const char* what1, const char* what2);
 * </code>
 *
 * For example, see the RefCountedObject class.
 *
 * The strings what1 and what2 can be used for debugging and are
 * stored in the reference. Note that what1 is mandatory and is
 * typically the function name or object class that is holding the
 * reference. What2 is any other additional information used to
 * distinguish the instance.
 */
template <typename _Type>
class Ref {
public:
    /**
     * Constructor that initializes the pointer to be empty.
     *
     * Note: we need the default constructor because otherwise Ref
     * cannot be placed in an STL container.
     */
    Ref(const char* what1 = "!!DEFAULT REF CONSTRUCTOR!!", const char* what2 = "")
        : object_(NULL), what1_(what1), what2_(what2)
    {
    }
    
    /**
     * Constructor that takes an initial pointer for assignment.
     */
    Ref(_Type* object, const char* what1, const char* what2 = "")
        : object_(object), what1_(what1), what2_(what2)
    {
        if (object_)
            object->add_ref(what1_, what2_);
    }

private:
    /**
     * Constructor (deliberately private and unimplemented) that takes
     * a _Type* pointer but no debugging string.
     *
     * This is intended to cause the compiler to complain both that
     * it's private, and to provide ambiguity (with the first
     * constructor) when called with a single argument of NULL, hence
     * forcing the caller to pass at least char* debug string.
     */
    Ref(_Type* object);

public:

    /**
     * Constructor that takes a temporary ref.
     */
    Ref(const TempRef<_Type> temp)
        : what1_(temp.what1()),
          what2_(temp.what2())
    {
        object_ = temp.object();
        if (object_) {
            object_->add_ref(what1_, what2_);
            temp.release();
        }
    }

    /**
     * Copy constructor.
     */
    Ref(const Ref& other)
        : what1_(other.what1_),
          what2_(other.what2_)
    {
        object_ = other.object();
        if (object_) {
            object_->add_ref(what1_, what2_);
        }
    }

    /**
     * Destructor.
     */
    ~Ref()
    {
        release();
    }

    /**
     * Release the reference on the object.
     */
    void release()
    {
        if (object_) {
            object_->del_ref(what1_, what2_);
            object_ = NULL;
        }
    }

    /**
     * Accessor for the object pointer.
     */
    _Type* object() const
    {
        return object_;
    }

    /**
     * Accessor for a const object pointer.
     */
    const _Type* const_object() const
    {
        return object_;
    }

    /**
     * Operator overload for pointer access.
     */
    _Type* operator->() const
    {
        ASSERT(object_ != 0);
        return object_;
    }

    /**
     * Operator overload for pointer access.
     */
    _Type& operator*() const
    {
        ASSERT(object_ != 0);
        return *object_;
    }

    /**
     * Assignment function.
     */
    void assign(_Type* new_obj)
    {
        if (object_ != new_obj)
        {
            if (new_obj != 0) {
                new_obj->add_ref(what1_, what2_);
            }
            
            if (object_ != 0) {
                object_->del_ref(what1_, what2_);
            }

            object_ = new_obj;
        }
    }

    /**
     * Assignment operator.
     */
    Ref& operator=(_Type* o)
    {
        assign(o);
        return *this;
    }

    /**
     * Assignment operator.
     */
    Ref& operator=(const Ref<_Type>& other)
    {
        assign(other.object_);
        return *this;
    }

    /**
     * Assignment operator from a temporary ref.
     */
    Ref& operator=(const TempRef<_Type>& temp)
    {
        if (object_ != temp.object()) {
            assign(temp.object());
        }
        temp.release();
        return *this;
    }
    
    /**
     * Equality operator.
     */
    bool operator==(_Type* o) const
    {
        return (object_ == o);
    }
     
    /**
     * Equality operator.
     */
    bool operator==(const Ref<_Type>& other) const
    {
        return (object_ == other.object_);
    }
    
   /**
     * Inequality operator.
     */
    bool operator!=(_Type* o) const
    {
        return (object_ != o);
    }
    
    /**
     * Equality operator.
     */
    bool operator!=(const Ref<_Type>& other) const
    {
        return (object_ != other.object_);
    }
 
    /**
     * Less than operator.
     */
    bool operator<(const Ref<_Type>& other) const
    {
        return (object_ < other.object_);
    }
    
private:
    /**
     * The object.
     */
    _Type* object_;

    /**
     * Debugging strings.
     */
    const char *what1_, *what2_;
};

} // namespace oasys

#endif /* _OASYS_REF_H_ */
