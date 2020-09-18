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
#include "util/Base16.h"

using namespace oasys;

DECLARE_TEST(ATest) {
    char buf[16];
    char buf16[32];
    char buf8[16];
    
    memset(buf, 0, 16);
    memset(buf8, 0, 16);

    buf[0] = 4;
    buf[1] = 8;
    buf[2] = 15;
    buf[3] = 16;
    buf[4] = 23;
    buf[5] = 42;

    size_t bytes = Base16::encode(reinterpret_cast<u_int8_t*>(buf), 6, 
                                  reinterpret_cast<u_int8_t*>(buf16), 32);
    CHECK_EQUAL(bytes, 6);
    CHECK_EQUALSTRN(buf16, "4080F00171A2", 12);
    
    bytes = Base16::decode(reinterpret_cast<u_int8_t*>(buf16), 12, 
                           reinterpret_cast<u_int8_t*>(buf8), 32);
    CHECK_EQUAL(bytes, 6);
    CHECK(memcmp(buf, buf8, 6) == 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ATest2) {
    char buf[16];
    char buf16[32];
    char buf8[16];
    
    memset(buf, 0, 16);
    memset(buf8, 0, 16);

    buf[0] = 0xff;
    buf[1] = 0;
    buf[2] = 0x33;

    size_t bytes = Base16::encode(reinterpret_cast<u_int8_t*>(buf), 3, 
                                  reinterpret_cast<u_int8_t*>(buf16), 32);
    CHECK_EQUAL(bytes, 3);
    CHECK_EQUALSTRN(buf16, "FF0033", 6);
    
    bytes = Base16::decode(reinterpret_cast<u_int8_t*>(buf16), 6, 
                           reinterpret_cast<u_int8_t*>(buf8), 32);
    CHECK_EQUAL(bytes, 3);
    CHECK(memcmp(buf, buf8, 3) == 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(ATest);
    ADD_TEST(ATest2);
}

DECLARE_TEST_FILE(Test, "base16 test");
