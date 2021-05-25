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

#ifndef _OASYS_XML_SERIALIZE_H_
#define _OASYS_XML_SERIALIZE_H_

#include <vector>
#include <sstream>
#include <sys/time.h>

#include "Serialize.h"
#include "../util/StringBuffer.h"
#include "../xml/XMLDocument.h"
#include "../xml/XMLObject.h"

namespace oasys {

/**
 * XMLMarshal implements common functionality for building up
 * an XML document.
 */
class XMLMarshal : public SerializeAction {
public:
    XMLMarshal(ExpandableBuffer *buf, const char *root_tag);

    // Virtual process functions inherited from SerializeAction
    void end_action();

    using SerializeAction::process;
    void process(const char *name, SerializableObject* object);
    void process(const char *name, u_int64_t *i);
    void process(const char *name, u_int32_t *i);
    void process(const char *name, u_int16_t *i);
    void process(const char *name, u_int8_t *i);
    void process(const char *name, bool *b);
    void process(const char *name, u_char *bp, u_int32_t len);
    void process(const char*            name, 
                  BufferCarrier<u_char>* carrier,
                  size_t*                lenp);
    void process(const char*            name,
                 BufferCarrier<u_char>* carrier,
                 size_t*                lenp,
                 u_char                 terminator);
    void process(const char*            name, 
                 BufferCarrier<u_char>* carrier);
    void process(const char*            name,
                 BufferCarrier<u_char>* carrier,
                 u_char                 terminator);
    void process(const char *name, std::string *s);
    void process(const char* name, const InAddrPtr& a);

    /// Accessor to the internal XMLDocument
    const XMLDocument& doc() const { return doc_; }
    
protected:
    StringBuffer buf_;  ///< completed document buffer
    XMLDocument doc_;
    XMLObject *current_node_;
};

/**
 * Interface designed to be implemented by third-party
 * XML parsers
 */
class XMLUnmarshal : public SerializeAction {
public:
    XMLUnmarshal()
        : SerializeAction(Serialize::UNMARSHAL,
          Serialize::CONTEXT_UNKNOWN) {}

    /**
     * Parse the provided string buffer.
     * @return next element tag found or 0 on failure
     */
    virtual const char * parse(const char *xml_doc) = 0;
};

} // namespace oasys

#endif // _OASYS_XML_SERIALIZE_H_
