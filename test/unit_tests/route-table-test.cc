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
#  include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>

#include "bundling/BundleActions.h"
#include "contacts/Link.h"
#include "conv_layers/NullConvergenceLayer.h"
#include "routing/RouteTable.h"


using namespace oasys;
using namespace dtn;


NullConvergenceLayer* cl;
LinkRef l1;
LinkRef l2;
LinkRef l3;

DECLARE_TEST(Init) {

    cl = new NullConvergenceLayer();
    
    l1 = Link::create_link("l1", Link::OPPORTUNISTIC, cl,
                           "l1-dest", 0, NULL);

    l2 = Link::create_link("l2", Link::OPPORTUNISTIC, cl,
                           "l2-dest", 0, NULL);

    l3 = Link::create_link("l3", Link::OPPORTUNISTIC, cl,
                           "l3-dest", 0, NULL);

    return UNIT_TEST_PASSED;
}

bool
add_entry(RouteTable* t, const std::string& dest, const LinkRef& link)
{
    return t->add_entry(new RouteEntry(EndpointIDPattern(dest), link));
}

bool
add_entry(RouteTable* t, const char* dest, const char* route_to)
{
    return t->add_entry(new RouteEntry(EndpointIDPattern(dest),
                                       EndpointIDPattern(route_to)));
}

DECLARE_TEST(GetMatching) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();
    
    CHECK(add_entry(&t, "*:*", l1));
    CHECK(add_entry(&t, "dtn://d2/*", l2));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d2"), &v), 3);
    CHECK_EQUAL(v.size(), 3);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d2");
    CHECK(v[0]->link() == l2);
    CHECK_EQUALSTR(v[1]->dest_pattern().c_str(), "*:*");
    CHECK(v[1]->link() == l1);
    CHECK_EQUALSTR(v[2]->dest_pattern().c_str(), "dtn://d2/*");
    CHECK(v[2]->link() == l2);
    v.clear();

    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d1/*", l2));
    CHECK(add_entry(&t, "dtn://d1", l3));

    CHECK_EQUAL(t.size(), 8);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 5);
    v.clear();
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1/test"), &v), 2);
    v.clear();

    t.clear();
                             
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntry) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();

    CHECK(t.del_entry(EndpointIDPattern("dtn://d1"), l1));
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);

    CHECK(! t.del_entry(EndpointIDPattern("dtn://d1"), l1));
    CHECK(! t.del_entry(EndpointIDPattern("dtn://d2"), l1));
    CHECK(! t.del_entry(EndpointIDPattern("dtn://d3"), l2));

    t.clear();

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntries) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();

    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);
    
    CHECK_EQUAL(t.size(), 2);

    CHECK(add_entry(&t, "*:*", l1));
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("*:*")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d2")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d3")), 1);
    CHECK_EQUAL(t.size(), 0);
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));

    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 3);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d3")), 3);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d2")), 3);
    CHECK_EQUAL(t.size(), 0);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("*:*")), 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntriesForNextHop) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 1);
    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);
    
    CHECK_EQUAL(t.size(), 2);

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 0);
    CHECK_EQUAL(t.del_entries_for_nexthop(l2), 1);
    CHECK_EQUAL(t.del_entries_for_nexthop(l3), 1);
    CHECK_EQUAL(t.size(), 0);
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 3);
    CHECK_EQUAL(t.del_entries_for_nexthop(l3), 3);
    CHECK_EQUAL(t.del_entries_for_nexthop(l2), 3);
    CHECK_EQUAL(t.size(), 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Recursive) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));

    CHECK(add_entry(&t, "dtn://a1", "dtn://d1"));

    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();

    CHECK(add_entry(&t, "dtn://a2", "dtn://a1"));

    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a2"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();

    CHECK(t.del_entry(EndpointIDPattern("dtn://d1"), l1));

    CHECK_EQUAL(t.size(), 2);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a1"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a2"), &v), 0);

    CHECK(add_entry(&t, "dtn://d1", l2));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l2);
    v.clear();

    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a2"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l2);
    v.clear();
    
    CHECK(t.del_entries(EndpointIDPattern("dtn://a1")));
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a2"), &v), 0);
    CHECK_EQUAL(v.size(), 0);
    
    CHECK(add_entry(&t, "dtn://a1", "dtn://a6"));
    CHECK(add_entry(&t, "dtn://a3", "dtn://a2"));
    CHECK(add_entry(&t, "dtn://a4", "dtn://a3"));
    CHECK(add_entry(&t, "dtn://a5", "dtn://a4"));
    CHECK(add_entry(&t, "dtn://a6", "dtn://a5"));

    log_always_p("/test",
                 "flamebox-ignore ign1 .* caused route table lookup loop");
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a1"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a2"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a3"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a4"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a5"), &v), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a6"), &v), 0);
    log_always_p("/test", "flamebox-ignore-cancel ign1");

    DO(t.clear());

    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://a1", "dtn://d1"));
    CHECK(add_entry(&t, "dtn://a2", "dtn://d1"));

    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a*"), &v), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://*"), &v), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();
    
    CHECK(add_entry(&t, "dtn://a3", "dtn://a2"));
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://a*"), &v), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern().c_str(), "dtn://d1");
    CHECK(v[0]->link() == l1);
    v.clear();
    
    t.clear();
    CHECK_EQUAL(t.size(), 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Cleanup) {

    l1->delete_link();
    l2->delete_link();
    l3->delete_link();
    
    l1 = NULL;
    l2 = NULL;
    l3 = NULL;

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(RouteTableTest) {
    ADD_TEST(Init);
    ADD_TEST(GetMatching);
    ADD_TEST(DelEntry);
    ADD_TEST(DelEntries);
    ADD_TEST(DelEntriesForNextHop);
    ADD_TEST(Recursive);
    ADD_TEST(Cleanup);
}

DECLARE_TEST_FILE(RouteTableTest, "route table test");
