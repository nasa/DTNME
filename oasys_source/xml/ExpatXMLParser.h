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

#ifndef _OASYS_EXPAT_XML_PARSER_H_
#define _OASYS_EXPAT_XML_PARSER_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef LIBEXPAT_ENABLED

#include <expat.h>
#include "XMLParser.h"
#include "debug/Logger.h"

// for compatibility with old versions of expat
#ifndef XMLCALL
#define XMLCALL
#endif

namespace oasys {

class XMLDocument;
class XMLObject;

class ExpatXMLParser : public XMLParser, public Logger {
public:
    /// Constructor
    ExpatXMLParser(const char* logpath);
    
    /// Destructor
    virtual ~ExpatXMLParser();

    /// Virtual from XMLParser
    bool parse(XMLDocument* doc, const std::string& data);

private:
    /// @{ Expat callbacks
    static void XMLCALL start_element(void* data,
                                      const char* element,
                                      const char** attr);
    
    static void XMLCALL end_element(void* data,
                                    const char* element);
    
    static void XMLCALL character_data(void* data,
                                       const XML_Char* s,
                                       int len);
    /// @}

    XMLDocument* doc_;	///< The XMLDocument being worked on
    XMLObject*   cur_;	///< The current XMLObject
};

} // namespace oasys

#endif /* LIBEXPAT_ENABLED */
#endif /* _OASYS_EXPAT_XML_PARSER_H_ */
