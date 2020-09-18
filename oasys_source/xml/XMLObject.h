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

#ifndef _OASYS_XML_OBJECT_H_
#define _OASYS_XML_OBJECT_H_

#include <string>
#include <vector>
#include "../debug/DebugUtils.h"

namespace oasys {

class StringBuffer;

/**
 * A simple object-based representation of an XML entity.
 *
 * Note that the class assumes memory management responsibility for
 * all child objects, i.e. when the destructor is called, all child
 * objects are recursively destroyed as well (using delete).
 */
class XMLObject {
public:
    /**
     * Type for the attribute list is a vector of strings of the form
     * "attr1" "val1" "attr2" "val2" ...
     */
    typedef std::vector<std::string> Attrs;

    /**
     * Type for the attribute list is a vector of strings of the form
     * "target1" "data1" "target2" "data2" ...
     */
    typedef std::vector<std::string> ProcInsts;

    /**
     * Type for the element list is a vector of XMLObject* pointers.
     */
    typedef std::vector<XMLObject*> Elements;
    
    /**
     * The constructor requires the tag name.
     */
    XMLObject(const std::string& tag);

    /**
     * The destructor recursively deletes all subelements.
     */
    ~XMLObject();

    /// @{ Accessors
    const std::string& tag()      const { return tag_; }
    const Attrs& attrs()          const { return attrs_; }
    const ProcInsts& proc_insts() const { return proc_insts_; }
    const Elements& elements()    const { return elements_; }
    const std::string& text()     const { return text_; }
    XMLObject* parent()           const { return parent_; }
    ///

    /**
     * Append an attribute/string value pair.
     */
    void add_attr(const std::string& attr, const std::string& val);
    
    /**
     * Append a processing instruction / value pair.
     */
    void add_proc_inst(const std::string& target,
                       const std::string& data);
    
    /**
     * Append a child element and assume memory management
     * responsibility for it.
     */
    void add_element(XMLObject* child);

    /**
     * Append some text data.
     */
    void add_text(const char* text, size_t len = 0);

    /**
     * Recursively generate formatted XML text and put it into the
     * given buffer.
     *
     * @param indent Indentation control for subelements. If -1, put
     *               all subelements and text on the same line. If
     *               >= 0 then put each tag on a new line indented
     *               with the given number of spaces.
     *
     * @param cur_indent The current cumulative indentation
     */
    void to_string(StringBuffer* buf, int indent, int cur_indent = 0) const;
    
protected:
    std::string tag_;
    Attrs       attrs_;
    ProcInsts   proc_insts_;
    Elements    elements_;
    std::string text_;
    XMLObject*  parent_;
    
    static std::string make_xml_safe(const std::string& str);

    /**
     * We don't support assignment of the class.
     */
    NO_ASSIGN_COPY(XMLObject);
};

} // namespace oasys

#endif /* _OASYS_XML_OBJECT_H_ */
