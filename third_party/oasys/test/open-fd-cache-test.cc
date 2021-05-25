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

#include <string>

#include "util/UnitTest.h"
#include "util/OpenFdCache.h"

using namespace oasys;

struct DummyClose {
    static void close(int fd) {
        (void)fd;
        log_debug_p("/test", "Syscall close(%d)", fd);
    }
};

DECLARE_TEST(Test1) {
    OpenFdCache<std::string, DummyClose> cache("/test/cache", 3);
    
    CHECK(cache.get_and_pin("test1") == -1);
    CHECK(cache.get_and_pin("test2") == -1);

    cache.put_and_pin("test1", 1);
    cache.unpin("test1");

    cache.put_and_pin("test2", 2);
    cache.unpin("test2");

    cache.put_and_pin("test3", 3);
    cache.unpin("test3");

    cache.put_and_pin("test4", 4);
    cache.unpin("test4");

    cache.put_and_pin("test5", 5);
    cache.unpin("test5");

    cache.put_and_pin("test6", 6);
    cache.unpin("test6");

    CHECK(cache.get_and_pin("test5") == 5);
    cache.unpin("test5");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Test1);
}

DECLARE_TEST_FILE(Test, "open-fd-cache-test");
