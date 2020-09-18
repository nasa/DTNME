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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/UnitTest.h>

#include "naming/EndpointID.h"
#include "naming/IPNScheme.h"

using namespace dtn;
using namespace oasys;

typedef enum {
    INVALID = 0,
    VALID   = 1
} valid_t;

typedef enum {
    UNKNOWN = 0,
    KNOWN   = 1
} known_t;

typedef enum {
    NOTEQUAL = 0,
    EQUAL    = 1
} equal_t;

typedef enum {
    NOMATCH = 0,
    MATCH = 1
} match_t;

bool valid(const char* str)
{
    EndpointID eid(str);
    return eid.valid();
}

bool known(const char* str)
{
    EndpointID eid(str);
    return eid.known_scheme();
}

bool equal(const char* str1, const char* str2)
{
    EndpointID eid1(str1);
    EndpointID eid2(str2);
    return (eid1 == eid2);
}

#define EIDCHECK(_valid, _known, _str)          \
do {                                            \
                                                \
    CHECK_EQUAL(valid(_str), _valid);           \
    CHECK_EQUAL(known(_str), _known);           \
} while (0)

#define EIDEQUAL(_equal, _eid1, _eid2)                  \
do {                                                    \
    CHECK_EQUAL(equal(_eid1, _eid2), _equal);           \
} while(0)

#define EIDMATCH(_match, _pattern, _eid)                                \
do {                                                                    \
    EndpointIDPattern p(_pattern);                                      \
    EndpointID eid(_eid);                                               \
                                                                        \
    CHECK(p.valid() && eid.valid());                                    \
                                                                        \
    CHECK_EQUAL((p.assign(_pattern), eid.assign(_eid), p.match(eid)),   \
                _match);                                                \
} while (0);

#define EIDPATTERNCHECK(_valid, _pattern)           \
do {                                                \
    EndpointIDPattern p(_pattern);                  \
    CHECK_EQUAL(p.valid(), _valid);                       \
} while (0);

