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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#ifdef XERCES_C_ENABLED

#include <sys/stat.h>
#include <errno.h>
#include <string>
#include "XercesXMLSerialize.h"
#include "../thread/Mutex.h"
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/util/Base64.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/validators/schema/SchemaGrammar.hpp>
#include <xercesc/internal/XMLGrammarPoolImpl.hpp>

namespace oasys {

XercesXMLUnmarshal::XercesXMLUnmarshal(bool validation,
                                       const char *schema)
    : Logger("XercesXMLUnmarshal", "/XercesXMLUnmarshal"), root_tag_str(NULL),
         root_elem_(0)
{    
    XercesXMLUnmarshal::lock_->lock("Constructing XercesXMLUnmarshal");

    if (validation) {
        // check that we have a good schema
        bool error = false;
        struct stat buf;

        if (stat(schema, &buf)) {
            log_warn("failed to open schema_file: %s", std::strerror(errno));
            error = true;
        } else if (! S_ISREG(buf.st_mode)) {
            log_warn("%s: not a regular file", schema);
            error = true;
        }

        if (error) {
            log_warn("disabling server message validation");
            validation = false;
        }
    }

    // initialize the xerces-c library
    xercesc::XMLPlatformUtils::Initialize();

    static const XMLCh LS[] = { xercesc::chLatin_L,
        xercesc::chLatin_S, xercesc::chNull };

     // Get a DOM implementation capable of "Load and Save"
    impl_ = xercesc::DOMImplementationRegistry::getDOMImplementation(LS);

    if (validation) {
        // create a grammar pool
        pool_ = new xercesc::XMLGrammarPoolImpl(
            xercesc::XMLPlatformUtils::fgMemoryManager);

        // create the DOM parser
        parser_ = (reinterpret_cast<xercesc::DOMImplementationLS *>(impl_))->
            createDOMBuilder(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS, 0,
                xercesc::XMLPlatformUtils::fgMemoryManager,
                XercesXMLUnmarshal::pool_);

        // use the SGXML scanner
        parser_->setProperty(xercesc::XMLUni::fgXercesScannerName,
            (void *) xercesc::XMLUni::fgSGXMLScanner);

        // turn on DOM validation
        if (parser_->canSetFeature(xercesc::XMLUni::fgDOMValidation, true))
            parser_->setFeature(xercesc::XMLUni::fgDOMValidation, true);

        // used cached grammar during message validation
        if (parser_->canSetFeature(xercesc::XMLUni::fgXercesUseCachedGrammarInParse, true))
            parser_->setFeature(xercesc::XMLUni::fgXercesUseCachedGrammarInParse, true);

        // check the schema when loading
        if (parser_->canSetFeature(xercesc::XMLUni::fgXercesSchemaFullChecking, true))
            parser_->setFeature(xercesc::XMLUni::fgXercesSchemaFullChecking, true);

        // look to the cache when validating messages with no namespace
        XMLCh empty_str = 0x00;
        parser_->setProperty(xercesc::XMLUni::fgXercesSchemaExternalNoNameSpaceSchemaLocation,
            (void *) &empty_str);

        // load the grammar pool
        XMLCh *w_schema = xercesc::XMLString::transcode(schema);
        parser_->loadGrammar(w_schema, xercesc::Grammar::SchemaGrammarType, true);
        xercesc::XMLString::release(&w_schema);
        pool_->lockPool();
    } else {
        parser_ = (reinterpret_cast<xercesc::DOMImplementationLS *>(impl_))->
            createDOMBuilder(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS, 0);

        // use the WFXML scanner
        parser_->setProperty(xercesc::XMLUni::fgXercesScannerName,
            (void *) xercesc::XMLUni::fgWFXMLScanner);
    }

    XercesXMLUnmarshal::lock_->unlock();
}

XercesXMLUnmarshal::~XercesXMLUnmarshal()
{
    XercesXMLUnmarshal::lock_->lock("Deconstructing XercesXMLUnmarshal");

    xercesc::XMLString::release(&root_tag_str);
    parser_->release();
    xercesc::XMLPlatformUtils::Terminate();

    XercesXMLUnmarshal::lock_->unlock();
}

const xercesc::DOMDocument *
XercesXMLUnmarshal::doc(const char *xml_doc)
{
    if (! xml_doc) {
        log_warn("parser received empty xml document");
        signal_error();
        return 0;
    }

    // load an error handler
    ValidationError error_handler;
    parser_->setErrorHandler(&error_handler);

    // parse the given xml document
    xercesc::MemBufInputSource message(
        reinterpret_cast<const XMLByte * const>(xml_doc),
        strlen(xml_doc), "message");
    xercesc::Wrapper4InputSource xml_source(&message, false);
    parser_->resetDocumentPool();
    doc_ = parser_->parse(xml_source);

    // was the message valid?
    if (error_handler.is_set()) {
        log_warn("message dropped\n\t%s \n\toffending message was: %s",
            error_handler.message(), xml_doc);
        signal_error();
        return 0;
    }

    return doc_;
}

// Parse, optionally validate, and build a DOM tree
// from the supplied xml document
const char *
XercesXMLUnmarshal::parse(const char *xml_doc)
{
    if (root_elem_) return next_elem();

    if (! xml_doc) {
        log_warn("parser received empty xml document");
        signal_error();
        return 0;
    }

    // load an error handler
    ValidationError error_handler;
    parser_->setErrorHandler(&error_handler);

    // parse the given xml document
    xercesc::MemBufInputSource message(
        reinterpret_cast<const XMLByte * const>(xml_doc),
        strlen(xml_doc), "message");
    xercesc::Wrapper4InputSource xml_source(&message, false);
    parser_->resetDocumentPool();
    doc_ = parser_->parse(xml_source);

    // was the message valid?
    if (error_handler.is_set()) {
        log_warn("message dropped\n\t%s \n\toffending message was: %s",
            error_handler.message(), xml_doc);
        signal_error();
        return 0;
    }

    // create a walker
    root_elem_ = doc_->getDocumentElement();
    walker_ = doc_->createTreeWalker(root_elem_,
        xercesc::DOMNodeFilter::SHOW_ELEMENT, 0, true);
    const XMLCh *root_tag = root_elem_->getTagName();
    root_tag_str = xercesc::XMLString::transcode(root_tag);
    return root_tag_str;
}

// Return the next element name
const char *
XercesXMLUnmarshal::next_elem()
{
    root_elem_ = reinterpret_cast<xercesc::DOMElement *>(
        walker_->nextNode());

    if (root_elem_) {
        const XMLCh *root_tag = root_elem_->getTagName();
        xercesc::XMLString::release(&root_tag_str);
        char *root_tag_str = xercesc::XMLString::transcode(root_tag);
        return root_tag_str;
    } else {
        walker_->release();
        return 0;
    }
}

void 
XercesXMLUnmarshal::process(const char *name, SerializableObject* object)
{
    const char *next =  next_elem();

    // if there are no more elements, just return
    if (next == 0) 
        return;

    // check that we actually have the right child, otherwise return
    if (strcmp(name, next) != 0) {
        log_warn("unexpected element found. Expected: %s; found: %s",
                 name, next);
        signal_error();
        return;
    }

    object->serialize(this);
}

void
XercesXMLUnmarshal::process(const char *name, u_int64_t *i)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    *i = atoll(value);

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

void
XercesXMLUnmarshal::process(const char *name, u_int32_t *i)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    *i = atoi(value);

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

void
XercesXMLUnmarshal::process(const char *name, u_int16_t *i)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    *i = atoi(value);

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

void
XercesXMLUnmarshal::process(const char *name, u_int8_t *i)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    *i = atoi(value);

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

void
XercesXMLUnmarshal::process(const char *name, bool *b)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    *b = (strcmp(value, "true") == 0) ? true : false;

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

void
XercesXMLUnmarshal::process(const char *name, u_char *bp, u_int32_t len)
{
    if (len < 2) return;

    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    std::string value(
        xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name)));

    char *sbp = reinterpret_cast<char *>(bp);
    memset(sbp, '\0', len);
    value.copy(sbp, len - 1);

    xercesc::XMLString::release(&tag_name);
}

