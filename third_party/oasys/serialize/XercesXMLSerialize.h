/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifndef _OASYS_XERCES_XML_SERIALIZE_H_
#define _OASYS_XERCES_XML_SERIALIZE_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef XERCES_C_ENABLED

#include "XMLSerialize.h"
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/framework/XMLGrammarPool.hpp>

namespace oasys {

class Mutex;

/**
 * <p>XercesXMLUnmarshal implements the SerializeAction and
 * XMLUnmarshal interface using the Xerces C++ Parser.
 * The class is designed to work in conjunction with
 * oasys serializtion where XML element names map to DTN
 * classes and attributes map to class data members.</p>
 * <p><code>parse</code> takes an xml document string, optionally
 * performs validation, builds an internal DOM tree, and
 * returns the first element tag found.  The calling code
 * instantiates a class based upon the tag name and calls
 * action() to unserialize the object.  Calling <code>parse</code>
 * again returns the next element tag found and the process is
 * repeated.</p>
 */
class XercesXMLUnmarshal : public XMLUnmarshal,
                           public Logger {
public:
    XercesXMLUnmarshal(bool validation, const char *schema=0);
    virtual ~XercesXMLUnmarshal();

    const xercesc::DOMDocument *doc(const char *xml_doc = 0);

    /**
     * Parse, optionally validate, and build a DOM tree from
     * the provided xml document.  Return the first element tag.
     * @param xml_doc source XML document
     * @return the first top-level element tag or 0 on failure
     */
    virtual const char *parse(const char *xml_doc = 0);

    // Virtual functions inherited from SerializeAction
    virtual void process(const char *name, SerializableObject* object);
    virtual void process(const char *name, u_int64_t *i);
    virtual void process(const char *name, u_int32_t *i);
    virtual void process(const char *name, u_int16_t *i);
    virtual void process(const char *name, u_int8_t *i);
    virtual void process(const char *name, bool *b);
    virtual void process(const char *name, 
                         u_char *bp,
                         u_int32_t len);


    virtual void process(const char*            name, 
                         BufferCarrier<u_char>* carrier);
    
    virtual void process(const char*            name,
                         BufferCarrier<u_char>* carrier,
                         u_char                 terminator);

    virtual void process(const char *name, std::string *s);

protected:
    /**
     * Used for iterating over DOM tree elements
     * @return char string of next top-level element in the DOM tree
     */
    virtual const char *next_elem();

    char *root_tag_str;
    xercesc::XMLGrammarPool *pool_;
    xercesc::DOMImplementation *impl_;
    xercesc::DOMBuilder *parser_;
    xercesc::DOMDocument *doc_;
    xercesc::DOMElement *root_elem_;
    xercesc::DOMTreeWalker *walker_;

    // avoid race conditions involving multiple simultaneous
    // constructions of XercesXMLUnmarshal
    static oasys::Mutex* lock_;
};

/**
 * ValidationError is a helper class to catch DOM parser errors.
 */
class ValidationError : public xercesc::DOMErrorHandler {
public:
    ValidationError()
        : set_(false), severity_(-1), message_(0)
    {
    }

    virtual ~ValidationError()
    {
        delete [] message_;
    }

    /**
     * Error handler registered with Xerces
     */
    virtual bool handleError(const xercesc::DOMError& domError);

    bool is_set()
    {
        return set_;
    }

    short get_severity()
    {
        return severity_;
    }

    const char * message()
    {
        return message_;
    }

private:
    bool set_;
    short severity_;
    char *message_;
};

} // namespace oasys

#endif // XERCES_C_ENABLED
#endif //_OASYS_XERCES_XML_SERIALIZE_H_
