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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "InterfaceTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include "oasys/util/StringBuffer.h"

namespace dtn {

InterfaceTable* InterfaceTable::instance_ = NULL;

InterfaceTable::InterfaceTable()
    : Logger("InterfaceTable", "/dtn/interface/table"),
      interfaces_activated_(false)
{
}

InterfaceTable::~InterfaceTable()
{
    oasys::ScopeLock sl(&lock_, "~InterfaceTable");

    //NOTREACHED;
    //printf("Deleting InterfaceTable\n");
    InterfaceList::iterator iter;
    for(iter = iflist_.begin();iter != iflist_.end(); iter++) {
        (*iter)->clayer()->interface_down(*iter);
        delete *iter;
    }

}
void InterfaceTable::shutdown() {
    //printf("In InterfaceTable::shutdown\n");
    delete instance_;
}

/**
 * Internal method to find the location of the given interface in the
 * list.
 */
bool
InterfaceTable::find(const std::string& name,
                     InterfaceList::iterator* iter)
{
    oasys::ScopeLock sl(&lock_, "InterfaceTable::find");

    Interface* iface;
    for (*iter = iflist_.begin(); *iter != iflist_.end(); ++(*iter)) {
        iface = **iter;
        
        if (iface->name() == name) {
            return true;
        }
    }        
    
    return false;
}

/**
 * Create and add a new interface to the table. Returns true if
 * the interface is successfully added, false if the interface
 * specification is invalid.
 */
bool
InterfaceTable::add(const std::string& name,
                    ConvergenceLayer* cl,
                    const char* proto,
                    int argc, const char* argv[])
{
    InterfaceList::iterator iter;
    
    oasys::ScopeLock sl(&lock_, "InterfaceTable::add");

    if (find(name, &iter)) {
        log_err("interface %s already exists", name.c_str());
        return false;
    }
    
    log_info("adding interface %s (%s)", name.c_str(), proto);

    Interface* iface = new Interface(name, proto, cl);

    if (! cl->interface_up(iface, argc, argv)) {
        log_err("convergence layer error adding interface %s", name.c_str());
        delete iface;
        return false;
    }

    iflist_.push_back(iface);

    if (interfaces_activated_) {
        cl->interface_activate(iface);
    }

    return true;
}

/**
 * Start the Interfaces running
 */
void
InterfaceTable:: activate_interfaces()
{
    InterfaceList::iterator iter;
    Interface* iface;

    oasys::ScopeLock sl(&lock_, "InterfaceTable::start_interfaces");

    for (iter = iflist_.begin(); iter != iflist_.end(); ++(iter)) {
        iface = *iter;
        iface->clayer()->interface_activate(iface);
    }

    interfaces_activated_ = true;
}


/**
 * Remove the specified interface.
 */
bool
InterfaceTable::del(const std::string& name)
{
    InterfaceList::iterator iter;
    Interface* iface;
    bool retval = false;
    
    log_info("removing interface %s", name.c_str());

    oasys::ScopeLock sl(&lock_, "InterfaceTable::del");

    if (! find(name, &iter)) {
        log_err("error removing interface %s: no such interface",
                name.c_str());
        return false;
    }

    iface = *iter;
    iflist_.erase(iter);

    if (iface->clayer()->interface_down(iface)) {
        retval = true;
    } else {
        log_err("error deleting interface %s from the convergence layer.",
                name.c_str());
        retval = false;
    }

    delete iface;
    return retval;
}

/**
 * Dumps the interface table into a string.
 */
void
InterfaceTable::list(oasys::StringBuffer *buf)
{
    InterfaceList::iterator iter;
    Interface* iface;

    oasys::ScopeLock sl(&lock_, "InterfaceTable::list");

    for (iter = iflist_.begin(); iter != iflist_.end(); ++(iter)) {
        iface = *iter;
        buf->appendf("%s: Convergence Layer: %s\n",
                     iface->name().c_str(), iface->proto().c_str());
        iface->clayer()->dump_interface(iface, buf);
        buf->append("\n");
    }
}

} // namespace dtn
