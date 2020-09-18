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
#include "util/StringAppender.h"

using namespace oasys;

#define SIZE 20

char buf[SIZE];

void 
reset_buf()
{
    memset(buf, '*', SIZE);
}

char*
convert_buf()
{
    static char ugly[SIZE];
    
    for (int i=0; i<SIZE; ++i)
    {
        if (buf[i] == '\0')
        {
            ugly[i] = '|';
        }
        else
        {
            ugly[i] = buf[i];
        }
    }
    return ugly;
}   

DECLARE_TEST(Test1) {
    reset_buf();
    StringAppender sa(buf, SIZE);
    
    CHECK_EQUAL(sa.append("hello"), 5);
    CHECK_EQUALSTR("hello|**************", convert_buf());
    CHECK_EQUAL(sa.length(), 5);
    CHECK_EQUAL(sa.desired_length(), 5);

    CHECK_EQUAL(sa.append(' '), 1);
    CHECK_EQUALSTR("hello |*************", convert_buf());
    CHECK_EQUAL(sa.length(), 6);
    CHECK_EQUAL(sa.desired_length(), 6);

    size_t ret = sa.appendf("%d", 12345);
    CHECK_EQUAL(ret, 5);
    CHECK_EQUALSTR("hello 12345|********", convert_buf());
    CHECK_EQUAL(sa.length(), 11);
    CHECK_EQUAL(sa.desired_length(), 11);
    
    ret = sa.appendf(" %d %s", 12345, "hurdy gurdy");
    CHECK_EQUAL(ret, 8);
    CHECK_EQUALSTR("hello 12345 12345 h|", convert_buf());
    CHECK_EQUAL(sa.length(), SIZE - 1);
    CHECK_EQUAL(sa.desired_length(), 29);

    CHECK_EQUAL(sa.append("hello"), 0);
    CHECK_EQUAL(sa.length(), SIZE - 1);
    CHECK_EQUAL(sa.desired_length(), 34);

    CHECK_EQUAL(sa.append('c'), 0);
    CHECK_EQUAL(sa.length(), SIZE - 1);
    CHECK_EQUAL(sa.desired_length(), 35);

    ret = sa.appendf("hello %d", 1234);
    CHECK_EQUAL(ret, 0);
    CHECK_EQUALSTR("hello 12345 12345 h|", convert_buf());
    CHECK_EQUAL(sa.length(), SIZE-1);
    CHECK_EQUAL(sa.desired_length(), 45);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Test1);
}

DECLARE_TEST_FILE(Test, "string appender test");