DECLARE_TEST(Invalid) {
    // test a bunch of invalid eids
    EIDCHECK(INVALID, UNKNOWN, "");
    EIDCHECK(INVALID, UNKNOWN, "foo");
    EIDCHECK(INVALID, UNKNOWN, ":foo");
    EIDCHECK(INVALID, UNKNOWN, "1abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "-abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "+abc:bar");
    EIDCHECK(INVALID, UNKNOWN, ".abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "abc@123:bar");
    EIDCHECK(INVALID, UNKNOWN, "abc_123:bar");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Unknown) {
    // test some valid but unknown eids
    EIDCHECK(VALID, UNKNOWN, "foo:");
    EIDCHECK(VALID, UNKNOWN, "foo:bar");
    EIDCHECK(VALID, UNKNOWN, "fooBAR123:bar");
    EIDCHECK(VALID, UNKNOWN, "foo+BAR-123.456:baz");
    EIDCHECK(VALID, UNKNOWN, "foo:bar/baz/0123456789_-+=.,\';:~");

    return UNIT_TEST_PASSED;
}
 
DECLARE_TEST(DTN) {
    // test parsing for some valid names
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu");
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu/");
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu/demux");
    EIDCHECK(VALID,   KNOWN, "dtn://10.0.0.1/");
    EIDCHECK(VALID,   KNOWN, "dtn://10.0.0.1/demux/some/more");
    EIDCHECK(VALID,   KNOWN, "dtn://255.255.255.255/");
    EIDCHECK(VALID,   KNOWN, "dtn://host/demux");
    EIDCHECK(VALID,   KNOWN, "dtn://host:1000/demux");

    // and the lone valid null scheme
    EIDCHECK(VALID,   KNOWN, "dtn:none");
    // and some invalid ones
    EIDCHECK(INVALID, KNOWN, "dtn:/");
    EIDCHECK(INVALID, KNOWN, "dtn://");
    EIDCHECK(INVALID, KNOWN, "dtn:host");
    EIDCHECK(INVALID, KNOWN, "dtn:/host");

    // test the service tag manipulation
    EndpointID eid;
    DO(eid.assign("dtn://host"));
    DO(eid.append_service_tag("/service"));
    CHECK_EQUALSTR(eid.c_str(), "dtn://host/service");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DTNMatch) {
    // valid matches
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux",
             "dtn://tier.cs.berkeley.edu/demux");
    
    EIDMATCH(MATCH,
             "dtn://10.0.0.1/demux",
             "dtn://10.0.0.1/demux");
    
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu/demux");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(MATCH,
             "dtn://*",
             "dtn://tier.cs.berkeley.edu");

    EIDMATCH(MATCH,
             "dtn://*/",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://*/*",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://*/*",
             "dtn://tier.cs.berkeley.edu/something");

    EIDMATCH(MATCH,
             "dtn://*/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(MATCH,
             "dtn://*.edu/foo",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/foo",
             "dtn://*.edu/foo");
    
    EIDMATCH(MATCH,
             "dtn://tier.*.berkeley.*/foo",
             "dtn://tier.cs.berkeley.edu/foo");
    
    EIDMATCH(MATCH,
             "dtn://*.edu/*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://*.*.*.edu/*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://*.edu/f*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=",
             "dtn://tier.cs.berkeley.edu/demux?foo=");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&bar=baz",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&bar=baz");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4");

    // invalid matches
    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host2/demux");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1:9999/demux");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1/demux2");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1/demux/something");

    // the query component is not used when matching DTN URIs.
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=",
             "dtn://tier.cs.berkeley.edu/demux?bar=");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?foo=baz");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?bar=foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=4&d=3");

    // the fragment component is not used when matching DTN URIs.
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux#foo",
             "dtn://tier.cs.berkeley.edu/demux#foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux#foo",
             "dtn://tier.cs.berkeley.edu/demux#bar");

    // the none ssp never matches
    EIDMATCH(NOMATCH,
             "dtn:none",
             "dtn://none");
    
    EIDMATCH(NOMATCH,
             "dtn:none",
             "dtn:none");

    // finally, don't match other schemes
    EIDMATCH(NOMATCH,
             "dtn://host",
             "dtnme://host");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Wildcard) {
    // test the special wildcard scheme
    EndpointIDPattern p;
    CHECK_EQUAL(VALID, (p.assign("*:*"), p.valid()));
    CHECK_EQUAL(KNOWN, (p.assign("*:*"), p.known_scheme()));

    CHECK_EQUAL(INVALID, (p.assign("*:"),     p.valid()));
    CHECK_EQUAL(INVALID, (p.assign("*:abc"),  p.valid()));
    CHECK_EQUAL(INVALID, (p.assign("*:*abc"), p.valid()));

    return UNIT_TEST_PASSED;
}
    
DECLARE_TEST(WildcardMatch) {
    // test wildcard matching with random strings
    EIDMATCH(MATCH, "*:*", "foo:bar");
    EIDMATCH(MATCH, "*:*", "flsdfllsdfgj:087490823uodf");
    EIDMATCH(MATCH, "*:*", "dtn://host/demux");

    // now we match the null eid as well
    EIDMATCH(MATCH, "*:*", "dtn:none");
    
    return UNIT_TEST_PASSED;
}

