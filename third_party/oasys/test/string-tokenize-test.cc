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
#include "util/StringUtils.h"

using namespace oasys;
using namespace std;

DECLARE_TEST(BasicTokenizeTest) {
    std::vector<std::string> tokens;

    CHECK_EQUAL(tokenize("one, two, three", ", ", &tokens), 3);
    CHECK_EQUAL(tokens.size(), 3);
    CHECK_EQUALSTR(tokens[0].c_str(), "one");
    CHECK_EQUALSTR(tokens[1].c_str(), "two");
    CHECK_EQUALSTR(tokens[2].c_str(), "three");

    CHECK_EQUAL(tokenize("one, ,, two, ,, three", ", ", &tokens), 3);
    CHECK_EQUAL(tokens.size(), 3);
    CHECK_EQUALSTR(tokens[0].c_str(), "one");
    CHECK_EQUALSTR(tokens[1].c_str(), "two");
    CHECK_EQUALSTR(tokens[2].c_str(), "three");

    CHECK_EQUAL(tokenize("thequickbrownfox", "ABC", &tokens), 1);
    CHECK_EQUAL(tokens.size(), 1);
    CHECK_EQUALSTR(tokens[0].c_str(), "thequickbrownfox");
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(EdgeConditionTest) {
    std::vector<std::string> tokens;
    
    CHECK_EQUAL(tokenize("", "", &tokens), 0);
    CHECK_EQUAL(tokens.size(), 0);

    CHECK_EQUAL(tokenize("ABC", "ABC", &tokens), 0);
    CHECK_EQUAL(tokens.size(), 0);
    
    CHECK_EQUAL(tokenize(" foo bar ", " ", &tokens), 2);
    CHECK_EQUAL(tokens.size(), 2);
    CHECK_EQUALSTR(tokens[0].c_str(), "foo");
    CHECK_EQUALSTR(tokens[1].c_str(), "bar");
    
    return UNIT_TEST_PASSED;
}


DECLARE_TESTER(Test) {    
    ADD_TEST(BasicTokenizeTest);
    ADD_TEST(EdgeConditionTest);
}

DECLARE_TEST_FILE(Test, "string tokenize test");
