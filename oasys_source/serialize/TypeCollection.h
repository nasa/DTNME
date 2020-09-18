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

#ifndef __OASYS_TYPE_COLLECTION_H__
#define __OASYS_TYPE_COLLECTION_H__

#include <map>

#include "../debug/Logger.h"
#include "Serialize.h"

namespace oasys {

/**
 * @class TypeCollectionInstance
 *
 * This templated type collection accomplishes severals things:
 *
 * - Enables different collections of type codes which can have the
 *   same numeric value, e.g. the same type codes can be reused in
 *   different projects that are then linked together.
 * - Type checked object creation from raw bits.
 * - Abstract type groups, e.g. Object* deserialized from concrete
 *   instantiations A, B, C who inherited from Object.
 * 
 * Example of use:
 * @code
 * // Type declaration for the Foobar collection of type codes
 * struct FoobarC {};
 * 
 * // Instantiate a typecollection for this collection (this goes in
 * // the .cc file)
 * TypeCollection<FoobarC>* TypeCollection<FoobarC>::instance_;
 * 
 * // An aggregate class Obj and concrete classes Foo, Bar
 * struct Obj : public SerializableObject {};
 * struct Foo : public Obj {
 *     Foo(TypeCollection<FoobarC>* b) {}
 *     void serialize(SerializeAction* a) {}
 * };
 * struct Bar : public Obj {
 *     Bar(TypeCollection<FoobarC>* b) {}
 *     void serialize(SerializeAction* a) {}
 * };
 * 
 * // in the .h file
 * TYPE_COLLECTION_DECLARE(FoobarC, Foo, 1);
 * TYPE_COLLECTION_DECLARE(FoobarC, Bar, 2);
 * TYPE_COLLECTION_GROUP(FoobarC, Obj, 1, 2);
 * 
 * // in the .cc file
 * TYPE_COLLECTION_DEFINE(FoobarC, Foo, 1);
 * TYPE_COLLECTION_DEFINE(FoobarC, Bar, 2);
 * 
 * // example of use
 * Foo* foo;
 * TypeCollectionInstance<FoobarC>* factory = 
 *     TypeCollectionInstance<FoobarC>::instance();
 * int err = factory->new_object(
 *               TypeCollectionCode<TestC, Foo>::TYPECODE, &foo);
 * @endcode
 */

class TypeCollectionHelper;

namespace TypeCollectionErr {
enum {
    TYPECODE = 1,
    MEMORY,
};
};

/**
 * Generic base class needed to stuff the templated class into a map.
 */
class TypeCollectionHelper {
public:
    virtual SerializableObject* new_object() = 0;
    virtual const char* name() const = 0;
    virtual ~TypeCollectionHelper() {}
};

/**
 * Conversion class from C++ types to their associated type codes. See
 * TYPE_COLLECTION_MAP macro below for a instantiation (specialization)
 * of this template to define the types.
 */
template<typename _Collection, typename _Type> class TypeCollectionCode;

/**
 * Generic base class for templated type collections.
 */
class TypeCollection {
public:
    /*!
     * Typedef for type codes.
     */
    typedef u_int32_t TypeCode_t;
    typedef std::map<TypeCode_t, TypeCollectionHelper*> TypeMap;
    
    enum {
        UNKNOWN_TYPE = 0xffffffff
    };

    /*!
     * Typedef for an allocator function usable as a callback
     */
    typedef int (*Allocator_t)(TypeCode_t typecode,
                               SerializableObject** data);

    /*!
     * Register a typecode into the type collection.
     */
    void reg(TypeCode_t typecode, TypeCollectionHelper* helper) {
        ASSERT(dispatch_.find(typecode) == dispatch_.end());
        dispatch_[typecode] = helper;
    }

    /*!
     * @return the stringified type code.
     */
    const char* type_name(TypeCode_t typecode) {
        if(dispatch_.find(typecode) == dispatch_.end()) {
            return "";
        } 
        return dispatch_[typecode]->name();
    }

    /*!
     * Output a list of the registered types in the TypeCollection.
     */
    void dump() const
    {
        log_info_p("/oasys/type-collection", "Registered types:");
        for (TypeMap::const_iterator i = dispatch_.begin();
             i != dispatch_.end(); ++i)
        {
            log_info_p("/oasys/type-collection", "%30s %8u",
                       i->second->name(), 
                       i->first);
        }
    }
    
protected:
    TypeMap  dispatch_;
};

template<typename _Collection>
class TypeCollectionInstance : public TypeCollection {
public:    
    /** 
     * Note: this should be multithread safe because the instance is
     * created by the static initializers of the program, at which
     * time there should be only one thread.
     */ 
    static TypeCollectionInstance<_Collection>* instance() {
        if(!instance_) {
            instance_ = new TypeCollectionInstance<_Collection>();
        }        
        return instance_;
    }

