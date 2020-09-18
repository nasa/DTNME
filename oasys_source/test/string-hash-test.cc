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

#include "util/UnitTest.h"
#include "util/StringUtils.h"

using namespace oasys;
using namespace std;
using namespace _std;

typedef StringHashSet TestHashSet;
typedef StringHashMap<int> TestHashMap;

DECLARE_TEST(HashSetSmokeTest) {
    TestHashSet s;

    CHECK(s.insert("the").second == true);
    CHECK(s.insert("quick").second == true);
    CHECK(s.insert("brown").second == true);
    CHECK(s.insert("fox").second == true);

    CHECK(s.find("quick") != s.end());
    CHECK(*(s.find("quick")) == "quick");
    CHECK(s.find("the") != s.end());
    CHECK(s.find("bogus") == s.end());

    CHECK(s.insert("the").second == false);
    CHECK(s.insert("xxx").second == true);
    CHECK(s.erase("xxx") == 1);
    CHECK(s.erase("xxx") == 0);
    CHECK(s.find("xxx") == s.end());

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(HashMapSmokeTest) {
    TestHashMap m;

    m["the"] = 1;
    m["quick"] = 2;
    m["brown"] = 3;
    m["fox"] = 4;

    CHECK(m.find("quick") != m.end());
    CHECK(m["quick"] == 2);
    CHECK(m.find("quick")->first == "quick");
    CHECK(m.find("quick")->second == 2);
    
    CHECK(m["quick"] == 2);
    m["quick"] = 5;
    CHECK(m["quick"] == 5);
    CHECK(m.find("quick")->first == "quick");
    CHECK(m.find("quick")->second == 5);
    
    CHECK(m.find("bogus") == m.end());

    CHECK(m.find("the") != m.end());
    CHECK(m["the"] == 1);

    CHECK(m.insert(TestHashMap::value_type("the", 100)).second == false);
    CHECK(m["the"] == 1);

    CHECK(m.insert(TestHashMap::value_type("xxx", 100)).second == true);
    CHECK(m["xxx"] == 100);

    CHECK(m.erase("xxx") == 1);
    CHECK(m.erase("xxx") == 0);

    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {    
    ADD_TEST(HashSetSmokeTest);
    ADD_TEST(HashMapSmokeTest);
}

DECLARE_TEST_FILE(Test, "string hash set/map test");
