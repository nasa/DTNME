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

#include <debug/DebugUtils.h>
#include <serialize/MarshalSerialize.h>
#include <util/ScratchBuffer.h>
#include <util/UnitTest.h>
#include <util/URI.h>

using namespace oasys;

typedef enum {
    INVALID = 0,
    VALID   = 1
} valid_t;

typedef enum {
    NOTEQUAL = 0,
    EQUAL    = 1
} equal_t;

uri_parse_err_t parse(const char* str)
{
    URI uri(str);
    return uri.parse_status();
}

std::string normalize(const char* str)
{
    URI uri;
    uri.set_normalize(true);
    uri.assign(str);
    return uri.uri();
}

int equal(const char* str1, const char* str2)
{
    URI uri1(str1);
    URI uri2(str2);
    return uri1 == uri2;
}

#define CHECK_PARSE(_str, _ok)                  \
do {                                            \
    CHECK_EQUAL(parse(_str), _ok);              \
} while (0)

#define CHECK_NORMALIZE(_uri, _norm)            \
do {                                            \
    CHECK_EQUALSTR(normalize(_uri), _norm);     \
} while(0)

#define CHECK_COMPONENTS(_uri1, _str)           \
do {                                            \
    URI _uri2(_str);                            \
    CHECK_EQUAL(_uri1.parse_status(),           \
                _uri2.parse_status());          \
    CHECK_EQUALSTR(_uri1.uri(), _str);          \
    CHECK_EQUALSTR(_uri1.scheme(),              \
                   _uri2.scheme());             \
    CHECK_EQUALSTR(_uri1.ssp(),                 \
                   _uri2.ssp());                \
    CHECK_EQUALSTR(_uri1.authority(),           \
                   _uri2.authority());          \
    CHECK_EQUALSTR(_uri1.userinfo(),            \
                   _uri2.userinfo());           \
    CHECK_EQUALSTR(_uri1.host(),                \
                   _uri2.host());               \
    CHECK_EQUALSTR(_uri1.port(),                \
                   _uri2.port());               \
    CHECK_EQUAL(_uri1.port_num(),               \
                _uri2.port_num());              \
    CHECK_EQUALSTR(_uri1.path(),                \
                   _uri2.path());               \
    CHECK_EQUALSTR(_uri1.query(),               \
                   _uri2.query());              \
    CHECK_EQUALSTR(_uri1.fragment(),            \
                   _uri2.fragment());           \
} while(0)

//----------------------------------------------------------------------

