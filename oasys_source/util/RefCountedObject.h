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

#ifndef _REFCOUNTEDOBJECT_H_
#define _REFCOUNTEDOBJECT_H_

#include "../debug/Formatter.h"
#include "../debug/Log.h"
#include "../thread/Atomic.h"

namespace oasys {

/**
 * Simple implementation of the add_ref / del_ref contract used by the
 * templated Ref class. This class maintains an integer count of the
 * live references on it, and calls the virtual function
 * no_more_refs() when the refcount goes to zero. The default
 * implementation of no_more_refs() simply calls 'delete this'.
 *
 * Also, this implementation declares add_ref and del_ref as const
 * methods, and declares refcount_ to be mutable, the reason for which
 * being that taking or removing a reference on an object doesn't
 * actually modify the object itself.
 *
 * Finally, the class also stores a Logger instance that's used for
 * debug logging of add_ref/del_ref calls. Note that we use a
 * contained logger (rather than inheritance) to avoid conflicts with
 * descendent classes that may themselves inherit from Logger.
 *
 * The RefCountedObject inherits from Formatter and defines a simple
 * implementation of Format that just includes the pointer value, but
 * other derived classes can (and should) override format to print
 * something more useful.
 * 
 */
class RefCountedObject : public Formatter {
public:
    /**
     * Constructor that takes the debug logging path to be used for
     * add and delete reference logging.
     */
    RefCountedObject(const char* logpath);

    /**
     * Virtual destructor declaration.
     */
    virtual ~RefCountedObject();

    /**
     * Bump up the reference count.
     *
     * @param what1 debugging string identifying who is incrementing
     *              the refcount
     * @param what2 optional additional debugging info
     */
    void add_ref(const char* what1, const char* what2 = "") const;

    /**
     * Bump down the reference count.
     *
     * @param what1 debugging string identifying who is decrementing
     *              the refcount
     * @param what2 optional additional debugging info
     */
    void del_ref(const char* what1, const char* what2 = "") const;

    /**
     * Hook called when the refcount goes to zero.
     */
    virtual void no_more_refs() const;

    /**
     * Virtual from Formatter
     */
    int format(char* buf, size_t sz) const;

    /**
     * Accessor for the refcount value.
     */
    u_int32_t refcount() const { return refcount_.value; }
    
protected:
    /// The reference count
    mutable atomic_t refcount_;
    
    /// Logger object used for debug logging
    Logger logger_;
};

} // namespace oasys

#endif /* _REFCOUNTEDOBJECT_H_ */