    /**
     * Get a new object from the given typecode. NOTE: This can fail!
     * Be sure to check the return value from the function.
     *
     * @return 0 on no error, MEMORY if cannot allocate new object,
     * and TYPECODE if the typecode is invalid for the given type.
     */
    template<typename _Type>
    int new_object(TypeCode_t typecode, _Type** obj, bool check_type = true)
    {
        // Check that the given typecode is within the legal bounds
        // for the _Type of the return
        if(check_type && 
           (TypeCollectionCode<_Collection, _Type>::TYPECODE_LOW  > typecode ||
            TypeCollectionCode<_Collection, _Type>::TYPECODE_HIGH < typecode))
        {
            return TypeCollectionErr::TYPECODE;
        }

        // Based on the lookup in the dispatch, create a new object
        // and cast it to the given type. Note the use of dynamic cast to 
        // safely downcast.
        ASSERT(dispatch_.find(typecode) != dispatch_.end());
        *obj = dynamic_cast<_Type*>(dispatch_[typecode]->new_object());
        if (*obj == NULL) {
            log_crit_p("/oasys/type-collection", "out of memory");
            return TypeCollectionErr::MEMORY;
        }
        
        return 0;
    }

private:
    static TypeCollectionInstance<_Collection>* instance_;
};

/**
 * Instantiate a template with the specific class and create a static
 * instance of this to register the class. Use the TYPE_COLLECTION_DEFINE macro
 * below.
 */
template<typename _Collection, typename _Class>
class TypeCollectionDispatch : public TypeCollectionHelper {
public:
    /** Register upon creation. */
    TypeCollectionDispatch<_Collection, _Class>
    (TypeCollection::TypeCode_t typecode, const char* name)
        : name_(name)
    {
        TypeCollectionInstance<_Collection>::instance()->reg(typecode, this);
    }

    /** 
     * The _Class takes an instance of the TypeCollection class in order to
     * distinguish that the constructor is being called to build the
     * serializable object via a typecollection.
     *
     * @return new_object. Note the use of a static cast to
     *     SerializableObject to avoid screwing up the pointers. If we had
     *     used a void* here, the pointer to the class may be off in the case
     *     of multiple inheritance.
     */
    SerializableObject* new_object() {
        return static_cast<SerializableObject*>(new _Class(Builder()));
    }

    const char* name() const { return name_; }

private:
    const char* name_;
};

/**
 * Macro to use to define a class to be used by the typecollection.
 */
#define TYPE_COLLECTION_DEFINE(_collection, _class, _typecode)          \
    oasys::TypeCollectionDispatch<_collection, _class>                  \
        _class ## TypeCollectionInstance(_typecode, #_collection "::" #_class);

/**
 * Define the typecollection C++ type -> typecode converter
 */
#define TYPE_COLLECTION_DECLARE(_Collection, _Class, _code)     \
namespace oasys {                                               \
    template<>                                                  \
    struct TypeCollectionCode<_Collection, _Class> {            \
        enum {                                                  \
            TYPECODE_LOW  = _code,                              \
            TYPECODE_HIGH = _code                               \
        };                                                      \
        enum {                                                  \
            TYPECODE = _code                                    \
        };                                                      \
    };                                                          \
}

/**
 * Define an aggregate supertype, e.g. if a range of type codes {1, 2,
 * 3} are assigned to classes A, B, C and they have a common abstract
 * serializable supertype S, then to unserialize an S object (which
 * could potentially be of concrete types A, B, C), you need to use
 * this macro in the type codes header:
 * @code
 * TYPE_COLLECTION_GROUP(Collection, S, 1, 3);
 * @endcode
 */
#define TYPE_COLLECTION_GROUP(_Collection, _Class, _low, _high) \
namespace oasys {                                               \
    template<>                                                  \
    struct TypeCollectionCode<_Collection, _Class> {            \
        enum {                                                  \
            TYPECODE_LOW  = _low,                               \
            TYPECODE_HIGH = _high,                              \
        };                                                      \
    };                                                          \
}

/**
 * Macro to wrap the annoyingly finicky template static instantiation.
 */
#define TYPE_COLLECTION_INSTANTIATE(_Collection)                \
template<> class oasys::TypeCollectionInstance<_Collection>*    \
   oasys::TypeCollectionInstance<_Collection>::instance_ = 0

}; // namespace oasys

#endif //__OASYS_TYPE_COLLECTION_H__
