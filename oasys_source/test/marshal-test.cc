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
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
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
#  include <oasys-config.h>
#endif

#include <limits.h>
#include <iostream>
#include <debug/DebugUtils.h>
#include <serialize/MarshalSerialize.h>
#include <util/UnitTest.h>

using namespace std;
using namespace oasys;

class OneOfEach : public SerializableObject {
public:
    OneOfEach() :
        a(200),
        b(-100),
        c(0x77),
        d(0xbaddf00d),
        a2(200),
        b2(-100),
        c2(0x77),
        d2(0xbaddf00d),
        e(56789),
        u(INT_MAX),
        u64(18446744073709551615ULL), // copy of ULLONG_MAX since it's non-standard
        u64alt(123456789012345ULL),        // test for unexpected sign extension
        s1("hello")
    {
        memset(s2, 0, sizeof(s2));
        strcpy(s2, "Zanzibar");

        const_len = 10;
        const_buf = (u_char*)malloc(10);
        for (int i = 0; i < 10; ++i) {
            const_buf[i] = 'a' + i;
        }

        nullterm_buf = (u_char*)strdup("Testing one two");
        nullterm_len = strlen((char*)nullterm_buf) + 1;

        null_buf = 0;
        null_len  = 0;
    }
    
    OneOfEach(const Builder&) :
        a(0),
        b(0),
        c(0),
        d(0),
        a2(0),
        b2(0),
        c2(0),
        d2(0),
        e(0),
        u(0),
        u64(0),
        u64alt(0),
        s1(""),
        const_len(0x99),
        nullterm_len(0x99),
        null_len(0x99),
        const_buf((u_char*)0xbaddf00d),
        nullterm_buf((u_char*)0xbaddf00d),
        null_buf((u_char*)0xbaddf00d)
    {
        memset(s2, 0, sizeof(s2));
    }
    
    virtual ~OneOfEach() {}
    
    void serialize(SerializeAction* action) {
        action->process("a", &a);
        action->process("b", &b);
        action->process("c", &c);
        action->process("d", &d);
        action->process("e", &e);
        action->process("u", &u);
        action->process("u64", &u64);
        action->process("u64alt", &u64alt);
        action->process("s1", &s1);
        action->process("s2", s2, sizeof(s2));

        if (action->action_code() == Serialize::UNMARSHAL)
        {
            BufferCarrier<u_char> bc;

            size_t size;
            action->process("const_buf", &bc);
            const_buf = bc.take_buf(&size);
            const_len = size;

            action->process("nullterm_buf", &bc, 0);
            nullterm_buf = bc.take_buf(&size);
            nullterm_len = size;
            --nullterm_len;

            action->process("null_buf", &bc);
            null_buf = bc.take_buf(&size);
            null_len = size;
        }
        else
        {
            BufferCarrier<u_char> bc(const_buf, const_len, false);
            action->process("const_buf", &bc);
            
            bc.set_buf(nullterm_buf, nullterm_len, false);
            action->process("nullterm_buf", &bc, 0);

            bc.set_buf(null_buf, null_len, false);
            action->process("null_buf", &bc);
        }
    }

    int32_t   a, b, c, d;
    int64_t   a2, b2, c2, d2;
    short     e;
    u_int32_t u;
    u_int64_t u64;
    u_int64_t u64alt;
    string    s1;
    char      s2[32];

    u_int32_t    const_len, nullterm_len, null_len;
    u_char    *const_buf, *nullterm_buf, *null_buf;
};

Builder b;
OneOfEach o1;
OneOfEach o2(b);

#define LEN 512
u_char buf[LEN];

DECLARE_TEST(Marshal) {
    memset(buf, 0, sizeof(char) * LEN);
    Marshal v(Serialize::CONTEXT_LOCAL, buf, LEN);
    v.set_logpath("/marshal-test");
    CHECK(v.action(&o1) == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Unmarshal) {
    Unmarshal uv(Serialize::CONTEXT_LOCAL, buf, LEN);
    uv.set_logpath("/marshal-test");
    CHECK(uv.action(&o2) == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MarshalSize) {

    MarshalSize sz1(Serialize::CONTEXT_LOCAL);
    sz1.action(&o1);

    MarshalSize sz2(Serialize::CONTEXT_LOCAL);
    sz2.action(&o2);

    CHECK_EQUAL(sz1.size(), sz2.size());
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Compare) {
    CHECK_EQUAL(o1.a, o2.a);
    CHECK_EQUAL(o1.b, o2.b);
    CHECK_EQUAL(o1.c, o2.c);
    CHECK_EQUAL(o1.d, o2.d);
    CHECK_EQUAL(o1.e, o2.e);
    CHECK_EQUAL(o1.u, o2.u);
    CHECK_EQUAL_U64(o1.u64, o2.u64);
    CHECK_EQUAL_U64(o1.u64alt, o2.u64alt);
    CHECK_EQUALSTR(o1.s1.c_str(), o2.s1.c_str());
    CHECK_EQUALSTRN(o1.s2, o2.s2, 32);
    
    CHECK_EQUAL(o1.const_len, o2.const_len);
    CHECK_EQUALSTRN(o1.const_buf, o2.const_buf, o1.const_len);

    CHECK_EQUAL(o1.nullterm_len, o2.nullterm_len);
    CHECK_EQUALSTRN(o1.nullterm_buf, o2.nullterm_buf, o1.nullterm_len);

    CHECK_EQUAL(o1.null_len, 0);
    CHECK(o1.null_buf == 0);
    CHECK_EQUAL(o1.null_len, 0);
    CHECK(o2.null_buf == 0);

    return UNIT_TEST_PASSED;
}
    
DECLARE_TESTER(MarshalTester) {
    ADD_TEST(Marshal);
    ADD_TEST(Unmarshal);
    ADD_TEST(MarshalSize);
    ADD_TEST(Compare);
}

DECLARE_TEST_FILE(MarshalTester, "marshal unit test");
