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

#ifndef _INTERFACE_TABLE_H_
#define _INTERFACE_TABLE_H_

#include <string>
#include <list>
#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

class ConvergenceLayer;
class Interface;

/**
 * The list of interfaces.
 */
typedef std::list<Interface*> InterfaceList;

/**
 * Class for the in-memory interface table.
 */
class InterfaceTable : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static InterfaceTable* instance() {
        if (instance_ == NULL) {
            PANIC("InterfaceTable::init not called yet");
        }
        return instance_;
    }
    static void shutdown();

    /**
     * Boot time initializer that takes as a parameter the actual
     * storage instance to use.
     */
    static void init() {
        if (instance_ != NULL) {
            PANIC("InterfaceTable::init called multiple times");
        }
        instance_ = new InterfaceTable();
    }

    /**
     * Constructor
     */
    InterfaceTable();

    /**
     * Destructor
     */
    virtual ~InterfaceTable();

    /**
     * Add a new interface to the table. Returns true if the interface
     * is successfully added, false if the interface specification is
     * invalid (or it already exists).
     */
    bool add(const std::string& name, ConvergenceLayer* cl, const char* proto,
             int argc, const char* argv[]);
    
    /**
     * Remove the specified interface.
     */
    bool del(const std::string& name);
    
    /**
     * List the current interfaces.
     */
    void list(oasys::StringBuffer *buf);

    /**
     * Signal from BundleDaemon to start Interfaces running
     */
    void activate_interfaces();

protected:

    static InterfaceTable* instance_;

    // serialize access
    oasys::SpinLock lock_;

    /**
     * All interfaces are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_interfaces.
     */
    InterfaceList iflist_;

    /**
     * Internal method to find the location of the given interface in
     * the list.
     */
    bool find(const std::string& name, InterfaceList::iterator* iter);


    // Whether or not thhe BundleDaemon has signaled Intreface are allowed to start
    bool interfaces_activated_;
};

} // namespace dtn

#endif /* _INTERFACE_TABLE_H_ */
