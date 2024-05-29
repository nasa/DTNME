/*
 *    Copyright 2005-2006 Intel Corporation
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
#include "serialize/SerializeStream.h"

using namespace oasys;

DECLARE_TEST(Stream1) {
    char buf[20];

    WriteMemoryStream str(buf, 20);

    CHECK(str.process_bits("1234567890", 10) == 10);
    CHECK(memcmp(buf, "1234567890", 10) == 0);
    CHECK(str.process_bits("1234567890", 10) == 10);
    CHECK(memcmp(buf, "12345678901234567890", 20) == 0);    
    CHECK(str.process_bits("1234567890", 1) < 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Stream2) {
    const char* buf = "12345678901234567890";

    ReadMemoryStream str(buf, 20);
    char in_buf[20];

    memset(in_buf, 0, 20);
    CHECK(str.process_bits(in_buf, 10) == 10);
    CHECK(memcmp(in_buf, "1234567890", 10) == 0);
    memset(in_buf, 0, 20);
    CHECK(str.process_bits(in_buf, 10) == 10);
    CHECK(memcmp(buf, "1234567890", 10) == 0);    
    memset(in_buf, 0, 20);
    CHECK(str.process_bits(in_buf, 1) < 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Stream1);
    ADD_TEST(Stream2);
}

DECLARE_TEST_FILE(Test, "Serialize stream test");
