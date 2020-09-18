/*
 *    Copyright 2006 Intel Corporation
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

#include "util/TokenBucket.h"
#include "util/UnitTest.h"

using namespace oasys;

// to make the test more predictable, we don't actually call the
// system call sleep() or usleep(), but instead spin until for the
// given amount of time has elapsed since the last call to safe_usleep
void safe_usleep(u_int32_t usecs) {
    static Time last;
    if (last.sec_ == 0) {
        last.get_time();
    }

    Time now;
    do {
        now.get_time();
    } while ((now - last).in_microseconds() < usecs);

    last = now;
}

DECLARE_TEST(Fast) {
    // bucket with a depth of 100 tokens and a replacement rate of
    // 10 tokens per ms (i.e. 10000 tokens per second)
    int rate = 10000;
    TokenBucket t("/test/tokenbucket", 100, rate);

    CHECK_EQUAL(t.tokens(), 100);
    CHECK_EQUAL(t.time_to_fill().in_milliseconds(), 0);
    
    // try_to_drain at a constant rate for a while
    for (int i = 0; i < 1000; ++ i) {
        CHECK(t.try_to_drain(10));
        safe_usleep(10000);
    }

    // let it fill up
    safe_usleep(1000000);
    t.update();
    CHECK_EQUAL(t.tokens(), 100);
    CHECK_EQUAL(t.time_to_fill().in_milliseconds(), 0);

    // make sure it doesn't over-fill
    safe_usleep(1000000);
    t.update();
    CHECK_EQUAL(t.tokens(), 100);
    CHECK_EQUAL(t.time_to_fill().in_milliseconds(), 0);
    
    // fully try_to_drain the bucket
    CHECK(t.try_to_drain(100));
    CHECK_EQUAL(t.tokens(), 0);

    // it should be able to catch up
    safe_usleep(10000);
    CHECK(t.try_to_drain(10));

    safe_usleep(1000);
    CHECK(t.try_to_drain(1));

    // fully empty the bucket
    safe_usleep(0);
    DO(t.empty());
    CHECK_EQUAL(t.tokens(), 0);

    // now spend a few seconds hammering on the bucket, making sure we
    // get the rate that we expect
    int nsecs    = 10;
    int interval = 10; // ms
    int total    = 0;
    for (int i = 0; i < (nsecs * (1000 / interval)); ++i) {
        safe_usleep(interval * 1000);
        while (t.try_to_drain(10)) {
            total += 10;
        }
        while (t.try_to_drain(1)) {
            total ++;
        }
    }

    double fudge = 0.01; // pct
    CHECK_GTU(total, (u_int) ((nsecs * rate) * (1.0 - fudge)));
    CHECK_LTU(total, (u_int) ((nsecs * rate) * (1.0 + fudge)));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Slow) {
    // bucket with a depth of only one token and a replacement rate of
    // 1 token per second
    TokenBucket t("/test/tokenbucket", 1, 1);
    CHECK_EQUAL(t.tokens(), 1);
    CHECK(t.try_to_drain(1));
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 1);
    CHECK(! t.try_to_drain(1));
    
    // fully empty the bucket
    safe_usleep(0);
    CHECK(t.try_to_drain(t.tokens()));
    CHECK_EQUAL(t.tokens(), 0);
    
    // now spend a few seconds hammering on the bucket, making sure we
    // get the rate that we should
    int nsecs    = 10;
    int interval = 10; // ms
    int total    = 0;
    for (int i = 0; i < (nsecs * (1000 / interval)); ++i) {
        safe_usleep(interval * 1000);
        if (t.try_to_drain(1)) {
            ++total;
        }
    }

    CHECK_EQUAL(total, nsecs);

    // and one more for good measure
    safe_usleep(1010000);
    CHECK(t.try_to_drain(1));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TimeToFill) {
    TokenBucket t("/test/tokenbucket", 10000, 1000);

    safe_usleep(0);

    CHECK_EQUAL(t.time_to_fill().in_milliseconds(), 0);

    CHECK(t.try_to_drain(5000));
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 5);

    safe_usleep(1000000);
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 4);

    safe_usleep(1000000);
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 3);

    CHECK(t.try_to_drain(1000));
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 4);
    safe_usleep(5000000);

    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 0);
    CHECK_EQUAL(t.tokens(), 10000);

    DO(t.drain(20000));
    CHECK_EQUAL(t.tokens(), -10000);
    CHECK_EQUAL((t.time_to_fill().in_milliseconds() + 500) / 1000, 20);
    
    DO(t.set_rate(100000));
    safe_usleep(0);
    DO(t.empty());

    u_int32_t ms = t.time_to_fill().in_milliseconds();
    CHECK_EQUAL(ms, 100);
    safe_usleep(101000);

    CHECK_EQUAL(t.time_to_fill().in_milliseconds(), 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Fast);
    ADD_TEST(Slow);
    ADD_TEST(TimeToFill);
}

DECLARE_TEST_FILE(Test, "token bucket test");
