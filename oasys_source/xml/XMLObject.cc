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

#include "XMLObject.h"
#include "util/StringBuffer.h"

namespace oasys {

//----------------------------------------------------------------------
XMLObject::XMLObject(const std::string& tag)
    : tag_(tag), parent_(NULL)
{
}

//----------------------------------------------------------------------
XMLObject::~XMLObject()
{
    Elements::iterator i;
    for (i = elements_.begin(); i != elements_.end(); ++i) {
        delete *i;
    }
}

//----------------------------------------------------------------------
void
XMLObject::add_attr(const std::string& attr, const std::string& val)
{
    attrs_.push_back(attr);
    attrs_.push_back(val);
}

//----------------------------------------------------------------------

void
XMLObject::add_proc_inst(const std::string& target,
                         const std::string& data)
{
    proc_insts_.push_back(target);
    proc_insts_.push_back(data);
}
    
//----------------------------------------------------------------------
void
XMLObject::add_element(XMLObject* child)
{
    elements_.push_back(child);
    child->parent_ = this;
}

//----------------------------------------------------------------------
void
XMLObject::add_text(const char* text, size_t len)
{
    if (len == 0) {
        len = strlen(text);
    }
    
    text_.append(text, len);
}

//----------------------------------------------------------------------
void
XMLObject::to_string(StringBuffer* buf, int indent, int cur_indent) const
{
    static const char* space = "                                        "
                               "                                        ";
    
    buf->appendf("%.*s<%s", cur_indent, space, tag_.c_str());
    for (unsigned int i = 0; i < attrs_.size(); i += 2)
    {
        buf->appendf(" %s=\"%s\"", attrs_[i].c_str(),
                     make_xml_safe(attrs_[i+1]).c_str());
    }

    // shorthand for attribute-only tags
    if (proc_insts_.empty() && elements_.empty() && text_.size() == 0)
    {
        buf->appendf("/>");
        return;
    }
    else
    {
        buf->appendf(">%s", (indent == -1) ? "" : "\n");

    }
    
    for (unsigned int i = 0; i < proc_insts_.size(); i += 2)
    {
        buf->appendf("<?%s %s?>%s",
                     proc_insts_[i].c_str(), proc_insts_[i+1].c_str(),
                     (indent == -1) ? "" : "\n");
    }
    
    for (unsigned int i = 0; i < elements_.size(); ++i)
    {
        elements_[i]->to_string(buf, indent, (indent > 0) ? cur_indent + indent : 0);
    }

    buf->append(text_);

    buf->appendf("%.*s</%s>", cur_indent, space, tag_.c_str());
}

std::string
XMLObject::make_xml_safe(const std::string& str)
{
    std::string result;
    
    for (size_t i = 0; i < str.length(); ++i) {
        switch (str[i]) {
            case '\"': result += "&quot;"; break;
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '\'': result += "&apos;"; break;
            default: result += str[i]; break;
        }
    }
         
    return result;
}

} // namespace oasys
