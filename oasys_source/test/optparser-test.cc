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

#include <string>
#include <stdio.h>

#include <debug/Log.h>
#include <util/OptParser.h>
#include <util/UnitTest.h>

using namespace oasys;

bool test 	= 0;
bool test_set 	= 0;
int port 	= 10;
int xyz 	= 50;
double f 	= 10.5;
u_int64_t u64   = 123456789123456789ULL;
u_int64_t size  = 1024;
std::string name("name");

OptParser p;

DECLARE_TEST(Init) {
    p.addopt(new BoolOpt("test", &test, "test flag", &test_set));
    p.addopt(new IntOpt("port", &port, "<port>", "listen port"));
    p.addopt(new IntOpt("xyz",  &xyz,  "<val>", "x y z"));
    p.addopt(new DoubleOpt("f", &f, "<f>", "f"));
    p.addopt(new UInt64Opt("u64", &u64, "<u64>", "u64"));
    p.addopt(new StringOpt("name", &name, "<name>", "app name"));
    p.addopt(new SizeOpt("size", &size, "<size>", "size"));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ValidArgString) {
    const char* invalid;

    CHECK(p.parse(""));
    CHECK_EQUAL(test, false);
    CHECK_EQUAL(test_set, false);
    CHECK_EQUAL(port, 10);
    CHECK_EQUALSTR(name.c_str(), "name");
    CHECK_EQUAL(xyz, 50);
    CHECK_EQUAL_U64(u64, 123456789123456789ULL);
    CHECK_EQUAL(size, 1024);
    CHECK(f == 10.5);
    
    CHECK(p.parse("test port=100 name=mike xyz=10 f=100.4 u64=9876543219876545321",
                  &invalid));
    CHECK_EQUAL(test, true);
    CHECK_EQUAL(test_set, true);
    CHECK_EQUAL(port, 100);
    CHECK_EQUALSTR(name.c_str(), "mike");
    CHECK_EQUAL(xyz, 10);
    CHECK_EQUAL_U64(u64, 9876543219876545321ULL);
    CHECK(f == 100.4);

    CHECK(p.parse("test=false"));
    CHECK_EQUAL(test, false);
    CHECK_EQUAL(test_set, true);
    
    CHECK(p.parse("test=F"));
    CHECK_EQUAL(test, false);
    CHECK_EQUAL(test_set, true);
    
    CHECK(p.parse("test=0"));
    CHECK_EQUAL(test, false);
    CHECK_EQUAL(test_set, true);
    
    CHECK(p.parse("test=TRUE"));
    CHECK_EQUAL(test, true);
    CHECK_EQUAL(test_set, true);

    CHECK(p.parse("test=T"));
    CHECK_EQUAL(test, true);
    CHECK_EQUAL(test_set, true);
    
    CHECK(p.parse("test=1"));
    CHECK_EQUAL(test, true);
    CHECK_EQUAL(test_set, true);
    
    CHECK(p.parse(""));
    CHECK_EQUAL(test, true);
    CHECK_EQUAL(test_set, true);
    CHECK_EQUAL(port, 100);
    CHECK_EQUALSTR(name.c_str(), "mike");
    CHECK_EQUAL(xyz, 10);

    CHECK(p.parse("f=10"));
    CHECK(f == 10);

    CHECK(p.parse("f=.10"));
    CHECK(f == .10);

    CHECK(p.parse("size=100"));
    CHECK(size == 100);

    CHECK(p.parse("size=128B"));
    CHECK(size == 128);

    CHECK(p.parse("size=1K"));
    CHECK(size == 1024);
    
    CHECK(p.parse("size=3M"));
    CHECK(size == 3145728);

    CHECK(p.parse("size=1G"));
    CHECK(size == 1073741824);

    CHECK(p.parse("size=2G"));
    CHECK(size == 2147483648ULL);
    
    CHECK(p.parse("size=10G"));
    CHECK(size == 10737418240ULL);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(InvalidArgStr) {
    const char* invalid;
    CHECK(p.parse("test=abc", &invalid) == false);
    
    CHECK(p.parse("port", &invalid) == false);
    CHECK(p.parse("port=", &invalid) == false);
    CHECK(p.parse("port=foo", &invalid) == false);
    CHECK(p.parse("port=10.5", &invalid) == false);

    CHECK(p.parse("test=", &invalid) == false);
    CHECK(p.parse("test=foo", &invalid) == false);

    CHECK(p.parse("f", &invalid) == false);
    CHECK(p.parse("f=", &invalid) == false);

    CHECK(p.parse("size=100X") == false);
    CHECK(p.parse("size=") == false);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ValidArgv) {
    const char* argv[50];

    argv[0] = "test=F";
    argv[1] = "port=99";
    argv[2] = "name=bowei";
    argv[3] = "xyz=1";
    argv[4] = "f=99.99";
    
    CHECK(p.parse(5, argv));
    CHECK_EQUAL(test, false);
    CHECK_EQUAL(test_set, true);
    CHECK_EQUAL(port, 99);
    CHECK_EQUALSTR(name.c_str(), "bowei");
    CHECK_EQUAL(xyz, 1);
    CHECK(f == 99.99);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ArgvParseAndShift) {
    const char* argv[50];

    argv[0] = "testfoo=T";
    argv[1] = "port=9";
    argv[2] = "bogus";
    argv[3] = "name=matt";
    argv[4] = "foo=bar";
    argv[5] = "f=1.23";
    
    CHECK_EQUAL(p.parse_and_shift(6, argv), 3);
    CHECK_EQUAL(port, 9);
    CHECK_EQUALSTR(name.c_str(), "matt");
    CHECK(f == 1.23);

    CHECK_EQUALSTR(argv[0], "testfoo=T");
    CHECK_EQUALSTR(argv[1], "bogus");
    CHECK_EQUALSTR(argv[2], "foo=bar");

    argv[0] = "bogus 1";
    argv[1] = "bogus 2";
    argv[2] = "bogus 3";
    argv[3] = "bogus 4";
    
    CHECK_EQUAL(p.parse_and_shift(4, argv), 0);
    CHECK_EQUALSTR(argv[0], "bogus 1");
    CHECK_EQUALSTR(argv[1], "bogus 2");
    CHECK_EQUALSTR(argv[2], "bogus 3");
    CHECK_EQUALSTR(argv[3], "bogus 4");

    CHECK_EQUAL(p.parse_and_shift(0, 0), 0);

    argv[0] = "test";
    CHECK_EQUAL(p.parse_and_shift(1, argv), 1);

    argv[0] = "port=foobar";
    const char* invalid;
    CHECK_EQUAL(p.parse_and_shift(1, argv, &invalid), -1);
    CHECK_EQUALSTR(invalid, argv[0]);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(OptParserTester) {
    ADD_TEST(Init);
    ADD_TEST(ValidArgString);
    ADD_TEST(InvalidArgStr);
    ADD_TEST(ValidArgv);
    ADD_TEST(ArgvParseAndShift);
}

DECLARE_TEST_FILE(OptParserTester, "OptParser Tester");
