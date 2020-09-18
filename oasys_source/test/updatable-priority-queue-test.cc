/*
 *    Copyright 2007 Intel Corporation
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
#include "util/Random.h"
#include "util/UpdatablePriorityQueue.h"

DECLARE_TEST(Basic) {
    oasys::UpdatablePriorityQueue<int> q;
    DO(q.push(2));
    DO(q.push(10));
    DO(q.push(3));
    DO(q.push(4));

    CHECK_EQUAL(q.sequence()[0], 10);
    CHECK_EQUAL(q.sequence()[1], 4);
    CHECK_EQUAL(q.sequence()[2], 3);
    CHECK_EQUAL(q.sequence()[3], 2);

    DO(q.sequence()[1] = 1);
    DO(q.update(1));
    
    CHECK_EQUAL(q.sequence()[0], 10);
    CHECK_EQUAL(q.sequence()[1], 2);
    CHECK_EQUAL(q.sequence()[2], 3);
    CHECK_EQUAL(q.sequence()[3], 1);

    DO(q.sequence()[3] = 20);
    DO(q.update(20));
    
    CHECK_EQUAL(q.sequence()[0], 20);
    CHECK_EQUAL(q.sequence()[1], 10);
    CHECK_EQUAL(q.sequence()[2], 3);
    CHECK_EQUAL(q.sequence()[3], 2);

    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TEST(Random) {
    int num_loops = 10;
    int num_updates = 100;
    int n = 1024;

    for (int j = 0; j < num_loops; ++j) {
        oasys::UpdatablePriorityQueue<int> q;
        
        for (int i = 0; i < n; ++i) {
            q.push(oasys::Random::rand(10000));
        }

        for (int k = 0; k < num_updates; ++k) {
            int x = oasys::Random::rand(1024);
            q.sequence()[x] = oasys::Random::rand(10000);
            oasys::UpdatablePriorityQueue<int>::iterator iter = q.sequence().begin();
            iter += x;
            q.update(iter);
        }

        int last = q.top();
        q.pop();
        while (!q.empty()) {
            int cur = q.top();
            q.pop();

            if (! (cur <= last)) {
                CHECK_LT(cur, last);
            }
            last = cur;
        }
    }

    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Basic);
    ADD_TEST(Random);
}

DECLARE_TEST_FILE(Test, "updatable priority queue test");
