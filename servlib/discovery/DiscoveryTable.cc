/*
 *    Copyright 2006 Baylor University
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "Discovery.h"
#include "DiscoveryTable.h"
#include "conv_layers/ConvergenceLayer.h"

namespace oasys {
    template<> dtn::DiscoveryTable* oasys::Singleton<dtn::DiscoveryTable,false>::instance_ = NULL;
}

namespace dtn {

DiscoveryTable::DiscoveryTable()
    : Logger("DiscoveryTable","/dtn/discovery/table")
{
}

DiscoveryTable::~DiscoveryTable()
{
}

void
DiscoveryTable::init()
{
    if (instance_ != NULL)
    {
        PANIC("DiscoveryTable already initialized");
    }
    instance_ = new DiscoveryTable();
}


void
DiscoveryTable::shutdown()
{
    for(DiscoveryList::iterator i = dlist_.begin();
        i != dlist_.end();
        i++)
    {
        Discovery* d = *i;
        d->shutdown();
        //delete d;
    }
    dlist_.clear();
}

bool
DiscoveryTable::find(const std::string& name,
                     DiscoveryList::iterator* iter)
{
    Discovery* disc;
    for (*iter = dlist_.begin(); *iter != dlist_.end(); (*iter)++)
    {
        disc = **iter;
        if (disc->name() == name)
            return true;
    }
    return false;
}

bool
DiscoveryTable::add(const std::string& name,
                    const char* afname,
                    int argc, const char* argv[],
                    const char** error)
{
    DiscoveryList::iterator iter;

    if (find(name, &iter))
    {
        *error = "agent exists with that name";
        return false;
    }

    std::string af(afname);
    Discovery* disc = Discovery::create_discovery(name, afname, argc, argv,
                                                  error);

    if (disc == NULL) {
        return false;
    }
    
    log_info("adding discovery agent %s (%s)",name.c_str(),afname);
    
    dlist_.push_back(disc);
    return true;
}

bool
DiscoveryTable::del(const std::string& name)
{
    DiscoveryList::iterator iter;
    Discovery* disc;

    log_info("removing discovery agent %s",name.c_str());

    if(! find(name,&iter))
    {
        log_err("error removing agent %s: no such agent",name.c_str());
        return false;
    }

    disc = *iter;
    dlist_.erase(iter);
    disc->shutdown(); 

    return true;
}

void
DiscoveryTable::dump(oasys::StringBuffer* buf)
{
    DiscoveryList::iterator iter;
    Discovery* disc;

    buf->appendf("\nDiscovery: %zu agents\n"
                 "---------\n",dlist_.size());
    for (iter = dlist_.begin(); iter != dlist_.end(); iter++)
    {
        disc = *iter;
        disc->dump(buf);
        buf->append("\n");
    }
}

} // namespace dtn
