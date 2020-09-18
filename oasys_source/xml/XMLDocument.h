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

#ifndef _OASYS_XML_DOCUMENT_H_
#define _OASYS_XML_DOCUMENT_H_

#include "../debug/DebugUtils.h"
#include "../util/StringBuffer.h"

namespace oasys {

class XMLObject;

/**
 * An object encapsulation of an XML document, consisting of some
 * amount of unparsed header information (i.e. processing
 * instructions, ENTITY references, etc), then a root tag XMLObject.
 */
class XMLDocument {
public:
    /**
     * Default constructor.
     */
    XMLDocument();

    /**
     * Destructor
     */
    ~XMLDocument();

    /// @{ Accessors
    const std::string header() const { return header_; }
    const XMLObject*  root()   const { return root_; }
    /// @}

    /**
     * Set the root tag. Assumes ownership of the object.
     */
    void set_root(XMLObject* root);

    /**
     * Append some header data
     */
    void add_header(const char* text, size_t len = 0);

    /**
     * Generate formatted XML text and put it into the given buffer.
     *
     * @param indent     The number of spaces to indent for subelements.
     */
    void to_string(StringBuffer* buf, int indent) const;

protected:
    std::string header_;
    XMLObject*  root_;

    NO_ASSIGN(XMLDocument);

};

} // namespace oasys

#endif /* _OASYS_XML_DOCUMENT_H_ */
