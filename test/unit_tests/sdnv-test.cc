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
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <string>

#include <oasys/util/UnitTest.h>
#include "bundling/SDNV.h"

using namespace dtn;
using namespace oasys;

u_char buf[64];

bool
test(u_int64_t val, int expected_len)
{
    int len;
    len = SDNV::encode(val, buf, sizeof(buf));
    if (len != expected_len) {
        log_err_p("/sdnv/test", "encode %llu expected %d byte(s), used %d",
                  U64FMT(val), expected_len, len);
        return UNIT_TEST_FAILED;
    }

    u_int64_t val2 = 0;
    len = SDNV::decode(buf, len, &val2);
    if (len != expected_len) {
        log_err_p("/sdnv/test", "decode %llu expected %d byte(s), used %d",
                  U64FMT(val), expected_len, len);
        return UNIT_TEST_FAILED;
    }
    
    int errno_; const char* strerror_;

    CHECK_EQUAL_U64(val, val2);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(OneByte) {
    u_int64_t val;
    
    // a few random value tests
    CHECK(test(0, 1) == UNIT_TEST_PASSED);
    CHECK(test(1, 1) == UNIT_TEST_PASSED);
    CHECK(test(2, 1) == UNIT_TEST_PASSED);
    CHECK(test(16, 1) == UNIT_TEST_PASSED);
    CHECK(test(99, 1) == UNIT_TEST_PASSED);
    CHECK(test(101, 1) == UNIT_TEST_PASSED);
    CHECK(test(127, 1) == UNIT_TEST_PASSED);

    // a few checks that the encoding is actually what we expect
    CHECK_EQUAL(SDNV::encode(0, buf, 1), 1);
    CHECK_EQUAL(buf[0], 0);

    CHECK_EQUAL(SDNV::encode(16, buf, 1), 1);
    CHECK_EQUAL(buf[0], 16);
    
    CHECK_EQUAL(SDNV::encode(127, buf, 1), 1);
    CHECK_EQUAL(buf[0], 127);

    // buffer length checks
    CHECK_EQUAL(SDNV::encode(0, buf, 0), -1);
    CHECK_EQUAL(SDNV::decode(buf, 0, &val), -1);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultiByte) {
    // some random value checks
    CHECK(test(1001, 2) == UNIT_TEST_PASSED);
    CHECK(test(0xabcd, 3) == UNIT_TEST_PASSED);
    CHECK(test(0xabcde, 3) == UNIT_TEST_PASSED);
    CHECK(test(0xabcdef, 4) == UNIT_TEST_PASSED);
    CHECK(test(0x0abcdef, 4) == UNIT_TEST_PASSED);
    CHECK(test(0xfabcdef, 4) == UNIT_TEST_PASSED);

    // now check that the encoding is what we expect 
    CHECK_EQUAL(SDNV::encode(0xab, buf, sizeof(buf)), 2);
    CHECK_EQUAL(buf[0], (1 << 7) | 0x1); // 10000001
    CHECK_EQUAL(buf[1], 0x2b);           // 00101011

    CHECK_EQUAL(SDNV::encode(0xabc, buf, sizeof(buf)), 2);
    CHECK_EQUAL(buf[0], (1 << 7) | 0x15); // 10010101
    CHECK_EQUAL(buf[1], 0x3c);            // 00111100

    CHECK_EQUAL(SDNV::encode(0xabcde, buf, sizeof(buf)), 3);
    CHECK_EQUAL(buf[0], (1 << 7) | 0x2a); // 10101010
    CHECK_EQUAL(buf[1], (1 << 7) | 0x79); // 11111001
    CHECK_EQUAL(buf[2], 0x5e);            // 01011110

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Bounds) {
    // check the boundary cases
    CHECK(test(0LL,                   1) == UNIT_TEST_PASSED);
    CHECK(test(0x7fLL,                1) == UNIT_TEST_PASSED);
    
    CHECK(test(0x80LL,                2) == UNIT_TEST_PASSED);
    CHECK(test(0x3fffLL,              2) == UNIT_TEST_PASSED);

    CHECK(test(0x4000LL,              3) == UNIT_TEST_PASSED);
    CHECK(test(0x1fffffLL,            3) == UNIT_TEST_PASSED);

    CHECK(test(0x200000LL,            4) == UNIT_TEST_PASSED);
    CHECK(test(0xffffffLL,            4) == UNIT_TEST_PASSED);
    
    CHECK(test(0x10000000LL,          5) == UNIT_TEST_PASSED);
    CHECK(test(0x7ffffffffLL,         5) == UNIT_TEST_PASSED);

    CHECK(test(0x800000000LL,         6) == UNIT_TEST_PASSED);
    CHECK(test(0x3ffffffffffLL,       6) == UNIT_TEST_PASSED);

    CHECK(test(0x40000000000LL,       7) == UNIT_TEST_PASSED);
    CHECK(test(0x1ffffffffffffLL,     7) == UNIT_TEST_PASSED);

    CHECK(test(0x2000000000000LL,     8) == UNIT_TEST_PASSED);
    CHECK(test(0x0fffffffffffffLL,    8) == UNIT_TEST_PASSED);

    CHECK(test(0x100000000000000LL,   9) == UNIT_TEST_PASSED);
    CHECK(test(0x7fffffffffffffffLL,  9) == UNIT_TEST_PASSED);

    CHECK(test(0x8000000000000000LL, 10) == UNIT_TEST_PASSED);
    CHECK(test(0xffffffffffffffffLL, 10) == UNIT_TEST_PASSED);
        
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(SDNVTest) {
    ADD_TEST(OneByte);
    ADD_TEST(MultiByte);
    ADD_TEST(Bounds);
}

DECLARE_TEST_FILE(SDNVTest, "sdnv test");
