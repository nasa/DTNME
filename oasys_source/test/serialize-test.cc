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

#include "util/UnitTest.h"
#include "serialize/MarshalSerialize.h"
#include "serialize/TextSerialize.h"
#include "serialize/KeySerialize.h"
#include "serialize/StringSerialize.h"
#include "serialize/TypeShims.h"
#include "util/ScratchBuffer.h"


using namespace std;
using namespace oasys;

#define CRC Serialize::USE_CRC

class OneOfEach : public oasys::SerializableObject {
public:
    OneOfEach() : 
        a(200), 
        b(-100), 
        c(0x77), 
        d(0xbaddf00d), 
        e(56789), 
        u(INT_MAX), 
        s1("hello") 
    {
        memset(s2, 0, sizeof(s2));
        strcpy(s2, "Zanzibar");
    }
    OneOfEach(bool) : a(0), b(0), c(0), d(0), e(0), u(0), s1("") {
        memset(s2, 0, sizeof(s2));
    }
    
    virtual ~OneOfEach() {}
    
    void serialize(oasys::SerializeAction* act) {
        act->process("a", &a);
        act->process("b", &b);
        act->process("c", &c);
        act->process("d", &d);
        act->process("e", &e);
        act->process("u", &u);
        act->process("s1", &s1);
        act->process("s2", s2, sizeof(s2));
    }

    bool equals(const OneOfEach& other) {
        return (a == other.a &&
                b == other.b &&
                c == other.c &&
                d == other.d &&
                e == other.e &&
                u == other.u &&
                s1.compare(other.s1) == 0 &&
                !memcmp(s2, other.s2, sizeof(s2)));
    }

private:
    int32_t    a, b, c, d;
    short  e;
    u_int32_t u;
    string s1;
    char   s2[32];
};

