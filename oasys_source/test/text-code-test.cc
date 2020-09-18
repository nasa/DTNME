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

#include <cstdlib>

#include "util/UnitTest.h"
#include "util/ScratchBuffer.h"
#include "util/TextCode.h"

using namespace oasys;

DECLARE_TEST(TextCode1) {
    char cbuf[] = { '1', '2', '3', '4', '5', 1, 2, 3, 4, 5 };

    ScratchBuffer<char*> out_buf;

    TextCode code(cbuf, 10, &out_buf, 5, 2);
    char output[] = "\t\t12345\n\t\t\\01\\02\\03\\04\\05\n\t\t\n";

    CHECK(memcmp(out_buf.buf(), output, strlen(output)) == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(TextCodeTester) {    
    ADD_TEST(TextCode1);
}

DECLARE_TEST_FILE(TextCodeTester, "text code test");
