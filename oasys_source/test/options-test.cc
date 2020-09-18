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

#include <util/Options.h>
#include <util/StringBuffer.h>
#include <util/UnitTest.h>

using namespace oasys;
StringBuffer buf;

namespace oasys {

class OptTester {
public:
    static bool set(Opt* opt, const char* str, size_t len) {
        return (opt->set(str, len) == 0);
    }

    static void get(Opt* opt, StringBuffer* buf) {
        opt->get(buf);
    }
};

}

#define TEST_VALID(_v, _e)                              \
    CHECK(OptTester::set(&opt, _v, strlen(_v)));        \
    CHECK_EQUAL_U64(val, _e);                           \
    buf.clear();                                        \
    OptTester::get(&opt, &buf);                         \
    CHECK_EQUALSTR(buf.c_str(), #_e);

#define TEST_VALID2(_v, _e, _vstr)                      \
    CHECK(OptTester::set(&opt, _v, strlen(_v)));        \
    CHECK_EQUAL_U64(val, _e);                           \
    buf.clear();                                        \
    OptTester::get(&opt, &buf);                         \
    CHECK_EQUALSTR(buf.c_str(), _vstr);

#define TEST_INVALID(_v)                                \
    CHECK(! OptTester::set(&opt, _v, strlen(_v)));      \

DECLARE_TEST(BoolOptTest) {
    bool val;
    BoolOpt opt("opt", &val);
    TEST_VALID("true", true);
    TEST_VALID("t", true);
    TEST_VALID("T", true);
    TEST_VALID("tRUe", true);

    TEST_VALID("1", true);

    TEST_VALID("false", false);
    TEST_VALID("f", false);
    TEST_VALID("F", false);
    TEST_VALID("FalSE", false);
    TEST_VALID("0", false);

    TEST_VALID("", true);

    TEST_INVALID("foobar");
    TEST_INVALID("999");

    return UNIT_TEST_PASSED;
}


// tests that apply to all the int types
#define GENERIC_INT_TESTS                       \
    TEST_VALID("0", 0);                         \
    TEST_VALID("0x0", 0);                       \
    TEST_VALID("0x1", 1);                       \
    TEST_VALID("0xa", 10);                      \
                                                \
    TEST_INVALID("");                           \
    TEST_INVALID("a");                          \
    TEST_INVALID("0a");                         \
    TEST_INVALID("0.1");                        \
    TEST_INVALID("0.");

DECLARE_TEST(IntOptTest) {
    int val;
    IntOpt opt("opt", &val);

    GENERIC_INT_TESTS;
    
    TEST_VALID("-1", -1);
    TEST_VALID("2147483647", 2147483647);
    TEST_VALID("-2147483646", -2147483646);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(UIntOptTest) {
    u_int val;
    UIntOpt opt("opt", &val);

    GENERIC_INT_TESTS;
    
    TEST_VALID2("4294967295", 4294967295UL, "4294967295");
    TEST_VALID2("0xffffffff", 4294967295UL, "4294967295");
    TEST_VALID2("-1", 4294967295UL, "4294967295");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(UInt64OptTest) {
    u_int64_t val;
    UInt64Opt opt("opt", &val);

    GENERIC_INT_TESTS;
    
    TEST_VALID2("18446744073709551615",
                18446744073709551615ULL,
                "18446744073709551615");
    TEST_VALID2("0xffffffffffffffff",
                18446744073709551615ULL,
                "18446744073709551615");
    TEST_VALID2("-1",
                18446744073709551615ULL,
                "18446744073709551615");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(UInt16OptTest) {
    u_int16_t val;
    UInt16Opt opt("opt", &val);

    GENERIC_INT_TESTS;
    
    TEST_VALID("65535", 65535);
    TEST_VALID("0xffff", 65535);

    TEST_INVALID("-1");
    TEST_INVALID("65536");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(UInt8OptTest) {
    u_int8_t val;
    UInt8Opt opt("opt", &val);

    GENERIC_INT_TESTS;
    
    TEST_VALID("255", 255);
    TEST_VALID("0xff", 255);

    TEST_INVALID("-1");
    TEST_INVALID("256");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SizeOptTest) {
    u_int64_t val;
    SizeOpt opt("opt", &val);

    TEST_VALID("0", 0);
    TEST_VALID("0B", 0);
    TEST_VALID("0K", 0);
    TEST_VALID("0M", 0);
    TEST_VALID("0G", 0);

    TEST_VALID("1", 1);
    TEST_VALID("1K", 1024);
    TEST_VALID("1M", 1048576);
    TEST_VALID("1G", 1073741824);

    TEST_VALID("5K", 5120);
    TEST_VALID("5M", 5242880);
    TEST_VALID2("5G", 5368709120ULL, "5368709120");

    TEST_VALID2("18446744073709551615",
                18446744073709551615ULL,
                "18446744073709551615");
    
    TEST_INVALID("0.1");
    TEST_INVALID("0.1K");
    TEST_INVALID("");
    TEST_INVALID("B");
    TEST_INVALID("K");
    TEST_INVALID("M");
    TEST_INVALID("G");
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DoubleOptTest) {
    double val;
    DoubleOpt opt("opt", &val);

    TEST_VALID("0", 0.000000);
    TEST_VALID("0.1", 0.100000);
    TEST_VALID("100.1", 100.100000);
    TEST_VALID("0xa", 10.000000);
    
    TEST_INVALID("");
    TEST_INVALID("a");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(RateOptTest) {
    u_int64_t val;
    RateOpt opt("opt", &val);

    TEST_VALID("0", 0);
    TEST_VALID("0bps", 0);
    TEST_VALID("0kbps", 0);
    TEST_VALID("0mbps", 0);
    TEST_VALID("0gbps", 0);

    TEST_VALID("1", 1);
    TEST_VALID("1kbps", 1000);
    TEST_VALID("1mbps", 1000000);
    TEST_VALID2("1gbps", 1000000000ULL, "1000000000");

    TEST_VALID("5kbps", 5000);
    TEST_VALID("5mbps", 5000000);
    TEST_VALID2("5gbps", 5000000000ULL, "5000000000");

    TEST_VALID2("18446744073709551615",
                18446744073709551615ULL,
                "18446744073709551615");
    
    TEST_INVALID("0.1");
    TEST_INVALID("0.1kbps");
    TEST_INVALID("");
    TEST_INVALID("bps");
    TEST_INVALID("kbps");
    TEST_INVALID("mbps");
    TEST_INVALID("gbps");
    return UNIT_TEST_PASSED;
}

oasys::EnumOpt::Case EnumOptCases[] = {
    {"a", 1},
    {"b", 2},
    {"c", 4},
    {0, 0}
};

DECLARE_TEST(EnumOptTest) {
    int val = 0;
    EnumOpt opt("opt", EnumOptCases, &val);
    TEST_VALID2("a", 1, "a");
    TEST_VALID2("b", 2, "b");
    TEST_VALID2("c", 4, "c");

    TEST_VALID2("A", 1, "a");
    TEST_VALID2("B", 2, "b");
    TEST_VALID2("C", 4, "c");

    TEST_INVALID("");
    TEST_INVALID("d");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BitFlagOptTest) {
    int val = 0;
    BitFlagOpt opt("opt", EnumOptCases, &val);
    TEST_VALID2("a", 1, "a");
    val = 0;
    TEST_VALID2("b", 2, "b");
    val = 0;
    TEST_VALID2("c", 4, "c");
    val = 0;
    
    TEST_VALID2("a", 1, "a");
    TEST_VALID2("b", 3, "ab");
    TEST_VALID2("c", 7, "abc");

    TEST_INVALID("");
    TEST_INVALID("d");

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(OptionsTester) {
    ADD_TEST(BoolOptTest);
    ADD_TEST(IntOptTest);
    ADD_TEST(UIntOptTest);
    ADD_TEST(UInt64OptTest);
    ADD_TEST(UInt16OptTest);
    ADD_TEST(UInt8OptTest);
    ADD_TEST(SizeOptTest);
    ADD_TEST(DoubleOptTest);
    ADD_TEST(RateOptTest);
    ADD_TEST(EnumOptTest);
    ADD_TEST(BitFlagOptTest);
}

DECLARE_TEST_FILE(OptionsTester, "Options Tester");
