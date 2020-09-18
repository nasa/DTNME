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

#include <stdlib.h>
#include <sys/stat.h>

#include "debug/Formatter.h"
#include "debug/Log.h"
#include "util/UnitTest.h"
#include "util/StringBuffer.h"
#include "thread/Thread.h"

using namespace oasys;

int count = 100000;

DECLARE_TEST(Init) {
    if (getenv("COUNT") != 0) {
        count = atoi(getenv("COUNT"));
    }
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(NoOutput) {
    log_always_p("/test", "testing %d iterations with no output", count);

    for (int i = 0; i < count; ++i) {
        log_debug_p("/XXX", "don't output me");
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Log) {
    log_always_p("/test", "testing %d iterations of log()", count);

    for (int i = 0; i < count; ++i) {
        log("/test", LOG_ALWAYS, "output me ");
    }

    return UNIT_TEST_PASSED;
}
    
DECLARE_TEST(Logf) {
    log_always_p("/test", "testing %d iterations of logf()", count);

    for (int i = 0; i < count; ++i) {
        logf("/test", LOG_ALWAYS, "output me %d %s %d", 1, "foo", 2);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(LogMultiline) {
    log_always_p("/test", "testing %d iterations of log_multiline()", count);

    // do half the iterations so the times are more roughly even
    for (int i = 0; i < count / 2; ++i) {
        log_multiline("/test", LOG_ALWAYS,
                      "output me\nandme\nandme\n\n\nandmetoo\n");
    }
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(LogProfileTest) {
    ADD_TEST(Init);
    ADD_TEST(Log);
    ADD_TEST(Logf);
    ADD_TEST(LogMultiline);
}

DECLARE_TEST_FILE(LogProfileTest, "LogProfileTest");

