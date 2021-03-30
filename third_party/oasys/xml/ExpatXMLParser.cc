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

#ifdef LIBEXPAT_ENABLED

#include "ExpatXMLParser.h"
#include "XMLDocument.h"
#include "XMLObject.h"

namespace oasys {

//----------------------------------------------------------------------
ExpatXMLParser::ExpatXMLParser(const char* logpath)
    : Logger("ExpatXMLParser", logpath)
{
}
    
//----------------------------------------------------------------------
ExpatXMLParser::~ExpatXMLParser()
{
}

//----------------------------------------------------------------------
bool
ExpatXMLParser::parse(XMLDocument* doc, const std::string& data)
{
    XML_Parser p = XML_ParserCreate(NULL);

    // set up the expat handlers
    XML_SetUserData(p, this);
    XML_SetElementHandler(p, start_element, end_element);
    XML_SetCharacterDataHandler(p, character_data);

    // cache the document and null out the object
    doc_ = doc;
    cur_ = NULL;

    if (XML_Parse(p, data.c_str(), data.length(), true) != XML_STATUS_OK) {
        log_err("parse error at line %u:\n%s",
                static_cast<u_int32_t>(XML_GetCurrentLineNumber(p)),
                XML_ErrorString(XML_GetErrorCode(p)));
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
void XMLCALL
ExpatXMLParser::start_element(void* data,
                              const char* element,
                              const char** attr)
{
    ExpatXMLParser* this2 = (ExpatXMLParser*)data;

    XMLObject* new_object = new XMLObject(element);
    if (this2->cur_ == NULL) {
        this2->doc_->set_root(new_object);
    } else {
        this2->cur_->add_element(new_object);
    }

    this2->cur_ = new_object;
    while (attr[0] != NULL) {
        ASSERT(attr[1] != NULL);
        this2->cur_->add_attr(attr[0], attr[1]);
        attr += 2;
    }
}

//----------------------------------------------------------------------
void XMLCALL
ExpatXMLParser::end_element(void* data,
                            const char* element)
{
    ExpatXMLParser* this2 = (ExpatXMLParser*)data;
    ASSERT(this2->cur_->tag() == element);
    this2->cur_ = this2->cur_->parent();
}

//----------------------------------------------------------------------
void XMLCALL
ExpatXMLParser::character_data(void* data,
                               const XML_Char* s,
                               int len)
{
    ExpatXMLParser* this2 = (ExpatXMLParser*)data;
    ASSERT(this2->cur_ != NULL);
    this2->cur_->add_text(s, len);
}

} // namespace oasys

#endif /* LIBEXPAT_ENABLED */
