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
#include "util/SafeRange.h"

using namespace oasys;

DECLARE_TEST(SafeRangeTest) {
    const char* str = "1234567890";

    SafeRange<const char> ranger(str, strlen(str));

    try 
    {
        for (size_t i = 0; i < strlen(str); ++i) {
            CHECK(ranger[i] == str[i]);
        }
    } 
    catch (oasys::SafeRange<const char>::Error e) 
    {
        CHECK(false);
    }

    try 
    {
        ranger[strlen(str)];
    } 
    catch (oasys::SafeRange<const char>::Error e)
    {
        return UNIT_TEST_PASSED;
    }    
    
    CHECK(false);
}

DECLARE_TESTER(UtilTest) {
    ADD_TEST(SafeRangeTest);
}

DECLARE_TEST_FILE(UtilTest, "Misc Util Tests");
