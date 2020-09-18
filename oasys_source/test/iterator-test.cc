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
#ifdef HAVE_CONFIG_H
#include <oasys-config.h>
#endif

#include "util/UnitTest.h"
#include "util/Iterators.h"

using namespace oasys;

typedef CountIterator<int> CItr;
typedef CombinedIterator
<
    CountIterator<int>, 
    std::vector<int>::const_iterator, 
    int
> CmItr;

struct Evens {
    bool operator()(int num) {
        return num % 2 == 0;
    }
};

typedef PredicateIterator<CmItr, int, Evens> EvenCmItr;

DECLARE_TEST(CountItrEmpty) {
    CItr begin;
    CItr end;
    
    CItr::get_endpoints(&begin, &end, 3, 3);
    for (CItr i = begin; i != end; ++i)
    {
        // shouldn't iterate at all
        CHECK(false);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(CountItr) {
    CItr begin;
    CItr end;
    
    CItr::get_endpoints(&begin, &end, 3, 6);
    CItr i = begin;
    CHECK(*i == 3);
    ++i;
    CHECK(*i == 4);
    ++i;
    CHECK(*i == 5);
    ++i;
    CHECK(i == end);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(CombinedItr) {
    CItr int_begin;
    CItr int_end;
    
    CItr::get_endpoints(&int_begin, &int_end, 3, 3);
    CmItr cm_begin, cm_end, cm_i;
    std::vector<int> v;
    CmItr::get_endpoints(&cm_begin, &cm_end, 
                         int_begin, int_end, v.begin(), v.end());
    for (cm_i = cm_begin; cm_i != cm_end; ++cm_i)
    {
        // should not be here
        CHECK(false);
    }
        
    CItr::get_endpoints(&int_begin, &int_end, 3, 9);
    CmItr::get_endpoints(&cm_begin, &cm_end, 
                         int_begin, int_end, v.begin(), v.end());
    cm_i = cm_begin;
    CHECK(*cm_i == 3);
    ++cm_i;
    CHECK(*cm_i == 4);
    ++cm_i;
    CHECK(*cm_i == 5);
    ++cm_i;
    CHECK(*cm_i == 6);
    ++cm_i;
    CHECK(*cm_i == 7);
    ++cm_i;
    CHECK(*cm_i == 8);
    ++cm_i;
    CHECK(cm_i == cm_end);
    
    v.push_back(11);
    v.push_back(22);
    v.push_back(33);
    CmItr::get_endpoints(&cm_begin, &cm_end, 
                         int_begin, int_end, v.begin(), v.end());
    cm_i = cm_begin;
    CHECK(*cm_i == 3);
    ++cm_i;
    CHECK(*cm_i == 4);
    ++cm_i;
    CHECK(*cm_i == 5);
    ++cm_i;
    CHECK(*cm_i == 6);
    ++cm_i;
    CHECK(*cm_i == 7);
    ++cm_i;
    CHECK(*cm_i == 8);
    ++cm_i;
    CHECK(*cm_i == 11);
    ++cm_i;
    CHECK(*cm_i == 22);
    ++cm_i;
    CHECK(*cm_i == 33);
    ++cm_i;
    CHECK(cm_i == cm_end);

    Evens pred;
    EvenCmItr evens(cm_begin, cm_end, pred);
    
    CHECK(*evens == 4);
    ++evens;
    CHECK(*evens == 6);
    ++evens;
    CHECK(*evens == 8);
    ++evens;
    CHECK(*evens == 22);
    ++evens;
    CHECK(evens == cm_end);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ItrTest) {
    ADD_TEST(CountItrEmpty);
    ADD_TEST(CountItr);
    ADD_TEST(CombinedItr);
}

DECLARE_TEST_FILE(ItrTest, "iterator test");
