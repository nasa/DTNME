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

#include <stdio.h>
#include <string>
#include <vector>

#include <oasys/util/UnitTest.h>
#include "bundling/GbofId.h"

using namespace dtn;
using namespace oasys;

DECLARE_TEST(Equality) {
    EndpointID _source[] = {
        EndpointID("dtn://test-a.dtn"),
        EndpointID("dtn://test-b.dtn"),
        EndpointID("")
    };

    u_int32_t _secs[]   = { 1234, 5678, 0 };
    u_int32_t _seqno[]  = { 1111, 2222, 0 };
    u_int32_t _off[]    = { 1000, 2000, 0 };
    u_int32_t _length[] = { 5000, 3000, 0 };

    std::vector<GbofId> others;
    others.push_back(GbofId(EndpointID("dtn:none"), BundleTimestamp(0, 0), 0, 0, 0));

    // first set of tests ignore fragment components since non-fragment
    for (EndpointID* source = &_source[0]; source != &_source[2]; source++)
    for (u_int32_t*  secs   = &_secs[0];   secs   != &_secs[2];   secs++)
    for (u_int32_t*  seqno  = &_seqno[0];  seqno  != &_seqno[2];  seqno++)
    {
        GbofId gbofid_a(*source, BundleTimestamp(*secs, *seqno), false, 0, 0);
        GbofId gbofid_b(*source, BundleTimestamp(*secs, *seqno), false, 999, 999);

        log_always_p("/test", "checking %s", gbofid_a.str().c_str());
        CHECK(gbofid_a == gbofid_b);
        CHECK(! (gbofid_a < gbofid_b));
        CHECK(! (gbofid_b < gbofid_a));

        for (std::vector<GbofId>::iterator i = others.begin(); i != others.end(); ++i) {
            log_always_p("/test", "checking %s vs %s", gbofid_a.str().c_str(), i->str().c_str());
            CHECK(!(gbofid_a == *i));
            CHECK((gbofid_a < *i) || (*i < gbofid_a));
        }
        
        others.push_back(gbofid_a);
    }

    // repeat with the fragmentation parts
    for (EndpointID* source = &_source[0]; source != &_source[2]; source++)
    for (u_int32_t*  secs   = &_secs[0];   secs   != &_secs[2];   secs++)
    for (u_int32_t*  seqno  = &_seqno[0];  seqno  != &_seqno[2];  seqno++)
    for (u_int32_t*  off    = &_off[0];    off    != &_off[2];    off++)
    for (u_int32_t*  length = &_length[0]; length != &_length[2]; length++)
    {
        GbofId gbofid_a(*source, BundleTimestamp(*secs, *seqno), true, *off, *length);
        GbofId gbofid_b(*source, BundleTimestamp(*secs, *seqno), true, *off, *length);

        log_always_p("/test", "checking %s", gbofid_a.str().c_str());
        CHECK(gbofid_a == gbofid_b);
        CHECK(! (gbofid_a < gbofid_b));
        CHECK(! (gbofid_b < gbofid_a));

        for (std::vector<GbofId>::iterator i = others.begin(); i != others.end(); ++i) {
            log_always_p("/test", "checking %s vs %s", gbofid_a.str().c_str(), i->str().c_str());
            CHECK(!(gbofid_a == *i));
            CHECK((gbofid_a < *i) || (*i < gbofid_a));
        }
        
        others.push_back(gbofid_a);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(GbofIDTest) {
    ADD_TEST(Equality);
}

DECLARE_TEST_FILE(GbofIDTest, "gbofid test");
