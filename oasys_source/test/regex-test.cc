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

#include "util/Regex.h"
#include "util/UnitTest.h"

using namespace oasys;

DECLARE_TEST(Regex1) {
    CHECK(Regex::match("ab*c", "abbbbbbbc") == 0);
    CHECK(Regex::match("ab*c", "ac") == 0);
    CHECK(Regex::match("ab*c", "abbbbbbb1333346c") == REG_NOMATCH);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Regex2) {
    Regex re("ab(:r+)bd", REG_EXTENDED);

    CHECK(re.match("ab:rrrbd") == 0);
    CHECK_EQUAL(re.get_match(1).rm_so, 2);
    CHECK_EQUAL(re.get_match(1).rm_eo, 6);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Regsub) {
    std::string s;
    CHECK(Regsub::subst("ab*c", "abbbbbbbc", "nothing", &s, REG_EXTENDED) == 0);
    CHECK_EQUALSTR(s.c_str(), "nothing");

    CHECK(Regsub::subst("ab*c", "abbbbbbbc", "\\0", &s, REG_EXTENDED) == 0);
    CHECK_EQUALSTR(s.c_str(), "abbbbbbbc");

    CHECK(Regsub::subst("a(b*)c", "abbbbbbbc", "\\1", &s, REG_EXTENDED) == 0);
    CHECK_EQUALSTR(s.c_str(), "bbbbbbb");

    CHECK(Regsub::subst("(a)(b*)(c)", "abbbbbbbc", "\\0 \\1 \\2 \\3", &s, REG_EXTENDED) == 0);
    CHECK_EQUALSTR(s.c_str(), "abbbbbbbc a bbbbbbb c");

    CHECK(Regsub::subst("ab*c", "xyz", "", &s, REG_EXTENDED) == REG_NOMATCH);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Regex1);
    ADD_TEST(Regex2);
    ADD_TEST(Regsub);
}

DECLARE_TEST_FILE(Test, "regex test");
