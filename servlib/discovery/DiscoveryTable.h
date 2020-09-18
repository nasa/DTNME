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

#ifndef _DISCOVERY_TABLE_H_
#define _DISCOVERY_TABLE_H_

#include <list>
#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Singleton.h>

namespace dtn {

class Discovery;

typedef std::list<Discovery*> DiscoveryList;

class DiscoveryTable : public oasys::Singleton<DiscoveryTable,false>,
                       public oasys::Logger
{
public:
    virtual ~DiscoveryTable();

    static void init();

    bool add(const std::string& name, const char* addr_family,
             int argc, const char* argv[], const char** error);

    bool del(const std::string& name);

    void dump(oasys::StringBuffer* buf);

    void shutdown();

protected:
    friend class DiscoveryCommand;
    friend class oasys::Singleton<DiscoveryTable>;

    DiscoveryTable();

    bool find(const std::string& name, DiscoveryList::iterator* iter);

    DiscoveryList dlist_;
}; // class DiscoveryTable
    
}; // namespace dtn

#endif // _DISCOVERY_TABLE_H_
