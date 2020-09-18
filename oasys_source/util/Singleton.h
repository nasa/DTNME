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

/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include "../debug/DebugUtils.h"

namespace oasys {

/**
 * Singleton utility template class. Usage:
 *
 * @code
 * // in .h file:
 * class MyClass : public oasys::Singleton<MyClass> {
 * private:
 *     friend class oasys::Singleton<MyClass>;
 *     MyClass();
 * };
 *
 * // in .cc file:
 * MyClass* oasys::Singleton<MyClass>::instance_;
 * @endcode
 *
 * Note that you should define a destructor for the class since it
 * will be called at process exit.
 */
template<typename _Class, bool _auto_create = true>
class Singleton;

/**
 * Common base class used to store all the singleton pointers and
 * allow for exit-time deletion of the instances.
 */
class SingletonBase {
public:
    /// Constructor that adds this instance to the array of all
    /// singletons. Must be called in a single-threaded context.
    SingletonBase();
    
    /// Virtual destructor to be overridden by derived classes.
    virtual ~SingletonBase();
    
    /// Array of pointers to all singletons
    static SingletonBase** all_singletons_;
    
    /// Count of the number of singletons
    static int num_singletons_;

    /// Flag to destroy singletons on exits
    //dz debug
    static bool destroy_singletons_on_exit_;

private:
    /**
     * Inner class that is instantiated once per program and is used
     * to delete all the singletons when the program exits.
     */
    class Fini {
    public:
        /// Destructor that clears out all the singletons
        ~Fini();
    };

    static Fini fini_;
};

/**
 * Singleton template with autocreation.
 */
template<typename _Class>
class Singleton<_Class, true> : public SingletonBase {
public:
    static _Class* instance() {
        // XXX/demmer this has potential race conditions if multiple
        // threads try to access the singleton for the first time
        
        if(instance_ == 0) {
            instance_ = new _Class();
        }
        ASSERT(instance_);

        return instance_;
    }
    
    static _Class* create() {
        if (instance_) {
            PANIC("Singleton create() method called more than once");
        }
        
        instance_ = new _Class();
        return instance_;
    }

    static void set_instance(_Class* instance) {
        if (instance_) {
            PANIC("Singleton set_instance() called with existing object");
        }
        instance_ = instance;
    }

    /**
     * PLEASE DON'T USE THIS UNLESS YOU REALLY KNOW WHAT YOU'RE DOING,
     * AS THIS CLEARLY BREAKS THE SINGLETON CONTRACT.
     *
     * Used if you really want to be able to replace the instance
     * (i.e. if it's not acting as a true singleton).
     */
    static void force_set_instance(_Class* instance) {
        instance_ = instance;
    }
    
protected:
    static _Class* instance_;
};

/**
 * Singleton template with no autocreation.
 */
template<typename _Class>
class Singleton<_Class, false> : public SingletonBase {
public:
    static _Class* instance() 
    {
        // XXX/demmer this has potential race conditions if multiple
        // threads try to access the singleton for the first time
        ASSERT(instance_);
        return instance_;
    }
    
    static _Class* create() 
    {
        if (instance_) 
        {
            PANIC("Singleton create() method called more than once");
        }
        
        instance_ = new _Class();
        return instance_;
    }

    static void set_instance(_Class* instance) 
    {
        if (instance_) 
        {
            PANIC("Singleton set_instance() called with existing object");
        }
        instance_ = instance;
    }
    
    /**
     * PLEASE DON'T USE THIS UNLESS YOU REALLY KNOW WHAT YOU'RE DOING,
     * AS THIS CLEARLY BREAKS THE SINGLETON CONTRACT.
     *
     * Used if you really want to be able to replace the instance
     * (i.e. if it's not acting as a true singleton).
     */
    static void force_set_instance(_Class* instance) {
        instance_ = instance;
    }
protected:
    static _Class* instance_;
};

/**
 * Reference to a Singleton. Usage:
 *
 * @code
 * void myfunc() {
 *     oasys::SingletonRef<MySingletonFoo> foo;
 *     foo->bar();
 * }
 * @endcode
 */
template<typename _Class>
class SingletonRef {
public:
    _Class* operator->() {
        return Singleton<_Class>::instance();
    }
};

} // namespace oasys

#endif // __SINGLETON_H__