// XXX CEJ These tests are invalid by RFC 3986
DECLARE_TEST(Ethernet) {
    EIDCHECK(VALID,   KNOWN, "eth://01:23:45:67:89:ab");
    EIDCHECK(VALID,   KNOWN, "eth://ff:ff:ff:ff:ff:ff");
    EIDCHECK(VALID,   KNOWN, "eth:01:23:45:67:89:ab");
    EIDCHECK(VALID,   KNOWN, "eth:ff:ff:ff:ff:ff:ff");
    EIDCHECK(INVALID, KNOWN, "eth:/ff:ff:ff:ff:ff:ff");
    EIDCHECK(INVALID, KNOWN, "eth://");
    EIDCHECK(INVALID, KNOWN, "eth://ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:ff:ff:");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(String) {
    EIDCHECK(VALID,   KNOWN, "str:anything");
    EIDCHECK(VALID,   KNOWN, "str://anything");
    EIDCHECK(VALID,   KNOWN, "str:/anything");

    EIDCHECK(INVALID, KNOWN, "str:");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(StringMatch) {
    EIDMATCH(MATCH, "str:foo", "str:foo");
    EIDMATCH(MATCH, "str://anything", "str://anything");

    EIDMATCH(NOMATCH, "str:none", "dtn:none");
    EIDMATCH(NOMATCH, "dtn:none", "str:none");

    EIDMATCH(NOMATCH, "str://host", "dtn://host");
    EIDMATCH(NOMATCH, "dtn://host", "str://host");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Session) {
    EIDCHECK(VALID,   KNOWN, "dtn-session:any:uri");
    EIDCHECK(VALID,   KNOWN, "dtn-session:any://other/uri");
    EIDCHECK(VALID,   KNOWN, "dtn-session:dtn:none");

    EIDCHECK(INVALID, KNOWN, "dtn-session:");
    EIDCHECK(INVALID, KNOWN, "dtn-session:not_a_uri");
    EIDCHECK(INVALID, KNOWN, "dtn-session:uri");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SessionMatch) {
    EIDMATCH(MATCH, "dtn-session:foo:bar", "dtn-session:foo:bar");
    EIDMATCH(MATCH, "dtn-session:foo://bar", "dtn-session:foo://bar");
    EIDMATCH(MATCH, "dtn-session:foo://*", "dtn-session:foo://bar");
    EIDMATCH(MATCH, "dtn-session:foo:*", "dtn-session:foo:bar");
    EIDMATCH(MATCH, "dtn-session:foo:*", "dtn-session:foo://bar");
    EIDMATCH(MATCH, "dtn-session:*:*", "dtn-session:foo:bar");
    EIDMATCH(MATCH, "dtn-session:*:*", "dtn-session:foo://bar");
    EIDMATCH(MATCH, "dtn-session:*", "dtn-session:foo:bar");

    EIDMATCH(NOMATCH, "dtn-session:foo:bar", "dtn-session:foo://bar");
    EIDMATCH(NOMATCH, "dtn-session:*:bar", "dtn-session:foo:baz");
    EIDMATCH(NOMATCH, "dtn-session:foo:*", "dtn-session:foo2:bar");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(URIGenericSyntax) {

    // scheme component validation
    EIDCHECK(INVALID, UNKNOWN, "");
    EIDCHECK(INVALID, UNKNOWN, ":");
    EIDCHECK(INVALID, UNKNOWN, "eid");
    EIDCHECK(VALID,   UNKNOWN, "e:");
    EIDCHECK(INVALID, UNKNOWN, "1eid:");
    EIDCHECK(VALID,   UNKNOWN, "eid1:");
    EIDCHECK(VALID,   UNKNOWN, "eid123+-.:");
    EIDCHECK(INVALID, UNKNOWN, "eid123+-.!:");

    // path component validation
    EIDCHECK(VALID,   UNKNOWN, "eid:");
    EIDCHECK(VALID,   UNKNOWN, "eid:?");
    EIDCHECK(VALID,   UNKNOWN, "eid:#");
    EIDCHECK(VALID,   UNKNOWN, "eid:/");
    EIDCHECK(VALID,   UNKNOWN, "eid://");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority//");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path/more_path");
    EIDCHECK(VALID,   UNKNOWN, "eid:path");
    EIDCHECK(VALID,   UNKNOWN, "eid:/path");
    EIDCHECK(VALID,   UNKNOWN, "eid:/path/more_path");
    EIDCHECK(VALID,   UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@");
    EIDCHECK(VALID,   UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@#");
    EIDCHECK(INVALID, UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@<");
    EIDCHECK(INVALID, UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@%");
    EIDCHECK(INVALID, UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@%1");
    EIDCHECK(VALID,   UNKNOWN, "eid:/path123-._~!$&;()*+,;=:@%1a");
    EIDCHECK(VALID,   UNKNOWN, "eid:/%1a");

    // query component validation
    EIDCHECK(VALID,   UNKNOWN, "eid:?");
    EIDCHECK(VALID,   UNKNOWN, "eid:path?");
    EIDCHECK(VALID,   UNKNOWN, "eid:/?");
    EIDCHECK(VALID,   UNKNOWN, "eid://?");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority?");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path?");
    EIDCHECK(VALID,   UNKNOWN, "eid:?#");
    EIDCHECK(VALID,   UNKNOWN, "eid:??");
    EIDCHECK(VALID,   UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?");
    EIDCHECK(VALID,   UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?#");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority?c3-._~!$&;()*+,;=:@/?");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path?c3-._~!$&;()*+,;=:@/?");
    EIDCHECK(INVALID, UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?<");
    EIDCHECK(INVALID, UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?%");
    EIDCHECK(INVALID, UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?%2");
    EIDCHECK(VALID,   UNKNOWN, "eid:?abc123-._~!$&;()*+,;=:@/?%2b");
    EIDCHECK(VALID,   UNKNOWN, "eid:?%2b");

    // fragment component validation
    EIDCHECK(VALID,   UNKNOWN, "eid:#");
    EIDCHECK(VALID,   UNKNOWN, "eid:path#");
    EIDCHECK(VALID,   UNKNOWN, "eid:path?#");
    EIDCHECK(VALID,   UNKNOWN, "eid:/#");
    EIDCHECK(VALID,   UNKNOWN, "eid://#");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority#");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path#");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path?#");
    EIDCHECK(VALID,   UNKNOWN, "eid:#?");
    EIDCHECK(VALID,   UNKNOWN, "eid:#abc123-._~!$&;()*+,;=:@/?");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority#c3-._~!$&;()*+,;=:@/?");
    EIDCHECK(VALID,   UNKNOWN, "eid://authority/path#c3-._~!$&;()*+,;=:@/?");
    EIDCHECK(INVALID, UNKNOWN, "eid:#abc123-._~!$&;()*+,;=:@/?<");
    EIDCHECK(INVALID, UNKNOWN, "eid:##");
    EIDCHECK(INVALID, UNKNOWN, "eid:#abc123-._~!$&;()*+,;=:@/?%");
    EIDCHECK(INVALID, UNKNOWN, "eid:#abc123-._~!$&;()*+,;=:@/?%3");
    EIDCHECK(VALID,   UNKNOWN, "eid:#abc123-._~!$&;()*+,;=:@/?%3c");
    EIDCHECK(VALID,   UNKNOWN, "eid:#%3c");

    // authority component validation
    EIDCHECK(VALID,   UNKNOWN, "eid://");
    EIDCHECK(VALID,   UNKNOWN, "eid:///path");
    EIDCHECK(VALID,   UNKNOWN, "eid://@/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@host/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo123-._~!$&;()*+,;=:@/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://userinfo123-._~!$&;()*+,;=:<@/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://userinfo123-._~!$&;()*+,;=:%@/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://userinfo123-._~!$&;()*+,;=:%4@/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo123-._~!$&;()*+,;=:%4d@/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://%4d@/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@/path@");
    EIDCHECK(VALID,   UNKNOWN, "eid://host/path@");
    EIDCHECK(VALID,   UNKNOWN, "eid://:");
    EIDCHECK(VALID,   UNKNOWN, "eid://:/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://:1/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://:123");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@:123/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@host:123/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://host:123");
    EIDCHECK(INVALID, UNKNOWN, "eid://host:123a");
    EIDCHECK(VALID,   UNKNOWN, "eid://host");
    EIDCHECK(VALID,   UNKNOWN, "eid://host/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://host?");
    EIDCHECK(VALID,   UNKNOWN, "eid://host123-._~!$&;()*+,;=:123/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://host123-._~!$&;()*+,;=<:123/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://host123-._~!$&;()*+,;=%:123/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://host123-._~!$&;()*+,;=%5:123/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://host123-._~!$&;()*+,;=%5e:123/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://%5e:123/path");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@0.1.2.3:80/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://[host");
    EIDCHECK(INVALID, UNKNOWN, "eid://[]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[v]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[v123]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[V123a]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[v123.]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[v123.a]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[v123.abc123-._~!$&;()*+,;=:]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[v123.abc123-._~!$&;()*+,;=:<]");
    EIDCHECK(VALID,   UNKNOWN, "eid://userinfo@[v123.abc123]:80/path");
    EIDCHECK(INVALID, UNKNOWN, "eid://userinfo@[v123.abc123]80/path");
    EIDCHECK(VALID,   UNKNOWN,
             "eid://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
    EIDCHECK(INVALID, UNKNOWN,
             "eid://[fffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
    EIDCHECK(INVALID, UNKNOWN,
             "eid://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:ff:ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::ff:ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[::ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:::ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:ff:]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:ff::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff::ff:ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff:ff:ff::ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:::ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff:ff::ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff::ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff::ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff::ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff::ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[::ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[gff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:gff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::gff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ffg]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:g]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff::g]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:gff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ffg:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff::ff:ff::ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff::ff:ff:ff:ff:ff::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::ff:ff:ff:ff:ff:ff::]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff::.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff:0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff::0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:ff.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:g.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:0.1.2.g]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:0.1..3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:0.1.g3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:0.1.2.3]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[::0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[:::0.1.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::01.1.2.3]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[::0.1.255.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::0.1.256.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::0.1.2.3.4]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::0.1.2.3xxx]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::0.1.2]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[::0.1111.2.3]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[0.1.2.3::ff:ff:ff:ff:ff:ff]");
    EIDCHECK(INVALID, UNKNOWN, "eid://[ff:ff:ff::0.1.2.3::ff:ff:ff]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff:ff:ff:ff:ff:0.1.2.3]");
    EIDCHECK(VALID,   UNKNOWN, "eid://[ff:ff:ff:ff:ff::0.1.2.3]");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(URIEquality) {

    EIDEQUAL(EQUAL,    "eid://simpletest", "eid://simpletest");
    EIDEQUAL(EQUAL,    "eid://simpletest", "eid://SimpleTest");
    EIDEQUAL(EQUAL,    "example://a/b/c/%7Bfoo%7D",
                       "example://a/./b/../b/%63/%7bfoo%7d");
    EIDEQUAL(EQUAL,    "example://a/b/c/%7Bfoo%7D",
                       "example://a/./b/../b/%63/%7bfoo%7d");
    EIDEQUAL(EQUAL,    "eid:/test/path", "eid:/test/path");
    EIDEQUAL(EQUAL,    "eid:/test../path./again.", "eid:/test../path./again.");
    EIDEQUAL(EQUAL,    "eid:/test../path/again..", "eid:/test../path/again..");
    EIDEQUAL(EQUAL,    "eid:", "eid:.");
    EIDEQUAL(EQUAL,    "eid:", "eid:..");
    EIDEQUAL(EQUAL,    "eid:", "eid:././././.");
    EIDEQUAL(EQUAL,    "eid:/", "eid:/././././.");
    EIDEQUAL(EQUAL,    "eid:", "eid:../../../../..");
    EIDEQUAL(EQUAL,    "eid:/", "eid:/../../../../..");
    EIDEQUAL(EQUAL,    "eid:/", "eid:/./.././../.");
    EIDEQUAL(EQUAL,    "eid:/t/e/s/t/", "eid:/t/./e/./s/./t/.");
    EIDEQUAL(EQUAL,    "eid:/t/e/s/t/", "eid:/t/../t/e/../e/s/../s/t/../t/");
    EIDEQUAL(EQUAL,    "eid:/t/e/s/t/", "eid:/t/e/../../t/e/s/t/../../s/t/");
    EIDEQUAL(EQUAL,    "eid:/", "eid:/t/./e/./s/./t/./.././.././.././../.");
    EIDEQUAL(EQUAL,    "eid:/", "eid:t/./e/./s/./t/./.././.././.././../.");
    EIDEQUAL(EQUAL,    "eid:/test../path./", "eid:/test../path././again../..");
    EIDEQUAL(EQUAL,    "eid:/", "eid:/test/..");
    EIDEQUAL(EQUAL,    "eid:/", "eid:test/..");
    EIDEQUAL(EQUAL,
             "eid://%7DUSERc%7B@host.c.%7B:55/Test/c/Path/?Cc%7D#Cc%7B",
             "EiD://%7dUSER%63%7b@HoST.%63.%7b:55/Test/%63/Path/%7b/../.?C%63%7d#C%63%7b");
         
    return UNIT_TEST_PASSED;
}

#define IPNCHECK(ssp, node, service)            \
{                                               \
    u_int64_t n, s;                             \
    CHECK(IPNScheme::parse(ssp, &n, &s));       \
    CHECK_EQUAL_U64(node, n);                   \
    CHECK_EQUAL_U64(service, s);                \
}

DECLARE_TEST(IPN) {
    EIDCHECK(VALID, KNOWN, "ipn:0.0");
    EIDCHECK(VALID, KNOWN, "ipn:0.1");
    EIDCHECK(VALID, KNOWN, "ipn:1.0");
    EIDCHECK(VALID, KNOWN, "ipn:1.1");
    EIDCHECK(VALID, KNOWN, "ipn:18446744073709551615.18446744073709551615");

    EIDCHECK(INVALID, KNOWN, "ipn:");
    EIDCHECK(INVALID, KNOWN, "ipn:.");
    EIDCHECK(INVALID, KNOWN, "ipn:1.");
    EIDCHECK(INVALID, KNOWN, "ipn:1.a");
    EIDCHECK(INVALID, KNOWN, "ipn:a.1");
    EIDCHECK(INVALID, KNOWN, "ipn:1.1a");

    EIDPATTERNCHECK(VALID, "ipn:*");
    EIDPATTERNCHECK(VALID, "ipn:1.*");
    EIDPATTERNCHECK(INVALID, "ipn:*.");
    EIDPATTERNCHECK(INVALID, "ipn:*a");
    EIDPATTERNCHECK(INVALID, "ipn:*.*");
    EIDPATTERNCHECK(INVALID, "ipn:1.*a");
    EIDPATTERNCHECK(INVALID, "ipn:*.1");

    EIDMATCH(MATCH, "ipn:1.1", "ipn:1.1");
    EIDMATCH(MATCH, "ipn:1.*", "ipn:1.1");
    EIDMATCH(MATCH, "ipn:*", "ipn:1.1");
    EIDMATCH(MATCH, "ipn:*", "ipn:999.999");
    EIDMATCH(NOMATCH, "ipn:1.1", "ipn:1.2");
    EIDMATCH(NOMATCH, "ipn:1.2", "ipn:1.1");
    EIDMATCH(NOMATCH, "ipn:2.1", "ipn:1.1");
    EIDMATCH(NOMATCH, "ipn:1.1", "ipn:2.1");

    IPNCHECK("1.1", 1, 1);
    IPNCHECK("0.0", 0, 0);
    
    IPNCHECK("999.999", 999, 999);
    IPNCHECK("18446744073709551615.18446744073709551615",
             18446744073709551615ULL, 18446744073709551615ULL);
    return UNIT_TEST_PASSED;
}    

DECLARE_TESTER(EndpointIDTester) {
    ADD_TEST(Invalid);
    ADD_TEST(Unknown);
    ADD_TEST(DTN);
    ADD_TEST(DTNMatch);
    ADD_TEST(Wildcard);
    ADD_TEST(WildcardMatch);
// XXX CEJ These tests are invalid by RFC 3986
//#ifdef __linux__
//    ADD_TEST(Ethernet);
//#endif
    ADD_TEST(String);
    ADD_TEST(StringMatch);
    ADD_TEST(Session);
    ADD_TEST(SessionMatch);
    ADD_TEST(URIGenericSyntax);
    ADD_TEST(URIEquality);
    ADD_TEST(IPN);
}

DECLARE_TEST_FILE(EndpointIDTester, "endpoint id test");
