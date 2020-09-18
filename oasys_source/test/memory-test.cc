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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif
#include <list>
#include "../debug/Log.h"
#include "../memory/Memory.h"

using namespace oasys;

class Foo_1 {};
std::list<Foo_1*> lFoos;

void
Alloc_1()
{
    lFoos.push_back(new Foo_1());
}

void
Alloc_2()
{
    lFoos.push_back(new Foo_1());
}

void Alloc_3()
{
    lFoos.push_back(new Foo_1());

    Alloc_1();
    Alloc_2();
}

class Big { char buf[100000]; };

void
delete_all_foo()
{
    while(lFoos.size() > 0)
    {
        Foo_1* obj = lFoos.front();
        delete obj;

        lFoos.pop_front();
    }
}

int
main(int argc, char* argv[])
{
#if 0
    Log::init(LOG_DEBUG);

#if ! OASYS_DEBUG_MEMORY_ENABLED
    log_crit("/memory", "test must be run with memory debugging enabled");
#else
    
    DbgMemInfo::init(DbgMemInfo::USE_SIGNAL, "/tmp/dump");

    log_info("/memory", "offset of data=%u\n", 
             offsetof(dbg_mem_t, block_));

    // Create 11 Foo_1 object in different places
    Alloc_3();
    Alloc_3();
    Alloc_3();
    Alloc_2();
    Alloc_1();

    DbgMemInfo::debug_dump();
    FILE* f = fopen("dump", "w");
    ASSERT(f != 0);

    // Delete all Foo_1 objects
    delete_all_foo();
    DbgMemInfo::debug_dump();

    std::list<int> l;
    l.push_back(2);
    l.push_back(3);
    l.push_back(4);
    l.push_back(5);
    l.push_back(6);
    l.push_back(7);
    l.push_back(8);
    l.push_back(9);
    l.push_back(1);
    DbgMemInfo::debug_dump(); 

    new Big();    

    while(1);
#endif
#endif
}