void 
XercesXMLUnmarshal::process(const char*            name, 
                            BufferCarrier<u_char>* carrier)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    // XXX/bowei -- I don't know the xercesc API but this seems to be
    // a leak which needs a release() call:
    std::string value(xercesc::XMLString::transcode(
                          root_elem_->getAttribute(tag_name)));
    xercesc::XMLString::release(&tag_name); 

    u_char* buf = static_cast<u_char*>(malloc(sizeof(u_char) * value.size()));
    memcpy(buf, value.data(), value.size());
    carrier->set_buf(buf, value.size(), true);
}
    
void 
XercesXMLUnmarshal::process(const char*            name,
                            BufferCarrier<u_char>* carrier,
                            u_char                 terminator)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    // XXX/bowei -- I don't know the xercesc API but this seems to be
    // a leak which needs a release() call:
    std::string value(xercesc::XMLString::transcode(
                          root_elem_->getAttribute(tag_name)));
    xercesc::XMLString::release(&tag_name); 

    u_char* buf = static_cast<u_char*>(malloc(sizeof(u_char) * value.size() + 1));
    memcpy(buf, value.data(), value.size());
    buf[value.size()] = terminator;
    carrier->set_buf(buf, value.size(), true);
}

/*
void
XercesXMLUnmarshal::process(const char* name, u_char** bp,
                            u_int32_t* lenp, int flags)
{
    if (flags & Serialize::ALLOC_MEM) {
        u_int32_t len = *lenp;
        if (flags & Serialize::NULL_TERMINATED)
            len += 1;
        *bp = static_cast<u_char*>(malloc(len));
        if (*bp == 0) {
            signal_error();
            return;
        }
        *lenp = len;
    }

    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    std::string value(
        xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name)));

    char *sbp = reinterpret_cast<char *>(*bp);
    if (flags & Serialize::NULL_TERMINATED) {
        memset(sbp, '\0', *lenp);
        value.copy(sbp, *lenp - 1);
    } else {
        value.copy(sbp, *lenp);
    }

    xercesc::XMLString::release(&tag_name);
}*/

void
XercesXMLUnmarshal::process(const char *name, std::string *s)
{
    XMLCh *tag_name = xercesc::XMLString::transcode(name);
    char *value = xercesc::XMLString::transcode(
        root_elem_->getAttribute(tag_name));

    s->assign(value);

    xercesc::XMLString::release(&tag_name);
    xercesc::XMLString::release(&value);
}

bool
ValidationError::handleError(const xercesc::DOMError &domError)
{
    severity_ = domError.getSeverity();

    delete [] message_;
    message_ = xercesc::XMLString::transcode(domError.getMessage());

    return set_ = true;
}

oasys::Mutex* XercesXMLUnmarshal::lock_ = new oasys::Mutex("XercesXMLUnmarshal");

} // namespace oasys
#endif // XERCES_C_ENABLED