int CompareTest(bool crc) {
    OneOfEach o1, o2(false);

#define LEN 256
    u_char buf[LEN];
    memset(buf, 0, sizeof(char) * LEN);

    oasys::MarshalSize sizer(Serialize::CONTEXT_NETWORK, crc);
    sizer.logpath("/marshal-test");
    sizer.action(&o1);

    oasys::Marshal v(Serialize::CONTEXT_NETWORK, buf, LEN, crc);
    v.set_logpath("/marshal-test");
    v.action(&o1);
    
    oasys::Unmarshal uv(Serialize::CONTEXT_NETWORK, buf, sizer.size(), crc);
    uv.set_logpath("/marshal-test");
    uv.action(&o2);

    ASSERT(o1.equals(o2));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(CompareTest_NOCRC) {
    return CompareTest(false);
}

DECLARE_TEST(CompareTest_CRC) {
    return CompareTest(true);
}

DECLARE_TEST(SizeTest) {
    OneOfEach o;
    size_t sz = 4 + 4 + 4 + 4 +
                2 + 
                4 +
                4 + 5 +
                32;

    oasys::MarshalSize sizer1(Serialize::CONTEXT_NETWORK, 0);
    sizer1.action(&o);
    CHECK_EQUAL(sizer1.size(), sz);
    
    oasys::MarshalSize sizer2(Serialize::CONTEXT_NETWORK, Serialize::USE_CRC);
    sizer2.action(&o);
    CHECK_EQUAL(sizer2.size(), sz + 4);

    return UNIT_TEST_PASSED;
}

/*
DECLARE_TEST(NullStringTest1) {
    char* test = "test string END";

    NullStringShim id(0);
    oasys::Unmarshal uv(Serialize::CONTEXT_LOCAL, 
                        reinterpret_cast<u_char*>(test), 
                        strlen(test) + 1, 0);
    uv.action(&id);
    CHECK(strcmp(test, id.value()) == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(NullStringTest2) {
    char* test = "test string END";

    // XXX/demmer why doesn't the temporary work??
    Builder b;
    NullStringShim id(b);
    oasys::Unmarshal uv(Serialize::CONTEXT_LOCAL, 
                        reinterpret_cast<u_char*>(test), 
                        strlen(test) + 1, 0);
    uv.action(&id);
    CHECK(strcmp(test, id.value()) == 0);

    return UNIT_TEST_PASSED;
}
*/

/*
DECLARE_TEST(TextSerializeTest1) {
    OneOfEach one;
    ScratchBuffer<u_char*> buf;
    
    TextMarshal marshal(Serialize::CONTEXT_LOCAL, &buf);
    marshal.action(&one);

    log_info_p("%s", (const char*)buf.buf());
        
    return UNIT_TEST_PASSED;
}

struct TestObject : oasys::SerializableObject {
    TestObject() : next_exists_(false), next_(0) {}

    void serialize(oasys::SerializeAction* action) {
        action->process("int32", &int32_);
        action->process("int16", &int16_);
        action->process("int8",  &int8_);
        action->process("str",   &str_);
        
        action->process("next_exists", &next_exists_);
        if (next_exists_) {
            if (action->action_code() == oasys::Serialize::UNMARSHAL) {
                next_ = new TestObject;
            }
            action->process("next", next_);
        }
    }

    u_int32_t int32_;
    u_int32_t int16_;
    u_int32_t int8_;

    std::string str_;

    bool      next_exists_;
    TestObject* next_;
};

DECLARE_TEST(TextSerializeTest2) {
    ScratchBuffer<u_char*> buf;
    TestObject obj1;
    
    obj1.int32_ = 1111;
    obj1.int16_ = 123;
    obj1.int8_  = 5;
    obj1.str_   = "this is a string of great ambition";

    TextMarshal marshal(Serialize::CONTEXT_LOCAL, &buf);
    marshal.action(&obj1);

    char m_txt[] = "# -- text marshal start --\n"
                   "int32: 1111\n"
                   "int16: 123\n"
                   "int8: 5\n"
                   "str: TextCode\n"
                   "\tthis is a string of great ambition\n"
                   "\t\n"
                   "next_exists: false\n";
    CHECK(memcmp(buf.buf(), m_txt, strlen(m_txt)) == 0);
    // log_info("/test", "%s", (const char*)buf.buf());

    const char* txt = "# -- text marshal start --\n"
                      "# comment\n"
                      "# another comment\n"
                      "int32: 1111\n"
                      "\t\tint16: 123\n"
                      "int8: 5\n"
                      "str: TextCode\n"
                      "this is a string of great \n"
                      "ambi\n"
                      "tion\n"
                      "\n"
                      "next_exists: true\n"
                      "next: SerializableObject\n"
                      "int32: 2222\n"
                      "int16: 1234\n"
                      "int8: 24\n"
                      "str: TextCode\n"
                      "hello, world\n"
                      "\n"
                      "next_exists: false\n";

    TestObject obj2;
    TextUnmarshal unmarshal(Serialize::CONTEXT_LOCAL, (u_char*)txt, strlen(txt));

    CHECK(unmarshal.action(&obj2) == 0);
               
    CHECK(obj2.int32_ == 1111);
    CHECK(obj2.int16_ == 123);
    CHECK(obj2.int8_  == 5);
    CHECK(obj2.str_   == "this is a string of great ambition");
    CHECK(obj2.next_exists_  == true);

    CHECK(obj2.next_->int32_ == 2222);
    CHECK(obj2.next_->int16_ == 1234);
    CHECK(obj2.next_->int8_  == 24);
    CHECK(obj2.next_->str_   == "hello, world");    
    CHECK(obj2.next_->next_exists_ == false);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TextSerializeTest3) {

    return UNIT_TEST_PASSED;
}
*/

//
// Key serialization tests
//
struct KeyObj_1 : public SerializableObject {
    int         number_;
    short       short_;
    bool        bool_;
    std::string str_;
    char*       c_str_;

    KeyObj_1(bool init) {
        if (init) {
            number_ = 0xcafe;
            short_  = 0xf00d;
            bool_   = true;
            str_    = "this is a string end";
            c_str_  = static_cast<char*>(malloc(20));
            strcpy(c_str_, "12345test");
        } else {
            number_ = 0;
            short_  = 0;
            bool_   = false;
            str_    = "";
            c_str_  = 0;
        }
    }

    KeyObj_1(const KeyObj_1& other)
        : SerializableObject(other)
    {
        number_ = other.number_;
        short_  = other.short_;
        bool_   = other.bool_;
        str_    = other.str_;
        c_str_  = strdup(other.c_str_);
    }

    virtual ~KeyObj_1() {
        if (c_str_) {
            free(c_str_);
            c_str_ = 0;
        }
    }

    void serialize(SerializeAction* action) {
        action->process("number", &number_);
        action->process("str",    &str_);        
        action->process("short",  &short_);
        action->process("bool",   &bool_);

        if (action->action_code() == Serialize::UNMARSHAL)
        {
            BufferCarrier<char> bc;
            action->process("c_str", &bc, 0);
            size_t len;
            c_str_ = bc.take_buf(&len);
        }
        else
        {
            BufferCarrier<char> bc(c_str_, 0, false);
            action->process("c_str", &bc, 0);
        }

    }
};

class LimitObj : public SerializableObject {
public:
    u_int       uzero_;
    u_int       szero_;
    int         one_;
    int         negone_;
    u_int       umax_;
    int         smax_;

    LimitObj(bool init) {
        if (init) {
            uzero_  = 0;
            szero_  = 0;
            one_    = 1;
            negone_ = -1;
            umax_   = 0xffffffff;
            smax_   = INT_MAX;
        } else {
            uzero_  = 0;
            szero_  = 0;
            one_    = 0;
            negone_ = 0;
            umax_   = 0;
            smax_   = 0;
        }
    }

    void serialize(SerializeAction* a) {
        a->process("uzero",  &uzero_);
        a->process("szero",  &szero_);
        a->process("one",    &one_);
        a->process("negone", &negone_);
        a->process("umax",   &umax_);
        a->process("smax",   &smax_);
    }
};

DECLARE_TEST(KeySerializeTest_1) {    
    ExpandableBuffer buf;
    KeyObj_1 obj1(true);
    
    buf.reserve(256);
    memset(buf.at(0), 0, 20);

    KeyMarshal ks(&buf);
    int err = ks.action(&obj1);

    CHECK(err == 0);
    CHECK_EQUALSTR(buf.at(0), 
                   "0000cafe00000014this is a string end"
                   "f00d10000000912345test");
    
    buf.clear();
    KeyMarshal ks2(&buf, "--");
    err = ks2.action(&obj1);

    CHECK(err == 0);
    CHECK_EQUALSTR(buf.at(0), 
                   "0000cafe--00000014this is a string end--"
                   "f00d--1--0000000912345test--");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(KeySerializeTest_2) {
    const char* str = "0000cafe00000014this is a string end"
                      "f00d10000000912345test";
    KeyUnmarshal ku(str, strlen(str));
    KeyObj_1 obj(false);
    
    int err = ku.action(&obj);
    
    CHECK(obj.number_ == 0xcafe);
    CHECK(obj.short_ == (short)0xf00d);
    CHECK(obj.bool_);
    CHECK_EQUALSTR(obj.str_.c_str(), "this is a string end");
    CHECK_EQUALSTR(obj.c_str_, "12345test");
    CHECK(err == 0);    

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(KeySerializeTest_3) {    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(StringSerializeTest) {
    KeyObj_1 key(true);
    StringSerialize s(Serialize::CONTEXT_LOCAL, 0);
    s.action(&key);
    CHECK_EQUALSTR(s.buf().c_str(),
                   "51966 this is a string end 61453 true 12345test");

    LimitObj obj(true);
    StringSerialize s2(Serialize::CONTEXT_LOCAL, StringSerialize::INCLUDE_NAME);
    s2.action(&obj);
    CHECK_EQUALSTR(s2.buf().c_str(),
                   "uzero 0 szero 0 one 1 "
                   "negone 4294967295 umax 4294967295 smax 2147483647")
                   
    return UNIT_TEST_PASSED;
}

std::string
key_maker(const SerializableObject& key, int opts = 0)
{
    StringSerialize s(Serialize::CONTEXT_LOCAL, opts);
    s.action(&key);

    return s.buf().c_str();
}
    
DECLARE_TEST(PrefixAdapterTest) {
    KeyObj_1 key(true);
    int prefix = 1000;
    
    CHECK_EQUALSTR(key_maker(prefix_adapter(prefix, key)).c_str(),
                   "1000 51966 this is a string end 61453 true 12345test");
    
    prefix = 2001;
    LimitObj obj(true);
    CHECK_EQUALSTR(key_maker(prefix_adapter(prefix, obj), 
                             StringSerialize::INCLUDE_NAME).c_str(),
                   "prefix 2001 uzero 0 szero 0 one 1 "
                   "negone 4294967295 umax 4294967295 smax 2147483647")    
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(MarshalTester) {
    ADD_TEST(SizeTest);
    ADD_TEST(CompareTest_NOCRC);
    ADD_TEST(CompareTest_CRC);

//    ADD_TEST(NullStringTest1);
//    ADD_TEST(NullStringTest2);

    // Text Serialize
    /*
    ADD_TEST(TextSerializeTest1);
    ADD_TEST(TextSerializeTest2);
    ADD_TEST(TextSerializeTest3);
    */

    // Key serialization tests
    ADD_TEST(KeySerializeTest_1);
    ADD_TEST(KeySerializeTest_2);
    ADD_TEST(KeySerializeTest_3);
    ADD_TEST(StringSerializeTest);
    ADD_TEST(PrefixAdapterTest);
};

DECLARE_TEST_FILE(MarshalTester, "marshalling test");
