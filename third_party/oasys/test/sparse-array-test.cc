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
#include "util/SparseArray.h"

using namespace oasys;

typedef SparseArray<char> SA;

void 
dump_range(const SA& sa)
{
    char buf[256];


    for (SA::BlockList::const_iterator itr = sa.blocks().begin();
         itr != sa.blocks().end(); ++itr)
    {
        memset(buf, 0, 256);
        memcpy(buf, itr->data_, itr->size_);
        printf("(offset %zu size %zu \"%s\") ", 
               itr->offset_, itr->size_, buf);
    }
    printf("\n");
}

bool
check_range(const SA&     sa, 
            const size_t* offsets,
            const size_t* sizes,
            const char**  data,
            size_t        count)
{
    dump_range(sa);
    
    size_t i = 0;
    for (SA::BlockList::const_iterator itr = sa.blocks().begin();
         itr != sa.blocks().end(); ++itr)
    {
        if (count - i == 0 ||
            itr->offset_ != offsets[i] || 
            itr->size_   != sizes[i])
        {
            return false;
        }

        if (memcmp(data[i], itr->data_, itr->size_) != 0)
        {
            return false;
        }
        ++i;
    }

    return true;
}

DECLARE_TEST(Test1) {
    SA sa;
    CHECK(sa.size() == 0);

    sa.range_write(1, 4, "1234");
    sa.range_write(6, 9, "123456789");
    sa.range_write(20, 3, "123");

    size_t      offsets[] = { 1, 6, 20 };
    size_t      sizes[]   = { 4, 9, 3 };
    const char* data[]    = { "1234", "123456789", "123" };
    
    CHECK(check_range(sa, offsets, sizes, data, 3));
    CHECK(sa.size() == 23);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test2) {
    SA sa;

    sa.range_write(1, 4, "1234");
    sa.range_write(1, 4, "5678");
    sa.range_write(7, 3, "123");

    size_t offsets[]   = {1, 7};
    size_t sizes[]     = {4, 3};
    const char* data[] = { "5678", "123"};
    
    CHECK(check_range(sa, offsets, sizes, data, 2));
    CHECK(sa.size() == 10);    

    return UNIT_TEST_PASSED;
}


DECLARE_TEST(Test3) {
    SA sa;

    sa.range_write(1, 4, "1234");
    sa.range_write(3, 4, "1234");
    sa.range_write(4, 5, "12345");
    sa.range_write(5, 5, "12345");

    size_t offsets[]      = {1};
    size_t sizes[]        = {9};
    const char* data[]    = {"121112345"};
    
    CHECK(check_range(sa, offsets, sizes, data, 1));
    CHECK(sa.size() == 10);    

    return UNIT_TEST_PASSED;
}


DECLARE_TEST(Test4) {
    SA sa;

    sa.range_write(2, 1, "1");
    sa.range_write(4, 1, "2");
    sa.range_write(7, 1, "3");
    sa.range_write(0, 10, "1234567890");

    size_t offsets[]      = {0};
    size_t sizes[]        = {10};
    const char* data[]    = { "1234567890"};
    
    CHECK(check_range(sa, offsets, sizes, data, 1));
    CHECK(sa.size() == 10);        

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test5) {
    SA sa;

    sa.range_write(1, 10,  "1234567890");
    sa.range_write(11, 10, "1234567890");
    sa.range_write(21, 10, "1234567890");

    size_t offsets[]      = {1, 11, 21};
    size_t sizes[]        = {10, 10, 10};
    const char* data[]    = {"1234567890", "1234567890", "1234567890"};
    
    CHECK(check_range(sa, offsets, sizes, data, 3));
    CHECK(sa.size() == 31);    

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test6) {
    SA sa;

    sa.range_write(3, 5,  "aaaaa");
    sa.range_write(0, 4,  "bbbb");
    sa.range_write(9, 11, "ccccccccccc");
    sa.range_write(7, 7,  "ddddddd");

    size_t offsets[]      = {0};
    size_t sizes[]        = {20};
    //                        12345678901234567890
    const char* data[]    = {"bbbbaaadddddddcccccc"};
    
    CHECK(check_range(sa, offsets, sizes, data, 1));
    CHECK(sa.size() == 20);    
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test7) {
    SA sa;

    sa.range_write(1, 10, "xxxxxxxxxx");
    sa.range_write(11, 10, "yyyyyyyyyy");
    sa.range_write(21, 10, "zzzzzzzzzz");

    size_t offsets[]      = {1, 11, 21};
    size_t sizes[]        = {10, 10, 10};
    const char* data[]    = {"xxxxxxxxxx", "yyyyyyyyyy", "zzzzzzzzzz"};
    
    CHECK(check_range(sa, offsets, sizes, data, 3));
    CHECK(sa.size() == 31);    
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test8) {
    SA sa;

    sa.range_write(3, 7,  "aaaaaaa");
    sa.range_write(2, 8, "bbbbbbbb");
    sa.range_write(0, 10, "cccccccccc");

    size_t offsets[]      = {0};
    size_t sizes[]        = {10};
    const char* data[]    = {"cccccccccc"};
    
    CHECK(check_range(sa, offsets, sizes, data, 1));
    CHECK(sa.size() == 10);    

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test9) {
    SA sa;

    sa.range_write(0, 4, "aaaa");
    sa.range_write(6, 4, "bbbb");
    sa.range_write(2, 6, "cccccc");

    size_t offsets[]      = {0};
    size_t sizes[]        = {10};
    const char* data[]    = {"aaccccccbb"};
    
    CHECK(check_range(sa, offsets, sizes, data, 1));
    CHECK(sa.size() == 10);    
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Test10) {
    SA sa;

    sa.range_write(0, 4, "aaaa");
    sa.range_write(6, 4, "bbbb");
    sa.range_write(2, 6, "cccccc");
    sa.range_write(20, 2, "dd");
    
    size_t offsets[]      = {0, 20};
    size_t sizes[]        = {10, 2};
    const char* data[]    = {"aaccccccbb", "dd"};
    
    CHECK(check_range(sa, offsets, sizes, data, 2));

    CHECK(sa[0] == 'a');
    CHECK(sa[2] == 'c');
    CHECK(sa[20] == 'd');
    CHECK(sa[21] == 'd');
    CHECK(sa[100] == 0);
    CHECK(sa[1000] == 0);
    CHECK(sa[10000] == 0);
    CHECK(sa[100000] == 0);
    CHECK(sa.size() == 22);    

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Test1);
    ADD_TEST(Test2);
    ADD_TEST(Test3);
    ADD_TEST(Test4);
    ADD_TEST(Test5);
    ADD_TEST(Test6);
    ADD_TEST(Test7);
    ADD_TEST(Test8);
    ADD_TEST(Test9);
    ADD_TEST(Test10);
}

DECLARE_TEST_FILE(Test, "sparse array test");
