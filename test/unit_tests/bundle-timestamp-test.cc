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

#include <oasys/util/UnitTest.h>
#include "bundling/BundleProtocol.h"
#include "bundling/BundleTimestamp.h"

using namespace dtn;

DECLARE_TEST(NowTest) {
    BundleTimestamp now, test;
    u_int64_t ts;

    now.seconds_ = BundleTimestamp::get_current_time();
    now.seqno_ = 90909;

    BundleProtocol::set_timestamp((u_char*)&ts, sizeof(ts), now);
    BundleProtocol::get_timestamp(&test, (u_char*)&ts, sizeof(ts));
    
    CHECK_EQUAL(now.seconds_, test.seconds_);
    CHECK_EQUAL(now.seqno_,   test.seqno_);
    
    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TEST(AlignmentTest) {
    BundleTimestamp now, test;
    u_char buf[16];

    now.seconds_ = BundleTimestamp::get_current_time();
    now.seqno_ = 90909;

    for (int i = 0; i < 8; ++i) {
        BundleProtocol::set_timestamp(&buf[i], sizeof(u_int64_t), now);
        BundleProtocol::get_timestamp(&test, &buf[i], sizeof(u_int64_t));
        
        CHECK_EQUAL(now.seconds_, test.seconds_);
        CHECK_EQUAL(now.seqno_,   test.seqno_);
    }
    
    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TESTER(BundleTimestampTester) {
    ADD_TEST(NowTest);
    ADD_TEST(AlignmentTest);
}

DECLARE_TEST_FILE(BundleTimestampTester, "bundle timestamp test");