DECLARE_TEST(BasicParse) {
    URI uri("scheme:ssp");
    
    CHECK_EQUALSTR(uri.scheme(),    "scheme");
    CHECK_EQUALSTR(uri.ssp(),       "ssp");
    CHECK_EQUALSTR(uri.authority(), "");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "");
    CHECK_EQUALSTR(uri.port(),      "");
    CHECK_EQUALSTR(uri.path(),      "ssp");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "");
    
    uri.assign("scheme://user@host:100/path?query#fragment");
    
    CHECK_EQUALSTR(uri.scheme(),    "scheme");
    CHECK_EQUALSTR(uri.ssp(),       "//user@host:100/path?query#fragment");
    CHECK_EQUALSTR(uri.authority(), "//user@host:100");
    CHECK_EQUALSTR(uri.userinfo(),  "user@");
    CHECK_EQUALSTR(uri.host(),      "host");
    CHECK_EQUALSTR(uri.port(),      "100");
    CHECK_EQUALSTR(uri.path(),      "/path");
    CHECK_EQUALSTR(uri.query(),     "?query");
    CHECK_EQUALSTR(uri.fragment(),  "#fragment");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Accessors) {
    URI uri;

    DO(uri.assign(std::string("scheme:ssp")));
    CHECK_COMPONENTS(uri, "scheme:ssp");
    
    DO(uri.set_scheme("scheme2"));
    CHECK_COMPONENTS(uri, "scheme2:ssp");
       
    DO(uri.set_ssp("ssp2"));
    CHECK_COMPONENTS(uri, "scheme2:ssp2");
    
    DO(uri.set_scheme("s"));
    CHECK_COMPONENTS(uri, "s:ssp2");

    DO(uri.set_ssp("//user@host:100/path?query#fragment"));
    CHECK_COMPONENTS(uri, "s://user@host:100/path?query#fragment");

    DO(uri.set_host("host2"));
    CHECK_COMPONENTS(uri, "s://user@host2:100/path?query#fragment");
       
    DO(uri.set_host("h1"));
    CHECK_COMPONENTS(uri, "s://user@h1:100/path?query#fragment");
       
    DO(uri.set_path("/a"));
    CHECK_COMPONENTS(uri, "s://user@h1:100/a?query#fragment");
       
    DO(uri.set_path("/aabbbaa"));
    CHECK_COMPONENTS(uri, "s://user@h1:100/aabbbaa?query#fragment");
       
    DO(uri.set_userinfo("joebob@"));
    CHECK_COMPONENTS(uri, "s://joebob@h1:100/aabbbaa?query#fragment");
       
    DO(uri.set_userinfo(""));
    CHECK_COMPONENTS(uri, "s://h1:100/aabbbaa?query#fragment");
       
    DO(uri.set_port("10"));
    CHECK_COMPONENTS(uri, "s://h1:10/aabbbaa?query#fragment");
       
    DO(uri.set_port("99"));
    CHECK_COMPONENTS(uri, "s://h1:99/aabbbaa?query#fragment");
       
    DO(uri.set_query(""));
    CHECK_COMPONENTS(uri, "s://h1:99/aabbbaa#fragment");
       
    DO(uri.set_query("?q2"));
    CHECK_COMPONENTS(uri, "s://h1:99/aabbbaa?q2#fragment");
       
    DO(uri.set_fragment("#bigfragment"));
    CHECK_COMPONENTS(uri, "s://h1:99/aabbbaa?q2#bigfragment");
       
    DO(uri.set_fragment(""));
    CHECK_COMPONENTS(uri, "s://h1:99/aabbbaa?q2");

    DO(uri.assign("scheme:"));
    DO(uri.set_authority("//host"));
    CHECK_EQUALSTR(uri.uri(),       "scheme://host");
    CHECK_EQUALSTR(uri.ssp(),       "//host");
    CHECK_EQUALSTR(uri.authority(), "//host");
    CHECK_EQUALSTR(uri.path(),      "");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "host");
    CHECK_EQUALSTR(uri.port(),      "");

    DO(uri.assign("scheme:"));
    DO(uri.set_path("/path"));
    CHECK_EQUALSTR(uri.uri(),       "scheme:/path"); 
    CHECK_EQUALSTR(uri.ssp(),       "/path");
    CHECK_EQUALSTR(uri.authority(), "");
    CHECK_EQUALSTR(uri.path(),      "/path");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "");
    CHECK_EQUALSTR(uri.port(),      "");
       
    DO(uri.assign("scheme:"));
    DO(uri.set_query("?a=b"));
    CHECK_EQUALSTR(uri.uri(),       "scheme:?a=b");
    CHECK_EQUALSTR(uri.authority(), "");
    CHECK_EQUALSTR(uri.path(),      "");
    CHECK_EQUALSTR(uri.query(),     "?a=b");
    CHECK_EQUALSTR(uri.fragment(),  "");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "");
    CHECK_EQUALSTR(uri.port(),      "");

    DO(uri.assign("scheme:"));
    DO(uri.set_fragment("#fragment"));
    CHECK_EQUALSTR(uri.uri(),       "scheme:#fragment");
    CHECK_EQUALSTR(uri.authority(), "");
    CHECK_EQUALSTR(uri.path(),      "");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "#fragment");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "");
    CHECK_EQUALSTR(uri.port(),      "");

    DO(uri.assign("scheme://"));
    DO(uri.set_host("host"));
    CHECK_EQUALSTR(uri.uri(),       "scheme://host");
    CHECK_EQUALSTR(uri.ssp(),       "//host");
    CHECK_EQUALSTR(uri.authority(), "//host");
    CHECK_EQUALSTR(uri.path(),      "");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "");
    CHECK_EQUALSTR(uri.userinfo(),  "");
    CHECK_EQUALSTR(uri.host(),      "host");
    CHECK_EQUALSTR(uri.port(),      "");

    DO(uri.assign("scheme://"));
    DO(uri.set_userinfo("mike@"));
    CHECK_EQUALSTR(uri.uri(),       "scheme://mike@");
    CHECK_EQUALSTR(uri.ssp(),       "//mike@");
    CHECK_EQUALSTR(uri.authority(), "//mike@");
    CHECK_EQUALSTR(uri.path(),      "");
    CHECK_EQUALSTR(uri.query(),     "");
    CHECK_EQUALSTR(uri.fragment(),  "");
    CHECK_EQUALSTR(uri.userinfo(),  "mike@");
    CHECK_EQUALSTR(uri.host(),      "");
    CHECK_EQUALSTR(uri.port(),      "");

    DO(uri.assign("scheme:"));
    DO(uri.set_authority("//user@host:99"));
    DO(uri.set_path("/path"));
    DO(uri.set_query("?q=a"));
    DO(uri.set_fragment("#fragment"));
    CHECK_EQUALSTR(uri.uri(),       "scheme://user@host:99/path?q=a#fragment");
    CHECK_EQUALSTR(uri.ssp(),       "//user@host:99/path?q=a#fragment");
    CHECK_EQUALSTR(uri.authority(), "//user@host:99");
    CHECK_EQUALSTR(uri.path(),      "/path");
    CHECK_EQUALSTR(uri.query(),     "?q=a");
    CHECK_EQUALSTR(uri.fragment(),  "#fragment");
    CHECK_EQUALSTR(uri.userinfo(),  "user@");
    CHECK_EQUALSTR(uri.host(),      "host");
    CHECK_EQUALSTR(uri.port(),      "99");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SchemeValidate) {
    // test validation of the scheme
    CHECK_PARSE("foo:",        URI_PARSE_OK);
    CHECK_PARSE("foo:bar",     URI_PARSE_OK);
    CHECK_PARSE("",            URI_PARSE_NO_URI);
    CHECK_PARSE("foo",         URI_PARSE_NO_SEP);
    CHECK_PARSE("fooBAR123:x", URI_PARSE_OK);
    CHECK_PARSE("foo+BAR-123.456:baz", URI_PARSE_OK);
    CHECK_PARSE("foo:bar/baz/0123456789_-+=.,\';:~", URI_PARSE_OK);
    CHECK_PARSE(":foo",        URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("1abc:bar",    URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("-abc:bar",    URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("+abc:bar",    URI_PARSE_BAD_SCHEME);
    CHECK_PARSE(".abc:bar",    URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("abc@123:bar", URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("abc_123:bar", URI_PARSE_BAD_SCHEME);
    CHECK_PARSE(":",           URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("uri",         URI_PARSE_NO_SEP);
    CHECK_PARSE("e:",          URI_PARSE_OK);
    CHECK_PARSE("1uri:",       URI_PARSE_BAD_SCHEME);
    CHECK_PARSE("uri1:",       URI_PARSE_OK);
    CHECK_PARSE("uri123+-.:",  URI_PARSE_OK);
    CHECK_PARSE("uri123+-.!:", URI_PARSE_BAD_SCHEME);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(PathValidate) {
    CHECK_PARSE("uri:",                             URI_PARSE_OK);
    CHECK_PARSE("uri:?",                            URI_PARSE_OK);
    CHECK_PARSE("uri:#",                            URI_PARSE_OK);
    CHECK_PARSE("uri:/",                            URI_PARSE_OK);
    CHECK_PARSE("uri://",                           URI_PARSE_OK);
    CHECK_PARSE("uri://authority",                  URI_PARSE_OK);
    CHECK_PARSE("uri://authority/",                 URI_PARSE_OK);
    CHECK_PARSE("uri://authority//",                URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path",             URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path/more_path",   URI_PARSE_OK);
    CHECK_PARSE("uri:path",                         URI_PARSE_OK);
    CHECK_PARSE("uri:/path",                        URI_PARSE_OK);
    CHECK_PARSE("uri:/path/more_path",              URI_PARSE_OK);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@",    URI_PARSE_OK);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@#",   URI_PARSE_OK);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@<",   URI_PARSE_BAD_PATH);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@%",   URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@%1",  URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:/path123-._~!$&;()*+,;=:@%1a", URI_PARSE_OK);
    CHECK_PARSE("uri:/%1a",                         URI_PARSE_OK);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(QueryValidate) {
    CHECK_PARSE("uri:?",                                      URI_PARSE_OK);
    CHECK_PARSE("uri:path?",                                  URI_PARSE_OK);
    CHECK_PARSE("uri:/?",                                     URI_PARSE_OK);
    CHECK_PARSE("uri://?",                                    URI_PARSE_OK);
    CHECK_PARSE("uri://authority?",                           URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path?",                      URI_PARSE_OK);
    CHECK_PARSE("uri:?#",                                     URI_PARSE_OK);
    CHECK_PARSE("uri:??",                                     URI_PARSE_OK);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?",             URI_PARSE_OK);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?#",            URI_PARSE_OK);
    CHECK_PARSE("uri://authority?c3-._~!$&;()*+,;=:@/?",      URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path?c3-._~!$&;()*+,;=:@/?", URI_PARSE_OK);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?<",            URI_PARSE_BAD_QUERY);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?%",            URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?%2",           URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:?abc123-._~!$&;()*+,;=:@/?%2b",          URI_PARSE_OK);
    CHECK_PARSE("uri:?%2b",                                   URI_PARSE_OK);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(FragmentValidate) {
    CHECK_PARSE("uri:#",                             URI_PARSE_OK);
    CHECK_PARSE("uri:path#",                         URI_PARSE_OK);
    CHECK_PARSE("uri:path?#",                        URI_PARSE_OK);
    CHECK_PARSE("uri:/#",                            URI_PARSE_OK);
    CHECK_PARSE("uri://#",                           URI_PARSE_OK);
    CHECK_PARSE("uri://authority#",                  URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path#",             URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path?#",            URI_PARSE_OK);
    CHECK_PARSE("uri:#?",                            URI_PARSE_OK);
    CHECK_PARSE("uri:#abc123-._~!$&;()*+,;=:@/?",    URI_PARSE_OK);
    CHECK_PARSE("uri://authority#c3-._~!$&;()*+,;=:@/?", URI_PARSE_OK);
    CHECK_PARSE("uri://authority/path#c3-._~!$&;()*+,;=:@/?", URI_PARSE_OK);
    CHECK_PARSE("uri:#abc123-._~!$&;()*+,;=:@/?<",   URI_PARSE_BAD_FRAGMENT);
    CHECK_PARSE("uri:##",                            URI_PARSE_BAD_FRAGMENT);
    CHECK_PARSE("uri:#abc123-._~!$&;()*+,;=:@/?%",   URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:#abc123-._~!$&;()*+,;=:@/?%3",  URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri:#abc123-._~!$&;()*+,;=:@/?%3c", URI_PARSE_OK);
    CHECK_PARSE("uri:#%3c",                          URI_PARSE_OK);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AuthorityValidate) {
    CHECK_PARSE("uri://",                                     URI_PARSE_OK);
    CHECK_PARSE("uri:///path",                                URI_PARSE_OK);
    CHECK_PARSE("uri://@/path",                               URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@",                            URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@/path",                       URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@host/path",                   URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo123-._~!$&;()*+,;=:@/path",    URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo123-._~!$&;()*+,;=:<@/path",   URI_PARSE_BAD_USERINFO);
    CHECK_PARSE("uri://userinfo123-._~!$&;()*+,;=:%@/path",   URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri://userinfo123-._~!$&;()*+,;=:%4@/path",  URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri://userinfo123-._~!$&;()*+,;=:%4d@/path", URI_PARSE_OK);
    
    CHECK_PARSE("uri://%4d@/path",                          URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@/path@",                    URI_PARSE_OK);
    CHECK_PARSE("uri://host/path@",                         URI_PARSE_OK);
    CHECK_PARSE("uri://:",                                  URI_PARSE_OK);
    CHECK_PARSE("uri://:/path",                             URI_PARSE_OK);
    CHECK_PARSE("uri://:1/path",                            URI_PARSE_OK);
    CHECK_PARSE("uri://:123",                               URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@:123/path",                 URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@host:123/path",             URI_PARSE_OK);
    CHECK_PARSE("uri://host:123",                           URI_PARSE_OK);
    CHECK_PARSE("uri://host:123a",                          URI_PARSE_BAD_PORT);
    CHECK_PARSE("uri://host",                               URI_PARSE_OK);
    CHECK_PARSE("uri://host/path",                          URI_PARSE_OK);
    CHECK_PARSE("uri://host?",                              URI_PARSE_OK);
    CHECK_PARSE("uri://host123-._~!$&;()*+,;=:123/path",    URI_PARSE_OK);
    CHECK_PARSE("uri://host123-._~!$&;()*+,;=<:123/path",   URI_PARSE_BAD_HOST);
    CHECK_PARSE("uri://host123-._~!$&;()*+,;=%:123/path",   URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri://host123-._~!$&;()*+,;=%5:123/path",  URI_PARSE_BAD_PERCENT);
    CHECK_PARSE("uri://host123-._~!$&;()*+,;=%5e:123/path", URI_PARSE_OK);
    CHECK_PARSE("uri://%5e:123/path",                       URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@0.1.2.3:80/path",           URI_PARSE_OK);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(IPLiteralValidate) {
    CHECK_PARSE("uri://[host",    URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[]",       URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[v]",      URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[v123]",   URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[V123a]",  URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[v123.]",  URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://[v123.a]", URI_PARSE_OK);

    CHECK_PARSE("uri://[v123.abc123-._~!$&;()*+,;=:]",  URI_PARSE_OK);
    CHECK_PARSE("uri://[v123.abc123-._~!$&;()*+,;=:<]", URI_PARSE_BAD_IP_LITERAL);
    CHECK_PARSE("uri://userinfo@[v123.abc123]:80/path", URI_PARSE_OK);
    CHECK_PARSE("uri://userinfo@[v123.abc123]80/path",  URI_PARSE_BAD_PORT);
    
    CHECK_PARSE("uri://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]",  URI_PARSE_OK);
    CHECK_PARSE("uri://[fffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]", URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffff]", URI_PARSE_BAD_IPV6);
    
    CHECK_PARSE("uri://[:ff:ff:ff:ff:ff:ff:ff:ff]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::ff:ff:ff:ff:ff:ff:ff:ff]",    URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[:ff:ff:ff:ff:ff:ff:ff]",        URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::ff:ff:ff:ff:ff:ff:ff]",       URI_PARSE_OK);
    CHECK_PARSE("uri://[:::ff:ff:ff:ff:ff:ff:ff]",      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:ff:]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:ff::]",    URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:]",        URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff::]",       URI_PARSE_OK);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:::]",      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff::ff:ff:ff:ff]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff::ff:ff:ff]",        URI_PARSE_OK);
    CHECK_PARSE("uri://[ff:ff:ff:ff:::ff:ff:ff]",       URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff::ff:ff:ff]",           URI_PARSE_OK);
    CHECK_PARSE("uri://[ff:ff::ff:ff:ff]",              URI_PARSE_OK);
    CHECK_PARSE("uri://[ff:ff::ff:ff]",                 URI_PARSE_OK);
    CHECK_PARSE("uri://[ff::ff:ff]",                    URI_PARSE_OK);
    CHECK_PARSE("uri://[ff::ff]",                       URI_PARSE_OK);
    CHECK_PARSE("uri://[::ff]",                         URI_PARSE_OK);
    CHECK_PARSE("uri://[::]",                           URI_PARSE_OK);
    CHECK_PARSE("uri://[:::]",                          URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[gff:ff:ff:ff:ff:ff:ff]",        URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[:gff:ff:ff:ff:ff:ff:ff]",       URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::gff:ff:ff:ff:ff:ff:ff]",      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ffg]",        URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:g]",       URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff::g]",      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:gff:ff:ff:ff]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ffg:ff:ff:ff:ff]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff::ff:ff::ff:ff:ff]",          URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff::ff:ff:ff:ff:ff::]",         URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::ff:ff:ff:ff:ff:ff::]",        URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff]",         URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:ff:ff]",   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:.1.2.3]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff::.1.2.3]",       URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff:0.1.2.3]", URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff::0.1.2.3]",   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:0.1.2.3]",       URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:ff.1.2.3]",   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:g.1.2.3]",    URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:0.1.2.g]",    URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:0.1..3]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:0.1.g3]",     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[0.1.2.3]",                      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[:0.1.2.3]",                     URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1.2.3]",                    URI_PARSE_OK);
    CHECK_PARSE("uri://[:::0.1.2.3]",                   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::01.1.2.3]",                   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1.255.3]",                  URI_PARSE_OK);
    CHECK_PARSE("uri://[::0.1.256.3]",                  URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1.2.3.4]",                  URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1.2.3xxx]",                 URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1.2]",                      URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[::0.1111.2.3]",                 URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[0.1.2.3::ff:ff:ff:ff:ff:ff]",   URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff::0.1.2.3::ff:ff:ff]",  URI_PARSE_BAD_IPV6);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff:ff:0.1.2.3]",    URI_PARSE_OK);
    CHECK_PARSE("uri://[ff:ff:ff:ff:ff::0.1.2.3]",      URI_PARSE_OK);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Normalize) {
    CHECK_NORMALIZE("uri://simpletest", "uri://simpletest");
    CHECK_NORMALIZE("uri://SimpleTest", "uri://simpletest");
    CHECK_NORMALIZE("example://a/./b/../b/%63/%7bfoo%7d",
                    "example://a/b/c/%7Bfoo%7D");
    CHECK_NORMALIZE("example://a/./b/../b/%63/%7bfoo%7d",
                    "example://a/b/c/%7Bfoo%7D");
    CHECK_NORMALIZE("uri:/test/path", "uri:/test/path");
    CHECK_NORMALIZE("uri:/test../path./again.", "uri:/test../path./again.");
    CHECK_NORMALIZE("uri:/test../path/again..", "uri:/test../path/again..");
    CHECK_NORMALIZE("uri:.", "uri:");
    CHECK_NORMALIZE("uri:..", "uri:");
    CHECK_NORMALIZE("uri:././././.", "uri:");
    CHECK_NORMALIZE("uri:/././././.", "uri:/");
    CHECK_NORMALIZE("uri:../../../../..", "uri:");
    CHECK_NORMALIZE("uri:/../../../../..", "uri:/");
    CHECK_NORMALIZE("uri:/./.././../.", "uri:/");
    CHECK_NORMALIZE("uri:/t/./e/./s/./t/.", "uri:/t/e/s/t/");
    CHECK_NORMALIZE("uri:/t/../t/e/../e/s/../s/t/../t/", "uri:/t/e/s/t/");
    CHECK_NORMALIZE("uri:/t/e/../../t/e/s/t/../../s/t/", "uri:/t/e/s/t/");
    CHECK_NORMALIZE("uri:/t/./e/./s/./t/./.././.././.././../.", "uri:/");
    CHECK_NORMALIZE("uri:t/./e/./s/./t/./.././.././.././../.", "uri:/");
    CHECK_NORMALIZE("uri:/test../path././again../..", "uri:/test../path./");
    CHECK_NORMALIZE("uri:/test/..", "uri:/");
    CHECK_NORMALIZE("uri:test/..", "uri:/");
    CHECK_NORMALIZE(
        "EiD://%7dUSER%63%7b@HoST.%63.%7b:55/Test/%63/Path/%7b/../.?C%63%7d#C%63%7b",
        "eid://%7DUSERc%7B@host.c.%7B:55/Test/c/Path/?Cc%7D#Cc%7B");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(QueryValue) {
    URI uri;
    DO(uri.assign("uri://authority/path?aaa=bbb;bbb=ccc"));
    std::string xxx = uri.query_value("aaa");
    CHECK_EQUALSTR(uri.query_value("aaa"), "bbb");
    CHECK_EQUALSTR(uri.query_value("bbb"), "ccc");
    CHECK_EQUALSTR(uri.query_value("ccc"), "");
    CHECK_EQUALSTR(uri.query_value("xxx"), "");

    DO(uri.assign("uri://authority/path#aaa=bbb;bbb=ccc"));
    CHECK_EQUALSTR(uri.query_value("aaa"), "");
    CHECK_EQUALSTR(uri.query_value("bbb"), "");
    CHECK_EQUALSTR(uri.query_value("ccc"), "");
    CHECK_EQUALSTR(uri.query_value("xxx"), "");
    
    DO(uri.assign("uri://authority/path"));
    CHECK_EQUALSTR(uri.query_value("aaa"), "");

    DO(uri.assign("uri://authority/path?aaa"));
    CHECK_EQUALSTR(uri.query_value("aaa"), "");

    DO(uri.assign("uri://authority/path?=aaa;"));
    CHECK_EQUALSTR(uri.query_value("aaa"), "");
    
    // check if first = is in the fragment

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Serialize) {
    ScratchBuffer<u_char*, 256> buf;
    {
        URI uri("scheme:ssp");
        URI uri2;
        MarshalCopy::copy(&buf, &uri, &uri2);
        CHECK_COMPONENTS(uri2, uri.uri());
    }

    {
        URI uri("scheme://user@host:99/path?q=a#fragment");
        URI uri2;
        MarshalCopy::copy(&buf, &uri, &uri2);
        CHECK_COMPONENTS(uri2, uri.uri());
    }

    {
        URI uri("bogus");
        URI uri2;
        MarshalCopy::copy(&buf, &uri, &uri2);
        CHECK_COMPONENTS(uri2, uri.uri());
    }

    {
        URI uri;
        URI uri2;
        MarshalCopy::copy(&buf, &uri, &uri2);
        CHECK_COMPONENTS(uri2, uri.uri());
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Subsume) {
    CHECK(URI("scheme:ssp").subsume(URI("scheme:ssp")));
    CHECK(! URI("scheme:ssp2").subsume(URI("scheme:ssp")));
    CHECK(! URI("scheme:ssp").subsume(URI("scheme:ssp2")));

    CHECK(URI("scheme://host/path").subsume(URI("scheme://host/")));
    CHECK(URI("scheme://host/path").subsume(URI("scheme://host")));

    CHECK(URI("scheme://host").subsume(URI("scheme://host")));
    CHECK(URI("scheme://host/").subsume(URI("scheme://host")));
    CHECK(! URI("scheme://host").subsume(URI("scheme://host/")));

    CHECK(URI("scheme://host/a/b").subsume(URI("scheme://host")));
    CHECK(URI("scheme://host/a/b").subsume(URI("scheme://host/")));
    CHECK(URI("scheme://host/a/b").subsume(URI("scheme://host/a")));
    CHECK(URI("scheme://host/a/b").subsume(URI("scheme://host/a/")));
    CHECK(URI("scheme://host/a/b").subsume(URI("scheme://host/a/b")));

    CHECK(URI("scheme://host/?query").subsume(URI("scheme://host")));
    CHECK(URI("scheme://host/#frag").subsume(URI("scheme://host")));

    CHECK(! URI("scheme://host/?query").subsume(URI("scheme://host/p")));
    CHECK(! URI("scheme://host/#frag").subsume(URI("scheme://host/p")));
    
    CHECK(URI("scheme://host/path?query").subsume(URI("scheme://host/path")));
    CHECK(URI("scheme://host/path#frag").subsume(URI("scheme://host/path")));

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(URITester) {
    ADD_TEST(BasicParse);
    ADD_TEST(Accessors);
    ADD_TEST(SchemeValidate);
    ADD_TEST(PathValidate);
    ADD_TEST(QueryValidate);
    ADD_TEST(FragmentValidate);
    ADD_TEST(AuthorityValidate);
    ADD_TEST(IPLiteralValidate);
    ADD_TEST(Normalize);
    ADD_TEST(QueryValue);
    ADD_TEST(Serialize);
    ADD_TEST(Subsume);
} 

DECLARE_TEST_FILE(URITester, "uri test");
