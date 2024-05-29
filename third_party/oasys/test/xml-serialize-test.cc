/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "util/UnitTest.h"
#include "util/StringBuffer.h"
#include "serialize/XMLSerialize.h"

using namespace oasys;
using namespace std;

class EmbeddedObject : public SerializableObject {
public:
    EmbeddedObject() {
        var1 = 300;
    }
    
    EmbeddedObject(int zero) {
        (void)zero;
        var1 = 0;
    }
    
    void serialize(SerializeAction* a) {
        a->process("var1", &var1);
    }
    
    string serialize_name() const { return string("EmbeddedObject"); }
    
    bool equals(const EmbeddedObject& other) const {
        return var1 == other.var1;
    }
    
    int var1;
};

class OtherEmbeddedObject : public SerializableObject {
public:
    OtherEmbeddedObject() {
        var1 = 400;
    }
    
    OtherEmbeddedObject(int zero) {
        (void)zero;
        var1 = 0;
    }
    
    void serialize(SerializeAction* a) {
        a->process("var1", &var1);
    }
    
    string serialize_name() const { return string("OtherEmbeddedObject"); }
    
    bool equals(const OtherEmbeddedObject& other) const {
        return var1 == other.var1;
    }
    
    int var1;
};


class TestObject : public SerializableObject {
public:
    TestObject() {
        var1 = 70000;
        var2 = -80000;
        var3 = 40000;
        var4 = -20000;
        var5 = 200;
        var6 = -100;
        var7 = true;
        var8 = std::string("&test<> s'tr\"ing");
    }
    
    TestObject(int zero) : embedded(zero) {
        (void)zero;
        var1 = 0;
        var2 = 0;
        var3 = 0;
        var4 = 0;
        var5 = 0;
        var6 = 0;
        var7 = false;
        var8 = std::string();
    }
    
    void serialize(SerializeAction* a) {
        a->process("var1", &var1);
        a->process("var2", &var2);
        a->process("var3", &var3);
        a->process("var4", &var4);
        a->process(NULL, &embedded);
        a->process("var5", &var5);
        a->process("var6", &var6);
        a->process("var7", &var7);
        a->process(NULL, &other_embedded);
        a->process("var8", &var8);        
    }
    
    string serialize_name() const { return string("TestObject"); }
    
    bool equals(const TestObject& other) const {
        return (var1 == other.var1 &&
                var2 == other.var2 &&
                var3 == other.var3 &&
                var4 == other.var4 &&
                var5 == other.var5 &&
                var6 == other.var6 &&
                var7 == other.var7 &&
                var8 == other.var8 &&
                embedded.equals(other.embedded) &&
                other_embedded.equals(other.other_embedded));
    }
                
    u_int32_t var1;
    int32_t var2;
    u_int16_t var3;
    int16_t var4;
    u_int8_t var5;
    int8_t var6;
    bool var7;
    string var8;
    EmbeddedObject embedded;
    OtherEmbeddedObject other_embedded;
};

static const char* doc_text =
        "<TestObject var1=\"70000\" var2=\"-80000\" var3=\"40000\" "
        "var4=\"-20000\" var5=\"200\" var6=\"-100\" var7=\"true\" "
        "var8=\"&amp;test&lt;&gt; s&apos;tr&quot;ing\"><EmbeddedObject "
        "var1=\"300\"/><OtherEmbeddedObject var1=\"400\"/></TestObject>";
static StringBuffer buffer;


// Test each of the process() functions in XMLMarshal.
DECLARE_TEST(MarshalTest) {
    XMLMarshal marshal(&buffer);
    TestObject obj;
    marshal.action(&obj);
            
    CHECK_EQUALSTR(buffer.c_str(), doc_text);
    return UNIT_TEST_PASSED;
}

// Test each of the process() functions in XMLUnmarshal.
DECLARE_TEST(UnmarshalProcessTest) {
    XMLUnmarshal unmarshal( buffer.data(), buffer.length() );
    TestObject obj(0);
    TestObject other;
    unmarshal.action(&obj);
    
    CHECK( !unmarshal.error() );        
    CHECK( obj.equals(other) );

    return UNIT_TEST_PASSED;
}

// Test the tree-traversal functions.
DECLARE_TEST(UnmarshalElementsTest) {
    std::string str;
    XMLUnmarshal unmarshal( buffer.data(), buffer.length() );
    
    CHECK( !unmarshal.error() );
    str = unmarshal.current_element();
    CHECK_EQUALSTR(str.c_str(), "TestObject");
    
    str = unmarshal.current_child();
    CHECK_EQUALSTR(str.c_str(), "EmbeddedObject");
    CHECK( unmarshal.in_one_level() );
    
    str = unmarshal.next_child();
    CHECK( str == std::string() );
    
    CHECK( unmarshal.out_one_level() );
    str = unmarshal.next_child();
    CHECK_EQUALSTR(str.c_str(), "OtherEmbeddedObject");
    
    str = unmarshal.next_child();
    CHECK( str == std::string() );
    
    str = unmarshal.next_child();
    CHECK( str == std::string() );
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {    
    ADD_TEST(MarshalTest);
    ADD_TEST(UnmarshalProcessTest);
    ADD_TEST(UnmarshalElementsTest);
}

DECLARE_TEST_FILE(Test, "xml serialize test");













