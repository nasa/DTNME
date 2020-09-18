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

#include "util/UnitTest.h"
#include "util/StringBuffer.h"
#include "xml/XMLDocument.h"
#include "xml/XMLObject.h"
#include "xml/ExpatXMLParser.h"
#include "xml/XMLParser.h"

using namespace oasys;
using namespace std;

std::string test_to_string(const XMLObject* obj, int indent = -1)
{
    StringBuffer buf;
    obj->to_string(&buf, indent);
    return std::string(buf.c_str(), buf.length());
}

DECLARE_TEST(ToStringTest) {
    {
        XMLObject obj("test");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test/>");
    }

    {
        XMLObject obj("test");
        obj.add_attr("a1", "v1");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test a1=\"v1\"/>");
    }
    
    {
        XMLObject obj("test");
        obj.add_attr("a1", "v1");
        obj.add_attr("a2", "v2");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test a1=\"v1\" a2=\"v2\"/>");
    }
    
    {
        XMLObject obj("test");
        obj.add_element(new XMLObject("tag"));
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test><tag/></test>");
    }

    {
        XMLObject obj("test");
        obj.add_element(new XMLObject("tag"));
        obj.add_element(new XMLObject("tag2"));
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test><tag/><tag2/></test>");
    }
    
    {
        XMLObject obj("test");
        obj.add_text("Some text here");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test>Some text here</test>");
    }

    {
        XMLObject obj("test");
        obj.add_proc_inst("xyz", "abc=\"foobar\"");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test><?xyz abc=\"foobar\"?></test>");
    }

    {
        XMLObject obj("test");
        obj.add_attr("a1", "v1");
        obj.add_element(new XMLObject("tag"));
        obj.add_text("Some text here");
        CHECK_EQUALSTR(test_to_string(&obj).c_str(),
                       "<test a1=\"v1\"><tag/>Some text here</test>");
    }
    
    return UNIT_TEST_PASSED;
}

bool test_parse(XMLParser* p, const std::string data)
{
    XMLDocument doc;
    if (! p->parse(&doc, data)) {
        return false;
    }

    int errno_; const char* strerror_;
        
    StringBuffer buf;
    doc.to_string(&buf, -1);
    CHECK_EQUALSTR(data.c_str(), buf.c_str());
    
    return UNIT_TEST_PASSED;
}

#if LIBEXPAT_ENABLED
DECLARE_TEST(ExpatParseTest) {
    ExpatXMLParser p("/test/expat");

    CHECK(test_parse(&p, "<test/>") == UNIT_TEST_PASSED);
    CHECK(test_parse(&p, "<test a=\"b\"/>") == UNIT_TEST_PASSED);
    CHECK(test_parse(&p, "<test a=\"b\" c=\"d\"/>") == UNIT_TEST_PASSED);
    CHECK(test_parse(&p, "<test><test2/></test>") == UNIT_TEST_PASSED);
    CHECK(test_parse(&p, "<test>Some text</test>") == UNIT_TEST_PASSED);
    
    return UNIT_TEST_PASSED;
}
#endif

DECLARE_TESTER(Test) {    
    ADD_TEST(ToStringTest);
#if LIBEXPAT_ENABLED
    ADD_TEST(ExpatParseTest);
#endif
}

DECLARE_TEST_FILE(Test, "xml test");
