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
#include "util/StringBuffer.h"
#include "util/ScratchBuffer.h"

using namespace oasys;

DECLARE_TEST(StringTest1) {
    StaticStringBuffer<10> buf;
    
    buf.append('X');

    return (strncmp("X", buf.c_str(), 10) == 0) ? 0 : UNIT_TEST_FAILED;
}

DECLARE_TEST(StringTest2) {
    StaticStringBuffer<10> buf;
    char buf2[256];
    
    buf.appendf("%d%d%d%s", 10, 20, 123, "foobarbaz");
    sprintf(buf2, "%d%d%d%s", 10, 20, 123, "foobarbaz");

    log_debug_p("\"%s\" \"%s\"", buf.c_str(), buf2);

    return (strncmp(buf2, buf.c_str(), 10) == 0) ? 0 : UNIT_TEST_FAILED;
}

DECLARE_TEST(ScratchTest1) {
    ScratchBuffer<char*> buf;
    char* buf2 = "1234567890";

    memcpy(buf.buf(11), buf2, 11);

    return (strncmp(buf2, buf.buf(), 10) == 0) ? 0 : UNIT_TEST_FAILED;
}

DECLARE_TEST(ScratchTest2) {
    ScratchBuffer<char*, 11> buf;
    char* buf2 = "1234567890";

    memcpy(buf.buf(11), buf2, 11);

    return (strncmp(buf2, buf.buf(), 10) == 0) ? 0 : UNIT_TEST_FAILED;
}

DECLARE_TESTER(Test) {    
    ADD_TEST(StringTest1);
    ADD_TEST(StringTest2);
    ADD_TEST(ScratchTest1);
    ADD_TEST(ScratchTest2);
}

DECLARE_TEST_FILE(Test, "static buffer test");
