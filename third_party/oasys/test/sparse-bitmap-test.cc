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
#  include <oasys-config.h>
#endif

#include "util/SparseBitmap.h"
#include "util/UnitTest.h"

using namespace oasys;
char buf[1024];

DECLARE_TEST(Extend) {
    SparseBitmap<int> b;
    CHECK_EQUAL(b.num_entries(), 0);
    CHECK_EQUAL(b.num_contiguous(), 0);
    CHECK_EQUAL(b.num_set(), 0);
    CHECK(!b.is_set(-1));
    CHECK(!b.is_set(0));
    CHECK(!b.is_set(1));
    CHECK(!b.is_set(-1));
    CHECK(!b.is_set(0));
    CHECK(!b.is_set(1));
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ ]");

    DO(b.set(0));
    CHECK(!b.is_set(-1));
    CHECK(b.is_set(0));
    CHECK(!b.is_set(1));
    CHECK_EQUAL(b.num_contiguous(), 1);
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_set(), 1);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0 ]");

    DO(b.set(0, 3));
    CHECK(!b.is_set(-1));
    CHECK(b.is_set(0, 3));
    CHECK(b.is_set(0));
    CHECK(b.is_set(1));
    CHECK(b.is_set(2));
    CHECK(!b.is_set(3));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 3);
    CHECK_EQUAL(b.num_set(), 3);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0..2 ]");

    DO(b.set(-10, 21));
    CHECK(b.is_set(-10, 21));
    CHECK(!b.is_set(-11));
    CHECK(b.is_set(-10));
    CHECK(b.is_set(10));
    CHECK(!b.is_set(11));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 21);
    CHECK_EQUAL(b.num_set(), 21);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -10..10 ]");

    DO(b.set(0));
    CHECK(b.is_set(-10, 21));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 21);
    CHECK_EQUAL(b.num_set(), 21);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -10..10 ]");

    DO(b.set(-10, 21));
    CHECK(b.is_set(-10, 21));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 21);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -10..10 ]");
    CHECK_EQUAL(b.num_set(), 21);

    DO(b.clear());
    DO(b.set(1));
    DO(b.set(2));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 2);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 1..2 ]");
    
    DO(b.set(0));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 3);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0..2 ]");
    
    DO(b.set(3, 3));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 6);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0..5 ]");
    
    DO(b.set(-3, 3));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 9);
    CHECK(b.is_set(-3, 9));
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -3..5 ]");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Sparse) {
    SparseBitmap<int> b;

    DO(b.set(0));
    DO(b.set(2));
    CHECK(b[0]);
    CHECK(!b[1]);
    CHECK(b[2]);
    CHECK_EQUAL(b.num_entries(), 2);
    CHECK_EQUAL(b.num_contiguous(), 1);
    CHECK_EQUAL(b.num_set(), 2);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0 2 ]");

    DO(b.set(-2));
    DO(b.set(4));
    CHECK(b[-2]);
    CHECK(!b[-1]);
    CHECK(b[0]);
    CHECK(!b[1]);
    CHECK(b[2]);
    CHECK(!b[3]);
    CHECK(b[4]);
    CHECK_EQUAL(b.num_entries(), 4);
    CHECK_EQUAL(b.num_contiguous(), 1);
    CHECK_EQUAL(b.num_set(), 4);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -2 0 2 4 ]");

    DO(b.set(-10, 4));
    CHECK_EQUAL(b.num_entries(), 5);
    CHECK_EQUAL(b.num_contiguous(), 4);
    CHECK_EQUAL(b.num_set(), 8);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ -10..-7 -2 0 2 4 ]");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Clear) {
    SparseBitmap<int> b;

    // erase whole entry
    DO(b.set(0));
    DO(b.clear(0));
    CHECK(!b[0]);
    CHECK_EQUAL(b.num_entries(), 0);
    CHECK_EQUAL(b.num_set(), 0);

    DO(b.set(0, 10));
    DO(b.clear(-1000, 2000));
    CHECK(!b[0]);
    CHECK_EQUAL(b.num_entries(), 0);
    CHECK_EQUAL(b.num_set(), 0);
    DO(b.clear());

    // shrink entry on left
    DO(b.set(0, 10));
    DO(b.clear(0, 5));
    CHECK(!b[0]);
    CHECK(!b[4]);
    CHECK(b[5]);
    CHECK(b[9]);
    CHECK(!b[10]);
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_set(), 5);
    CHECK_EQUAL(b.num_contiguous(), 5);
    DO(b.clear());

    // shrink entry on right
    DO(b.set(0, 10));
    DO(b.clear(5, 5));
    CHECK(b[0]);
    CHECK(b[4]);
    CHECK(!b[5]);
    CHECK(!b[9]);
    CHECK(!b[10]);
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 5);
    CHECK_EQUAL(b.num_set(), 5);
    DO(b.clear());

    // shrink two
    DO(b.set(10, 10));
    DO(b.set(30, 10));
    CHECK_EQUAL(b.num_entries(), 2);
    CHECK_EQUAL(b.num_set(), 20);
    
    DO(b.clear(15, 20));
    CHECK(b.is_set(10));
    CHECK(b.is_set(14));
    CHECK(!b.is_set(15));
    CHECK(!b.is_set(34));
    CHECK(b.is_set(35));
    CHECK(b.is_set(39));
    CHECK(!b.is_set(40));
    CHECK_EQUAL(b.num_entries(), 2);
    CHECK_EQUAL(b.num_set(), 10);

    // don't do anything
    DO(b.clear(0, 100));
    CHECK_EQUAL(b.num_set(), 0);
    CHECK_EQUAL(b.num_entries(), 0);
    DO(b.set(10, 10));
    CHECK_EQUAL(b.num_set(), 10);
    DO(b.clear(0, 10));
    DO(b.clear(20, 10));
    CHECK_EQUAL(b.num_set(), 10);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Compact) {
    SparseBitmap<int> b;

    DO(b.set(1));
    DO(b.set(3));
    DO(b.set(5));
    DO(b.set(7));
    DO(b.set(9));

    CHECK_EQUAL(b.num_contiguous(), 1);
    CHECK_EQUAL(b.num_entries(), 5);
    CHECK_EQUAL(b.num_set(), 5);

    DO(b.set(1, 5));
    CHECK_EQUAL(b.num_contiguous(), 5);
    CHECK_EQUAL(b.num_entries(), 3);
    CHECK_EQUAL(b.num_set(), 7);
    
    DO(b.set(1, 9));
    CHECK_EQUAL(b.num_contiguous(), 9);
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_set(), 9);

    DO(b.clear());
    DO(b.set(0));
    DO(b.set(10));
    DO(b.set(0, 10));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 11);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 0..10 ]");
    
    DO(b.clear());
    DO(b.set(10));
    DO(b.set(5, 5));
    CHECK_EQUAL(b.num_entries(), 1);
    CHECK_EQUAL(b.num_contiguous(), 6);
    DO(b.format(buf, sizeof(buf)));
    CHECK_EQUALSTR(buf, "[ 5..10 ]");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SetClear) {
    SparseBitmap<int> b;

    DO(b.set(1));
    DO(b.set(3));
    DO(b.set(5));
    DO(b.set(7));

    DO(b.clear(1));
    CHECK_EQUAL(b.first(), 3);
    CHECK_EQUAL(b.num_entries(), 3);

    DO(b.clear(3));
    CHECK_EQUAL(b.first(), 5);
    CHECK_EQUAL(b.num_entries(), 2);

    DO(b.clear(5));
    CHECK_EQUAL(b.first(), 7);
    CHECK_EQUAL(b.num_entries(), 1);

    DO(b.clear(7));
    CHECK_EQUAL(b.num_entries(), 0);

    DO(b.set(1));
    DO(b.set(2));
    DO(b.set(3));

    CHECK_EQUAL(b.first(), 1);
    CHECK_EQUAL(b.num_set(), 3);

    DO(b.clear(1));
    CHECK_EQUAL(b.first(), 2);
    CHECK_EQUAL(*(b.begin()), 2);
    CHECK_EQUAL(b.num_set(), 2);

    DO(b.clear(2));
    CHECK_EQUAL(b.first(), 3);
    CHECK_EQUAL(*(b.begin()), 3);
    CHECK_EQUAL(b.num_set(), 1);

    DO(b.clear(3));
    CHECK_EQUAL(b.num_entries(), 0);
    CHECK(b.begin() == b.end());
    
    DO(b.set(1));
    DO(b.set(2));
    DO(b.set(3));

    CHECK_EQUAL(b.last(), 3);
    CHECK_EQUAL(b.num_set(), 3);

    DO(b.clear(3));
    CHECK_EQUAL(b.last(), 2);
    CHECK_EQUAL(*(--b.end()), 2);
    CHECK_EQUAL(b.num_set(), 2);

    DO(b.clear(2));
    CHECK_EQUAL(b.last(), 1);
    CHECK_EQUAL(*(--b.end()), 1);
    CHECK_EQUAL(b.num_set(), 1);

    DO(b.clear(1));
    CHECK_EQUAL(b.num_entries(), 0);
    CHECK(b.begin() == b.end());
    

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Iterator) {
    SparseBitmap<int> b;

    SparseBitmap<int>::iterator i, j, k;

    DO(i = b.begin());
    CHECK(i == b.end());
    CHECK(b.begin() == b.end());

    DO(b.set(1));
    DO(i = b.begin());
    CHECK_EQUAL(*i, 1);
    CHECK(i == b.begin());
    CHECK(i != b.end());
    CHECK(b.first() == 1);
    CHECK(b.last() == 1);
    DO(++i);
    CHECK(i == b.end());
    DO(--i);
    CHECK(i != b.end());
    CHECK(i == b.begin());
    CHECK_EQUAL(*i, 1);

    DO(b.set(2));
    DO(b.set(5, 2));
    DO(i = b.begin());
    CHECK_EQUAL(*i++, 1);
    CHECK_EQUAL(*i++, 2);
    CHECK_EQUAL(*i++, 5);
    CHECK_EQUAL(*i++, 6);
    CHECK(i == b.end());
    CHECK_EQUAL(*--i, 6);
    CHECK_EQUAL(*--i, 5);
    CHECK_EQUAL(*--i, 2);
    CHECK_EQUAL(*--i, 1);
    CHECK(i == b.begin());
    CHECK(b.first() == 1);
    CHECK(b.last() == 6);

    CHECK_EQUAL(*i++, 1);
    CHECK_EQUAL(*i, 2);
    CHECK_EQUAL(*++i, 5);
    CHECK_EQUAL(*i, 5);

    DO(i = b.begin());
    DO(i.skip_contiguous());
    CHECK_EQUAL(*i, 2);
    DO(i.skip_contiguous());
    CHECK_EQUAL(*i, 2);
    DO(i++);
    CHECK_EQUAL(*i, 5);
    DO(i.skip_contiguous());
    CHECK_EQUAL(*i, 6);
    DO(i.skip_contiguous());
    CHECK_EQUAL(*i, 6);

    DO(i = b.end() - 1);
    CHECK_EQUAL(*i, 6);

    DO(i = b.begin());
    CHECK_EQUAL(*i, 1);
    DO(i += 1);
    CHECK_EQUAL(*i, 2);
    DO(i += 2);
    CHECK_EQUAL(*i, 6);

    DO(i -= 3);
    CHECK_EQUAL(*i, 1);
    
    DO(i += 2);
    CHECK_EQUAL(*i, 5);
    DO(i -= 2);
    CHECK_EQUAL(*i, 1);

    DO(i += 2);
    DO(i -= 1);
    CHECK_EQUAL(*i, 2);

    DO(b.set(10, 10000000));
    DO(i = b.begin());
    CHECK_EQUAL(*i, 1);
    DO(i += 4);
    CHECK_EQUAL(*i, 10);
    DO(i += (10000000 - 1));
    CHECK_EQUAL(*i, 10000009);
    DO(i++);
    CHECK(i == b.end());
    DO(i--);
    CHECK_EQUAL(*i, 10000009);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Extend);
    ADD_TEST(Sparse);
    ADD_TEST(Clear);
    ADD_TEST(Compact);
    ADD_TEST(SetClear);
    ADD_TEST(Iterator);
}

DECLARE_TEST_FILE(Test, "sparse bitmap test");
